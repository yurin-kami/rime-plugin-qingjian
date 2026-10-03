/*
 * GlossFilter：给候选打双语释义注释的 filter 组件。
 *
 * 挂在输入方案的 engine/filters 里（建议放在 uniquifier 之后）。对每条候选：
 *   文本全为 ASCII（英文词）→ 转小写查英→中词典；
 *   文本含非 ASCII（如中文）→ 查中→英词典；
 *   命中后把释义写入候选 comment，前端显示在候选右侧。
 *
 * 配置（schema 或 custom 补丁）：
 *   gloss_filter:
 *     dictionaries_zh_en: [ qingjian ]   # 中→英词典名，按优先级
 *     dictionaries_en_zh: [ qingjian ]   # 英→中词典名
 *     overwrite_comment: false              # 是否覆盖已有注释
 *
 * 词典文件解析：对每个名字，依次在用户数据目录、共享数据目录下的
 * qingjian/ 子目录里找 "<名字>.zh_en.bin" / "<名字>.en_zh.bin"。
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <rime/filter.h>
#include <rime/translation.h>

#include "gloss_dictionary.h"

namespace rime {
class Candidate;
}

namespace qingjian {

class GlossFilter : public rime::Filter {
 public:
  /* 构造：固定配置命名空间为 gloss_filter（未带别名时）。 */
  explicit GlossFilter(const rime::Ticket& ticket);

  /* 包装 Translation，惰性地给候选打注释。 */
  rime::an<rime::Translation> Apply(rime::an<rime::Translation> translation,
                                    rime::CandidateList* candidates) override;

  /* 给单条候选打注释；由包装 Translation 的 Peek 惰性调用。 */
  rime::an<rime::Candidate> Annotate(rime::an<rime::Candidate> cand);

 private:
  /* 读配置并加载两个方向的词典。 */
  void Initialize();

  /* 是否已加载配置与词典。 */
  bool initialized_ = false;
  /* 中→英词典（按优先级）。 */
  GlossDictionary zh_en_;
  /* 英→中词典（按优先级）。 */
  GlossDictionary en_zh_;
  /* 已有注释时是否覆盖。 */
  bool overwrite_comment_ = false;
};

/* 惰性包装：在 Peek 时对候选打注释，避免一次性处理整页候选。 */
class GlossFilterTranslation : public rime::CacheTranslation {
 public:
  /* 记录所属 filter，供 Peek 回调使用。 */
  GlossFilterTranslation(rime::an<rime::Translation> translation,
                         GlossFilter* filter)
      : CacheTranslation(translation), filter_(filter) {}

  /* 取出一条候选并打注释。 */
  rime::an<rime::Candidate> Peek() override;

 private:
  /* 所属的 GlossFilter。 */
  GlossFilter* filter_;
};

}
