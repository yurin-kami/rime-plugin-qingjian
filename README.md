# Rime 青简释义插件

[![CI](https://github.com/yurin-kami/rime-plugin-qingjian/actions/workflows/ci.yml/badge.svg)](https://github.com/yurin-kami/rime-plugin-qingjian/actions/workflows/ci.yml)

把[青简](https://qingjian.app)输入法的招牌功能做成 Rime 插件：输入中文时，
候选词右侧显示对应的英文释义；输入英文时，右侧显示中文释义。释义始终是辅助
信息，不改动候选本身与选词逻辑。

```text
1  开发     v. develop / v. exploit
2  编程     v. program
3  架构     n. architecture
```

## 特性

- 双向释义：中文候选 → 英文释义，英文候选 → 中文释义。
- 多词典可切换：按名字引用词典，按优先级叠层查找（个人词典优先于随包词典）。
- 微秒级查词：释义表预打包成 mmap 友好的二进制索引，启动不解析几十万行文本。
- 跨前端：实现于 librime 层，ibus-rime、fcitx5-rime、小狼毫、鼠须管通用。

## 架构

详见 [arch.md](arch.md)。核心是一个 `gloss_filter` 组件（librime Filter），
在候选产出后把释义写入 `Candidate` 的 comment 字段。

## 依赖

- 运行时：librime ≥ 1.0（本机已装 1.17）。
- 构建：`g++`（C++17）、`boost` 头文件（`pacman -S boost`）、librime 源码树
  （供内部头文件，见 Makefile 的 `RIME_SRC`）。
- 数据生成：Python 3（仅 `install.sh` 生成数据时用）。

## 安装

一键安装（需要 root 安装 `.so`，数据装到用户目录无需 root）：

```bash
# 先装构建依赖（仅编译 .so 需要）
sudo pacman -S boost

# 下载青简释义表，然后构建并安装
./install.sh all --tsv-dir /path/to/qingjian/assets/glossary
```

`install.sh` 做的事：编译 `librime-qingjian.so` 并装到 `/usr/lib/rime-plugins/`，
把释义表生成 `qingjian.zh_en.bin` / `qingjian.en_zh.bin` 装到 Rime 用户目录的
`qingjian/` 子目录，并安装配置示例。

## 启用

在你的输入方案（如 `double_pinyin`、`luna_pinyin`）的补丁文件里加：

```yaml
patch:
  engine/filters/+:
    - gloss_filter
  gloss_filter:
    dictionaries_zh_en: [ qingjian ]   # 中→英词典，按优先级
    dictionaries_en_zh: [ qingjian ]   # 英→中词典
    overwrite_comment: false           # 已有注释时不覆盖
```

然后重新部署（fcitx5-rime 输入法图标 → 重新部署）。

## 多词典切换

一个词典名对应 `<数据目录>/qingjian/<名字>.zh_en.bin`（中→英）与
`<名字>.en_zh.bin`（英→中）。切换只需改配置里的名字或顺序：

```yaml
gloss_filter:
  dictionaries_zh_en: [ 我的生词本, qingjian ]  # 先查个人，再查随包
```

个人词典放用户数据目录（如 `~/.local/share/fcitx5/rime/qingjian/`），随包词典放
共享数据目录（`/usr/share/rime-data/qingjian/`）。

## 数据

默认词典数据来自青简 `assets/glossary/`（GPL-3.0-or-later，LLM 生成，约 23.2 万条
中→英 + 4.5 万条英→中）。数据文件不进本仓库，由 `tools/build_gloss.py` 从上游
TSV 生成：

```bash
python3 tools/build_gloss.py glossary-en.tsv qingjian.zh_en.bin
python3 tools/build_gloss.py glossary-zh.tsv qingjian.en_zh.bin --lowercase-key
```

## 开发

```bash
make check    # C++ 单测 + Python 单测 + 跨语言对拍
make so       # 编译 librime-qingjian.so（需 boost 与 RIME_SRC）
```

## 许可

GPL-3.0-or-later（与默认词典数据一致）。
