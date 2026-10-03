# 构建脚本：编译纯 C++ 单元测试与 librime 插件 .so。
#
# 常用目标：
#   make test   编译并运行纯 C++ 单测（不依赖 librime）
#   make so     编译 librime-qingjian.so（依赖 librime 头与 boost）
#   make clean  清理产物
#
# 可覆盖变量：
#   RIME_SRC    librime 源码树路径（默认 .analysis/librime）
#   CXX / CXXFLAGS

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

RIME_SRC ?= .analysis/librime

PURE_SRC := $(wildcard src/gloss_bin.cc src/gloss_dictionary.cc)
TEST_SRC := $(filter-out tests/test_gloss_fixture.cc,$(wildcard tests/test_*.cc))
TEST_BIN := $(patsubst tests/%.cc,build/%,$(TEST_SRC))

PLUGIN_SRC := src/gloss_filter.cc src/module.cc
PLUGIN_OBJ_DEPS := $(PURE_SRC) src/gloss_filter.h src/gloss_dictionary.h src/gloss_bin.h

RIME_INC := -Ibuild -I$(RIME_SRC)/src -I$(RIME_SRC)/include

.PHONY: all test test-py test-data check so clean

all: test

# 生成 build/rime/build_config.h，与已装 librime 的日志 ABI 保持一致。
# 放于 build/rime/ 下，因为 common.h 用 <rime/build_config.h> 引入。
build/rime/build_config.h: build_config.h.in
	@mkdir -p build/rime
	sed 's|@RIME_PLUGINS_DIR@|rime-plugins|' build_config.h.in > $@

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

# 编译插件 .so：需要 librime 内部头、boost 头，链接 -lrime -lglog。
so: build/rime/build_config.h $(PLUGIN_OBJ_DEPS)
	$(CXX) $(CXXFLAGS) -fPIC -shared $(RIME_INC) \
		$(PLUGIN_SRC) $(PURE_SRC) -o librime-qingjian.so -lrime -lglog

clean:
	rm -rf build librime-qingjian.so
