/*
 * GlossBin 实现：内存映射 + 定宽索引二分查找。
 *
 * Linux/macOS 用 POSIX mmap，Windows 用 CreateFileMapping/MapViewOfFile，
 * 二者均为零拷贝只读映射，语义一致。文件格式与写入端
 * （tools/build_gloss.py）严格一致：小端序、key 按 UTF-8 字节序升序。
 */
#include "gloss_bin.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <filesystem>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cstring>
#include <limits>
#include <utility>

namespace qingjian {

/* 文件内使用的常量与小端整数读取工具。 */
namespace {

constexpr size_t kHeaderSize = 16;
constexpr size_t kEntrySize = 16;
constexpr const char kMagic[8] = {'Q', 'J', 'G', 'L', 'O', 'S', 'S', '1'};

/* 小端读取无符号 32 位整数。 */
uint32_t read_u32_le(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
         (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

/* 小端读取无符号 64 位整数。 */
uint64_t read_u64_le(const uint8_t* p) {
  return uint64_t(read_u32_le(p)) | (uint64_t(read_u32_le(p + 4)) << 32);
}

}

/* 析构：释放映射。 */
GlossBin::~GlossBin() { Reset(); }

/* 移动构造：接管对方的映射，并把对方置空。 */
GlossBin::GlossBin(GlossBin&& other) noexcept {
  data_ = other.data_;
  data_len_ = other.data_len_;
  count_ = other.count_;
  entries_ = other.entries_;
  arena_ = other.arena_;
  error_ = std::move(other.error_);
#ifdef _WIN32
  file_handle_ = other.file_handle_;
  mapping_handle_ = other.mapping_handle_;
  other.file_handle_ = nullptr;
  other.mapping_handle_ = nullptr;
#endif
  other.data_ = nullptr;
  other.data_len_ = 0;
  other.count_ = 0;
  other.entries_ = nullptr;
  other.arena_ = nullptr;
}

/* 移动赋值：先释放自身映射，再接管对方。 */
GlossBin& GlossBin::operator=(GlossBin&& other) noexcept {
  if (this != &other) {
    Reset();
    data_ = other.data_;
    data_len_ = other.data_len_;
    count_ = other.count_;
    entries_ = other.entries_;
    arena_ = other.arena_;
    error_ = std::move(other.error_);
#ifdef _WIN32
    file_handle_ = other.file_handle_;
    mapping_handle_ = other.mapping_handle_;
    other.file_handle_ = nullptr;
    other.mapping_handle_ = nullptr;
#endif
    other.data_ = nullptr;
    other.data_len_ = 0;
    other.count_ = 0;
    other.entries_ = nullptr;
    other.arena_ = nullptr;
  }
  return *this;
}

/* 释放映射并复位所有字段。 */
void GlossBin::Reset() {
#ifdef _WIN32
  if (data_ != nullptr) {
    ::UnmapViewOfFile(data_);
  }
  if (mapping_handle_ != nullptr) {
    ::CloseHandle(static_cast<HANDLE>(mapping_handle_));
  }
  if (file_handle_ != nullptr) {
    ::CloseHandle(static_cast<HANDLE>(file_handle_));
  }
  mapping_handle_ = nullptr;
  file_handle_ = nullptr;
#else
  if (data_ != nullptr) {
    ::munmap(data_, data_len_);
  }
#endif
  data_ = nullptr;
  data_len_ = 0;
  count_ = 0;
  entries_ = nullptr;
  arena_ = nullptr;
}

/* 打开并映射文件，校验 magic、索引区与字符串区边界。 */
bool GlossBin::Open(const std::string& path) {
  Reset();
  error_.clear();
#ifdef _WIN32
  std::filesystem::path wpath = std::filesystem::u8path(path);
  HANDLE fh = ::CreateFileW(wpath.c_str(), GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                            nullptr);
  if (fh == INVALID_HANDLE_VALUE) {
    error_ = "无法打开文件: " + path;
    return false;
  }
  LARGE_INTEGER size;
  if (!::GetFileSizeEx(fh, &size) ||
      size.QuadPart < static_cast<LONGLONG>(kHeaderSize) ||
      static_cast<unsigned long long>(size.QuadPart) >
          std::numeric_limits<size_t>::max()) {
    error_ = "文件过小或读取元信息失败";
    ::CloseHandle(fh);
    return false;
  }
  HANDLE mh = ::CreateFileMappingW(fh, nullptr, PAGE_READONLY, 0, 0, nullptr);
  if (mh == nullptr) {
    error_ = "映射失败";
    ::CloseHandle(fh);
    return false;
  }
  void* p = ::MapViewOfFile(mh, FILE_MAP_READ, 0, 0, 0);
  if (p == nullptr) {
    error_ = "映射失败";
    ::CloseHandle(mh);
    ::CloseHandle(fh);
    return false;
  }
  file_handle_ = fh;
  mapping_handle_ = mh;
  data_ = static_cast<uint8_t*>(p);
  data_len_ = static_cast<size_t>(size.QuadPart);
#else
  int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    error_ = "无法打开文件: " + path;
    return false;
  }
  struct stat st;
  if (::fstat(fd, &st) != 0 || st.st_size < static_cast<off_t>(kHeaderSize)) {
    error_ = "文件过小或读取元信息失败";
    ::close(fd);
    return false;
  }
  data_len_ = static_cast<size_t>(st.st_size);
  void* p = ::mmap(nullptr, data_len_, PROT_READ, MAP_SHARED, fd, 0);
  ::close(fd);
  if (p == MAP_FAILED) {
    data_ = nullptr;
    data_len_ = 0;
    error_ = "映射失败";
    return false;
  }
  data_ = static_cast<uint8_t*>(p);
#endif
  if (std::memcmp(data_, kMagic, sizeof(kMagic)) != 0) {
    error_ = "magic 不匹配，不是释义索引文件";
    Reset();
    return false;
  }
  count_ = read_u64_le(data_ + 8);
  if (count_ > (data_len_ - kHeaderSize) / kEntrySize) {
    error_ = "索引区越界，文件损坏";
    Reset();
    return false;
  }
  size_t index_bytes = static_cast<size_t>(count_) * kEntrySize;
  entries_ = data_ + kHeaderSize;
  arena_ = data_ + kHeaderSize + index_bytes;
  const size_t arena_size = data_len_ - kHeaderSize - index_bytes;
  for (uint64_t i = 0; i < count_; ++i) {
    const uint8_t* entry = entries_ + i * kEntrySize;
    const uint64_t key_end = uint64_t(read_u32_le(entry)) +
                             read_u32_le(entry + 4);
    const uint64_t value_end = uint64_t(read_u32_le(entry + 8)) +
                               read_u32_le(entry + 12);
    if (key_end > arena_size || value_end > arena_size) {
      error_ = "字符串区越界，文件损坏";
      Reset();
      return false;
    }
  }
  return true;
}

/* 二分查找：命中返回 value，未命中返回 nullopt。 */
std::optional<std::string_view> GlossBin::Lookup(std::string_view key) const {
  if (data_ == nullptr) {
    return std::nullopt;
  }
  uint64_t lo = 0;
  uint64_t hi = count_;
  while (lo < hi) {
    uint64_t mid = lo + (hi - lo) / 2;
    const uint8_t* e = entries_ + mid * kEntrySize;
    uint32_t key_off = read_u32_le(e);
    uint32_t key_len = read_u32_le(e + 4);
    std::string_view ek(
        reinterpret_cast<const char*>(arena_ + key_off), key_len);
    int cmp = key.compare(ek);
    if (cmp == 0) {
      uint32_t val_off = read_u32_le(e + 8);
      uint32_t val_len = read_u32_le(e + 12);
      return std::string_view(
          reinterpret_cast<const char*>(arena_ + val_off), val_len);
    }
    if (cmp < 0) {
      hi = mid;
    } else {
      lo = mid + 1;
    }
  }
  return std::nullopt;
}

}
