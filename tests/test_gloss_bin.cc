/*
 * GlossBin 单元测试：构造临时索引文件，验证打开、查词命中/未命中、
 * 空表与损坏文件（magic 错误、索引越界）等边界。
 */
#include "minitest.h"
#include "bin_writer.h"

#include "../src/gloss_bin.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

using qingjian::GlossBin;
using qingjian_test::build_gloss;
using qingjian_test::temp_path;

TEST(打开空表并查词未命中) {
  std::string path = temp_path("empty");
  build_gloss(path, {});
  GlossBin bin;
  CHECK(bin.Open(path));
  CHECK(bin.empty());
  CHECK_EQ(bin.size(), static_cast<size_t>(0));
  CHECK(!bin.Lookup("开发").has_value());
  std::remove(path.c_str());
}

TEST(命中返回对应释义) {
  std::string path = temp_path("hit");
  build_gloss(path, {{"中文", "Chinese language"},
                     {"开发", "v. develop\tn. development"},
                     {"架构", "architecture"}});
  GlossBin bin;
  CHECK(bin.Open(path));
  CHECK_EQ(bin.size(), static_cast<size_t>(3));
  auto v = bin.Lookup("开发");
  CHECK(v.has_value());
  CHECK_EQ(*v, std::string_view("v. develop\tn. development"));
  CHECK_EQ(*bin.Lookup("中文"), std::string_view("Chinese language"));
  CHECK_EQ(*bin.Lookup("架构"), std::string_view("architecture"));
  std::remove(path.c_str());
}

TEST(未命中返回空) {
  std::string path = temp_path("miss");
  build_gloss(path, {{"中文", "Chinese language"}});
  GlossBin bin;
  CHECK(bin.Open(path));
  CHECK(!bin.Lookup("不存在的词").has_value());
  CHECK(!bin.Lookup("中").has_value());
  std::remove(path.c_str());
}

TEST(magic不匹配拒绝打开) {
  std::string path = temp_path("badmagic");
  std::ofstream f(path, std::ios::binary);
  f.write("WRONGMAG", 8);
  f.close();
  GlossBin bin;
  CHECK(!bin.Open(path));
  CHECK(!bin.last_error().empty());
  std::remove(path.c_str());
}

TEST(索引越界拒绝打开) {
  std::string path = temp_path("truncated");
  std::ofstream f(path, std::ios::binary);
  f.write("QJGLOSS1", 8);
  std::vector<uint8_t> count;
  qingjian_test::push_u64_le(count, 100);
  f.write(reinterpret_cast<const char*>(count.data()), count.size());
  f.close();
  GlossBin bin;
  CHECK(!bin.Open(path));
  std::remove(path.c_str());
}

TEST(移动语义后仍可查词) {
  std::string path = temp_path("move");
  build_gloss(path, {{"你好", "hello"}});
  GlossBin bin;
  CHECK(bin.Open(path));
  GlossBin moved = std::move(bin);
  CHECK(!bin);
  CHECK(moved);
  CHECK_EQ(*moved.Lookup("你好"), std::string_view("hello"));
  std::remove(path.c_str());
}

TEST_MAIN()
