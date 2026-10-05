#!/usr/bin/env bash
# 集成测试：在临时环境里加载插件 .so/.dylib，驱动真实输入，验证候选注释。
#
# 场景一（中→英）：luna_pinyin_simp 输入 kaifa，候选「开发」注释含 develop。
# 场景二（英→中）：test_en 输入 development，候选注释含「发展」。
#
# 前置：已执行 make so（产出 librime-qingjian.so 或 .dylib），系统已装
# librime 与 luna_pinyin 方案数据。无需 root——把 librime 复制到临时目录，
# 让 librime 的 plugins 模块从临时 rime-plugins 目录加载插件。
#
# Linux  用 LD_LIBRARY_PATH 指向临时 librime 副本；
# macOS  用 install_name_tool 把驱动改链接到临时副本（brew 的 librime
#        带绝对 install_name，DYLD_LIBRARY_PATH 无法覆盖）。
#
# 用法：tests/run_integration.sh

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 平台探测：扩展名、开发库名与运行时 SONAME 各不相同。
UNAME_S="$(uname -s)"
case "${UNAME_S}" in
  Darwin)
    PLUGIN_EXT=".dylib"
    RIME_LIB_NAME="librime.dylib"
    RIME_SONAME="librime.1.dylib"
    ;;
  *)
    PLUGIN_EXT=".so"
    RIME_LIB_NAME="librime.so"
    RIME_SONAME="librime.so.1"
    ;;
esac

PLUGIN_SO="${REPO_DIR}/librime-qingjian${PLUGIN_EXT}"
RIME_LIB="$(pkg-config --variable=libdir rime 2>/dev/null || true)/${RIME_LIB_NAME}"

[[ -f "${PLUGIN_SO}" ]] || { echo "先执行 make so 生成 ${PLUGIN_SO}" >&2; exit 1; }
[[ -n "${RIME_LIB}" && -f "${RIME_LIB}" ]] || { echo "找不到 ${RIME_LIB_NAME}（需安装 librime）" >&2; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

mkdir -p "${TMP}/lib/rime-plugins" "${TMP}/user/qingjian" "${TMP}/shared"

# 复制 librime 到临时目录（按运行时 SONAME 命名），使 plugins 模块从
# ${TMP}/lib/rime-plugins 加载插件，而非系统插件目录。
cp -L "${RIME_LIB}" "${TMP}/lib/${RIME_SONAME}"
cp "${PLUGIN_SO}" "${TMP}/lib/rime-plugins/librime-qingjian${PLUGIN_EXT}"

# 释义数据：从 fixture 生成最小词典，不依赖外部青简数据。
python3 "${REPO_DIR}/tools/build_gloss.py" \
  "${REPO_DIR}/tests/fixtures/integration_zh_en.tsv" \
  "${TMP}/user/qingjian/qingjian.zh_en.bin"
python3 "${REPO_DIR}/tools/build_gloss.py" \
  "${REPO_DIR}/tests/fixtures/integration_en_zh.tsv" \
  "${TMP}/user/qingjian/qingjian.en_zh.bin" --lowercase-key

# 给 luna_pinyin_simp 加 gloss_filter 补丁。
cat > "${TMP}/user/luna_pinyin_simp.custom.yaml" <<'EOF'
patch:
  engine/filters/+:
    - gloss_filter
  gloss_filter:
    dictionaries_zh_en: [ qingjian ]
    dictionaries_en_zh: [ qingjian ]
    overwrite_comment: false
EOF

# 把 test_en 加入 schema_list，使 workspace_update 部署它。
cat > "${TMP}/user/default.custom.yaml" <<'EOF'
patch:
  schema_list/+:
    - schema: test_en
EOF

# 构造临时 shared 目录：软链系统 rime-data 全部内容，再放入英文测试方案。
# workspace_update 只扫描共享目录，故 test_en 必须放这里。
if [[ "${UNAME_S}" == "Darwin" ]]; then
  RIME_DATA_DIR="/Library/Input Methods/Squirrel.app/Contents/SharedSupport"
  [[ -d "${RIME_DATA_DIR}" ]] || {
    echo "找不到 ${RIME_DATA_DIR}（需安装鼠须管 Squirrel）" >&2
    exit 1
  }
else
  RIME_DATA_DIR="/usr/share/rime-data"
fi
for f in "${RIME_DATA_DIR}"/*; do
  ln -s "$f" "${TMP}/shared/" 2>/dev/null || true
done
cp "${REPO_DIR}/tests/integration/test_en.schema.yaml" "${TMP}/shared/"
cp "${REPO_DIR}/tests/integration/test_en.dict.yaml" "${TMP}/shared/"

# 编译驱动（只用公开 API，无需 boost），链接临时 librime 副本。
RIME_CFLAGS="$(pkg-config --cflags rime 2>/dev/null || true)"
if [[ "${UNAME_S}" == "Darwin" ]]; then
  # 把临时副本的 install_name id 改为绝对路径，驱动链接后即加载该副本。
  install_name_tool -id "${TMP}/lib/${RIME_SONAME}" "${TMP}/lib/${RIME_SONAME}" 2>/dev/null
  codesign --force --sign - "${TMP}/lib/${RIME_SONAME}" >/dev/null 2>&1

  # 插件编译时链接了 brew 的绝对 librime install_name，需改指临时副本，
  # 否则插件与驱动各绑一个 librime 实例，模块注册互不可见。
  TMP_PLUGIN="${TMP}/lib/rime-plugins/librime-qingjian${PLUGIN_EXT}"
  RIME_ID="$(otool -L "${TMP_PLUGIN}" | awk '/librime\.1\.dylib/{print $1; exit}')"
  if [[ -n "${RIME_ID}" ]]; then
    install_name_tool -change "${RIME_ID}" "${TMP}/lib/${RIME_SONAME}" \
      "${TMP_PLUGIN}" 2>/dev/null
    codesign --force --sign - "${TMP_PLUGIN}" >/dev/null 2>&1
  fi

  ln -s "${RIME_SONAME}" "${TMP}/lib/librime.dylib"
  c++ -std=c++17 -O2 ${RIME_CFLAGS} "${REPO_DIR}/tests/integration_driver.cc" \
    -o "${TMP}/driver" "${TMP}/lib/librime.dylib"
else
  ln -s "${RIME_SONAME}" "${TMP}/lib/librime.so"
  c++ -std=c++17 -O2 ${RIME_CFLAGS} "${REPO_DIR}/tests/integration_driver.cc" \
    -o "${TMP}/driver" -L"${TMP}/lib" -lrime
fi

echo "== 运行集成测试 =="
if [[ "${UNAME_S}" == "Darwin" ]]; then
  "${TMP}/driver" "${TMP}/shared" "${TMP}/user"
else
  LD_LIBRARY_PATH="${TMP}/lib" "${TMP}/driver" "${TMP}/shared" "${TMP}/user"
fi
