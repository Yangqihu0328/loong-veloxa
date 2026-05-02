# 归档：DevTool Phase A · Inspector 实施

**日期：** 2026-05-02
**任务 ID：** TASK-20260502-01
**复杂度级别：** Level 4（plan escalation 自 Level 3 升级；16 子任务跨 5 子系统 / 4 安全威胁 / 8 轮 build 多 Phase 结构）
**安全相关：** ✅ 是（T2 路径穿越 / T3 敏感数据 redact / T5 overlay 隔离 / T7 buffer overflow 4 个威胁面）
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260502-01-devtool-inspector`（基于 main `679304e`，归档时由用户决策合并方式）
**任务时长：** ~338 min（VAN ~10 min + Plan ~22 min + Build ~281 min + Reflect ~25 min；不含 archive 自身耗时）

---

## 1. 任务概述

DevTool 三件套蓝图主交付（TASK-20260430-04）的 **subtask_a 主线 Phase A · Inspector 实施版** — Veloxa 引擎首个 **Level 4 实施类大件任务**（区别于蓝图任务 V2=a），16 子任务跨 4 Phase 8 轮 build 完成，最终 ctest 从 main baseline `1062/1062 PASS` 演进到 **1169/1169 ON PASS（+107 测）/ 1065/1065 OFF PASS（+3 测）**，A14 链接闭包零自动化守门到位。

| 维度 | V1 选择 | V2 选择 | V3 选择 | V4 选择 | V5 安全 |
|---|---|---|---|---|---|
| 子系统范围 | **Phase A Inspector 16 子任务**（A.0 前置改造 6 + A.1 DevTool UI 4 → escalation 后 8 + A.2 安全单测 4 + A.3 example/reflect 2）| — | — | — | — |
| 实施模式 | — | **b 完整实施**（含 build / ctest / commit / merge）| — | — | — |
| 复杂度 | — | — | — | ~~Level 3~~ → **Level 4**（plan escalation A 升级）| — |
| 创意需求 | — | — | ❌ 否（creative #1 splitter dock 已锁定）| — | — |
| 安全标注 | — | — | — | — | ✅ 是（T2/T3/T5/T7 全程守门）|

**意图判读**：用户用「**实施**」二字（非「规划」）→ 主交付是端到端可运行的 DevTool Inspector，从 C API 到 dogfood UI 到 Hello example smoke 全链路落地，并保证 A14「DevTool 关闭时构建产物零变化」spec 验收。

---

## 2. 技术方案

### 2.1 工作流路径（Level 4 实施类多轮次 Build 中间态）

```
/van → /plan → /build × 8 轮（中途 escalation 1 次） → /reflect → /archive
                ↑ 8 轮均按子任务分组提交 + 5 次 round-pause commits
                ↑ 轮次 3 中途触发 plan escalation（A.1 4 → 8 子任务 + 384 行 detail plan）
```

| 阶段 | plan 估时 | 实测 | × plan ×0.6 | 备注 |
|---|---|---|---|---|
| **VAN** | 30 min / 18 min ×0.6 | ~10 min | **0.56×** | grep 实证 R1 callsite 4 处（远低于 spec 估 10-15）+ 6/6 前置验证 PASS |
| **Plan** | 60 min / 36 min ×0.6 | ~22 min | **0.61×** | B1-B8 一次 AskQuestion 选 all_recommended + Phase 0 11 子段实测填写 |
| **Plan escalation**（轮次 3 中途）| — | ~30 min | — | brainstorming B-A1.1=b/B-A1.2=a + 架构修正 M1/M2/M3 + plan +384 行 |
| **Build × 8 轮**（含 escalation 后 5 轮）| 441 min ×0.6 | ~281 min | **0.64×** | 「大件实现」桶下沿外，触发桶系数下调 |
| **Reflect** | 60 min / 36 min ×0.6 | ~25 min | **0.69×** | 13 段全维度 + 架构评估 + 长期影响分析 |
| **Archive**（本阶段）| 45 min / 27 min ×0.6 | TBD | TBD | 待回填 |
| **主线合计** | — / 567 min ×0.6 | **~368 min（不含 archive）** | **0.65×** | 与本任务 build 阶段 0.64× 总系数一致 |

**plan ×0.6 第 18-37 数据点入库（20 个新数据点群组）**：详见 reflection §1.2 + systemPatterns 「大件实现」桶系数下调 + 3 子桶（极细致 plan / 批量小子任务 / escalation 后子任务）段。

### 2.2 16 子任务实施全景（Phase A.0/A.1/A.2/A.3）

| 子任务 | 名称 | 实测 | plan ×0.6 | 比值 | 关键产出 |
|:-:|---|---:|---:|:-:|---|
| **Phase A.0 — 前置改造（6 子任务，~95 min）**  |  |  |  |  |  |
| A.0.1 | I1 Application 双 Document 槽 + callsite 重命名 | ~10 min | 54 min | **0.18×** | target_document_ + devtool_document_ 双槽 + 7 callsite 改名（R1 漏改 0 处）|
| A.0.2 | I4 LayoutBox::ToJson() | ~10 min | 18 min | 0.56× | LayoutBox::ToJson + 4 单测，**闭环技术债 #26** |
| A.0.3 | I5 DOM Serializer::ToJson + T3 redact | ~15 min | 27 min | 0.56× | RedactionPolicy enum + EscapeJsonString + IsSensitiveAttribute（T3 落地）|
| A.0.4 | I3 PaintCommand kOverlayHighlight + T5 | ~15 min | 18 min | 0.83× | kOverlayHighlight enum + ResetOverlayCommands 协议（T5 落地）|
| A.0.5 | C API 内部 C++ inspector_data.h | ~25 min | 36 min | 0.69× | vx_devtool 静态库 + SerializeDocument/LayoutBox/ComputedStyle |
| A.0.6 | C API 公开 vx_view_serialize_dom_json + T7 | ~20 min | 36 min | 0.56× | T7 双调用 + max_size 守卫，**闭环技术债 #40** |
| **Phase A.1 — dogfood UI（escalation 后 8 子任务，~143 min）** |  |  |  |  |  |
| A.1.1 | InspectorOverlay::InjectHoverHighlight | ~10 min | 18 min | 0.56× | M2 修正签名（DisplayList& 而非 Document*）|
| A.1.2 | DevTool resource 编译期嵌入（B-A1.1=b）| ~15 min | 27 min | 0.56× | Python codegen + CMake `add_custom_command` + vx_devtool_resources（**T2 路径穿越威胁面消除**）|
| A.1.3 | inspector_panel.{html,css,js} 编写 | ~25 min | 54 min | 0.46× | splitter dock + 3 tabs + R2-verified CSS + DOM tree 渲染 JS |
| A.1.4 | JS native binding `vx_devtool_get_dom_json` | ~22 min | 36 min | 0.61× | DomBindings::SetDevtoolTargetDocument + RegisterDevtoolBindings |
| A.1.5 | InputDispatchSplitter（hit-area 路由 + drag capture）| ~12 min | 27 min | 0.44× | 纯算法状态机 + 7 单测 + R1 mitigation 守卫 |
| A.1.6 | Application 双 UpdateManager + LoadDevtoolDocument（M1）| ~22 min | 36 min | 0.61× | owned_devtool_document_ unique_ptr + canvas Translate + placeholder 替换 |
| A.1.7 | vx_view_attach_devtool C API + F12 hotkey | ~14 min | 27 min | 0.52× | VxDevtoolOptions + VX_KEY_F12 + Application 内化 hotkey 拦截 |
| A.1.8 | dogfood headless smoke + JS execution wiring | ~28 min | 45 min | 0.62× | EvalDevtoolScript + R2 panel-side defensive + **3 R2 P3 候选产出** |
| **Phase A.2 + A.3 — 安全测 + example（6 子任务，~43 min）** |  |  |  |  |  |
| A.2.1 | T3 redaction policy + C API（vx_inspector_set_redaction_policy）| ~13 min | 18 min | 0.72× | VxRedactionPolicy enum + Application + DomBindings 三处统一安全策略 |
| A.2.2 | T5 多帧 overlay 隔离集成测 | ~6 min | 18 min | 0.33× | MultiFrameInjectStaysIsolatedAfterReset + 负控制测 |
| A.2.3 | T7 buffer overflow 4 边界测 | ~7 min | 18 min | 0.39× | Zero / UINT32_MAX / AtExactNeeded / OneBelowNeeded（plan 4GB 拒绝错误修正）|
| A.2.4 | A14 链接闭包 ctest smoke 自动化 | ~8 min | 9 min | **0.89×** | pure CMake script + nm 8 符号黑名单（每次 ctest 跑）|
| A.3.1 | hello_devtool example + headless smoke | ~7 min | 27 min | 0.26× | SDL2 真窗口 + DevTool attach + F12 toggle + 运行时 T3 验证 |
| A.3.2 | Phase A integration verify + reflect prep | ~2 min | 18 min | **0.11×** | 全 Phase A 16/16 ctest 全 PASS + progress.md 完成快照 |

**Phase A 总计：16 子任务 / ~281 min vs 441 min plan ×0.6 = 0.64×**（中位 0.56×，分布偏左极窄档为主）

### 2.3 17 commits 演进时间线

| Commit | 子任务/事件 | 含义 |
|---|---|---|
| `e43a5be` | docs(plan) | VAN + Plan 落盘 |
| `0e8e40c` | A.0.1 | 双 Document 槽位 |
| `4131700` | A.0.2 | LayoutBox::ToJson + #26 闭环 |
| `30d54ee` | A.0.3 | DOM Serializer::ToJson + T3 |
| `e98f9fa` | A.0.4 | PaintCommand kOverlayHighlight + T5 |
| `fefaa07` | round 1 pause | 轮次 1 暂停 marker |
| `3f3bd38` | A.0.5 | vx_devtool 静态库 + inspector_data |
| `c9dec00` | A.0.6 | vx_view_serialize_dom_json + T7 + #40 闭环 |
| `d9ec183` | round 2 pause | 轮次 2 暂停 marker |
| `c3cab4e` + `e2ff26a` | round 3 abort + plan escalation | A.1 4 → 8 子任务 + plan +384 行 |
| `91dbcea` | A.1.1 | InspectorOverlay::InjectHoverHighlight |
| `7c26352` | A.1.2 | 编译期嵌入 + T2 消除 |
| `5c76346` | round 3 pause | 轮次 3 暂停 marker |
| `cdb8605` | A.1.3 | inspector_panel.{html,css,js} + R2 mitigation |
| `42adc89` | A.1.4 | vx_devtool_get_dom_json JS native binding |
| `a411cc9` | round 4 pause | 轮次 4 暂停 marker |
| `aecfc08` | A.1.5 | InputDispatchSplitter |
| `990029e` | round 5 pause | 轮次 5 暂停 marker |
| `0b9a1a8` | A.1.6 | Application 双 UpdateManager + M1 修正 |
| `05c5a30` | round 6 pause | 轮次 6 暂停 marker |
| `9a14850` | A.1.7 | vx_view_attach_devtool C API + F12 |
| `4dd555a` | A.1.8 | dogfood smoke + JS exec wiring + R2 三连暴露 |
| `3b70ebf` | round 7 pause | 轮次 7 暂停 marker（Phase A.1 8/8 完成）|
| `bb729e9` | A.2.1 | T3 redaction policy + C API |
| `1023ddd` | A.2.2 + A.2.3 | T5 多帧隔离 + T7 边界 |
| `62c959d` | A.2.4 | A14 链接闭包 ctest smoke |
| `1ec6b80` | A.3.1 | hello_devtool example + smoke |
| `d8245c5` | A.3.2 | Phase A 16/16 finalize |
| `57d46b6` | reflect | docs(reflect) — Level 4 全维度回顾 |

**总计：** 28 commits（含 plan + 8 build round + 5 round-pause + 1 escalation + 1 finalize + 1 reflect）

### 2.4 11 处 plan-fact reconcile（架构修正 + API 假设错误 + brainstorming 推翻）

| # | plan 假设 | 实际修正 | 来源子任务 | 严重度 |
|:-:|---|---|:-:|:-:|
| 1 | **M1**: A.0.1 仅双 Document 槽 | 实质需 A.1.6 双 UpdateManager 配套 | A.0.1 / A.1.6 | 🟢 中 |
| 2 | **M2**: InjectHoverHighlight(target_doc, ...) | Document 不持 DisplayList → 改 DisplayList& 签名 | A.1.1 | 🟢 plan 锁定签名修正 |
| 3 | **M3**: B-A1.1=b 编译期嵌入路径 | brainstorming 推翻 B4=B（T2 威胁面消除）| A.1.2 | 🟢 brainstorming 阶段修正 |
| 4 | A.0.2 String API 大写 | 实际 lower-case + 无 `contains` | A.0.2 | 🟡 校准 ~3 min |
| 5 | A.0.5 ComputedStyle::SetProperty | 不存在 → 改「显式列 10 个常用属性」 | A.0.5 | 🟠 校准 ~5 min |
| 6 | A.1.4 RegisterDevtoolBindings(ctx, target_doc) | C 函数无 closure → 改 InstanceData 扩展 | A.1.4 | 🟢 修正后更对齐既有模式 |
| 7 | A.1.6 Canvas::Translate() 直接调 | 不存在 → 改 PushState + SetTransform + PopState | A.1.6 | 🟢 等价范式 |
| 8 | A.1.8 FindElementById/InnerHtmlLength API | 不存在 → 改「JS exec status + EvalDevtoolScript 重入」契约 | A.1.8 | 🟢 改后更严格 |
| 9 | A.1.8 inspector_panel.js 假设 DomBindings 完整 | R2 三连缺陷暴露 → panel-side defensive | A.1.8 | 🟠 R2 P3 候选 3 项 |
| 10 | A.2.3 4 GB max_size 上限拒绝 | 是错的 → UINT32_MAX 应允许 | A.2.3 | 🟢 plan 描述错误修正 |
| 11 | A.2.1 plan 仅 C-API 路径单测 | JS binding hardcode 死角 → 同步实施 SetRedactionPolicy 统一 | A.2.1 | 🟢 plan 隐含死角主动修正 |

### 2.5 关键决策

1. **plan escalation 中途触发（轮次 3）**: 识别 A.1 子系统级工作量（4 → 8 子任务，~120 → 240 min plan），用户选 D 返回 /plan，escalation 投入 ~30 min plan 时间换 5 轮 build 实测 0.99× 完美匹配（escalation 后 plan 估时质量显著优于初始 plan）
2. **B-A1.1=b 编译期嵌入推翻 B4=B（T2 威胁面消除）**: brainstorming 阶段决策推翻，用 Python codegen + CMake `add_custom_command` 替代 runtime 文件读取，**T2 路径穿越威胁面完全消除**（不需 8 步守卫）+ 减 1 篇 creative + 加快交付 ~3-5h
3. **F12 hotkey 内化到 Core 层**: 拦截放在 Application::InjectInput 而非 C API 包装层，因 hotkey 状态需跨 API 调用持久化；通过 anon-namespace `kVxKeyF12` 常量解耦头依赖
4. **owned vs raw pointer 双轨设计**: A.0.1 外部 attach 路径（raw devtool_document_）+ A.1.6 内部 LoadDevtoolDocument 路径（owned_devtool_document_ unique_ptr）协同，向后兼容地添加 ownership
5. **JS 路径与 C-API 路径策略统一（A.2.1 安全死角主动修正）**: DomBindings::SetRedactionPolicy + VxDevtoolGetDomJson 读 binding policy 而非 hardcode；Application::set_redaction_policy live-sync 到 DomBindings；消除「JS 路径绕过 redaction」死角
6. **A14 链接闭包零自动化守门（A.2.4）**: pure CMake script + nm + 8 符号黑名单（RegisterDevtoolBindings / kInspectorPanel*×3 / InspectorOverlay / InjectHoverHighlight / InputDispatchSplitter / SerializeDocument），每次 ctest 自动跑替代手动 `nm | grep`
7. **dogfood R2 缺陷处理策略 = panel-side defensive 优先**: A.1.8 暴露 DomBindings 三连缺陷（Element.children / addEventListener / innerHTML setter），选 (b) panel JS try/catch + binding 可用性 check 让主链路验证打通 + 3 P3 候选清单沉淀，**不卡在引擎修复**
8. **C ABI stub 公开表面 vs DevTool 闭包精确区分**: A14 严格条件是「子目录不参与 link」（nm 验证），不是「字节零增长」；C ABI stub 即使 #else 只 return INVALID_STATE 也占字节属公开表面非 DevTool 链接

### 2.6 安全决策（4 威胁面 mitigation）

| # | 威胁 | mitigation 设计 | 落地子任务 | 验证 |
|:-:|---|---|:-:|---|
| **T2** | DevTool resource runtime 加载路径穿越 | **完全消除** — B-A1.1=b 编译期嵌入路径推翻 B4=B；DevTool resource 全部嵌入 binary，无 runtime 文件读取 | A.1.2 | T2 不存在（不需 8 步守卫）|
| **T3** | Inspector 敏感数据 redact | RedactionPolicy::kRedactSensitive 默认；input[type=password] value 自动 [REDACTED]；C-API 路径 + JS binding 路径**统一安全策略**；vx_inspector_set_redaction_policy C API embedder 受控切换 + 无效 enum 拒绝防御 | A.0.3 + A.1.4 + A.2.1 | 5 单测 + 反向探针 + hello_devtool example 运行时验证输出 `T3 redaction OK` |
| **T5** | DisplayList overlay 跨帧累积 | PaintCommand::kOverlayHighlight enum + ResetOverlayCommands erase-remove 协议 + InspectorOverlay::InjectHoverHighlight 仅追加不 reset | A.0.4 + A.1.1 + A.2.2 | 7 单测 + 多帧隔离集成测 + 负控制测 + 反向探针 |
| **T7** | C API buffer overflow（双调用模式）| max_size + caller buffer 太小 + nullptr out_len 三层守卫 + 4 边界测（0 / UINT32_MAX / AtExactNeeded / OneBelowNeeded）| A.0.6 + A.2.3 | 11 单测 + 反向探针验证 boundary fence |
| **A14** | DevTool 关闭时构建产物零变化 | 多层 zero-byte guard（.cc 文件 #ifdef block + CMake `if(VX_BUILD_DEVTOOL)` 条件 link + ctest cmake script nm 自动化）| A.0.5/A.0.6/A.1.4/A.1.6/A.1.7/A.2.1 + A.2.4 | 自动化 ctest smoke 每次跑 nm 验证 8 符号黑名单零命中 + libvx_api.a OFF 12844 + libvx_core.a OFF 1775582 持平 |

---

## 3. 实现摘要

### 3.1 文件变更清单（~32 个文件 touch）

#### 新建文件（~14）

| 操作 | 文件路径 | 说明 |
|:-:|---|---|
| 创建 | `veloxa/core/layout/layout_box.cc` | LayoutBox::ToJson 实现（A.0.2 闭环 #26）|
| 创建 | `veloxa/devtool/CMakeLists.txt` | vx_devtool 子目录 root（A.0.5）|
| 创建 | `veloxa/devtool/inspector/CMakeLists.txt` | vx_devtool STATIC 库（A.0.5）|
| 创建 | `veloxa/devtool/inspector/inspector_data.{h,cc}` | 内部 C++ API（A.0.5）|
| 创建 | `veloxa/devtool/inspector/inspector_overlay.{h,cc}` | InjectHoverHighlight（A.1.1）|
| 创建 | `veloxa/devtool/resources/CMakeLists.txt` + `embed_resources.py` + `inspector_resources.h` + `inspector_panel.{html,css,js}` | 编译期嵌入资源 + Python codegen（A.1.2 + A.1.3）|
| 创建 | `veloxa/devtool/input_dispatch_splitter.{h,cc}` | 纯算法状态机（A.1.5）|
| 创建 | `tests/devtool/inspector/{CMakeLists.txt,inspector_data_test.cc,inspector_overlay_test.cc}` | DevTool 测试树（A.0.5 + A.1.1 + A.2.2）|
| 创建 | `tests/devtool/resources/{CMakeLists.txt,inspector_resources_test.cc,inspector_panel_smoke_test.cc}` | 资源测试（A.1.2 + A.1.3）|
| 创建 | `tests/devtool/input_dispatch_splitter_test.cc` | InputSplitter 测试（A.1.5）|
| 创建 | `tests/api/{devtool_api_test.cc,devtool_attach_api_test.cc,devtool_redaction_policy_test.cc}` | C API 测试（A.0.6 + A.1.7 + A.2.1）|
| 创建 | `tests/core/application_devtool_test.cc` | Application DevTool 测试（A.1.6）|
| 创建 | `tests/integration/devtool_dogfood_smoke_test.cc` | 集成 smoke（A.1.8）|
| 创建 | `tests/script/devtool_bindings_test.cc` | JS native binding 测试（A.1.4）|
| 创建 | `tests/smoke/devtool_a14_link_closure.cmake` | A14 自动化守门（A.2.4）|
| 创建 | `examples/hello_devtool.cc` | SDL2 + DevTool 用户范例（A.3.1）|

#### 主要修改文件

| 操作 | 文件路径 | 说明 |
|:-:|---|---|
| 修改 | `veloxa/core/application.{h,cc}` | A.0.1（双 Document 槽）+ A.1.6（双 UpdateManager + LoadDevtoolDocument + canvas Translate）+ A.1.7（F12 hotkey + SetDevtoolHotkey）+ A.1.8（devtool_script_engine_ + EvalDevtoolScript）+ A.2.1（redaction_policy_）|
| 修改 | `veloxa/core/dom/serializer.{h,cc}` | A.0.3（RedactionPolicy enum + ToJson + EscapeJsonString + IsSensitiveAttribute T3）|
| 修改 | `veloxa/core/render/{paint_command,renderer}.{h,cc}` | A.0.4（kOverlayHighlight + OverlayHighlight 工厂 + ResetOverlayCommands T5）|
| 修改 | `veloxa/core/update_manager.{h,cc}` | A.1.6（mutable_display_list getter）|
| 修改 | `veloxa/core/layout/layout_box.h` | A.0.2（ToJson 声明 + 文档 schema）|
| 修改 | `veloxa/api/veloxa_api.{h,cc}` | A.0.6（VxResult 扩展 + vx_view_serialize_dom_json + T7）+ A.1.7（VxDevtoolOptions + VX_KEY_F12 + vx_view_attach_devtool/detach/loaded）+ A.2.1（VxRedactionPolicy + vx_inspector_set_redaction_policy）|
| 修改 | `veloxa/script/dom_bindings.{h,cc}` | A.1.4（SetDevtoolTargetDocument + RegisterDevtoolBindings + VxDevtoolGetDomJson）+ A.2.1（SetRedactionPolicy + redaction_policy 字段 + 统一策略）|
| 修改 | `veloxa/platform/sdl2/sdl2_input_translate.cc` | A.1.7（SDLK_F12 → VX_KEY_F12 mapping）|
| 修改 | `veloxa/devtool/resources/inspector_panel.js` | A.1.8（R2 panel-side defensive try/catch + binding 可用性 check）|
| 修改 | `veloxa/core/CMakeLists.txt` | A.1.6（vx_devtool_resources 条件 link）|
| 修改 | `veloxa/script/CMakeLists.txt` | A.1.4（vx_devtool 条件 link + VX_BUILD_DEVTOOL define）|
| 修改 | `veloxa/api/CMakeLists.txt` | A.0.6（vx_devtool 条件 link + VX_BUILD_DEVTOOL define）|
| 修改 | `tests/CMakeLists.txt` | 多次累积（A.0.3 + A.0.5 + A.0.6 + A.1.4 + A.1.6 + A.1.7 + A.2.1 + A.2.4 + A.3.1 各 add_test 注册）|
| 修改 | `examples/CMakeLists.txt` | A.3.1（hello_devtool 条件构建）|
| 修改 | `tests/core/application_test.cc` | A.0.1（4 callsite 重命名 + 2 新测）|
| 修改 | `CMakeLists.txt`（root）| A.0.5（option(VX_BUILD_DEVTOOL OFF) + add_subdirectory(veloxa/devtool)）|

### 3.2 公开 C API 新增（DevTool 子系统对外接口）

```c
/* DevTool lifecycle */
VxResult vx_view_attach_devtool(VxView* view, const VxDevtoolOptions* opts);
VxResult vx_view_detach_devtool(VxView* view);
int      vx_view_devtool_loaded(VxView* view);   /* 1 / 0 / -1 */

/* DOM introspection */
VxResult vx_view_serialize_dom_json(VxView* view, char* out_buf,
                                     uint32_t* out_len, uint32_t max_size);

/* T3 redaction policy */
VxResult vx_inspector_set_redaction_policy(VxView* view,
                                           VxRedactionPolicy policy);

/* Constants & types */
#define VX_KEY_F12 0x40000045u
typedef enum { VX_REDACTION_REDACT_SENSITIVE = 0, VX_REDACTION_NONE = 1 } VxRedactionPolicy;
typedef struct { uint32_t devtool_width; uint8_t enable_f12_hotkey; } VxDevtoolOptions;
```

### 3.3 内部 C++ API（D7=C 双层 API 第一层）

```cpp
namespace vx::devtool {
  // SerializeDocument — DevTool Inspector DOM JSON envelope
  vx::String SerializeDocument(const dom::Document* doc, dom::RedactionPolicy policy);
  // SerializeLayoutBox — pure delegate to LayoutBox::ToJson()
  vx::String SerializeLayoutBox(const layout::LayoutBox* box);
  // SerializeComputedStyle — 显式列 10 个常用属性
  vx::String SerializeComputedStyle(const css::ComputedStyle& style);

  // Inspector overlay 注入（A.1.1 / I3 / A5 / T5）
  class InspectorOverlay {
    static void InjectHoverHighlight(render::DisplayList& list,
        const layout::LayoutBox* hovered_box,
        gfx::Color color = ..., f32 stroke_width = 2.0f);
  };

  // 输入路由（A.1.5）
  class InputDispatchSplitter {
    enum class DispatchTarget { kTarget, kDevTool, kSplitterHandle };
    DispatchTarget RouteToDocument(f32 mouse_x, f32 mouse_y,
        bool is_pointer_down, bool is_pointer_up);
    // ... drag capture state machine
  };
}
```

### 3.4 JS native binding

```js
// 在 DevTool QuickJS context 内可见的全局函数
vx_devtool_get_dom_json()  // 返回 target Document 的 JSON envelope（T3 redaction 默认应用）
```

实施细节：通过 `JS_GetContextOpaque(ctx) → DomBindings → devtool_target_document()` 跨 Document recover target；统一使用 `bindings->redaction_policy()` 而非 hardcode（A.2.1 安全策略统一修正）

### 3.5 编译期嵌入资源（B-A1.1=b T2 消除）

```cmake
# veloxa/devtool/resources/CMakeLists.txt
find_package(Python3 REQUIRED)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/inspector_resources.cc
  COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/embed_resources.py
          ${CMAKE_CURRENT_SOURCE_DIR}
          ${CMAKE_CURRENT_BINARY_DIR}/inspector_resources.cc
  DEPENDS embed_resources.py inspector_panel.html
                            inspector_panel.css
                            inspector_panel.js)
add_library(vx_devtool_resources STATIC
  ${CMAKE_CURRENT_BINARY_DIR}/inspector_resources.cc)
```

DevTool 启动时通过 `kInspectorPanelHtml` + `ReplaceFirst("__INLINE_CSS__", kInspectorPanelCss)` + `ReplaceFirst("__INLINE_JS__", kInspectorPanelJs)` runtime 拼接 placeholder。

---

## 4. 测试覆盖

### 4.1 ctest 增量统计

| 维度 | main baseline | Phase A 完成 | 增量 |
|---|:-:|:-:|:-:|
| **DEVTOOL=ON ctest** | 1062/1062 PASS | **1169/1169 PASS** | **+107 测** |
| **DEVTOOL=OFF ctest** | 1062/1062 PASS | **1065/1065 PASS** | **+3 测**（NULL view + redaction OFF stub + A14 link-closure smoke）|

### 4.2 测试套件分布（按 Phase）

| Phase | 新测套件数 | 新测数 | 主要覆盖 |
|---|:-:|:-:|---|
| A.0 | 5 套件 | ~40 测 | LayoutBox::ToJson / DOM Serializer + T3 / PaintCommand kOverlayHighlight + T5 / inspector_data internal API / vx_view_serialize_dom_json + T7 |
| A.1 | 6 套件 | ~50 测 | inspector_overlay / inspector_resources / inspector_panel_smoke / dom_bindings (devtool path) / input_dispatch_splitter / application_devtool / dogfood_smoke / attach_api |
| A.2 | 3 套件 | ~12 测 | redaction_policy 5 + T7 边界 +4 + T5 多帧隔离 +2 + A14 link-closure smoke +1 |
| A.3 | 1 套件 | 1 测 | hello_devtool_smoke（SDL2 + headless smoke）|

### 4.3 反向探针执行率 100%

16 子任务全部含反向探针 step 5，其中 13 子任务精准 FAIL（捕获能力 verified）+ 3 子任务沉淀「无效探针」陷阱（详见 reflection §4.4）

### 4.4 A14 链接闭包零自动化守门（首创）

`tests/smoke/devtool_a14_link_closure.cmake` — pure CMake script + nm 8 符号黑名单 + ctest add_test 注册，每次 ctest 跑（ON+OFF 双侧），人工反向探针 verified 检测能力（vx_view_load_html 临时加黑名单 → smoke 精准 FAIL）

---

## 5. 经验教训（4 大跨子任务复用，从 reflection §5 提取）

### 5.1 plan-fact reconcile 是 Level 4 大件任务的常态（11 处修正）

**模式：** plan 完美率不可期待；build phase 持续 grep + 实证才是质量保证。

**SOP：** 每子任务 step 0 加「plan 假设的 API/structure grep 验证」环节（已沉淀为改进建议 #2 P0）

### 5.2 多层 A14 zero-byte guard 范式锁定（5 子任务复用）

**模式：** `.cc 文件 #ifdef block` + `CMake if(VX_BUILD_DEVTOOL) 条件 link` + `ctest cmake script nm 自动化`

**沉淀：** systemPatterns 新增「子系统关闭守门 ctest smoke 范式」段含 cmake template，下次涉及 conditional 子系统直接复用

### 5.3 C ABI stub 公开表面 vs DevTool 闭包精确区分（3 处出现）

**模式：** A14 spec 「链接闭合零」严格条件是「子目录不参与 link」（`nm` 验证），不是「字节零增长」

**沉淀：** 下次评估 size guard 用 `nm libvx_xxx.a | grep <DevTool 内部符号>` 而非裸字节差

### 5.4 dogfood = R2 缺陷暴露清单产出（A.1.8 集中产出 3 P3 候选）

**模式：** dogfood 任务暴露引擎缺陷不是失败，是设计意图；处理策略 = panel-side defensive `try/catch` + binding 可用性 check 让主链路验证通过 + 缺陷沉淀给独立 P3 任务，**不卡在引擎修复**

**沉淀：** systemPatterns 「dogfood 路径 = 探测性 acceptance test」段扩充「缺陷处理策略 SOP」子段

---

## 6. 改进建议落实状态

### 6.1 P0 立即（3 项 — archive 阶段统一落实/沉淀）

| # | 建议 | 落实状态 |
|:-:|---|---|
| 1 | 大件 Phase 任务在 build 阶段中途识别低估时主动 escalation | ✅ 沉淀到 `systemPatterns.md` § plan escalation 中途触发段；writing-plans skill 改动留待下次任务前由用户审视落实 |
| 2 | 每子任务 step 0 加「plan 假设的 API/structure grep 验证」环节 | ✅ 沉淀到 `systemPatterns.md`（隐含在「plan-fact reconcile 是 Level 4 大件任务常态」教训段）；TDD skill `.cursor/rules/skills/test-driven-development.mdc` 改动留待下次任务前由用户审视落实 |
| 3 | 反向探针选择优先「保留代码但故意修改 string literal / 参数 / 边界」类型 | ✅ 沉淀到 `systemPatterns.md` § 反向探针有效性陷阱清单段（4 类陷阱 + SOP + 检查清单完整） |

### 6.2 P1 下次（5 项 — 迁移到 activeContext 待处理事项）

| # | 建议 | 迁移状态 |
|:-:|---|---|
| 4 | 多子任务连续完成时分 commit 前 `git add -p` 选择性 stage | ⏳ 待迁移到 activeContext |
| 5 | A14 类「子系统关闭守门」自动化 ctest smoke 范式 template 化 | ✅ 已 template 化（`tests/smoke/devtool_a14_link_closure.cmake` + systemPatterns cmake template）|
| 6 | StatusOr<T>::status() 使用规范统一为三元守卫（codebase audit）| ⏳ 待迁移到 activeContext + audit 行动项 |
| 7 | dogfood 任务 panel-side defensive 优先策略入 systemPatterns | ✅ 已沉淀到 systemPatterns 「dogfood 路径」段「实证更新」子段 |
| 8 | C ABI stub 公开表面 vs DevTool 闭包精确区分纳入 spec template「A14 解读」附录段 | ⏳ 待迁移到 activeContext |

### 6.3 P2 长期（2 项 — 已沉淀到 systemPatterns/tasks）

| # | 建议 | 落实状态 |
|:-:|---|---|
| 9 | escalation 后 plan 估时质量提升的实证记录入 systemPatterns | ✅ 已沉淀到 `systemPatterns.md` § plan escalation 中途触发 § 价值证明 + 数据点 + plan ×0.6 矩阵「escalation 后子任务」子桶 |
| 10 | dogfood 暴露的 R2 引擎缺陷清单（3 个 P3 候选）独立立项 | ⏳ 待迁移到 tasks.md「待处理事项 / R3+ 候选段」（archive 阶段执行）|

---

## 7. 安全评估总结

| 维度 | 状态 | 备注 |
|---|:-:|---|
| **T2 路径穿越威胁面** | ✅ 完全消除 | B-A1.1=b 编译期嵌入路径推翻 B4=B；DevTool resource 全部嵌入 binary，无 runtime 文件读取 |
| **T3 Inspector 敏感数据 redact** | ✅ 完整 mitigation | RedactionPolicy 默认 redact + 4 处生效 + C-API + JS binding 路径统一安全策略 + zero-init memory 防御 |
| **T5 DisplayList overlay 隔离** | ✅ 完整 mitigation | kOverlayHighlight + ResetOverlayCommands + InjectHoverHighlight 仅追加协议 + 多帧隔离测 + 负控制测 |
| **T7 C API buffer overflow** | ✅ 完整 mitigation | 双调用 + max_size + caller buffer + nullptr out_len 三层守卫 + 4 边界测 + 反向探针 boundary fence |
| **T8 共享容器 mutation propagation** | ⊘ N/A | 双 Document 隔离设计避免共享容器 mutation；T8 不涉及本 Phase |
| **A14 链接闭包零** | ✅ 自动化守门 | ctest smoke 每次跑 nm 验证 8 符号黑名单 + libvx_api.a OFF 12844 + libvx_core.a OFF 1775582 持平 |
| 输入验证 | ✅ | vx_inspector_set_redaction_policy 拒绝无效 enum；T7 三层守卫 |
| 数据保护（加密/脱敏）| ✅ | T3 redaction 4 处生效，统一来源 Application::redaction_policy_ |
| 依赖审计 | ✅ | 零新 FetchContent；Python3 build-time only 不进 binary |
| 错误信息脱敏 | ✅ | Status 错误信息不含敏感数据 |

---

## 8. 架构影响（Level 4 必填）

### 8.1 对 Veloxa 引擎架构的正向影响

- **解锁 2 项历史技术债闭环**（#26 LayoutBox.Dump / #40 C API DOM introspection）— 引擎可调试性 / 内省性显著提升
- **dogfood 完整链路验证** — Veloxa 引擎能驱动 DevTool UI（panel.html/css/js + JS native binding）= HTML/CSS/JS 子集足够构建真实交互式 UI 的 acceptance proof
- **双层 API（D7=C）落地** — 为未来 CDP / IDE / 外部插件接入预留路径
- **A14 链接闭包零自动化守门** — 任何后续 DevTool 子系统扩展不会出现 OFF binary 意外膨胀
- **5 大可复用架构范式**（双 UpdateManager / 双层 API / #ifdef + CMake 双层 / canvas Translate / 资源嵌入）— 后续 Phase B/C 估时整体下调 ~25%

### 8.2 R2 引擎缺陷暴露 → P3 候选 3 项（独立立项）

| # | 缺陷 | 文件位置 | 临时 mitigation | 优先级 |
|:-:|---|---|---|:-:|
| 1 | DomBindings 缺 `Element.children` 集合 getter（HTMLCollection 风格）| `veloxa/script/dom_bindings.cc` | inspector_panel.js inline `if (!tabs.children) return` 防御 | P3 |
| 2 | DomBindings 缺 `element.addEventListener` | `veloxa/script/dom_bindings.cc` | inspector_panel.js inline `if (typeof btn.addEventListener !== "function") return` 防御 | P3 |
| 3 | DomBindings 缺 `element.innerHTML` setter | `veloxa/script/dom_bindings.cc` | renderDomTree silent no-op，但 vx_devtool_get_dom_json JSON 已覆盖核心数据契约 | P3 |

**P3 修复后 DevTool UI 视觉自动恢复**（DOM tree panel 可正确渲染）+ 第三方 DevTool-like 工具可在 Veloxa 上实现

### 8.3 长期影响（reflection §11 摘要）

- **12 月内**: TASK-30-04-A 已落地，TASK-30-04-B Performance Overlay（估时下调 ~30% → ~3.5-5 h）+ TASK-30-04-C Hot Reload（估时下调 ~20% → ~2.5-4 h）+ DomBindings R2 P3 修复（~3-5 h plan ×0.6）
- **3-6 月内**: DevTool 三件套主线落地后，Veloxa 引擎从「无可视化诊断工具」升级到「Chrome DevTools Inspector + Performance + Hot Reload 三件套等价能力」
- **1 年内**: DevTool 主线完整版（含 Console + JS Debugger + 完整 UI Editor）落地后，Veloxa 进入「dogfood 完整闭环」状态 — 引擎开发可全程在 DevTool 内 inspect / debug / hot reload，开发体验对标商业引擎

### 8.4 与既有 architectural patterns 协同度

| 既有模式 | 协同度 | 备注 |
|---|:-:|---|
| 中央调度协议（UpdateManager）| ✅✅ 完美 | A.1.6 双 UpdateManager 是该模式自然扩展 |
| 两层事件模型 | ✅✅ | A.1.5 InputDispatchSplitter 路由后 DevTool Document 复用三阶段冒泡 |
| Pimpl + JSContext opaque bridge | ✅✅ | A.1.4 DomBindings::SetDevtoolTargetDocument 通过 JS_GetContextOpaque 扩展 |
| 嵌入式优先 | ✅✅ | T2 编译期嵌入消除路径穿越威胁面 + 单 binary 部署 |
| Display List / Command Buffer | ✅✅ | A.0.4 PaintCommand::kOverlayHighlight + I3 注入点 + T5 reset 协议 |
| 双层 API（D7=C 内部 + 公开）| ✅✅ 新加 | A.0.5 + A.0.6 范式，后续插件式子系统标准 |
| #ifdef + CMake 双层 conditional 子系统 | ✅✅ 新模式锁定 | 5 子任务复用，新模式入库 |

---

## 9. 长期维护建议

### 9.1 测试维护

- **A14 链接闭包 ctest smoke** — 任何新 vx_devtool_* lib / 新 DevTool 内部符号必须**同步更新** `tests/smoke/devtool_a14_link_closure.cmake` 的符号黑名单；否则 OFF 链接闭包零保证失效
- **R2-verified CSS 子集 grep 表** — A.1.3 plan §0.9 锁定的 CSS shorthand/longhand 支持表是 inspector_panel.css 编辑约束；未来引擎扩展支持新 CSS 特性时**应同步**「verified」表，否则 panel 视觉可能 silently 退化
- **反向探针陷阱清单** — `systemPatterns.md` § 反向探针有效性陷阱清单段需在每次 reflection 阶段验证「是否新增陷阱类型」

### 9.2 代码维护

- **C ABI 表面 vs DevTool 闭包区分** — 任何新公开 C API（`vx_view_*` / `vx_inspector_*`）即使 #else 路径都属公开 ABI 表面，会在 OFF binary 中占字节但**不算 A14 违反**；用 `nm libvx_api.a | grep <DevTool 内部符号>` 验证而非裸字节差
- **DomBindings R2 三连缺陷** — DomBindings 修复 `Element.children` / `addEventListener` / `innerHTML setter` 后，`inspector_panel.js` 的 panel-side defensive try/catch 可清理（但保留作为未来 R2 候选的安全网）
- **F12 hotkey 范式扩展** — 当前 `Application::MaybeHandleDevtoolHotkey` 是单 hotkey 拦截；未来加 F11（hot reload）/ F1（help）等系统级 hotkey 应泛化为 hotkey table（map<key_code, callback>）

### 9.3 文档维护

- **techContext StatusOr 使用规范** — 全 codebase grep `StatusOr.*\.status\(\)` audit 发现误用应统一为 `r.ok() ? Status::Ok() : r.status()` 三元守卫；考虑加 clang-tidy custom check 强制规范
- **plan ×0.6 矩阵第 18-37 数据点** — 20 个数据点已入库，下次类似 Level 4 实施类任务（Phase B/C）应对照「极细致 plan + 决策跳过率高 + plan-fact reconcile 低」子桶（0.40-0.60×）+「批量小子任务」子桶（0.30-0.50×）+「escalation 后子任务」子桶（~1.0×）选择基础系数

### 9.4 后续任务依赖

| 后续任务 | 依赖本任务的产出 | 估时调整 |
|---|---|---|
| **TASK-30-04-B Performance Overlay** | 双 UpdateManager / 双层 API / #ifdef+CMake / canvas Translate / 资源嵌入 5 大架构范式 | 估时下调 ~30% → ~3.5-5 h |
| **TASK-30-04-C Hot Reload** | F12 hotkey 范式 + DevTool resource embed | 估时下调 ~20% → ~2.5-4 h |
| **DomBindings R2 P3 修复**（新候选）| A.1.8 暴露 3 缺陷 | ~3-5 h plan ×0.6（Level 3 单子任务集中修复）|
| **扩展段 4 项**（Console / JS Debugger / CDP / 完整 UI Editor）| 取决于 R2 P3 修复进度 | TBD |

---

## 10. 参考文档

- **设计 spec**: `docs/specs/2026-04-30-devtool-design.md`（复用 TASK-30-04 蓝图主交付，A1-A14 验收 + T1-T8 威胁建模）
- **实现 plan**: `docs/plans/2026-05-02-devtool-inspector.md`（本任务专属 build 级 plan，~700 + 384 escalation 行）
- **创意设计**: `memory-bank/creative/creative-devtool-screen-layout.md`（splitter dock + HUD overlay 双层结构 5 决策已锁定，复用无新需求）
- **回顾文档**: `memory-bank/reflection/reflection-TASK-20260502-01.md`（13 段全维度 / ~880 行 / Level 4 全面回顾）
- **蓝图 plan 基线**: `docs/plans/2026-04-30-devtool.md` §Phase A 段（蓝图阶段参照）
- **蓝图 archive**: `memory-bank/archive/archive-TASK-20260430-04.md`（前置任务，blueprint 主交付）

---

**归档完成。Memory Bank 已更新，准备接受新任务（推荐 `/van TASK-30-04-B Performance Overlay` / `DomBindings R2 P3 修复` / 用户优先级排期决定）。**
