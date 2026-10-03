/*
 * GlossDictionary 实现：按优先级顺序加载、查找，以及词典路径解析。
 */
#include "gloss_dictionary.h"

#include <filesystem>
#include <utility>

namespace qingjian {

/* 按优先级加载索引文件，失败文件跳过，返回成功数量。 */
size_t GlossDictionary::Load(const std::vector<std::string>& paths) {
  bins_.clear();
  for (const std::string& path : paths) {
    GlossBin bin;
    if (bin.Open(path)) {
      bins_.push_back(std::move(bin));
    }
  }
  return bins_.size();
}

/* 按优先级返回第一个命中的 value。 */
std::optional<std::string_view> GlossDictionary::Lookup(
    std::string_view key) const {
  for (const GlossBin& bin : bins_) {
    std::optional<std::string_view> value = bin.Lookup(key);
    if (value.has_value()) {
      return value;
    }
  }
  return std::nullopt;
}

/* 把词典名按优先级解析成存在的文件路径。 */
std::vector<std::string> ResolveGlossPaths(
    const std::vector<std::string>& base_dirs,
    const std::vector<std::string>& names,
    const std::string& suffix) {
  std::vector<std::string> result;
  for (const std::string& name : names) {
    for (const std::string& dir : base_dirs) {
      std::filesystem::path candidate =
          std::filesystem::path(dir) / (name + "." + suffix);
      if (std::filesystem::exists(candidate)) {
        result.push_back(candidate.string());
        break;
      }
    }
  }
  return result;
}

}
