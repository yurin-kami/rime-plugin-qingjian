/*
 * 测试辅助：在测试里构造一份最小 gloss.bin 文件。
 *
 * 与 tools/build_gloss.py 的产物格式一致，供纯 C++ 单测使用。
 */
#pragma once

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace qingjian_test {

/* 小端写无符号 32 位整数到字节缓冲。 */
inline void push_u32_le(std::vector<uint8_t>& buf, uint32_t v) {
  buf.push_back(static_cast<uint8_t>(v & 0xff));
  buf.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
  buf.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
  buf.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
}

/* 小端写无符号 64 位整数到字节缓冲。 */
inline void push_u64_le(std::vector<uint8_t>& buf, uint64_t v) {
  push_u32_le(buf, static_cast<uint32_t>(v & 0xffffffff));
  push_u32_le(buf, static_cast<uint32_t>(v >> 32));
}

/*
 * 按格式写一份测试索引。items 须已按 key 的 UTF-8 字节序升序排好。
 * 每条：key 字节后接 value 字节，全部压入 arena，索引记录四段偏移长度。
 */
inline void build_gloss(
    const std::string& path,
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
inline std::string temp_path(const char* tag) {
  return std::string("/tmp/qingjian_test_") + tag + "_" +
         std::to_string(static_cast<long long>(getpid())) + ".bin";
}

}
