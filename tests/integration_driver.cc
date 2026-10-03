/*
 * 集成测试驱动：通过公开 API 驱动一次真实输入，断言候选右侧出现释义。
 *
 * 由 tests/run_integration.sh 准备临时环境后调用：
 *   LD_LIBRARY_PATH=<tmp>/lib <driver> <shared_data_dir> <user_data_dir>
 *
 * 依赖：系统已装 librime 与 rime-luna-pinyin；用户目录里已放好 gloss_filter
 * 补丁与 qingjian/ 释义数据。
 */
#include <rime_api.h>

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

int g_failures = 0;

void check(bool cond, const char* what) {
  if (cond) {
    std::printf("[PASS] %s\n", what);
  } else {
    std::printf("[FAIL] %s\n", what);
    ++g_failures;
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::printf("用法: %s <shared_data_dir> <user_data_dir>\n", argv[0]);
    return 2;
  }
  const char* shared_dir = argv[1];
  const char* user_dir = argv[2];

  RimeApi* api = rime_get_api();

  RimeTraits traits;
  std::memset(&traits, 0, sizeof(traits));
  RIME_STRUCT_INIT(RimeTraits, traits);
  traits.shared_data_dir = shared_dir;
  traits.user_data_dir = user_dir;
  traits.app_name = "rime.qingjian-test";
  traits.log_dir = "";

  api->setup(&traits);
  api->initialize(&traits);

  api->deployer_initialize(&traits);
  check(api->deploy_schema("luna_pinyin"), "部署方案 luna_pinyin");
  api->start_maintenance(True);
  for (int i = 0; i < 2000 && api->is_maintenance_mode(); ++i) {
    usleep(10000);
  }
  api->join_maintenance_thread();

  RimeSessionId session = api->create_session();
  check(api->select_schema(session, "luna_pinyin"), "选择方案 luna_pinyin");

  api->simulate_key_sequence(session, "kaifa");

  RimeContext ctx;
  std::memset(&ctx, 0, sizeof(ctx));
  RIME_STRUCT_INIT(RimeContext, ctx);
  api->get_context(session, &ctx);

  bool found = false;
  for (int i = 0; i < ctx.menu.num_candidates; ++i) {
    const RimeCandidate& c = ctx.menu.candidates[i];
    std::printf("  候选 %d: %s | 注释: %s\n", i + 1, c.text,
                c.comment ? c.comment : "(无)");
    if (c.text && std::strcmp(c.text, "开发") == 0 && c.comment &&
        std::strstr(c.comment, "develop") != nullptr) {
      found = true;
    }
  }
  check(found, "候选「开发」右侧出现英文释义 develop");

  api->free_context(&ctx);
  api->destroy_session(session);
  api->finalize();

  std::printf("%s\n", g_failures == 0 ? "集成测试通过" : "集成测试失败");
  return g_failures == 0 ? 0 : 1;
}
