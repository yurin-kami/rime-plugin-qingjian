/*
 * 释义方向判定与英文键归一化的单元测试。
 */
#include "minitest.h"

#include "../src/gloss_direction.h"

#include <string>

using qingjian::AsciiLower;
using qingjian::Classify;
using qingjian::GlossDirection;

TEST(中文文本判为中译英方向) {
  CHECK(Classify("开发") == GlossDirection::kZhEn);
  CHECK(Classify("中文语言") == GlossDirection::kZhEn);
}

TEST(英文文本判为英译中方向) {
  CHECK(Classify("development") == GlossDirection::kEnZh);
  CHECK(Classify("Hello World") == GlossDirection::kEnZh);
}

TEST(空文本判为英文方向) {
  CHECK(Classify("") == GlossDirection::kEnZh);
}

TEST(英文键转小写) {
  CHECK_EQ(AsciiLower("Development"), std::string("development"));
  CHECK_EQ(AsciiLower("HELLO"), std::string("hello"));
  CHECK_EQ(AsciiLower("中文"), std::string("中文"));
}

TEST_MAIN()
