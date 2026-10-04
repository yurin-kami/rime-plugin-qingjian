# Rime 青简释义插件

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

编译插件（`make so`）需要以下开发包；单元测试（`make check`）只需 `g++` 与
`python3`。此外还需 librime 源码树（内部头文件，见下）。

| 依赖 | Arch (pacman) | Ubuntu/Debian (apt) | Fedora (dnf) | macOS (Homebrew) |
| --- | --- | --- | --- | --- |
| 编译工具链 | base-devel python | build-essential python3 | gcc-c++ make python3 | Xcode 命令行工具 |
| boost | boost | libboost-all-dev | boost-devel | boost |
| glog | glog | libgoogle-glog-dev | glog-devel | glog |
| yaml-cpp | yaml-cpp | libyaml-cpp-dev | yaml-cpp-devel | yaml-cpp |
| leveldb | leveldb | libleveldb-dev | leveldb-devel | leveldb |
| marisa | marisa | libmarisa-dev | marisa-devel | marisa |
| opencc | opencc | libopencc-dev | opencc-devel | opencc |
| librime（链接） | librime | librime-dev | librime-devel | librime |

librime 内部头不随发行版分发，须克隆源码树供 `make so` 使用（默认
`RIME_SRC=.analysis/librime`，可用变量覆盖）：

```bash
git clone --depth 1 https://github.com/rime/librime .analysis/librime
```

## 安装

先按上表装好依赖并克隆 librime 源码树，再一键构建安装（`.so` 需 root，
数据装到用户目录无需 root）：

```bash
# 依赖（示例：Ubuntu；其他发行版见上表）
# sudo apt install -y build-essential python3 libboost-all-dev libgoogle-glog-dev \
#   libyaml-cpp-dev libleveldb-dev libmarisa-dev libopencc-dev librime-dev

git clone --depth 1 https://github.com/rime/librime .analysis/librime

# 下载青简释义表后构建安装
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
