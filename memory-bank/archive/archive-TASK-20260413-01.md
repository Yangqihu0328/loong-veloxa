# 归档：QuickJS 脚本引擎集成

**日期：** 2026-04-13  
**任务 ID：** TASK-20260413-01  
**复杂度级别：** Level 4  
**状态：** ✅ 已完成  

## 任务概述

通过 **quickjs-ng**（CMake 一等公民）将 QuickJS 引入 Veloxa：新增 **`vx_script`** 静态库与 **`vx::script::QuickjsEngine`**，提供全局 `Eval` 能力、基础资源边界（内存上限、源码长度上限），**不**接入 `Application` / C API，为后续 DOM/事件绑定留白。

## 技术方案

- **上游：** `FetchContent` 固定 **`quickjs-ng v0.14.0`**，目标 **`qjs`** + **`qjs-libc`**；关闭 `QJS_ENABLE_INSTALL`、`QJS_BUILD_EXAMPLES`。  
- **宿主封装：** `Init` / `Shutdown` / `EvalGlobal`；`Status` / `StatusOr<std::string>` 与 Foundation 一致。  
- **创意对齐（`creative-quickjs-host.md`）：** `JS_SetMemoryLimit` **32MiB**/Runtime；**不**调用 `js_std_add_helpers` / `js_std_init_handlers`；首版**不**注册 `JS_SetInterruptHandler`（Phase 2）。  
- **根 CMake：** 全局 `-Wall -Wextra -Wpedantic -Werror` 改为 **`$<COMPILE_LANGUAGE:CXX>`**，避免 FetchContent 的 C 源码触发 `-Werror=pedantic`。

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `veloxa/script/CMakeLists.txt` | FetchContent + `vx_script`、链接 `qjs` `qjs-libc` |
| 创建 | `veloxa/script/quickjs_engine.h` | `QuickjsEngine` API |
| 创建 | `veloxa/script/quickjs_engine.cc` | 生命周期、`EvalGlobal`、256KiB 源上限、异常 `FreeValue`→`GetException` |
| 创建 | `tests/script/quickjs_engine_test.cc` | 4 用例：算术、语法错误、`throw`、超长源拒绝 |
| 修改 | `CMakeLists.txt` | `add_subdirectory(veloxa/script)`；CXX-only 严格告警 |
| 修改 | `tests/CMakeLists.txt` | 注册 `quickjs_engine_test` |
| 修改 | `docs/plans/2026-04-13-quickjs-integration.md` | 创意对齐首合入要求（在 main 工作流提交中已存在） |
| 修改 | `memory-bank/techContext.md` / `systemPatterns.md` | 脚本落地说明、FetchContent+C 编译模式 |

### 关键决策

1. **quickjs-ng 优于 bellard 手写 CMake** — 可复现、维护成本低。  
2. **异常路径以上游 `api-test.c` 为准** — `JS_IsException` 时先 `JS_FreeValue` 再 `JS_GetException` 再 stringify。  
3. **`vx_script` 不链入 `vx_core`** — 控制默认链接面，脚本由宿主按需链接。

### 安全决策

- **输入：** 源码 **256KiB** 上限在进入 `JS_Eval` 前校验（`kInvalidArgument`）。  
- **资源：** **`JS_SetMemoryLimit`** 限制 QuickJS 堆；**未**启用 `js_std_*`，缩小 OS/`import` 攻击面。  
- **执行预算：** 首版无 `JS_SetInterruptHandler`；对不可信脚本须在集成层限制调用（见回顾「安全评估」）。  
- **依赖审计：** 固定 tag；无自动化 CVE 扫描，建议周期性人工对照（记入 `techContext` 技术债）。

## 测试覆盖

- **单元 / 安全：** `quickjs_engine_test` — `EvalArithmetic`、`EvalSyntaxError`、`EvalThrownError`、`RejectsOversizedSource`。  
- **回归：** 全量 **`ctest`** — **796 passed, 0 failed**（合并后基线）。

## 经验教训

- FetchContent + 根全局 **`-Werror`** 易污染 **C** 子工程 → 计划阶段应 checklist **语言限定**。  
- WSL **DNS** 失败时需 **代理** 或离线 bundle 文档化。  
- QuickJS **异常**勿直接 stringify `JS_Eval` 返回值而不按上游顺序取异常。

## 架构与长期维护

- **`veloxa/script/`** 与 `systemPatterns.md` 规划目录一致；后续 DOM 绑定应独立 TASK，并复审信任边界。  
- **技术债：** `techContext` #44 — interrupt 预算、`JSMallocFunctions` 与 Foundation 对齐。

## 参考文档

- 设计规格：`docs/specs/2026-04-13-quickjs-integration-design.md`  
- 实现计划：`docs/plans/2026-04-13-quickjs-integration.md`  
- 创意设计：`memory-bank/creative/creative-quickjs-host.md`  
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260413-01.md`  
