/*
 * GlossFilter 实现：读配置、解析词典路径、按候选文本方向查词、写注释。
 *
 * 与 librime 自带的 reverse_lookup_filter（src/rime/gear/reverse_lookup_filter.cc）
 * 同构：用 CacheTranslation 惰性包装，在 Peek 里对每条候选打注释。
 */
#include "gloss_filter.h"

#include <cctype>

#include <rime/candidate.h>
#include <rime/common.h>
#include <rime/config.h>
#include <rime/deployer.h>
#include <rime/engine.h>
#include <rime/gear/translator_commons.h>
#include <rime/schema.h>
#include <rime/service.h>

namespace qingjian {

namespace {

/* 文本是否全为 ASCII（据此判定英文候选）。 */
bool IsAscii(std::string_view text) {
  for (unsigned char c : text) {
    if (c >= 0x80) {
      return false;
    }
  }
  return true;
}

/* ASCII 转小写（词典里英文键为小写）。 */
std::string ToLower(std::string_view text) {
  std::string lower;
  lower.reserve(text.size());
  for (unsigned char c : text) {
    lower.push_back(static_cast<char>(std::tolower(c)));
  }
  return lower;
}

/* 读取配置里的字符串列表到 out。 */
void ReadStringList(rime::Config* config, const std::string& path,
                    std::vector<std::string>* out) {
  rime::an<rime::ConfigList> list = config->GetList(path);
  if (!list) {
    return;
  }
  for (size_t i = 0; i < list->size(); ++i) {
    rime::an<rime::ConfigValue> item =
        rime::As<rime::ConfigValue>(list->GetAt(i));
    if (item) {
      out->push_back(item->str());
    }
  }
}

}  // namespace

GlossFilter::GlossFilter(const rime::Ticket& ticket) : rime::Filter(ticket) {
  /* 组件未带别名时，配置命名空间固定为 gloss_filter。 */
  if (ticket.name_space == "filter") {
    name_space_ = "gloss_filter";
  }
}

void GlossFilter::Initialize() {
  initialized_ = true;
  if (!engine_) {
    return;
  }
  rime::Config* config =
      engine_->schema() ? engine_->schema()->config() : nullptr;
  if (!config) {
    return;
  }
  config->GetBool(name_space_ + "/overwrite_comment", &overwrite_comment_);
  std::vector<std::string> zh_names;
  std::vector<std::string> en_names;
  ReadStringList(config, name_space_ + "/dictionaries_zh_en", &zh_names);
  ReadStringList(config, name_space_ + "/dictionaries_en_zh", &en_names);
  rime::Deployer& deployer = rime::Service::instance().deployer();
  std::vector<std::string> dirs = {
      (deployer.user_data_dir / "qingjian").to_utf8_string(),
      (deployer.shared_data_dir / "qingjian").to_utf8_string(),
  };
  zh_en_.Load(ResolveGlossPaths(dirs, zh_names, "zh_en.bin"));
  en_zh_.Load(ResolveGlossPaths(dirs, en_names, "en_zh.bin"));
}

rime::an<rime::Translation> GlossFilter::Apply(
    rime::an<rime::Translation> translation, rime::CandidateList* candidates) {
  if (!initialized_) {
    Initialize();
  }
  if (zh_en_.empty() && en_zh_.empty()) {
    return translation;
  }
  return rime::New<GlossFilterTranslation>(translation, this);
}

rime::an<rime::Candidate> GlossFilter::Annotate(
    rime::an<rime::Candidate> cand) {
  if (!cand) {
    return cand;
  }
  if (!cand->comment().empty() && !overwrite_comment_) {
    return cand;
  }
  std::string_view text = cand->text();
  std::optional<std::string_view> gloss;
  if (IsAscii(text)) {
    gloss = en_zh_.Lookup(ToLower(text));
  } else {
    gloss = zh_en_.Lookup(text);
  }
  if (!gloss.has_value() || gloss->empty()) {
    return cand;
  }
  std::string value(*gloss);
  rime::an<rime::Candidate> genuine =
      rime::Candidate::GetGenuineCandidate(cand);
  if (rime::an<rime::Phrase> phrase = rime::As<rime::Phrase>(genuine)) {
    phrase->set_comment(value);
  } else if (rime::an<rime::SimpleCandidate> simple =
                 rime::As<rime::SimpleCandidate>(genuine)) {
    simple->set_comment(value);
  }
  return cand;
}

rime::an<rime::Candidate> GlossFilterTranslation::Peek() {
  rime::an<rime::Candidate> cand = CacheTranslation::Peek();
  if (cand) {
    cand = filter_->Annotate(cand);
  }
  return cand;
}

}  // namespace qingjian
