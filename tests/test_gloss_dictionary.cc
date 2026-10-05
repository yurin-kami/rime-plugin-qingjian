/*
 * GlossDictionary 单元测试：验证多词典优先级查找、失败文件跳过、空表。
 */
#include "minitest.h"
#include "bin_writer.h"

#include "../src/gloss_dictionary.h"

#include <cstdio>
#include <filesystem>
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

TEST(非ASCII目录路径可解析) {
  std::filesystem::path base = std::filesystem::temp_directory_path() /
      std::filesystem::u8path(std::string("qj_青简_") +
                              std::to_string(qingjian_test::current_pid()));
  std::filesystem::path user = base / std::filesystem::u8path("用户数据");
  std::filesystem::create_directories(user);
  const std::filesystem::path file =
      user / std::filesystem::u8path("词典.zh_en.bin");
  build_gloss(file.u8string(), {{"开发", "develop"}});
  CHECK(std::filesystem::exists(file));

  std::vector<std::string> paths = qingjian::ResolveGlossPaths(
      {user.u8string()}, {"词典"}, "zh_en.bin");
  CHECK_EQ(paths.size(), static_cast<size_t>(1));
  CHECK_EQ(paths[0], file.u8string());

  GlossDictionary dict;
  CHECK_EQ(dict.Load(paths), static_cast<size_t>(1));
  CHECK_EQ(*dict.Lookup("开发"), std::string_view("develop"));
  std::filesystem::remove_all(base);
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

TEST(路径解析用户目录优先) {
  std::filesystem::path base = std::filesystem::temp_directory_path() /
      ("qj_resolve_" + std::to_string(qingjian_test::current_pid()));
  std::filesystem::path user = base / "user";
  std::filesystem::path shared = base / "shared";
  std::filesystem::create_directories(user);
  std::filesystem::create_directories(shared);
  build_gloss((user / "a.zh_en.bin").u8string(), {{"开发", "user"}});
  build_gloss((shared / "a.zh_en.bin").u8string(), {{"开发", "shared"}});
  build_gloss((shared / "b.zh_en.bin").u8string(), {{"中文", "Chinese"}});

  std::vector<std::string> paths = qingjian::ResolveGlossPaths(
      {user.u8string(), shared.u8string()}, {"a", "b", "missing"}, "zh_en.bin");
  CHECK_EQ(paths.size(), static_cast<size_t>(2));
  CHECK_EQ(paths[0], (user / "a.zh_en.bin").u8string());
  CHECK_EQ(paths[1], (shared / "b.zh_en.bin").u8string());

  std::filesystem::remove_all(base);
}

TEST_MAIN()
