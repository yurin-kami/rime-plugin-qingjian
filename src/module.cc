/*
 * 模块注册：把 gloss_filter 组件登记进 librime 的 Registry。
 *
 * 模块名须与 .so 文件名对应（librime-qingjian.so → 模块名 "qingjian"），
 * librime 的 plugins 模块加载 .so 后按此名调用 initialize。
 */
#include <rime/component.h>
#include <rime/registry.h>
#include <rime_api.h>

#include "gloss_filter.h"

using namespace rime;

static void rime_qingjian_initialize() {
  Registry& r = Registry::instance();
  r.Register("gloss_filter", new Component<qingjian::GlossFilter>);
}

static void rime_qingjian_finalize() {}

RIME_REGISTER_MODULE(qingjian)
