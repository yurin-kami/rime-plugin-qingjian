#!/usr/bin/env bash
# 集成测试：在临时环境里加载插件 .so，驱动真实输入，验证候选注释。
#
# 场景一（中→英）：luna_pinyin_simp 输入 kaifa，候选「开发」注释含 develop。
# 场景二（英→中）：test_en 输入 development，候选注释含「发展」。
#
# 前置：已执行 make so（产出 librime-qingjian.so），系统已装 librime 与
# rime-luna-pinyin。无需 root（LD_LIBRARY_PATH 指向临时 librime 副本，
# 使 plugins 模块从临时目录加载插件）。
#
# 用法：tests/run_integration.sh

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLUGIN_SO="${REPO_DIR}/librime-qingjian.so"
RIME_LIB="$(pkg-config --variable=libdir rime 2>/dev/null)/librime.so"

[[ -f "${PLUGIN_SO}" ]] || { echo "先执行 make so 生成 ${PLUGIN_SO}" >&2; exit 1; }
[[ -n "${RIME_LIB}" && -f "${RIME_LIB}" ]] || { echo "找不到 librime.so" >&2; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

mkdir -p "${TMP}/lib/rime-plugins" "${TMP}/user/qingjian" "${TMP}/shared"

# 复制 librime 到临时目录（按 SONAME 命名，使 LD_LIBRARY_PATH 覆盖生效），
# 从而使 plugins 模块从 ${TMP}/lib/rime-plugins 加载。
cp -L "${RIME_LIB}" "${TMP}/lib/librime.so.1"
cp "${PLUGIN_SO}" "${TMP}/lib/rime-plugins/librime-qingjian.so"

# 释义数据装到用户目录。
cp "${REPO_DIR}/data/qingjian/qingjian.zh_en.bin" "${TMP}/user/qingjian/"
cp "${REPO_DIR}/data/qingjian/qingjian.en_zh.bin" "${TMP}/user/qingjian/"

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
for f in /usr/share/rime-data/*; do
  ln -s "$f" "${TMP}/shared/" 2>/dev/null || true
done
cp "${REPO_DIR}/tests/integration/test_en.schema.yaml" "${TMP}/shared/"
cp "${REPO_DIR}/tests/integration/test_en.dict.yaml" "${TMP}/shared/"

# 编译驱动（只用公开 API，无需 boost）。
g++ -std=c++17 -O2 "${REPO_DIR}/tests/integration_driver.cc" -o "${TMP}/driver" -lrime

echo "== 运行集成测试 =="
LD_LIBRARY_PATH="${TMP}/lib" "${TMP}/driver" "${TMP}/shared" "${TMP}/user"
