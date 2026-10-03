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

}  // namespace qingjian
