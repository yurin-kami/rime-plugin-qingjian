/*
 * 跨语言对拍：读回 Python 工具生成的 gloss.bin，断言已知词条命中。
 *
 * 用法：build/test_gloss_fixture <gloss.bin>
 * 由 make test-data 生成样本后调用。
 */
#include "minitest.h"

#include "../src/gloss_bin.h"

#include <string_view>

const char* g_bin_path = nullptr;

TEST(读回Python生成的索引) {
  qingjian::GlossBin bin;
  CHECK(bin.Open(g_bin_path));
  CHECK_EQ(bin.size(), static_cast<size_t>(5));
  CHECK_EQ(*bin.Lookup("开发"), std::string_view("v. develop / v. exploit"));
  CHECK_EQ(*bin.Lookup("中文"), std::string_view("n. Chinese"));
  CHECK_EQ(*bin.Lookup("苹果"), std::string_view("n. apple"));
  CHECK(!bin.Lookup("不存在").has_value());
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("用法: %s <gloss.bin>\n", argv[0]);
    return 2;
  }
  g_bin_path = argv[1];
  return minitest::run_all();
}
