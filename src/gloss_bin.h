/*
 * 释义表二进制索引读取器。
 *
 * 文件布局（小端序）见 arch.md：
 *   [0,8)        magic，固定 8 字节 "QJGLOSS1"
 *   [8,16)       条目数 N（u64 LE）
 *   [16, 16+16N) 定宽索引，每条 16 字节：
 *                key_off/key_len/val_off/val_len（各 u32 LE）
 *   [16+16N, 尾) 字符串 arena：key 字节后接 value 字节，由偏移与长度分隔
 *
 * 运行时 mmap 整个文件，对索引二分查找；命中返回指向映射内存的
 * string_view，零拷贝、零分配。文件按 key 的 UTF-8 字节序排序。
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace qingjian {

class GlossBin {
 public:
  GlossBin() = default;
  ~GlossBin();

  /* 持有映射，不可拷贝；允许移动。 */
  GlossBin(const GlossBin&) = delete;
  GlossBin& operator=(const GlossBin&) = delete;
  GlossBin(GlossBin&& other) noexcept;
  GlossBin& operator=(GlossBin&& other) noexcept;

  /* 打开并映射文件；失败返回 false，原因见 last_error()。 */
  bool Open(const std::string& path);

  /* 查词：命中返回 value，未命中返回 nullopt。 */
  std::optional<std::string_view> Lookup(std::string_view key) const;

  /* 条目数。 */
  size_t size() const { return count_; }
  bool empty() const { return count_ == 0; }

  /* 是否已成功打开。 */
  explicit operator bool() const { return data_ != nullptr; }

  const std::string& last_error() const { return error_; }

 private:
  void Reset();

  uint8_t* data_ = nullptr;    /* mmap 基址 */
  size_t data_len_ = 0;        /* 映射字节数 */
  uint64_t count_ = 0;         /* 条目数 */
  const uint8_t* entries_ = nullptr;  /* 索引区基址 */
  const uint8_t* arena_ = nullptr;    /* 字符串池基址 */
  std::string error_;
};

}  // namespace qingjian
