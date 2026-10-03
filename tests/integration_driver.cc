/*
 * 集成测试驱动：通过公开 API 驱动真实输入，断言候选右侧出现释义。
 *
 * 场景一（中→英）：luna_pinyin_simp 输入 "kaifa"，候选「开发」注释含 develop。
 * 场景二（英→中）：test_en 输入 "development"，候选注释含「发展」。
 *
 * 由 tests/run_integration.sh 准备临时环境后调用：
 *   LD_LIBRARY_PATH=<tmp>/lib <driver> <shared_data_dir> <user_data_dir>
 */
#include <rime_api.h>

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

/* 失败用例计数。 */
int g_failures = 0;

/* 记录一个检查结果，失败时累计。 */
void check(bool cond, const char* what) {
  if (cond) {
    std::printf("[PASS] %s\n", what);
  } else {
    std::printf("[FAIL] %s\n", what);
    ++g_failures;
  }
}

/*
 * 模拟输入一段按键，逐条打印候选，返回「候选 want_text 的注释是否
 * 包含 want_substr」。
 */
bool check_comment(RimeApi* api, RimeSessionId session, const char* keys,
                   const char* want_text, const char* want_substr) {
  api->simulate_key_sequence(session, keys);
  RimeContext ctx;
  std::memset(&ctx, 0, sizeof(ctx));
  RIME_STRUCT_INIT(RimeContext, ctx);
  api->get_context(session, &ctx);

  bool found = false;
  for (int i = 0; i < ctx.menu.num_candidates; ++i) {
    const RimeCandidate& c = ctx.menu.candidates[i];
    std::printf("  候选 %d: %s | 注释: %s\n", i + 1, c.text,
                c.comment ? c.comment : "(无)");
    if (c.text && c.comment && std::strcmp(c.text, want_text) == 0 &&
        std::strstr(c.comment, want_substr) != nullptr) {
      found = true;
    }
  }
  api->free_context(&ctx);
  return found;
}

}

/* 初始化 librime、部署方案、分别跑中→英与英→中两个场景。 */
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
  /* 部署可能因「已是最新」而返回 false，只记录不判失败。 */
  std::printf("[INFO] deploy luna_pinyin_simp 返回: %d\n",
              static_cast<int>(api->deploy_schema("luna_pinyin_simp")));
  std::printf("[INFO] deploy test_en 返回: %d\n",
              static_cast<int>(api->deploy_schema("test_en")));
  api->start_maintenance(True);
  for (int i = 0; i < 2000 && api->is_maintenance_mode(); ++i) {
    usleep(10000);
  }
  api->join_maintenance_thread();

  /* 场景一：中→英。 */
  RimeSessionId s1 = api->create_session();
  check(api->select_schema(s1, "luna_pinyin_simp"), "选择方案 luna_pinyin_simp");
  check(check_comment(api, s1, "kaifa", "开发", "develop"),
        "候选「开发」右侧出现英文释义 develop");
  api->destroy_session(s1);

  /* 场景二：英→中。 */
  RimeSessionId s2 = api->create_session();
  check(api->select_schema(s2, "test_en"), "选择方案 test_en");
  check(check_comment(api, s2, "development", "development", "发展"),
        "候选 development 右侧出现中文释义 发展");
  api->destroy_session(s2);

  api->finalize();

  std::printf("%s\n", g_failures == 0 ? "集成测试通过" : "集成测试失败");
  return g_failures == 0 ? 0 : 1;
}
