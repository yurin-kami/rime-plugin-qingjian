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
brew install boost librime
```

（librime 的 glog / yaml-cpp / leveldb / marisa / opencc 依赖会一并装上；
Makefile 会自动加上 Homebrew 的头文件与库路径。）

**Windows**（MSVC + vcpkg，与小狼毫/librime 官方一致）

先装 Visual Studio（勾选「使用 C++ 的桌面开发」工作负载）与
[vcpkg](https://github.com/microsoft/vcpkg)，然后装依赖：

```powershell
vcpkg install boost glog yaml-cpp leveldb marisa-trie opencc --triplet x64-windows-static
```

Windows 用 CMake 构建（见下方「开发」），还需要一份应用了本仓库补丁的 librime 源码树。

Linux / macOS 也要再克隆一份 librime 源码树（插件编译需要它的内部头文件）：

```bash
git clone --depth 1 https://github.com/rime/librime .analysis/librime
```

不想放到 `.analysis/` 的话，克隆到任意路径，编译时用 `make so RIME_SRC=<路径>` 指定。

## 安装

装好依赖、克隆完 librime 源码树后，一条命令构建安装：

```bash
./install.sh all --tsv-dir /path/to/qingjian/assets/glossary
```

`.so`（macOS 上为 `.dylib`）会装到系统插件目录，数据装到你的 Rime 用户目录
（无需 root）。Linux 用 sudo 安装插件；macOS 鼠须管（Squirrel）用户则按脚本
提示，把编译好的 `.dylib` 拖进弹出的 Finder 窗口——`/Library/Input Methods/`
受系统 TCC 保护，sudo 命令行写不进，只能走 Finder 授权；数据装到
`~/Library/Rime/qingjian/`。

`install.sh` 做的事：编译 `librime-qingjian.so` 并装到 `/usr/lib/rime-plugins/`，
把释义表生成 `qingjian.zh_en.bin` / `qingjian.en_zh.bin` 装到 Rime 用户目录的
`qingjian/` 子目录，并安装配置示例。macOS 鼠须管用户会自动链接其内嵌的
librime（而非 Homebrew 版）再安装——否则插件与宿主各绑一个 librime 实例，
模块注册互不可见。

**Windows（小狼毫）**：把编译出的 `librime-qingjian.dll` 复制到小狼毫的
`rime-plugins\` 目录（与 `rime.dll` 同级）。小狼毫使用的 librime 还必须包含
`patches/librime-windows-plugin-loading.patch`；不能只替换插件 DLL。

释义数据与配置和 Linux 相同（`.bin` 放用户数据目录的 `qingjian\` 子目录，配置示例
改名为 `<你的方案>.custom.yaml`）。完整词库从青简仓库获取：

```powershell
git clone --depth 1 https://github.com/qingjian-team/qingjian .analysis/qingjian
python tools/build_gloss.py .analysis/qingjian/assets/glossary/glossary-en.tsv `
  <Rime用户目录>\qingjian\qingjian.zh_en.bin
python tools/build_gloss.py .analysis/qingjian/assets/glossary/glossary-zh.tsv `
  <Rime用户目录>\qingjian\qingjian.en_zh.bin --lowercase-key
```

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

然后重新部署（fcitx5-rime 输入法图标 → 重新部署；macOS 鼠须管 → 重新部署）。

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

Windows（MSVC）等价命令：

```powershell
# 纯 C++ 单测 + Python 单测（无需 librime）
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

# 编译 librime-qingjian.dll（需先构建好 librime，并指定其 rime.lib 导入库）
# 用独立构建目录 build-plugin，避免与上面的单测构建目录冲突
cmake -S . -B build-plugin -A x64 -DCMAKE_TOOLCHAIN_FILE=<vcpkg.cmake> `
  -DBUILD_PLUGIN=ON -DRIME_SRC=<librime 源码树> -DRIME_LIBRARY=<rime.lib>
cmake --build build-plugin --config Release
```

构建 Windows librime 前应用本仓库补丁：

```powershell
git -C <librime源码树> apply <本仓库>/patches/librime-windows-plugin-loading.patch
cmake -S <librime源码树> -B <librime源码树>/build -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg.cmake> -DVCPKG_TARGET_TRIPLET=x64-windows-static `
  -DBUILD_STATIC=ON -DENABLE_EXTERNAL_PLUGINS=ON
cmake --build <librime源码树>/build --config Release
```

> 注意：librime 上游尚未实现 Windows 下「从插件目录加载外部 DLL 插件」——
> `plugins/plugins_module.cc` 的 `current_module_path()` 在 `_WIN32` 分支为空。
> 本仓库代码与构建已支持 Windows，并附补丁
> `patches/librime-windows-plugin-loading.patch`，应用到 librime 后即可加载。

macOS 鼠须管手动编译（`install.sh so` 会自动带上这些参数）：

```bash
make so RIME_LOGGING=0 \
  RIME_LIB="/Library/Input Methods/Squirrel.app/Contents/Frameworks/librime.1.dylib"
```

## 许可

GPL-3.0-or-later（与默认词典数据一致）。

## 致谢

候选释义数据与释义显示功能参考自[青简](https://qingjian.app)输入法（[qingjian-team/qingjian](https://github.com/qingjian-team/qingjian)），在此致谢。
