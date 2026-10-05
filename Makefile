# 构建脚本：编译纯 C++ 单元测试与 librime 插件 .so。
#
# 常用目标：
#   make test   编译并运行纯 C++ 单测（不依赖 librime）
#   make so     编译 librime-qingjian.so（依赖 librime 头与 boost）
#   make clean  清理产物
#
# 可覆盖变量：
#   RIME_SRC       librime 源码树路径（默认 .analysis/librime）
#   CXX / CXXFLAGS
#   RIME_LOGGING   是否启用日志宏（1/0，默认 1，与发行版 librime 一致）
#   RIME_LIB       链接的 librime 库（默认 -lrime；macOS 鼠须管需指定其
#                  内嵌 librime 路径，并配合 RIME_LOGGING=0）
#
# macOS 鼠须管（Squirrel）示例：
#   make so RIME_LOGGING=0 \
#     RIME_LIB="/Library/Input Methods/Squirrel.app/Contents/Frameworks/librime.1.dylib"

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

RIME_SRC ?= .analysis/librime
RIME_LOGGING ?= 1
RIME_LIB ?= -lrime

# 平台相关：librime 的 PluginManager 只扫描本平台的动态库扩展名
#（Linux 为 .so，macOS 为 .dylib），产物必须匹配才能被加载。
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
PLUGIN_EXT := .dylib
# Homebrew 的头文件/库不在默认搜索路径，需显式加入。
BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
RIME_INC_EXTRA := -isystem $(BREW_PREFIX)/include
RIME_LIB_EXTRA := -L$(BREW_PREFIX)/lib
# clang 对 librime 头里 RIME_REGISTER_MODULE 宏的 {0} 初始化
# 报 -Wmissing-field-initializers，屏蔽以保持零警告。
CXXFLAGS += -Wno-missing-field-initializers
else
PLUGIN_EXT := .so
RIME_INC_EXTRA :=
RIME_LIB_EXTRA :=
endif
PLUGIN_SO := librime-qingjian$(PLUGIN_EXT)

PURE_SRC := $(wildcard src/gloss_bin.cc src/gloss_dictionary.cc)
TEST_SRC := $(filter-out tests/test_gloss_fixture.cc,$(wildcard tests/test_*.cc))
TEST_BIN := $(patsubst tests/%.cc,build/%,$(TEST_SRC))

PLUGIN_SRC := src/gloss_filter.cc src/module.cc
PLUGIN_OBJ_DEPS := $(PURE_SRC) src/gloss_filter.h src/gloss_dictionary.h src/gloss_bin.h

RIME_INC := -Ibuild -isystem $(RIME_SRC)/src -isystem $(RIME_SRC)/include $(RIME_INC_EXTRA)

.PHONY: all test test-py test-data check so test-integration clean

all: test

# 生成 build/rime/build_config.h，与目标 librime 的日志 ABI 保持一致。
# 放于 build/rime/ 下，因为 common.h 用 <rime/build_config.h> 引入。
# RIME_LOGGING=0 时注释掉 RIME_ENABLE_LOGGING（鼠须管内嵌 librime 未启日志）。
build/rime/build_config.h: build_config.h.in
	@mkdir -p build/rime
	@if [ "$(RIME_LOGGING)" = "0" ]; then \
		sed -e 's|@RIME_PLUGINS_DIR@|rime-plugins|' \
		    -e 's|#define RIME_ENABLE_LOGGING|/* RIME_ENABLE_LOGGING 关闭 */|' \
		    build_config.h.in > $@; \
	else \
		sed 's|@RIME_PLUGINS_DIR@|rime-plugins|' build_config.h.in > $@; \
	fi

# 纯 C++ 单测：不依赖 librime，直接编译并运行。
build/%: tests/%.cc $(PURE_SRC) tests/minitest.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -I. -Itests $< $(PURE_SRC) -o $@

test: $(TEST_BIN)
	@set -e; for t in $(TEST_BIN); do echo "== $$t =="; $$t; done

# Python 单测（TSV 解析与二进制写入）。
test-py:
	python3 -m unittest tests.test_build_gloss -v

# 跨语言对拍：Python 生成样本索引，C++ 读回校验。
build/sample.bin: tests/fixtures/sample.tsv tools/build_gloss.py
	@mkdir -p build
	python3 tools/build_gloss.py tests/fixtures/sample.tsv build/sample.bin

build/test_gloss_fixture: tests/test_gloss_fixture.cc $(PURE_SRC) tests/minitest.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -I. -Itests tests/test_gloss_fixture.cc $(PURE_SRC) -o $@

test-data: build/test_gloss_fixture build/sample.bin
	build/test_gloss_fixture build/sample.bin

check: test test-py test-data

# 集成测试：编译 .so 后，在临时环境加载并驱动真实输入验证候选注释。
test-integration: so
	bash tests/run_integration.sh

# 编译插件 .so/.dylib：需要 librime 内部头、boost 头。
# 默认链接 -lrime -lglog；RIME_LOGGING=0 时不链 glog，且不定义
# RIME_ENABLE_LOGGING（匹配未启用日志的内嵌 librime，如鼠须管）。
# -DGLOG_USE_GLOG_EXPORT：新版 glog 要求，见其 logging.h 的导出宏检查。
ifeq ($(RIME_LOGGING),0)
LOGGING_FLAGS :=
LOGGING_LIBS :=
else
LOGGING_FLAGS := -DGLOG_USE_GLOG_EXPORT
LOGGING_LIBS := -lglog
endif

so: build/rime/build_config.h $(PLUGIN_OBJ_DEPS)
	$(CXX) $(CXXFLAGS) -fPIC -shared $(LOGGING_FLAGS) $(RIME_INC) \
		$(PLUGIN_SRC) $(PURE_SRC) -o $(PLUGIN_SO) \
		$(if $(RIME_LIB),"$(RIME_LIB)",-lrime) $(LOGGING_LIBS) $(RIME_LIB_EXTRA)

clean:
	rm -rf build librime-qingjian.so librime-qingjian.dylib
