/* GlossDictionary 实现：按优先级顺序加载与查找。 */
#include "gloss_dictionary.h"

#include <utility>

namespace qingjian {

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

}  // namespace qingjian
