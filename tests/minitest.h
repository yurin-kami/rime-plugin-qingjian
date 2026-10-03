/*
 * 最小测试框架：注册用例 + 断言，不依赖任何第三方库。
 *
 * 用法：
 *   TEST(名字) { ... CHECK(条件); CHECK_EQ(a, b); }
 *   TEST_MAIN()
 *
 * 每个测试文件编译为独立可执行程序，main 返回失败用例数。
 */
#pragma once

#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace minitest {

struct Case {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<Case>& cases() {
  static std::vector<Case> all;
  return all;
}

struct Registrar {
  Registrar(const std::string& name, std::function<void()> fn) {
    cases().push_back(Case{name, std::move(fn)});
  }
};

/* 依次运行全部用例，返回失败个数。 */
inline int run_all() {
  int failed = 0;
  for (const Case& c : cases()) {
    try {
      c.fn();
      std::printf("[PASS] %s\n", c.name.c_str());
    } catch (const std::exception& e) {
      std::printf("[FAIL] %s: %s\n", c.name.c_str(), e.what());
      ++failed;
    } catch (...) {
      std::printf("[FAIL] %s: 未知异常\n", c.name.c_str());
      ++failed;
    }
  }
  std::printf("%zu 个用例, %d 个失败\n", cases().size(), failed);
  return failed;
}

}  // namespace minitest

#define TEST(name)                                                     \
  static void minitest_fn_##name();                                    \
  static ::minitest::Registrar minitest_reg_##name(#name,             \
                                                    minitest_fn_##name); \
  static void minitest_fn_##name()

#define TEST_MAIN()                                  \
  int main() { return ::minitest::run_all(); }

/* 断言失败抛出异常，由 run_all 捕获并标记失败。 */
#define CHECK(cond)                                                     \
  do {                                                                  \
    if (!(cond))                                                        \
      throw std::runtime_error(std::string("CHECK 失败: ") + #cond +    \
                               " @ " __FILE__ ":" +                     \
                               std::to_string(__LINE__));               \
  } while (0)

#define CHECK_EQ(a, b)                                                   \
  do {                                                                   \
    auto _a = (a);                                                       \
    auto _b = (b);                                                       \
    if (!(_a == _b))                                                     \
      throw std::runtime_error(std::string("CHECK_EQ 失败: ") + #a +     \
                               " == " + #b + " @ " __FILE__ ":" +        \
                               std::to_string(__LINE__));                \
  } while (0)
