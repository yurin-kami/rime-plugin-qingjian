/*
 * 释义方向的判定与英文键归一化（纯函数，供 Filter 与单测共用）。
 *
 * 规则：候选文本全为 ASCII 判为英文词，走英→中词典；含非 ASCII（如中文）
 * 判为中文，走中→英词典。英文词典的键为小写，故查词前把小写化。
 */
#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace qingjian {

enum class GlossDirection {
  kEnZh,  /* 英文候选 → 查英→中词典 */
  kZhEn,  /* 中文候选 → 查中→英词典 */
};

/* 判定文本的释义方向。 */
inline GlossDirection Classify(std::string_view text) {
  for (unsigned char c : text) {
    if (c >= 0x80) {
      return GlossDirection::kZhEn;
    }
  }
  return GlossDirection::kEnZh;
}

/* ASCII 转小写（英文词典键为小写）。 */
inline std::string AsciiLower(std::string_view text) {
  std::string lower;
  lower.reserve(text.size());
  for (unsigned char c : text) {
    lower.push_back(static_cast<char>(std::tolower(c)));
  }
  return lower;
}

}  // namespace qingjian
