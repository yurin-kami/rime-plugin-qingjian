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
#   - 插件 .so/.dylib → $RIME_PLUGINS_DIR（默认取 rime.pc 的 pluginsdir；
#     macOS 鼠须管用其内嵌 librime 的 rime-plugins 目录，受系统 TCC 保护，
#     无法 sudo 命令行写入，本脚本会弹出 Finder 窗口引导拖拽）
#   - 数据与配置    → $RIME_USER_DIR（默认自动探测 fcitx5-rime / 鼠须管 /
#     ibus-rime 的用户目录，无需 root）
#
# 释义表来自青简 assets/glossary/（glossary-en.tsv、glossary-zh.tsv）。

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 自动探测 Rime 用户数据目录：优先 fcitx5-rime，其次 macOS 鼠须管
#（Squirrel，~/Library/Rime），再次 ibus-rime；都未安装时按系统取默认
#（macOS 取鼠须管目录，其余取 fcitx5），可用 RIME_USER_DIR 覆盖。
detect_rime_dir() {
  local fcitx5_dir="${XDG_DATA_HOME:-$HOME/.local/share}/fcitx5/rime"
  local squirrel_dir="${HOME}/Library/Rime"
  local ibus_dir="${XDG_CONFIG_HOME:-$HOME/.config}/ibus/rime"
  if [[ -d "${fcitx5_dir}" ]]; then
    echo "${fcitx5_dir}"
  elif [[ -d "${squirrel_dir}" ]]; then
    echo "${squirrel_dir}"
  elif [[ -d "${ibus_dir}" ]]; then
    echo "${ibus_dir}"
  elif [[ "$(uname -s)" == "Darwin" ]]; then
    echo "${squirrel_dir}"
  else
    echo "${fcitx5_dir}"
  fi
}

# 探测插件安装目录：macOS 鼠须管优先（其内嵌 librime 只扫描自己的
# rime-plugins 目录）；否则取 rime.pc 的 pluginsdir；最后回退默认路径。
detect_plugins_dir() {
  local squirrel_dir="/Library/Input Methods/Squirrel.app/Contents/Frameworks/rime-plugins"
  if [[ -d "${squirrel_dir}" ]]; then
    echo "${squirrel_dir}"
    return
  fi
  local pc_dir
  pc_dir="$(pkg-config --variable=pluginsdir rime 2>/dev/null || true)"
  if [[ -n "${pc_dir}" ]]; then
    echo "${pc_dir}"
    return
  fi
  echo "/usr/lib/rime-plugins"
}

# 安装目录：统一在此定义，全脚本只从这里取值。
PLUGINS_DIR="${RIME_PLUGINS_DIR:-$(detect_plugins_dir)}"
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
  local make_args=(RIME_SRC="${rime_src}")
  # macOS 鼠须管：插件必须链接其内嵌 librime（@rpath、无 glog），
  # 否则会与 brew 的 librime 绑成两个实例，模块注册互不可见。
  local squirrel_dir="/Library/Input Methods/Squirrel.app/Contents/Frameworks/rime-plugins"
  local squirrel_lib="/Library/Input Methods/Squirrel.app/Contents/Frameworks/librime.1.dylib"
  local is_squirrel=false
  if [[ -f "${squirrel_lib}" ]] && [[ "${PLUGINS_DIR}" == "${squirrel_dir}" ]]; then
    make_args+=(RIME_LOGGING=0 RIME_LIB="${squirrel_lib}")
    is_squirrel=true
  fi
  (cd "${REPO_DIR}" && make so "${make_args[@]}")
  # 产物扩展名随平台（Linux .so / macOS .dylib），按实际存在者安装。
  local src=""
  for ext in .so .dylib; do
    if [[ -f "${REPO_DIR}/librime-qingjian${ext}" ]]; then
      src="${REPO_DIR}/librime-qingjian${ext}"
      break
    fi
  done
  [[ -n "${src}" ]] || { echo "未找到编译产物 librime-qingjian.so/.dylib" >&2; exit 1; }

  local dst="${PLUGINS_DIR}/$(basename "${src}")"

  # macOS 鼠须管：/Library/Input Methods 受系统 TCC 保护，sudo 命令行写入
  # 会被拒（Operation not permitted），只能走 Finder 授权复制。
  if [[ "${is_squirrel}" == "true" ]]; then
    if [[ -f "${dst}" ]]; then
      echo "插件已存在：${dst}"
      echo "如需更新，请把下面高亮的文件拖到弹出的 Finder 窗口覆盖（会提示输密码）："
    else
      echo "macOS 鼠须管插件目录受系统保护，无法用 sudo 命令行写入。"
      echo "请把下面高亮的文件拖到弹出的 Finder 窗口（会提示输密码）："
    fi
    echo "  ${src}"
    open "${PLUGINS_DIR}"
    open -R "${src}"
    return
  fi

  sudo mkdir -p "${PLUGINS_DIR}"
  sudo install -m755 "${src}" "${dst}"
  echo "插件已安装到 ${dst}"
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
  sed -n '2,15p' "${BASH_SOURCE[0]}"
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
