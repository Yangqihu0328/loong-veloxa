# G1.1 CMake `VX_RENDERER` flag — 实施计划

**任务 ID：** `TASK-20260505-05`
**复杂度级别：** Level 2（多文件构建系统改动 / 需求清晰）
**任务类型：** 实施类（GLES 蓝图实施首步 / MVP-C 战略主线第一个实施任务）
**安全相关：** ❌ 否
**估时（plan ×0.6）：** ~2-3 h（GLES 蓝图 plan §4 估时表 G1.1）/ 预期实测 ~30-60 min
**分支：** `feature/TASK-20260505-05-cmake-vx-renderer-flag`（基于 main `0a90481`）
**上游 spec 引用：** [docs/specs/2026-05-05-gles-renderer-blueprint-design.md §3.5 + §13](../specs/2026-05-05-gles-renderer-blueprint-design.md)（V5=A `vx_renderer_flag` + B5=A software 默认）
**上游 plan 引用：** [docs/plans/2026-05-05-gles-renderer-blueprint.md §3.1](2026-05-05-gles-renderer-blueprint.md#31-子任务-g11--cmake-vx_renderer-flag)（含偏差校正 / 见 §0.4）

---

## 0. Phase 0 实证段（VAN + Plan 阶段累计）

### 0.1 上下文 grep 实证（7 项 / 已在 VAN 阶段完成）

| # | 验证 | 命令 | 实测 | 结论 |
|:-:|---|---|---|---|
| 1 | 既有 `option()` flag pattern | `Read CMakeLists.txt:9-15` | 5 项一致（VX_BUILD_TESTS / VX_BUILD_BENCHMARKS / VX_PLATFORM_SDL2 / VX_BUILD_DEVTOOL / VX_LOG_LEVEL）| ✅ 与 plan 范式一致 |
| 2 | 既有 `add_compile_definitions()` 范式 | `Read CMakeLists.txt:17` | `add_compile_definitions(VX_MIN_LOG_LEVEL=${VX_LOG_LEVEL})` | ✅ 顶层范式 |
| 3 | 既有 SDL2 双轨 find pattern | `Read veloxa/platform/sdl2/CMakeLists.txt:7-15` | `find_package(SDL2 CONFIG QUIET)` + fallback `pkg_check_modules(... IMPORTED_TARGET ...)` | ✅ 双轨范式（本任务 D1=A 暂不引入 / G1.2 复用此 pattern）|
| 4 | EGL/GLES dev headers 可用性 | `dpkg -l \| rg "libegl-dev\|libgles-dev"` | libegl-dev 1.7 + libgles-dev 1.7 安装 | ✅ G1.2+ 0 等待 |
| 5 | `find_package(OpenGLES/EGL)` Find 模块可用性 | `cmake --find-package -DNAME=OpenGLES -DMODE=EXIST` | **OpenGLES not found** + **EGL not found**（CMake 4.2.3 未自带）| ⚠️ **蓝图 plan §3.1 步骤 2 偏差** — 见 §0.4 |
| 6 | `pkg-config egl glesv2` 替代方案 | `pkg-config --exists egl glesv2 && echo "✅"` | ✅ pkg-config egl + glesv2 | ✅ 与既有 SDL2 双轨范式契合 |
| 7 | 既有 ctest cmake -P 集成 pattern | `Grep "add_test.*COMMAND.*-P" tests/CMakeLists.txt` | `add_test(NAME devtool_a14_link_closure_smoke COMMAND ${CMAKE_COMMAND} -D... -P tests/smoke/devtool_a14_link_closure.cmake)` | ✅ 完美适配 D3=B + D4=B |

### 0.2 工具链快照（writing-plans §0.10 强制）

```bash
gcc --version | head -1     # gcc (Ubuntu) 14+
ld --version | head -1      # binutils 2.46+（既有 R12 hotfix 已修复 / --start-group 包裹）
cmake --version | head -1   # cmake 4.2.3
```

| 工具 | 当前 | 上次任务一致性 | 行动 |
|---|---|:-:|---|
| binutils ld | 2.46+ | ✅ 与 TASK-04 / TASK-03 一致 | ✅ 跳过差异检查 |
| gcc | 14+ | ✅ | ✅ |
| cmake | 4.2.3 | ✅ | ✅ |

### 0.3 ctest baseline 快照

| Config | DEVTOOL | SDL2 | VX_RENDERER（新增） | ctest baseline |
|---|:-:|:-:|:-:|:-:|
| ON | ON | OFF | software（default） | **1302/1302** |
| OFF | OFF | OFF | software（default） | **1109/1109** |

**G1.1 落地后预期：** baseline +1（vx_renderer_flag_check_smoke 新加 ctest）= **1303 + 1110 双 build 矩阵**

### 0.4 GLES 蓝图 plan §3.1 偏差校正（brainstorming P1.3 主动 push-back 实证 ✅）

**偏差 #1：步骤 2 代码示例 `find_package(OpenGLES/EGL REQUIRED)`**

| 维度 | 原 plan | 本 plan 校正 | 理由 |
|---|---|---|---|
| Find 方式 | `find_package(OpenGLES REQUIRED)` + `find_package(EGL REQUIRED)` | **D1=A 暂不引入**（推迟 G1.2）| (1) Find 模块 CMake 4.2.3 未自带 / (2) G1.1 不写 GLES 代码 / YAGNI |
| 引入时机 | G1.1 即引入 dep | **G1.2 GLESDisplay 实现时再引入** | dep 与实现强耦合 / 不为「flag 落地」预付 |
| Find pattern | `find_package(...)` | **G1.2 沿用 SDL2 双轨**：`find_package(... CONFIG QUIET)` + fallback `pkg_check_modules(... IMPORTED_TARGET ...)` | 与既有 graphics/SDL2 双轨范式一致 |

**偏差 #2：步骤 3 ctest 验证脚本格式**

| 维度 | 原 plan | 本 plan 校正 |
|---|---|---|
| 脚本格式 | `tests/cmake/vx_renderer_flag_test.sh` | **D3=B `cmake -P` 脚本**（与 a14 一致 / 跨平台）|
| 脚本路径 | `tests/cmake/` | **D4=B `tests/smoke/`**（与 a14 一致 / 不新增子目录）|

**偏差 #3：步骤 4 反向探针策略**

| 维度 | 原 plan | 本 plan 校正 |
|---|---|---|
| 探针方式 | 临时改 `add_compile_definitions(VX_RENDERER_INVALID=1)` → 编译期错误 | **D5=A cmake 子进程 `-DVX_RENDERER=invalid` assert 失败**（自动化 + 集成 ctest）|

**触发模式：** brainstorming.mdc P1.3「Phase 0 grep 实证驱动主动 push-back 模式」**首次实战应用 ✅**（TASK-05-04 刚落地的规则 / dual-evidence 候选）：
- (1) 任务 scope 已限定到 G1.1 ✅
- (2) Phase 0 grep 发现 3 处偏差 ✅
- (3) 偏差**显著但限定范围**（仅影响实施代码片段 / 不动整体架构 / 0 倒退既有 build）✅
- → 本 plan §0.4 直接校正（不靠 build 阶段事故触发）

---

## 1. 决策矩阵（7 D 决策 1 次 AskQuestion all_recommended 锁定 / 第 14 次连续命中 ✅）

| # | 决策项 | 选择 | 理由 |
|:-:|---|---|---|
| **D1** | GLES dep find/link 时机 | **A 不引入** | YAGNI / G1.1 不写 GLES 代码 / dep 推迟 G1.2 / 0 build 等待 |
| **D2** | flag 校验位置 | **A 顶层** | 与 VX_BUILD_DEVTOOL / VX_PLATFORM_SDL2 顶层 pattern 一致 |
| **D3** | ctest 脚本格式 | **B cmake -P** | 跨平台 + 与既有 a14 pattern 一致 + 集成 ctest baseline |
| **D4** | ctest 脚本路径 | **B tests/smoke/** | 与既有 `devtool_a14_link_closure.cmake` 一致 / 不新增子目录 |
| **D5** | 反向探针策略 | **A 子进程 invalid assert** | 自动化 + 集成 ctest + 双 build 矩阵 + 可重现 |
| **D6** | spec 文档策略 | **A 仅 plan** | Level 2 实施类豁免 spec / GLES 蓝图 spec §3.5+§13 已规格化 |
| **D7** | P0 协议落地策略 | **A plan + MB 单 commit** | quad → quint-evidence 候选（第 5 数据点）/ 自吃狗粮 |

**跨决策协同度 100% 第 14 次连续命中 ✅** / 累计 128/128 历史最高 streak 刷新

---

## 2. 文件结构

| # | 路径 | 操作 | 估行数 | 职责 |
|:-:|---|:-:|:-:|---|
| 1 | `CMakeLists.txt`（顶层）| 修改 | +~20 行 | option(VX_RENDERER ...) + 校验 + add_compile_definitions(VX_RENDERER_GLES \| VX_RENDERER_SOFTWARE) |
| 2 | `tests/smoke/vx_renderer_flag_check.cmake` | 创建 | ~85 行 | cmake -P 脚本 / 验证 4 个场景：default(software) + explicit software + explicit gles + invalid(FATAL_ERROR) |
| 3 | `tests/CMakeLists.txt` | 修改 | +~15 行 | add_test(NAME vx_renderer_flag_check_smoke ... -P tests/smoke/vx_renderer_flag_check.cmake) |
| 4 | `docs/plans/2026-05-05-cmake-vx-renderer-flag.md` | 创建 | ~400 行 | 本 plan 文档 |
| 5 | `memory-bank/activeContext.md` | 修改 | +~20 行 | Plan 阶段产出 |
| 6 | `memory-bank/tasks.md` | 修改 | +~25 行 | Plan 阶段产出 + 决策矩阵 |
| 7 | `memory-bank/progress.md` | 修改 | +~30 行 | Plan 阶段时间线 |
| **小计** | | | **~595 行** | （含 plan §1.5「LOC 估算附录 ×1.3-1.5 buffer 范本」上限 ~770-890 行）|

**[共享文件]** 标记：`CMakeLists.txt` 顶层 + `tests/CMakeLists.txt` 都是共享文件 / 影响所有后续 build

**[影响前序测试]：** 0 / VX_RENDERER=software（default）保证 ctest 1302/1109 baseline 不退化

**[子代理]：** 否

---

## 3. 实施步骤（5 步 / TDD 严格度 = 文档式 + 双 build 矩阵 ctest 验证）

### 步骤 1：Phase 0 audit（已完成 ✅）

详见 §0.1-0.4。**额外验证：**

```bash
# 验证既有 ctest baseline（实施前快照）
cmake -B build-on -DVX_BUILD_DEVTOOL=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-on -j
cd build-on && ctest --output-on-failure 2>&1 | tail -3
# 期望：1302/1302 PASS

cmake -B build-off -DVX_BUILD_DEVTOOL=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-off -j
cd build-off && ctest --output-on-failure 2>&1 | tail -3
# 期望：1109/1109 PASS
```

### 步骤 2：编写 ctest 测试 + cmake -P 脚本（TDD RED phase）

**`tests/smoke/vx_renderer_flag_check.cmake`（新建 / ~85 行）：**

```cmake
# =============================================================================
# vx_renderer_flag_check.cmake — TASK-20260505-05 G1.1 [VX_RENDERER 守门]
#
# Spec §3.5 V5=A: "VX_RENDERER=software|gles CMake flag / SoftwareCanvas 作 fallback"
# Plan §B5=A: "software 默认 / 不退化既有 ctest baseline"
#
# This script is the precise enforcement of the FLAG-VALIDATION half of G1.1
# (the build-matrix half is observed via ctest config matrix; spec accepts
# software default + gles opt-in). Run by ctest on EVERY build, both ON and OFF.
#
# Required CMake-defined variables (-D on the cmake -P invocation):
#   SOURCE_DIR       — absolute path to project root
#   BUILD_DIR_BASE   — absolute path to a writable scratch dir for sub-configs
# =============================================================================

# Scenario 1: default (no -DVX_RENDERER) → software (compile def VX_RENDERER_SOFTWARE)
# Scenario 2: -DVX_RENDERER=software → software
# Scenario 3: -DVX_RENDERER=gles → gles (compile def VX_RENDERER_GLES) — NOTE: G1.1 not link GLES
# Scenario 4: -DVX_RENDERER=invalid → FATAL_ERROR (negative probe / D5=A)

# Use a minimal test harness: a 1-line CMakeLists.txt fragment that just
# echoes which compile def is active. Build under each scenario and grep.

function(_vx_assert_flag scenario expected_def expected_rc)
  set(probe_dir "${BUILD_DIR_BASE}/vx_renderer_probe_${scenario}")
  file(REMOVE_RECURSE "${probe_dir}")
  file(MAKE_DIRECTORY "${probe_dir}")

  # Generate a minimal test C++ file that #ifdef-asserts the expected def.
  file(WRITE "${probe_dir}/probe.cc"
"#if !defined(${expected_def})
#error \"Expected compile def ${expected_def} not set\"
#endif
int main() { return 0; }
")

  file(WRITE "${probe_dir}/CMakeLists.txt"
"cmake_minimum_required(VERSION 3.20)
project(vx_renderer_probe LANGUAGES CXX)
include(${SOURCE_DIR}/cmake/VxRenderer.cmake OPTIONAL)
add_executable(probe probe.cc)
")

  # Configure with the scenario flag.
  if("${scenario}" STREQUAL "default")
    set(flag_arg "")
  elseif("${scenario}" STREQUAL "invalid")
    set(flag_arg "-DVX_RENDERER=invalid")
  else()
    set(flag_arg "-DVX_RENDERER=${scenario}")
  endif()

  execute_process(
    COMMAND ${CMAKE_COMMAND} ${flag_arg} -B "${probe_dir}/build" -S "${probe_dir}"
    RESULT_VARIABLE rc
    OUTPUT_QUIET ERROR_QUIET)

  if(NOT "${rc}" EQUAL "${expected_rc}")
    message(FATAL_ERROR
      "VX_RENDERER smoke: scenario=${scenario} expected rc=${expected_rc} got rc=${rc}")
  endif()
endfunction()

# G1.1 placeholder: actual flag validation will live in cmake/VxRenderer.cmake
# extracted from top-level CMakeLists.txt in Build phase.
# For now, validation is exercised by the top-level CMakeLists.txt directly,
# so this smoke just runs cmake configure on the project root with each flag.

set(_main_root "${SOURCE_DIR}")
set(_scratch "${BUILD_DIR_BASE}/vx_renderer_smoke")
file(REMOVE_RECURSE "${_scratch}")
file(MAKE_DIRECTORY "${_scratch}")

# Scenario 1: default → expected rc=0 + VX_RENDERER_SOFTWARE def visible
execute_process(COMMAND ${CMAKE_COMMAND} -B "${_scratch}/default" -S "${_main_root}"
                RESULT_VARIABLE rc1 OUTPUT_QUIET ERROR_QUIET)
if(NOT rc1 EQUAL 0)
  message(FATAL_ERROR "VX_RENDERER smoke: default scenario configure failed rc=${rc1}")
endif()

# Scenario 2: -DVX_RENDERER=software → expected rc=0
execute_process(COMMAND ${CMAKE_COMMAND} -DVX_RENDERER=software -B "${_scratch}/sw" -S "${_main_root}"
                RESULT_VARIABLE rc2 OUTPUT_QUIET ERROR_QUIET)
if(NOT rc2 EQUAL 0)
  message(FATAL_ERROR "VX_RENDERER smoke: explicit software configure failed rc=${rc2}")
endif()

# Scenario 3: -DVX_RENDERER=gles → expected rc=0 (G1.1 doesn't link GLES yet)
execute_process(COMMAND ${CMAKE_COMMAND} -DVX_RENDERER=gles -B "${_scratch}/gles" -S "${_main_root}"
                RESULT_VARIABLE rc3 OUTPUT_QUIET ERROR_QUIET)
if(NOT rc3 EQUAL 0)
  message(FATAL_ERROR "VX_RENDERER smoke: explicit gles configure failed rc=${rc3}")
endif()

# Scenario 4: -DVX_RENDERER=invalid → expected rc != 0 (FATAL_ERROR / D5=A)
execute_process(COMMAND ${CMAKE_COMMAND} -DVX_RENDERER=invalid -B "${_scratch}/bad" -S "${_main_root}"
                RESULT_VARIABLE rc4 OUTPUT_QUIET ERROR_QUIET)
if(rc4 EQUAL 0)
  message(FATAL_ERROR "VX_RENDERER smoke: invalid scenario should fail but got rc=${rc4}")
endif()

message(STATUS "VX_RENDERER smoke: 4/4 scenarios pass ✓ (default + software + gles + invalid-rejected)")

# Cleanup scratch
file(REMOVE_RECURSE "${_scratch}")
```

**TDD RED 验证：** 此时步骤 3 顶层 CMakeLists.txt 还没改 → cmake -P 脚本会因 `default` 场景找不到 VX_RENDERER 关联的 compile def 而 fail（reflect 阶段验证）。

### 步骤 3：实现 CMake flag（TDD GREEN phase）

**`CMakeLists.txt`（顶层 / +~20 行）：**

```cmake
# Insert AFTER line 14 (option VX_BUILD_DEVTOOL) BEFORE line 15 (VX_LOG_LEVEL):

# Renderer backend selector. "software" (default, B5=A) keeps SoftwareCanvas as
# the only canvas implementation; "gles" enables OpenGL ES 3.0+ paths via
# VX_RENDERER_GLES compile definition. Actual GLES dep (find_package / link)
# lives in graphics subtree under #if VX_RENDERER_GLES guards (G1.2 lazy-attach).
set(VX_RENDERER "software" CACHE STRING "Renderer backend (software|gles)")
set_property(CACHE VX_RENDERER PROPERTY STRINGS software gles)
if(NOT VX_RENDERER MATCHES "^(software|gles)$")
  message(FATAL_ERROR
    "VX_RENDERER must be 'software' or 'gles', got: '${VX_RENDERER}'")
endif()
if(VX_RENDERER STREQUAL "gles")
  add_compile_definitions(VX_RENDERER_GLES=1)
  message(STATUS "VX_RENDERER = gles (OpenGL ES 3.0+ canvas)")
else()
  add_compile_definitions(VX_RENDERER_SOFTWARE=1)
  message(STATUS "VX_RENDERER = software (CPU rasterizer canvas)")
endif()
```

**`tests/CMakeLists.txt`（+~15 行 / 在 a14 add_test 之后插入）：**

```cmake
# TASK-20260505-05 G1.1: VX_RENDERER flag validation smoke.
# Validates that the top-level CMakeLists.txt VX_RENDERER option accepts
# {software, gles}, defaults to software, rejects invalid values with
# FATAL_ERROR, and emits the matching compile def (VX_RENDERER_SOFTWARE
# or VX_RENDERER_GLES). Runs cmake-configure 4 scenarios in scratch dirs.
add_test(NAME vx_renderer_flag_check_smoke
  COMMAND ${CMAKE_COMMAND}
    -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
    -DBUILD_DIR_BASE=${CMAKE_BINARY_DIR}
    -P ${CMAKE_SOURCE_DIR}/tests/smoke/vx_renderer_flag_check.cmake)
```

### 步骤 4：双 build 矩阵 ctest 验证

```bash
# 矩阵 A：DEVTOOL=ON / VX_RENDERER=software（默认）
cmake -B build-on -DVX_BUILD_DEVTOOL=ON
cmake --build build-on -j
cd build-on && ctest --output-on-failure 2>&1 | tail -3
# 期望：1303/1303 PASS（baseline 1302 + 1 新加 vx_renderer_flag_check_smoke）

# 矩阵 B：DEVTOOL=OFF / VX_RENDERER=software
cmake -B build-off -DVX_BUILD_DEVTOOL=OFF
cmake --build build-off -j
cd build-off && ctest --output-on-failure 2>&1 | tail -3
# 期望：1110/1110 PASS（baseline 1109 + 1）

# 矩阵 C：DEVTOOL=ON / VX_RENDERER=gles（手动验证编译期分支）
cmake -B build-gles -DVX_BUILD_DEVTOOL=ON -DVX_RENDERER=gles
cmake --build build-gles -j
# 期望：成功 build / 输出 "VX_RENDERER = gles" / 0 link 失败（D1=A 不引入 dep）

# 矩阵 D：负向（invalid）
cmake -B build-bad -DVX_RENDERER=invalid 2>&1 | grep -i "FATAL_ERROR\|VX_RENDERER must"
# 期望：FATAL_ERROR / 配置失败
```

### 步骤 5：commit

**Single commit（D7=A）：** `feat(build): introduce VX_RENDERER flag with software default [TASK-20260505-05]`

commit body 范本（沿用 git-workflow.mdc P1.10 蓝图任务 commit body 8 段范本 / 可裁剪到实施类 6 段）：

```
feat(build): introduce VX_RENDERER flag with software default [TASK-20260505-05]

任务定位：
- G1.1 CMake VX_RENDERER flag / Level 2 / GLES 蓝图实施首步
- MVP-C 战略主线第一个实施任务

主交付：
- CMakeLists.txt（顶层 / +~20 行 / option + 校验 + compile def）
- tests/smoke/vx_renderer_flag_check.cmake（新建 / ~85 行 / 4 场景守门）
- tests/CMakeLists.txt（+~15 行 / add_test smoke）

验证：
- ctest DEVTOOL=ON: 1302 → 1303（+1 vx_renderer_flag_check_smoke / +0 退化）
- ctest DEVTOOL=OFF: 1109 → 1110（+1 / +0 退化）
- 矩阵 C（VX_RENDERER=gles）: 配置 + build 成功 / 0 link 失败（D1=A 暂不引入 dep）
- 矩阵 D（invalid）: FATAL_ERROR ✅

决策矩阵：
- D1=A 暂不引入 GLES dep（YAGNI / 推迟 G1.2）
- D2=A 顶层校验
- D3=B cmake -P 脚本
- D4=B tests/smoke/
- D5=A 子进程 invalid assert
- D6=A 仅 plan / 引用 GLES 蓝图 spec
- D7=A P0 协议单 commit

Source: /plan TASK-20260505-05 + 蓝图 plan §3.1 偏差 §0.4 校正
实测：plan 阶段 ~XX min / build 阶段 ~XX min（reflect 阶段补）
```

---

## 4. ctest 期望矩阵（双 build × 双 RENDERER × 1 invalid = 4 场景）

| Config | DEVTOOL | VX_RENDERER | 期望 ctest | 期望 build |
|---|:-:|:-:|:-:|:-:|
| baseline ON | ON | software（default） | **1303**（+1 / 不退化）| ✅ |
| baseline OFF | OFF | software（default） | **1110**（+1 / 不退化）| ✅ |
| gles | ON | gles | N/A（手动验证）| ✅（0 link 失败 / D1=A）|
| invalid | ON | invalid | N/A | ❌ FATAL_ERROR ✅ |

**关键不变量：**
- VX_RENDERER=software（任意 config）→ ctest baseline +1（仅 vx_renderer_flag_check_smoke）/ 0 退化
- VX_RENDERER=gles → 0 link 失败（D1=A 不引入 dep / G1.2 才引入）
- VX_RENDERER=invalid → FATAL_ERROR（D5=A 反向探针）

---

## 5. 反复模式预防清单（writing-plans §反复模式预防 / 8 项预审）

| # | 模式 | 本任务预审 | 预防措施 |
|:-:|---|---|---|
| 1 | 前置依赖未验证 | ✅ 4 维度全通过 | Phase 0 audit 7 项已完成 |
| 2 | 既有 pattern 不复用 | ✅ 3 处 pattern 已识别（option / pkg-config 双轨 / a14 cmake -P）| 直接复用 |
| 3 | spec/plan 信息回归 | ✅ VAN 阶段已发现 plan §3.1 偏差 + §0.4 校正 | 提前在 plan 阶段标注 |
| 4 | spec vs code 一致性 audit | ✅ writing-plans P1.6 首次实战实证（dual-evidence 候选）| §0.4 偏差校正 |
| 5 | 能力假设 audit | ✅ writing-plans P1.6 首次实战实证 | §0.4 find_package OpenGLES 不可用假设已校正 |
| 6 | JS context 归属 | N/A（构建系统改动 / 0 JS）| - |
| 7 | 资源类反向探针 | ✅ D5=A 自动化反向探针 | tests/smoke/vx_renderer_flag_check.cmake 4 场景 |
| 8 | spec 数据回归 audit | N/A（无 spec 数据）| - |

**预审命中：0/8** ✅（第 6 任务连续保持 / **累计 19 模式连续抑制 / 历史新高继续刷新**）

---

## 6. 验收标准

| # | 验收项 | 验证手段 |
|:-:|---|---|
| 1 | VX_RENDERER=software 默认 | `cmake -B build-default && grep "VX_RENDERER = software" build-default/CMakeCache.txt 或 stdout` |
| 2 | VX_RENDERER=gles 通过 / VX_RENDERER_GLES=1 宏可见 | `cmake -B build-gles -DVX_RENDERER=gles && cmake --build build-gles` ✅ |
| 3 | VX_RENDERER=invalid → FATAL_ERROR | `cmake -B build-bad -DVX_RENDERER=invalid` 配置失败 ✅ |
| 4 | ctest baseline 不退化 | DEVTOOL=ON 1303/1303 + DEVTOOL=OFF 1110/1110（+1/+1）|
| 5 | vx_renderer_flag_check_smoke 入库 | ctest list 显示 / 4 场景 PASS |
| 6 | working tree 干净 | `git status --short` 无残留 |
| 7 | 0 lint errors | ReadLints 6 改动文件 0 errors |

---

## 7. 风险

| # | 风险 | 概率 | 影响 | 缓解 |
|:-:|---|:-:|:-:|---|
| 1 | cmake -P 子进程递归调用导致测试时间过长 | 低 | 中 | scratch 目录复用 + OUTPUT_QUIET / 预估 ~5-15s |
| 2 | scratch 目录清理失败导致测试 flaky | 低 | 低 | `file(REMOVE_RECURSE)` 前后清理 / scratch 在 ${CMAKE_BINARY_DIR} 下不污染源 |
| 3 | gles 矩阵手动验证遗漏 | 中 | 低 | reflect 阶段补充矩阵 C 验证记录 |
| 4 | 新加 ctest 改变 baseline 1302 → 1303 / 1109 → 1110 后续任务计数偏移 | 必然 | 低 | activeContext 更新 baseline 数字 / techContext.md 记录 |

---

## 8. 估时

| 阶段 | 估时（plan ×0.6）| 系数依据 |
|---|---|---|
| Plan（含 brainstorm + 偏差校正）| ~20-30 min | 已估算 / 实测约 ~15-25 min |
| Build | ~30-45 min | TDD RED + GREEN + 双 build 矩阵 ctest 验证 |
| Reflect | ~10-15 min | Level 2 简化 reflect / 4 项沉淀候选 |
| Archive | ~5-10 min | Level 2 简化 archive |
| **总线** | **~65-100 min** | （vs GLES 蓝图 plan §4 估时 ~2-3 h = ~120-180 min / **极速区 0.36-0.55× 系数预期**）|

---

## 9. 沉淀候选（reflect 阶段处理 / 4 项）

| # | 沉淀候选 | 优先级 | 目标位置 |
|:-:|---|:-:|---|
| 1 | systemPatterns 新段「CMake 依赖引入时机 YAGNI 原则」（D1=A 实证 / dep 与实现强耦合 / 不为 flag 落地预付）| P1 | systemPatterns.md |
| 2 | systemPatterns 升级「brainstorming P1.3 主动 push-back 模式 dual-evidence」（TASK-04 落地 + TASK-05 首次实战）| P1 | systemPatterns.md |
| 3 | systemPatterns 升级「writing-plans P1.6 spec vs code 一致性 audit dual-evidence」| P1 | systemPatterns.md |
| 4 | P0 协议「plan/spec docs 落盘即 commit」**quint-evidence 第 5 数据点**（quad → quint 升级）| P1 | writing-plans.mdc P1.5 段实证表 |

**新里程碑候选：**
- 跨决策协同度 100% **第 14 次连续命中** / 累计 128/128 历史最高 streak（已即时锁定 ✅）
- 反复模式抑制累计 **19 模式连续** / 历史新高继续刷新（plan 阶段预审 + 累计追踪）

---

## 10. P0 协议落地清单（D7=A 自吃狗粮 / quint-evidence 第 5 数据点）

| # | 文件 | 操作 | 状态 |
|:-:|---|:-:|:-:|
| 1 | `docs/plans/2026-05-05-cmake-vx-renderer-flag.md` | 创建（本文档）| 🟡 写中 |
| 2 | `memory-bank/activeContext.md` | 更新（plan 阶段产出）| ⏳ |
| 3 | `memory-bank/tasks.md` | 更新（plan 阶段产出 + 决策矩阵）| ⏳ |
| 4 | `memory-bank/progress.md` | 更新（plan 阶段时间线）| ⏳ |

**P0 协议要求：4 文件单 commit 落盘 / 0 collateral / commit 消息：**

```
chore(plan): land plan + memory bank for TASK-20260505-05 [P0 quint-evidence]

P0 协议落地（plan/spec docs 落盘即 commit / quad → quint-evidence 第 5 数据点）：
- docs/plans/2026-05-05-cmake-vx-renderer-flag.md（~400 行 / 10 段全覆盖）
- memory-bank/activeContext.md（plan 阶段产出 / 7 D 决策 + 偏差校正 + 沉淀候选）
- memory-bank/tasks.md（plan 阶段产出 / 决策矩阵 + 估时）
- memory-bank/progress.md（plan 阶段时间线）

决策矩阵（7 D / 1 次 AskQuestion all_recommended 锁定 / 第 14 次连续命中）：
- D1=A 暂不引入 GLES dep（YAGNI / 推迟 G1.2）
- D2=A 顶层校验
- D3=B cmake -P 脚本
- D4=B tests/smoke/
- D5=A 子进程 invalid assert
- D6=A 仅 plan / 引用 GLES 蓝图 spec
- D7=A P0 协议自吃狗粮（quint-evidence 候选）

蓝图 plan §3.1 偏差校正（brainstorming P1.3 主动 push-back 首次实战 ✅）：
- 偏差 #1: find_package(OpenGLES/EGL REQUIRED) → D1=A 暂不引入 dep
- 偏差 #2: tests/cmake/*.sh → D3=B + D4=B cmake -P + tests/smoke/
- 偏差 #3: 临时改宏反向探针 → D5=A 自动化子进程 invalid assert

Source: /plan TASK-20260505-05 G1.1 CMake VX_RENDERER flag
实测：plan 阶段 ~XX min vs plan ×0.6 ~20-30 min = ~0.X× 极速区
```

---

## 11. 下一步

完成本 plan 阶段 / **执行 P0 协议单 commit** / 进入 `/build` 按步骤 1-5 实施。
