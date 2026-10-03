#!/usr/bin/env bash
# 青简释义插件一键安装脚本。
#
# 子命令：
#   ./install.sh data   --tsv-dir <青简 glossary 目录>   生成并安装释义数据
#   ./install.sh so     [RIME_SRC]                       编译并安装插件 .so
#   ./install.sh config [方案名]                         安装配置示例
#   ./install.sh all    --tsv-dir <目录> [RIME_SRC]      依次执行上面三步
#
# 安装目录（统一默认值，均可用环境变量覆盖）：
#   - 插件 .so      → $RIME_PLUGINS_DIR（默认取 rime.pc 的 pluginsdir，
#                     即 /usr/lib/rime-plugins，需 root）
#   - 数据与配置    → $RIME_USER_DIR（默认自动探测 fcitx5-rime / ibus-rime
#                     的用户目录，无需 root）
#
# 释义表来自青简 assets/glossary/（glossary-en.tsv、glossary-zh.tsv）。

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 自动探测 Rime 用户数据目录：优先 fcitx5-rime，其次 ibus-rime，
# 两者都未安装时默认 fcitx5（可用 RIME_USER_DIR 覆盖）。
detect_rime_dir() {
  local fcitx5_dir="${XDG_DATA_HOME:-$HOME/.local/share}/fcitx5/rime"
  local ibus_dir="${XDG_CONFIG_HOME:-$HOME/.config}/ibus/rime"
  if [[ -d "${fcitx5_dir}" ]]; then
    echo "${fcitx5_dir}"
  elif [[ -d "${ibus_dir}" ]]; then
    echo "${ibus_dir}"
  else
    echo "${fcitx5_dir}"
  fi
}

# 安装目录：统一在此定义，全脚本只从这里取值。
PLUGINS_DIR="${RIME_PLUGINS_DIR:-$(pkg-config --variable=pluginsdir rime 2>/dev/null || true)}"
PLUGINS_DIR="${PLUGINS_DIR:-/usr/lib/rime-plugins}"
RIME_DIR="${RIME_USER_DIR:-$(detect_rime_dir)}"
DATA_DIR="${RIME_DIR}/qingjian"

cmd_data() {
  local tsv_dir="${1:?用法: install.sh data --tsv-dir <目录>}"
  local zh_en_tsv="${tsv_dir}/glossary-en.tsv"
  local en_zh_tsv="${tsv_dir}/glossary-zh.tsv"
  [[ -f "${zh_en_tsv}" ]] || { echo "找不到 ${zh_en_tsv}" >&2; exit 1; }
  [[ -f "${en_zh_tsv}" ]] || { echo "找不到 ${en_zh_tsv}" >&2; exit 1; }
  mkdir -p "${DATA_DIR}"
  python3 "${REPO_DIR}/tools/build_gloss.py" "${zh_en_tsv}" "${DATA_DIR}/qingjian.zh_en.bin"
  python3 "${REPO_DIR}/tools/build_gloss.py" "${en_zh_tsv}" "${DATA_DIR}/qingjian.en_zh.bin" --lowercase-key
  echo "数据已安装到 ${DATA_DIR}"
}

cmd_so() {
  local rime_src="${1:-${REPO_DIR}/.analysis/librime}"
  (cd "${REPO_DIR}" && make so RIME_SRC="${rime_src}")
  sudo install -Dm755 "${REPO_DIR}/librime-qingjian.so" "${PLUGINS_DIR}/librime-qingjian.so"
  echo "插件已安装到 ${PLUGINS_DIR}/librime-qingjian.so"
}

cmd_config() {
  local schema="${1:-luna_pinyin}"
  local target="${RIME_DIR}/${schema}.custom.yaml"
  if [[ -e "${target}" ]]; then
    echo "目标已存在，请手动合并示例：${REPO_DIR}/examples/luna_pinyin.custom.yaml"
  else
    cp "${REPO_DIR}/examples/luna_pinyin.custom.yaml" "${target}"
    echo "配置示例已写入 ${target}"
  fi
}

usage() {
  sed -n '2,14p' "${BASH_SOURCE[0]}"
}

# 解析子命令与参数。
subcmd="${1:-}"; shift || true
case "${subcmd}" in
  data)
    [[ "${1:-}" == "--tsv-dir" ]] && shift
    cmd_data "${1:-}";;
  so)
    cmd_so "${1:-}";;
  config)
    cmd_config "${1:-}";;
  all)
    [[ "${1:-}" == "--tsv-dir" ]] && shift
    cmd_data "${1:-}"
    cmd_so "${2:-}"
    cmd_config "luna_pinyin";;
  help|-h|--help)
    usage;;
  *)
    usage >&2; exit 1;;
esac
