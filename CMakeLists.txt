# 跨平台构建：纯 C++ 单元测试 + librime-qingjian 插件。
#
# 常用：
#   cmake -S . -B build                      # 生成（含纯 C++ 单测）
#   cmake --build build                      # 构建
#   ctest --test-dir build --output-on-failure  # 跑单测
#
# 构建插件（需 librime 内部头 + librime 库 + glog）：
#   cmake -S . -B build -DBUILD_PLUGIN=ON \
#     -DRIME_SRC=<librime 源码树> -DRIME_LIBRARY=<librime 库/导入库>
#   cmake --build build
#
# 说明：Makefile 仍是 Linux/macOS 的常用入口；本文件主要面向 Windows（MSVC
# 与小狼毫/librime 官方一致），同时也兼容 Linux/macOS。

cmake_minimum_required(VERSION 3.15)
project(rime-qingjian LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  # /utf-8：源码为 UTF-8（含中文注释与字符串字面量）。
  add_compile_options(/W4 /utf-8)
else()
  add_compile_options(-Wall -Wextra)
endif()

# 生成 build/rime/build_config.h（等价于 Makefile 的 build/rime/build_config.h 规则）。
set(RIME_PLUGINS_DIR "rime-plugins")
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/rime)
configure_file(build_config.h.in ${CMAKE_BINARY_DIR}/rime/build_config.h @ONLY)

# ---- 纯 C++ 核心（不依赖 librime）----
add_library(qingjian_core STATIC
  src/gloss_bin.cc
  src/gloss_dictionary.cc
)
target_include_directories(qingjian_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---- 纯 C++ 单测（不依赖 librime）----
enable_testing()
foreach(test_name test_gloss_bin test_gloss_dictionary test_gloss_direction)
  add_executable(${test_name} tests/${test_name}.cc)
  target_include_directories(${test_name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(${test_name} PRIVATE qingjian_core)
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# ---- 跨语言对拍 + Python 单测（可选，需 Python3）----
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  add_executable(test_gloss_fixture tests/test_gloss_fixture.cc)
  target_include_directories(test_gloss_fixture PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(test_gloss_fixture PRIVATE qingjian_core)

  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/sample.bin
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_gloss.py
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/sample.tsv
            ${CMAKE_BINARY_DIR}/sample.bin
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_gloss.py
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/sample.tsv)
  add_custom_target(test-data DEPENDS ${CMAKE_BINARY_DIR}/sample.bin)
  add_dependencies(test_gloss_fixture test-data)
  add_test(NAME test_gloss_fixture
           COMMAND test_gloss_fixture ${CMAKE_BINARY_DIR}/sample.bin)

  add_test(NAME test_build_gloss
           COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_build_gloss -v
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ---- 插件（需 librime 内部头 + librime 库 + glog）----
option(BUILD_PLUGIN "构建 librime-qingjian 插件（需 librime 与 glog）" OFF)
if(BUILD_PLUGIN)
  set(RIME_SRC "" CACHE PATH "librime 源码树路径（内部头 src/ include/）")
  set(RIME_LIBRARY "" CACHE FILEPATH "librime 库（Windows 用 rime.lib 导入库）")
  set(GLOG_LIBRARY "" CACHE FILEPATH "glog 库（缺省自动查找）")

  if(NOT RIME_SRC)
    message(FATAL_ERROR "BUILD_PLUGIN=ON 需要 -DRIME_SRC=<librime 源码树>")
  endif()

  add_library(rime-qingjian MODULE
    src/gloss_filter.cc
    src/module.cc
  )
  target_link_libraries(rime-qingjian PRIVATE qingjian_core)

  # 本仓库生成的 build_config.h 优先于 librime 的（SYSTEM 头排在其后）。
  target_include_directories(rime-qingjian PRIVATE ${CMAKE_BINARY_DIR})
  target_include_directories(rime-qingjian SYSTEM PRIVATE
    ${RIME_SRC}/src
    ${RIME_SRC}/include)

  target_compile_definitions(rime-qingjian PRIVATE GLOG_USE_GLOG_EXPORT)
  if(WIN32)
    # 从 librime.dll 导入 RimeRegisterModule / Registry 等符号。
    target_compile_definitions(rime-qingjian PRIVATE RIME_IMPORTS)
  endif()

  if(RIME_LIBRARY)
    target_link_libraries(rime-qingjian PRIVATE ${RIME_LIBRARY})
  else()
    target_link_libraries(rime-qingjian PRIVATE rime)
  endif()

  # 内部头引用 Boost；显式传播头目录，不能依赖系统搜索路径。
  find_package(Boost REQUIRED)
  target_include_directories(rime-qingjian SYSTEM PRIVATE ${Boost_INCLUDE_DIRS})

  if(GLOG_LIBRARY)
    target_link_libraries(rime-qingjian PRIVATE ${GLOG_LIBRARY})
  else()
    find_package(glog CONFIG QUIET)
    if(glog_FOUND)
      target_link_libraries(rime-qingjian PRIVATE glog::glog)
    else()
      target_link_libraries(rime-qingjian PRIVATE glog)
    endif()
  endif()

  # 产物名：librime-qingjian.dll / librime-qingjian.so。
  set_target_properties(rime-qingjian PROPERTIES
    PREFIX "librime-"
    OUTPUT_NAME "qingjian")
endif()
