#!/usr/bin/env bash
# 青简释义插件一键安装脚本。
#
# 子命令：
#   ./install.sh data --tsv-dir <青简 glossary 目录>   生成并安装释义数据到用户目录
#   ./install.sh so [RIME_SRC]                         编译并安装插件 .so（需 root、boost）
#   ./install.sh config [方案名]                       安装配置示例
#   ./install.sh all --tsv-dir <目录> [RIME_SRC]       依次执行上面三步
#
# 说明：数据装在 Rime 用户目录（无需 root）；插件 .so 需写入 /usr/lib/rime-plugins/，
# 需 sudo。释义表来自青简 assets/glossary/（glossary-en.tsv、glossary-zh.tsv）。

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 自动探测 Rime 用户数据目录：优先 fcitx5-rime，其次 ibus-rime。
detect_rime_dir() {
  if [[ -d "${XDG_DATA_HOME:-$HOME/.local/share}/fcitx5/rime" ]]; then
    echo "${XDG_DATA_HOME:-$HOME/.local/share}/fcitx5/rime"
  elif [[ -d "${XDG_CONFIG_HOME:-$HOME/.config}/ibus/rime" ]]; then
    echo "${XDG_CONFIG_HOME:-$HOME/.config}/ibus/rime"
  else
    echo "${XDG_DATA_HOME:-$HOME/.local/share}/fcitx5/rime"
  fi
}

PLUGINS_DIR="/usr/lib/rime-plugins"
RIME_DIR="$(detect_rime_dir)"

cmd_data() {
  local tsv_dir="${1:?用法: install.sh data --tsv-dir <目录>}"
  local zh_en_tsv="${tsv_dir}/glossary-en.tsv"
  local en_zh_tsv="${tsv_dir}/glossary-zh.tsv"
  [[ -f "${zh_en_tsv}" ]] || { echo "找不到 ${zh_en_tsv}" >&2; exit 1; }
  [[ -f "${en_zh_tsv}" ]] || { echo "找不到 ${en_zh_tsv}" >&2; exit 1; }
  local out="${RIME_DIR}/qingjian"
  mkdir -p "${out}"
  python3 "${REPO_DIR}/tools/build_gloss.py" "${zh_en_tsv}" "${out}/qingjian.zh_en.bin"
  python3 "${REPO_DIR}/tools/build_gloss.py" "${en_zh_tsv}" "${out}/qingjian.en_zh.bin" --lowercase-key
  echo "数据已安装到 ${out}"
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
  *)
    echo "用法: $0 {data|so|config|all} [参数]" >&2; exit 1;;
esac
