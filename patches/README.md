# librime Windows 外部插件加载补丁

`librime-windows-plugin-loading.patch` 修复 librime 上游在 Windows 下无法使用
外部 DLL 插件的三个问题，使 `librime-qingjian.dll` 能被自动扫描、加载并正常链接。

已提交上游 PR：**https://github.com/rime/librime/pull/1243**（三个 commit）。

## 解决的问题

librime 的插件模块（`plugins/plugins_module.cc`）在启动时通过
`current_module_path()` 定位自身所在目录，再扫描其下的 `rime-plugins/` 子目录，
动态加载其中的插件。该机制在 Windows 上有三处缺口：

1. **插件目录探测**：`current_module_path()` 的 `_WIN32` 分支是空实现（
   `// TODO: implement this when ready to support DLL plugins on Windows`），
   导致 `LoadPlugins("rime-plugins")` 退化为相对路径扫描，永远找不到插件。
2. **`dl` 依赖**：`ENABLE_EXTERNAL_PLUGINS` 下无条件链接 `dl`（`dlopen`），
   Windows 无此库，开启该选项直接链接失败。
3. **符号导出**：librime 在 Windows 只导出 `RIME_DLL` 标记的符号，而外部插件
   依赖的内部类（如 `CacheTranslation`、`Candidate::GetGenuineCandidate`）未
   标记，故插件链接时出现 undefined reference（Linux 默认全导出，无此问题）。

## 补丁内容（三个 commit）

**commit 1 — `feat(plugins): load external DLL plugins on Windows`**

1. `plugins/plugins_module.cc`
   - 用 `GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
     GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, &rime_require_module_plugins)`
     + `GetModuleFileNameW` 实现 `symbol_location()`，语义等价 POSIX 的 `dladdr`。
   - `current_module_path()` 提升到公共分支，两个平台复用。
2. `src/CMakeLists.txt`
   - `dl` 依赖改为 `ENABLE_EXTERNAL_PLUGINS AND NOT WIN32`。

**commit 2 — `fix(plugins): export all symbols from rime.dll on Windows`**

- `src/CMakeLists.txt` 给 `rime` 目标加：
  - `WINDOWS_EXPORT_ALL_SYMBOLS ON`（MSVC，经 `.def` 全导出）；
  - `if(MINGW) target_link_libraries(rime "-Wl,--export-all-symbols")`（MinGW，
    `WINDOWS_EXPORT_ALL_SYMBOLS` 对 MinGW ld 无效，需显式链接器 flag）。

**commit 3 — `docs(windows): document external plugin support`**

- `README-windows.md` 新增「Build external plugins」一节：说明
  `ENABLE_EXTERNAL_PLUGINS=ON` 在 Windows 可用，插件 DLL 从 `rime.dll` 同级的
  `rime-plugins\` 目录加载。

### 风格约定（对齐 librime）

- `#include <windows.h>` 置于 `#ifdef _WIN32` 块内（同 `deployment_tasks.cc`）。
- 不加 `NOMINMAX` / `WIN32_LEAN_AND_MEAN`：新增代码与后续
  `current_module_path()` / `rime_plugins_initialize()` 均不用 `std::min`/`std::max`。
- C API 调用不加 `::` 前缀（对齐同文件 `dladdr(...)`）；用 `fs::` 别名。

## 验证

- `git apply --check` 对 librime master 干净应用。
- **Windows 端到端**（MinGW-w64，gcc 16.2，MSYS2 环境）：构建 librime
  （`ENABLE_EXTERNAL_PLUGINS=ON`）+ 插件 DLL，驱动真实输入，确认：
  ```
  loading plugin 'qingjian' from ...\librime-qingjian.dll
  registering component: gloss_filter
  loaded plugin: qingjian
  候选 1: development | 注释: 发展
  ```
- `symbol_location()` 函数体在 MSVC `cl` 19.44（`/W4 /std:c++17`）下零警告编译。
- 另确认 MSVC `<windows.h>` 会污染 `std::min`/`std::max`，但本补丁路径不受影响。

## 应用方法

对一份 librime 源码树：

```bash
cd /path/to/librime
git apply /path/to/rime-plugin-qingjian/patches/librime-windows-plugin-loading.patch
# 或先干跑校验
git apply --check /path/to/rime-plugin-qingjian/patches/librime-windows-plugin-loading.patch
```

构建 librime 时开启外部插件支持：

```bash
cmake -B build -DENABLE_EXTERNAL_PLUGINS=ON -DBUILD_SHARED_LIBS=ON ...
cmake --build build
```

构建出的 `rime.dll` 会自动扫描其同级 `rime-plugins/` 目录加载插件，且导出的
全部符号可供外部插件链接。

## 说明

- 该补丁面向 **librime master** 生成，已通过 `git apply --check` 校验可干净应用。
- 若你的 librime 版本不同，`git apply` 可能报上下文不匹配，按上述三处思路手改即可。
