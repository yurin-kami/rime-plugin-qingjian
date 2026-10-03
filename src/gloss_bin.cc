/*
 * GlossBin 实现：mmap 映射 + 定宽索引二分查找。
 *
 * 依赖 POSIX（mmap/open/fstat），目标平台为 Linux。文件格式与写入端
 * （tools/build_gloss.py）严格一致：小端序、key 按 UTF-8 字节序升序。
 */
#include "gloss_bin.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>

namespace qingjian {

namespace {

constexpr size_t kHeaderSize = 16;
constexpr size_t kEntrySize = 16;
constexpr const char kMagic[8] = {'Q', 'J', 'G', 'L', 'O', 'S', 'S', '1'};

/* 小端读取无符号整数（文件格式约定为小端）。 */
uint32_t read_u32_le(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
         (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

uint64_t read_u64_le(const uint8_t* p) {
  return uint64_t(read_u32_le(p)) | (uint64_t(read_u32_le(p + 4)) << 32);
}

}  // namespace

GlossBin::~GlossBin() { Reset(); }

GlossBin::GlossBin(GlossBin&& other) noexcept {
  data_ = other.data_;
  data_len_ = other.data_len_;
  count_ = other.count_;
  entries_ = other.entries_;
  arena_ = other.arena_;
  error_ = std::move(other.error_);
  other.data_ = nullptr;
  other.data_len_ = 0;
  other.count_ = 0;
  other.entries_ = nullptr;
  other.arena_ = nullptr;
}

GlossBin& GlossBin::operator=(GlossBin&& other) noexcept {
  if (this != &other) {
    Reset();
    data_ = other.data_;
    data_len_ = other.data_len_;
    count_ = other.count_;
    entries_ = other.entries_;
    arena_ = other.arena_;
    error_ = std::move(other.error_);
    other.data_ = nullptr;
    other.data_len_ = 0;
    other.count_ = 0;
    other.entries_ = nullptr;
    other.arena_ = nullptr;
  }
  return *this;
}

void GlossBin::Reset() {
  if (data_ != nullptr) {
    munmap(data_, data_len_);
  }
  data_ = nullptr;
  data_len_ = 0;
  count_ = 0;
  entries_ = nullptr;
  arena_ = nullptr;
  error_.clear();
}

bool GlossBin::Open(const std::string& path) {
  Reset();
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
    error_ = "mmap 失败";
    return false;
  }
  data_ = static_cast<uint8_t*>(p);
  if (std::memcmp(data_, kMagic, sizeof(kMagic)) != 0) {
    error_ = "magic 不匹配，不是释义索引文件";
    Reset();
    return false;
  }
  count_ = read_u64_le(data_ + 8);
  size_t index_bytes = static_cast<size_t>(count_) * kEntrySize;
  if (kHeaderSize + index_bytes > data_len_) {
    error_ = "索引区越界，文件损坏";
    Reset();
    return false;
  }
  entries_ = data_ + kHeaderSize;
  arena_ = data_ + kHeaderSize + index_bytes;
  return true;
}

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

}  // namespace qingjian
