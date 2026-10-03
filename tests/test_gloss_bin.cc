/*
 * GlossBin 单元测试：构造临时索引文件，验证打开、查词命中/未命中、
 * 空表与损坏文件（magic 错误、索引越界）等边界。
 */
#include "minitest.h"

#include "../src/gloss_bin.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

using qingjian::GlossBin;

namespace {

/* 小端写无符号整数到字节缓冲。 */
void push_u32_le(std::vector<uint8_t>& buf, uint32_t v) {
  buf.push_back(static_cast<uint8_t>(v & 0xff));
  buf.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
  buf.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
  buf.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
}

void push_u64_le(std::vector<uint8_t>& buf, uint64_t v) {
  push_u32_le(buf, static_cast<uint32_t>(v & 0xffffffff));
  push_u32_le(buf, static_cast<uint32_t>(v >> 32));
}

/*
 * 按格式写一份测试索引。items 须已按 key 的 UTF-8 字节序升序排好。
 * 每条：key 字节后接 value 字节，全部压入 arena，索引记录四段偏移长度。
 */
void build_gloss(const std::string& path,
                 const std::vector<std::pair<std::string, std::string>>& items) {
  std::vector<uint8_t> arena;
  std::vector<uint8_t> index;
  for (const auto& item : items) {
    uint32_t key_off = static_cast<uint32_t>(arena.size());
    arena.insert(arena.end(), item.first.begin(), item.first.end());
    uint32_t val_off = static_cast<uint32_t>(arena.size());
    arena.insert(arena.end(), item.second.begin(), item.second.end());
    push_u32_le(index, key_off);
    push_u32_le(index, static_cast<uint32_t>(item.first.size()));
    push_u32_le(index, val_off);
    push_u32_le(index, static_cast<uint32_t>(item.second.size()));
  }
  std::ofstream f(path, std::ios::binary);
  f.write("QJGLOSS1", 8);
  std::vector<uint8_t> count;
  push_u64_le(count, items.size());
  f.write(reinterpret_cast<const char*>(count.data()), count.size());
  f.write(reinterpret_cast<const char*>(index.data()), index.size());
  f.write(reinterpret_cast<const char*>(arena.data()), arena.size());
}

/* 取一个不重复的临时文件路径。 */
std::string temp_path(const char* tag) {
  return std::string("/tmp/qingjian_test_") + tag + "_" +
         std::to_string(static_cast<long long>(getpid())) + ".bin";
}

}  // namespace

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
  push_u64_le(count, 100);
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
