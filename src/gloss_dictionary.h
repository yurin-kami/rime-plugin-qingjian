/*
 * 多词典抽象层：一个方向（中→英 或 英→中）下按优先级组合若干 GlossBin。
 *
 * 优先级即 Load 传入的路径顺序：排前面的先查、命中即返回，用于
 * 「个人词典优先于随包词典」的叠层查找。加载失败的文件静默跳过。
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gloss_bin.h"

namespace qingjian {

class GlossDictionary {
 public:
  /* 按优先级顺序加载索引文件；失败文件跳过，返回成功数量。 */
  size_t Load(const std::vector<std::string>& paths);

  /* 查词：返回第一个命中的 value，全未命中返回 nullopt。 */
  std::optional<std::string_view> Lookup(std::string_view key) const;

  /* 成功加载的索引文件数。 */
  size_t dictionaries() const { return bins_.size(); }

  /* 是否未加载任何词典。 */
  bool empty() const { return bins_.empty(); }

 private:
  std::vector<GlossBin> bins_;
};

/*
 * 把词典名解析成索引文件路径。
 *
 * 规则：按 names 顺序，对每个名字依次在 base_dirs 里找
 * "<目录>/<名字>.<suffix>"，取第一个存在的文件；结果顺序等于名字的
 * 优先级顺序。base_dirs 顺序即「同名时目录越靠前越优先」（如用户目录
 * 优先于共享目录）。
 */
std::vector<std::string> ResolveGlossPaths(
    const std::vector<std::string>& base_dirs,
    const std::vector<std::string>& names,
    const std::string& suffix);

}  // namespace qingjian
