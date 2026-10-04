# 架构说明

## 目标

把青简输入法的候选释义功能——「候选词右侧显示对应释义」——做成一个 Rime 插件：
输入中文时，候选词右侧显示对应的英文释义；输入英文时，右侧显示中文释义。
释义始终是辅助信息，不改动候选词本体与选词逻辑。

## 形态：librime 原生插件（`.so`）

插件编译为动态库 `librime-qingjian.so`，安装到 librime 的插件目录
`/usr/lib/rime-plugins/`。librime 启动时由内置的 `plugins` 模块扫描该目录、
`dlopen` 加载并调用模块的 `initialize`，把组件注册进 Registry（见
`librime/src/rime/config/plugins.cc` 与 `src/rime/gear/gears_module.cc` 的同款机制）。

选择原生 `.so` 而非 Lua 插件：释义表约 28 万条，用 C++ `mmap` + 二分查找，
不进入 Lua GC 堆。

## 组件：一个 Filter

插件注册一个 `gloss_filter` 组件（`Filter` 子类），挂在输入方案的
`engine/filters` 里。Filter 在翻译器产出候选之后、候选上屏之前运行，
逐条读取候选文本、查释义表、把结果写进 `Candidate` 的 `comment` 字段。
`comment` 就是前端渲染在候选右侧的那行字，ibus-rime、fcitx5-rime、小狼毫、
鼠须管都会显示，因此插件对所有前端生效。

实现参考 librime 自带的 `reverse_lookup_filter`（`src/rime/gear/reverse_lookup_filter.cc`）：
用一个 `CacheTranslation` 包装原 Translation，在 `Peek()` 里对每条候选打注释。

## 数据：释义表与二进制索引

### 源数据

- 中→英：`glossary-en.tsv`，约 23.2 万条。
- 英→中：`glossary-zh.tsv`，约 4.5 万条（键为小写）。

TSV 格式：`词\t[词性. ]译词[|读音]\t…`，`#` 开头为注释行，按码点排序，
UTF-8 无 BOM。释义最多取两条，词性可省略，读音以 `|` 分隔（仅日语用）。

### 二进制索引 `gloss.bin`

启动时不解析几十万行文本，而是预处理成定宽索引 + 字符串池（arena），
用 `mmap` 零拷贝映射后二分查找。这与青简把释义表打包成 `.qj` mmap 容器的
思路一致（青简还多了哈希索引，本插件用有序二分，等价 O(log n)、无哈希冲突）。

布局（小端序）：

```
偏移 0   8 字节  magic "QJGLOSS1"
偏移 8   8 字节  entry 数量 N
偏移 16  N × 16 字节 定宽索引，每条：
         key_off:u32  key_len:u32  val_off:u32  val_len:u32   （指向 arena）
之后    arena：按索引顺序，每条 = key 字节 + value 字节（由偏移与长度分隔）
```

查找：`mmap` 整个文件，对索引做二分，`memcmp` 比较 key，命中后按
`val_off/val_len` 返回 `string_view`，零分配。

## 多词典可切换

释义数据层抽象为「词典」：一个方向（中→英或英→中）对应一个 `gloss.bin` 文件。
插件支持在配置里列出一个或多个词典名，按优先级顺序查找（个人词典优先于随包词典，
与青简的 `LayeredTranslator` 一致）。切换 = 改配置里启用的词典名或优先级，
部署后生效。

默认词典为青简（`qingjian`）。其他词典（如 CC-CEDICT）只要用同一工具
转成 `gloss.bin` 即可接入，无需改代码。

## 配置

在输入方案的 `.schema.yaml`（或 `.custom.yaml` 补丁）里：

```yaml
engine:
  filters:
    - gloss_filter        # 可加在 uniquifier 之后

gloss_filter:
  # 中→英词典，按优先级排列，先查排在前面的词典
  dictionaries_zh_en: [ qingjian ]
  # 英→中词典
  dictionaries_en_zh: [ qingjian ]
  # 是否覆盖已有注释
  overwrite_comment: false
```

词典文件解析顺序：对每个名字，先查用户数据目录 `<user>/qingjian/<名>.zh_en.bin`，
再查共享数据目录 `<shared>/qingjian/<名>.zh_en.bin`（英→中同理，后缀 `.en_zh.bin`）。

## 代码结构

```
src/gloss_bin.h/.cc        二进制索引格式 + mmap 读取器（纯 C++，可单测）
src/gloss_dictionary.h/.cc 多词典抽象层（组合若干 gloss_bin，按优先级查找）
src/gloss_filter.h/.cc     Filter 组件（对接 librime，写 comment）
src/module.cc              模块注册（RIME_REGISTER_MODULE）
tools/build_gloss.py       TSV → gloss.bin 预处理
tests/                     单元测试（纯 C++，不依赖 librime）
Makefile                   构建 .so、跑测试
install.sh                 手动安装脚本
plum/                      plum 配方
```

## 构建与依赖

- 编译器：`g++`（C++17）。
- 链接：`-lrime`；发行版 librime 另需 `-lglog`（与日志 ABI 一致）。
  macOS 鼠须管内嵌 librime 未启用 glog，用 `RIME_LOGGING=0` 跳过并改用
  其内嵌 `librime.1.dylib`（`RIME_LIB` 指定）。
- 头文件：librime 内部头（`src/rime/*.h`、`include/`）来自 librime 源码树，
  Arch 的 `librime` 包只提供 `rime_api.h`，故构建时用 `-I` 指向一份 librime 源码。
- 需系统包：`boost`（`common.h` 引用了 boost 头文件）；`glog` 仅发行版需要。

> 注意：`build_config.h` 由 cmake 从 `build_config.h.in` 生成，本仓库的 Makefile
> 会生成一份等价的 `build/build_config.h`（默认定义 `RIME_ENABLE_LOGGING`，
> `RIME_LOGGING=0` 时注释掉，与目标 librime 保持一致）。

## 许可

默认词典数据来自青简 `assets/glossary/`（GPL-3.0-or-later），插件随 GPL-3.0-or-later。
代码与数据分开：数据目录不进 git，由 `tools/build_gloss.py` 从上游 TSV 生成。
