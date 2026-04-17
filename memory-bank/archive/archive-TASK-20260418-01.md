# 归档：消化关键技术债务

**日期：** 2026-04-18
**任务 ID：** TASK-20260418-01
**复杂度级别：** Level 3（中等功能 / 跨 2 子系统）
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260418-01-tech-debt`

## 任务概述

针对 TASK-20260414-01 归档遗留的 4 条 P1 技术债一次性修复，保证对外 API 行为不变：

1. **#45** — `dom_bindings.cc` 全局状态（`g_bound_doc`、`g_tracked_callbacks`、`g_*_class_id`）迁移，使 `DomBindings` 可安全支持多实例 / 多 Document
2. **#46** — `StyleGetProp` getter 实装读路径，取代始终返回 `""` 的存根
3. **#48** — `SoftwareCanvas::DrawText` 缓存 HarfBuzz `hb_font_t`，消除每帧 `hb_ft_font_create_referenced`/`hb_font_destroy` 开销
4. **#50** — 修复 `addEventListener` 回调生命周期 UAF（`Unbind` 先释放 `JSValue callback` 再销毁 C++ lambda）

涉及子系统：`veloxa/script`（`dom_bindings`）、`veloxa/text`（`font_manager`）、`veloxa/graphics/software`（`software_canvas`）。

## 技术方案

### 设计决策（头脑风暴阶段全部定案，无需 `/creative`）

| 债务 | 方案 | 选择 | 理由 |
|------|------|------|------|
| #45 | A1 pimpl + 幂等注册 | ✅ | `InstanceData` 容纳全部实例状态；`JSClassID` 作为 QuickJS runtime 级常量保留为文件级静态，配 `JS_IsRegisteredClass` 幂等注册；header 零 `quickjs.h` 依赖 |
| #46 | B1 按类型分发序列化 | ✅ | `SerializeCssValue` 单次 switch 覆盖 length/color/number/auto/inherit/initial 6 种类型；Enum（display 等）暂返 `""` 作为已知残留 |
| #48 | C1 缓存嵌入 FontEntry | ✅ | 避免新增 `HbFontCache` 单独类；每 `FontHandle` 一个 `hb_font_t`；size 变化用 `hb_ft_font_changed` 就地重配置，指针稳定 |
| #50 | D1 listener_elements + 顺序控制 | ✅ | `InstanceData::listener_elements` 去重记录绑定元素；`Unbind` 严格 **先** `em->RemoveEventListeners(el)` **再** `FreeAll callbacks` |

### 架构特点

- **pimpl + `JS_SetContextOpaque` 桥接**：`DomBindings::InstanceData` 前向声明 `public:`、定义 `private:`；`~DomBindings()` non-default 避免不完整类型 delete；`friend struct DomBindingsInternal` 提供 `.cc` 内零-public-API 访问
- **`JSClassID` 层次化**：进程/runtime 常量（文件级 static）vs 实例状态（`InstanceData`）严格分离
- **lambda-JSValue 同寿命**：C++ lambda 捕获 `JSValue` 时，lambda 拷贝销毁必须先于 `JS_FreeValue`
- **CMake `PUBLIC` 传播**：`vx_text` 中 HarfBuzz/FreeType 从 `PRIVATE` 升 `PUBLIC`，让下游测试目标直接继承 include/link，不在 tests/CMakeLists.txt 重复 `pkg_check_modules`

## 实现摘要

**变更规模：** 13 文件，+2458/-88 行，7 个提交（含 `/plan`、`/build` × 5、`/reflect`），856/856 测试通过（较基线 +14 / 0 回归）。

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| **规划产物（6b7e68c）** | | |
| 创建 | `docs/specs/2026-04-18-tech-debt-dom-bindings-design.md` | 4 条债务方案 A1/B1/C1/D1 的设计规格 |
| 创建 | `docs/plans/2026-04-18-tech-debt-dom-bindings.md` | TDD 粒度实现计划（6 Phase） |
| **Phase 1 #45（255c74d）** | | |
| 修改 | `veloxa/script/dom_bindings.h` | pimpl 化：`std::unique_ptr<InstanceData>` 替换所有字段；`~DomBindings()` non-default；`struct InstanceData;` 前向声明放 `public:`，`friend struct DomBindingsInternal;` |
| 修改 | `veloxa/script/dom_bindings.cc` | 定义 `DomBindings::InstanceData`；`g_bound_doc`/`g_tracked_callbacks` 迁移进来；`s_element_class_id`/`s_style_class_id` 文件级 static + `JS_IsRegisteredClass` 幂等注册；所有回调通过 `JS_GetContextOpaque` 取 `InstanceData*` |
| 修改 | `tests/script/dom_bindings_test.cc` | +2 测试（`JSClassIdStableAcrossBindings`、`MultipleInstancesIndependentDocuments`） |
| **Phase 2 #50（d105c36）** | | |
| 修改 | `veloxa/script/dom_bindings.cc` | `Unbind` 严格顺序：遍历 `listener_elements` 调 `em->RemoveEventListeners(el)` → `tracked_callbacks.FreeAll()` → 清 context opaque；`ElementRemoveEventListener` 同步从 `listener_elements` 移除 |
| 修改 | `tests/script/dom_bindings_test.cc` | +2 测试（`UnbindClearsListenersFromEventManager`、`UnbindThenBoundReflectsState`） |
| **Phase 3 #46（081896a）** | | |
| 修改 | `veloxa/script/dom_bindings.cc` | 新增匿名 namespace `UnitSuffix` / `AppendCStr` / `AppendNumber` / `SerializeCssValue`；`StyleGetProp` 读 `inline_declarations()` → `SerializeCssValue` → `JS_NewStringLen`；`#include <cstdio>` |
| 修改 | `tests/script/dom_bindings_test.cc` | +7 测试（`StyleGetBackgroundColor`/`StyleGetWidthPx`/`StyleGetAuto`/`StyleGetPercent`/`StyleGetOpacity`/`StyleGetUnsetReturnsEmpty`/`StyleGetDisplayEnumCurrentlyEmpty`） |
| **Phase 4 #48（d73d303）** | | |
| 修改 | `veloxa/text/font_manager.h` | 前向声明 `struct hb_font_t;`；新增 `hb_font_t* GetHbFont(FontHandle, u32 pixel_size);`；`FontEntry` 增 `hb_font` + `hb_pixel_size` 字段 |
| 修改 | `veloxa/text/font_manager.cc` | `#include <hb.h>`/`<hb-ft.h>`；`GetHbFont` 实现（首次 `hb_ft_font_create_referenced`；size 变化 `hb_ft_font_changed`；同 size 直接返回缓存）；`Shutdown` 先 `hb_font_destroy` 后 `FT_Done_Face` |
| 修改 | `veloxa/graphics/software/software_canvas.cc` | `DrawText` 改 `font_manager_->GetHbFont(font, pixel_size)`；nullptr 走 fallback；不再 `hb_font_destroy`（归 FontManager 持有） |
| 修改 | `veloxa/text/CMakeLists.txt` | `Freetype::Freetype` / `${HARFBUZZ_LIBRARIES}` / `${HARFBUZZ_INCLUDE_DIRS}` 从 `PRIVATE` 升 `PUBLIC`，让下游测试继承 |
| 修改 | `tests/text/font_manager_test.cc` | +3 测试（`GetHbFontReusesPerHandle`/`GetHbFontHandlesSizeChange`/`GetHbFontInvalidHandle`）；`#include <ft2build.h>` + `FT_FREETYPE_H` |
| **Phase 5 文档（ea1a95b、1b27e98）** | | |
| 修改 | `memory-bank/{tasks,activeContext,progress,techContext,systemPatterns}.md` | 阶段切换；技术债 #45/#46/#48/#50 标记已解决；补充 DomBindings pimpl + HarfBuzz/CMake 经验段 |
| 创建 | `memory-bank/reflection/reflection-TASK-20260418-01.md` | 反思文档 |
| 修改 | `.cursor/rules/skills/writing-plans.mdc` | 新增「CMake 链接方向约束分析」段（P1 闭环） |

### 关键决策

1. **`JSClassID` 不是实例状态**——必须作为进程/runtime 级常量处理（文件级 static），幂等注册模板 `JS_NewClassID` + `JS_IsRegisteredClass` + `JS_NewClass`
2. **`InstanceData` 前向声明放 public，定义放 private**——让匿名 namespace 内的 C 回调能命名类型，但不破坏 pimpl 封装（`data_` 仍 private，外部无法访问）
3. **`Unbind` 顺序硬约束**（核心修复）——先销毁 C++ lambda 拷贝，再 `JS_FreeValue(callback)`；反序会触发 UAF
4. **`hb_ft_font_changed` 而非销毁重建**——指针稳定利于缓存；仅重读 FT_Face 已变更的 metric
5. **CMake 依赖升 `PUBLIC` 而非下游补链**——下游若会 include 或间接 symbol 依赖上游的第三方依赖，必须在提供方 `PUBLIC`；禁止在多个 `CMakeLists.txt` 重复 `pkg_check_modules(FOO …)`

### 安全决策

**本任务不涉及外部接口 / 认证 / 数据保护变更。** 属纯内部重构 + 性能优化 + 内存安全修复。特别地：

- **#50 UAF 修复是内存安全改进**（销毁顺序约束）
- **#45 pimpl** 消除了全局 mutable state 带来的 TOCTOU 面——多 `DomBindings` 实例独立

## 测试覆盖

| 模块 | 新增测试数 | 类型 | 状态 |
|------|-----------|------|------|
| `dom_bindings_test` | 11 | 单元 + 集成（`QuickjsEngine` 真实启动） | PASS |
| `font_manager_test` | 3 | 单元（真实 FT_Face + hb_font_t 缓存行为） | PASS |
| **合计** | **14** | — | **856/856 全量通过（0 回归）** |

### TDD 循环

- Phase 1：`MultipleInstancesIndependentDocuments` 真 RED（旧 `g_bound_doc` 被 b2 覆盖）→ pimpl 后 GREEN
- Phase 2：白盒 + 行为等价（`EventManager` 无 public listener 计数 API，完整 UAF 复现需 `LayoutBox*`；commit 注明此限制，代码审查为主要保障）
- Phase 3：5 个 `StyleGet*` 真 RED（旧 stub 返 `""`）→ `SerializeCssValue` 后 GREEN；Enum 测试确认 `""` 为已知残留
- Phase 4：编译失败型 RED（`GetHbFont` 未声明）→ 实现后 GREEN

## 经验教训

从 `reflection-TASK-20260418-01.md` 提取核心：

### 技术

1. **pimpl 前向声明位置决定可用范围**——`public:` 允许 `.cc` 内自由函数命名类型
2. **`JS_SetContextOpaque` 是 QuickJS 桥接 C++ 实例的规范通道**——避免全局 map / thread-local
3. **C++ lambda 捕获与 `JSValue` 必须同寿命**——释放顺序：先销毁 lambda 拷贝，后释放被捕获资源
4. **`hb_ft_font_changed` 替代销毁重建**——缓存指针稳定、响应 size 变化
5. **CMake 静态库 `PRIVATE` 依赖不传播**——下游要用时升 `PUBLIC`

### 流程

6. **每 Phase 独立提交 + 每 Phase 先跑回归** 是 Level 3 任务最佳节奏
7. **前置测试有意设计为"暴露旧 bug"** 产生最强的 RED→GREEN 证据链
8. **CMake 链接方向应前置分析**（已落实到 `writing-plans.mdc`）

### 反复模式闭环

触发「前置依赖/环境/API 能力未验证」第 9+ 次（CMake 变体）。建议#1 **已强制落地**到 `.cursor/rules/skills/writing-plans.mdc`「CMake 链接方向约束分析」段。

## 残留技术债（后续任务）

- **P2**：`StyleGetProp` Enum 读路径（display 等）——需要 `PropertyId → enum string` 反查表
- **P2**：`DomBindings`/`EventManager` 双向析构安全——当前仅保证 `DomBindings` 先析构；反向需弱引用机制
- **（原 #47）**：`removeEventListener` 按 `(type, handler)` 精确移除——需 `EventManager` 扩展 API

## 参考文档

- 设计规格：`docs/specs/2026-04-18-tech-debt-dom-bindings-design.md`
- 实现计划：`docs/plans/2026-04-18-tech-debt-dom-bindings.md`
- 反思文档：`memory-bank/reflection/reflection-TASK-20260418-01.md`
- 上游归档（债务来源）：`memory-bank/archive/archive-TASK-20260414-01.md`

## 提交历史

```
6b7e68c docs(plan): design + plan for TASK-20260418-01 tech debt cleanup
255c74d refactor(script): migrate DomBindings globals to pimpl InstanceData (#45)
d105c36 fix(script): call RemoveEventListeners before freeing JS callbacks (#50)
081896a feat(script): implement StyleGetProp read path (#46)
d73d303 perf(text): cache hb_font_t per font handle (#48)
ea1a95b docs(memory-bank): mark TASK-20260418-01 build phase complete
1b27e98 docs(reflect): add reflection for TASK-20260418-01
```
