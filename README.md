# Rime 青简释义插件

把[青简](https://qingjian.app)输入法的候选释义功能做成 Rime 插件：输入中文时，
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
- 二进制索引：释义表预打包成二进制索引，mmap 映射后二分查找，启动时不逐行解析文本。
- 跨前端：实现于 librime 层，ibus-rime、fcitx5-rime、小狼毫、鼠须管通用。

## 架构

详见 [arch.md](arch.md)。核心是一个 `gloss_filter` 组件（librime Filter），
在候选产出后把释义写入 `Candidate` 的 comment 字段。

## 依赖

编译插件（`make so`）需要几个开发库；只跑单元测试（`make check`）的话，
只要 `g++` 和 `python3` 就够。

按你的系统装依赖（选一条）：

**Arch Linux**

```bash
sudo pacman -S --needed base-devel python boost glog yaml-cpp leveldb marisa opencc librime
```

**Ubuntu / Debian**

```bash
sudo apt install -y build-essential python3 libboost-all-dev libgoogle-glog-dev \
  libyaml-cpp-dev libleveldb-dev libmarisa-dev libopencc-dev librime-dev
```

**Fedora**

```bash
sudo dnf install -y gcc-c++ make python3 boost-devel glog-devel \
  yaml-cpp-devel leveldb-devel marisa-devel opencc-devel librime-devel
```

**macOS**（先装 Homebrew，并 `xcode-select --install` 装命令行工具）

```bash
brew install boost glog yaml-cpp leveldb marisa opencc librime
```

无论哪个系统，都要再克隆一份 librime 源码树（插件编译需要它的内部头文件）：

```bash
git clone --depth 1 https://github.com/rime/librime .analysis/librime
```

不想放到 `.analysis/` 的话，克隆到任意路径，编译时用 `make so RIME_SRC=<路径>` 指定。

## 安装

装好依赖、克隆完 librime 源码树后，一条命令构建安装：

```bash
./install.sh all --tsv-dir /path/to/qingjian/assets/glossary
```

`.so` 会装到系统插件目录（需 sudo 输密码），数据装到你的 Rime 用户目录（无需 root）。

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
