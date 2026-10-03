/*
 * GlossDictionary 单元测试：验证多词典优先级查找、失败文件跳过、空表。
 */
#include "minitest.h"
#include "bin_writer.h"

#include "../src/gloss_dictionary.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using qingjian::GlossDictionary;
using qingjian_test::build_gloss;
using qingjian_test::temp_path;

TEST(优先级高的词典先命中) {
  std::string personal = temp_path("dict_personal");
  std::string bundled = temp_path("dict_bundled");
  build_gloss(personal, {{"开发", "v. code"}});
  build_gloss(bundled, {{"开发", "v. develop"}, {"中文", "n. Chinese"}});

  GlossDictionary dict;
  CHECK_EQ(dict.Load({personal, bundled}), static_cast<size_t>(2));
  CHECK_EQ(dict.dictionaries(), static_cast<size_t>(2));
  CHECK_EQ(*dict.Lookup("开发"), std::string_view("v. code"));
  CHECK_EQ(*dict.Lookup("中文"), std::string_view("n. Chinese"));
  std::remove(personal.c_str());
  std::remove(bundled.c_str());
}

TEST(失败文件被跳过) {
  std::string personal = temp_path("dict_ok");
  build_gloss(personal, {{"开发", "v. code"}});

  GlossDictionary dict;
  CHECK_EQ(dict.Load({"/tmp/不存在_文件.bin", personal}),
           static_cast<size_t>(1));
  CHECK_EQ(dict.dictionaries(), static_cast<size_t>(1));
  CHECK_EQ(*dict.Lookup("开发"), std::string_view("v. code"));
  std::remove(personal.c_str());
}

TEST(全部未命中返回空) {
  std::string bundled = temp_path("dict_miss");
  build_gloss(bundled, {{"中文", "n. Chinese"}});

  GlossDictionary dict;
  dict.Load({bundled});
  CHECK(!dict.Lookup("不存在").has_value());
  std::remove(bundled.c_str());
}

TEST(全部加载失败为空) {
  GlossDictionary dict;
  CHECK_EQ(dict.Load({"/tmp/不存在_1.bin", "/tmp/不存在_2.bin"}),
           static_cast<size_t>(0));
  CHECK(dict.empty());
  CHECK(!dict.Lookup("开发").has_value());
}

TEST_MAIN()
