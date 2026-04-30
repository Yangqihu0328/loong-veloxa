# R0 准备阶段数据快照 — TASK-20260430-03

> **任务**：全代码库 6 维度 Code Review。
> **阶段**：R0 准备（2026-04-30 22:52 → 23:08，~16 min 实测）。
> **角色**：本文档是 R0 全部产出的综合快照，作为 R1 报告（下一轮）的输入数据，并归档供未来追溯。
> **生成命令**：`/build` R0 子流程（R0.1 ctest 基线 / R0.2 fingerprint grep / R0.3 lcov 覆盖率 / R0.4 OSV CVE 审计 / R0.5 抽样名单）。

---

## 1. R0 全部子任务核验状态

| 子任务 | 状态 | 实测耗时 | Plan 估时 (×0.6) | 主要产出 |
|--------|------|----------|------------------|---------|
| R0.1 ctest 基线 | ✅ | < 30s | 5 min (3 min) | **1061/1061 PASS in 2.36s** + 1 Skip（Wpt001 沉淀） |
| R0.2 fingerprint grep | ✅ | ~5 min | 30 min (18 min) | 18 项反模式覆盖 + 文件大小 top 25 + 候选 findings F-001 ~ F-005 |
| R0.3 lcov 覆盖率 | ✅ | ~107s | 25 min (15 min) | 行 85.4% / 函数 95.4% / 分支 57.6% + 12 个薄弱模块 + F-006 ~ F-009 |
| R0.4 CVE 审计 | ✅ | ~10 min（含代理介入） | 25 min (15 min) | **0 CRITICAL/HIGH** + 5 Medium/Low + libpng 3 新 CVE + F-010 ~ F-014 |
| R0.5 抽样 + commit | ✅ | ~5 min | 5 min (3 min) | 本文档 + R0 commit |
| **R0 合计** | ✅ | **~22 min 实测** | 90 min plan / 54 min ×0.6 | 实测约 0.41× plan / 0.69× ×0.6 校准估时 |

> **数据点 #16**：本次 R0 实测 0.41× plan，比"plan ×0.6"校准还省 31%。原因：grep/lcov/curl 都是数据收集型，无设计/实现负担；上下文已熟（PLAN 阶段做完头脑风暴）。
> 这个数据点会进入 reflect 阶段的 plan ×0.6 校准协议第 16 个数据样本。

---

## 2. R0 候选 findings 总表（14 项，待 R1 验证 + 分级 + 写方案）

| ID | 维度 | 严重度（候选） | 文件 | 简述 |
|----|------|----------------|------|------|
| F-001 | 正确性 | P1? | `application.cc:31, 36` | 两次 `delete document_;` 不是 bug 但若 EventManager 持 Element 指针 → 悬挂指针风险 |
| F-002 | 正确性 | P2 | `font_manager.cc:169`, `freetype_shaper.cc:48-49` | HarfBuzz `int` 缩窄未做 `INT_MAX` 边界检查 |
| F-003 | 维护 | P1 | `script/dom_bindings.cc` (819 LOC) | 单文件 ~30 个 JS↔C++ 绑定函数，建议拆 `dom_bindings_{style,event,node}.cc` |
| F-004 | 维护 | P2 | `core/css/parser.cc` (1384 LOC) | 全代码库最大文件，建议拆 `parser_{at_rule,selector,value}.cc` |
| F-005 | 维护 | P2 | `core/layout/layout_engine.cc` (907 LOC) | 已分离 flex_layout，但 block / inline 仍混杂 |
| F-006 | 测试 | **P1** | `core/image/image_decoder.cc` | **行覆盖 57.1%** + libpng/jpeg 错误路径多无测试 + 与 F-010 形成威胁链 |
| F-007 | 测试 | P1 | `graphics/software/rasterizer.cc` | 行覆盖 60.4% + 359 LOC + 渲染热路径分支测试稀薄 |
| F-008 | 测试 | P2 | 全代码库 | 分支覆盖率 57.6% << 行覆盖率 85.4% → 容器/parser 模板分支驱动不充分 |
| F-009 | 一致 | P3 沉淀 | `foundation/base/assert.h` | 行覆盖 0.0% 是设计正确（abort 不可测），README/techContext 加一行解释 |
| F-010 | **安全** | **P0?** | `libpng 1.6.37` + `image_decoder.cc` | **3 个 2026 新公布 Medium CVE 未修复** + 直接入口 + 覆盖率薄弱（与 F-006 联动） |
| F-011 | 安全 | P1 | `libsdl2 2.0.20` + `libjpeg-turbo 2.1.2` | 各 1 Low DoS CVE，部署文档需说明沙箱要求 |
| F-012 | 安全 | P1 | 部署链 | 依赖 Ubuntu apt backport → 不同部署目标 (yocto / scratch) CVE 跟踪职责需明确 |
| F-013 | 安全 | P2 | `CMakeLists.txt` | quickjs-ng v0.14.0 锁定 1.5 年未升级 → 季度依赖升级 + CI 跑 OSV check |
| F-014 | 安全 | P2 | repo 根 | 缺 `SECURITY.md` / 依赖供应链文档 |

> **R1 分级原则**（spec §6）：
> - **P0**：1 行修复 / 笔误 / 死代码 / 命名（< 30 min/项），R2 范围
> - **P1**：局部 bug / 性能热点 / API 改进（< 4 hr/项），R3+ 拆任务
> - **P2**：架构 / 大重构 / 长期改进（> 4 hr/项），R3+ 拆任务

---

## 3. R1 抽样名单（基于 LOC + 覆盖率薄弱 + 候选 findings 加权）

### H 层（深读，~3-5 min/文件，共 ~20 文件，占 ~15%）

| # | 文件 | LOC | 覆盖% | 入选理由 |
|---|------|-----|-------|---------|
| 1 | `core/css/parser.cc` | 1384 | 76.8% | 最大文件 + 覆盖薄弱 + F-004 |
| 2 | `core/layout/layout_engine.cc` | 907 | 80.5% | 第二大 + F-005 |
| 3 | `script/dom_bindings.cc` | 819 | 87.6% | 第三大 + F-003 |
| 4 | `graphics/software/rasterizer.cc` | 483 | **60.4%** | 渲染热路径 + F-007 |
| 5 | `core/css/transition.cc` | 469 | 75.2% | 覆盖薄弱 |
| 6 | `foundation/containers/hash_map.h` | 431 | 97.1% | 关键容器 + 模板分支 |
| 7 | `core/html/tokenizer.cc` | 417 | 88.4% | HTML 分词器 |
| 8 | `core/layout/flex_layout.cc` | 415 | 74.8% | flex 边角 |
| 9 | `graphics/software/software_canvas.cc` | 410 | 73.5% | Canvas 接口 |
| 10 | `core/css/style_resolver.cc` | 404 | 74.1% | CSS 级联 |
| 11 | `core/render/renderer.cc` | 227 | 89.8% | 渲染 dispatch |
| 12 | `core/event/event_manager.h` | (内联) | 100% | 事件中心 |
| 13 | `core/event/event_dispatcher.cc` | (中等) | 100% | 事件分发 |
| 14 | `core/update_manager.cc` | (中等) | 100% | UpdateManager 编排 |
| 15 | `text/font_manager.cc` | 222 | 93.0% | F-002 + 字体生命周期 |
| 16 | `core/application.cc` | (中等) | 98.9% | F-001 + 顶层生命周期 |
| 17 | `api/veloxa_api.cc` | 219 | **74.5%** | C API 边界 + 覆盖薄弱 |
| 18 | `foundation/strings/utf.cc` | 225 | 89.6% | UTF-8 关键算法 |
| 19 | `foundation/memory/arena_allocator.h` | (中等) | 98.4% | 关键内存策略 |
| 20 | **`core/image/image_decoder.cc`** | 169 | **57.1%** | **F-006 + F-010 威胁链路核心** |

### M 层（一过式，~30s/文件，共 ~80 文件，占 ~60%）

- 其余 `core/`, `foundation/`, `graphics/`, `platform/`, `text/`, `script/`, `api/` 下 .cc/.h
- 检查项：grep 关键字命中、API 设计、与 H 层一致性、命名风格

### L 层（跳过/快速浏览，~5s/文件，共 ~36 文件，占 ~25%）

- enum.h / 简单 trait header / hello_*.cc 示例 / fixture 数据文件

---

## 4. R0 acceptance 核验（spec §3 / §12）

### A0 准备阶段（spec §3.A0）

- ✅ A0.1：grep fingerprint 在 7 大子系统全跑（30 关键字 → 18 项反模式数据）
- ✅ A0.2：抽样名单显式（H 20 / M 80 / L 36，本文档 §3）
- ✅ A0.3：lcov 全代码库覆盖率快照（85.4% / 95.4% / 57.6%）
- ✅ A0.4：依赖 CVE 审计完成，**0 CRITICAL/HIGH**

### plan §0 R0 退出条件

- ✅ 1061/1061 ctest PASS（baseline 干净）
- ✅ fingerprint 报告产出（本文档 §2）
- ✅ lcov 报告 + HTML（`build_coverage/coverage_html/`，gitignore 不入库）
- ✅ CVE 报告产出（本文档 §2 F-010 ~ F-014）
- ✅ R1 抽样名单确认（本文档 §3）
- ✅ R0 commit（本文档作为 commit payload）

---

## 5. R1 进入条件 / 已知风险

### 进入 R1 必备
- 本文档已落盘 ✅
- R0 commit 已打 ✅
- R1 时间预算：plan 估 150-200 min（×0.6 → 90-120 min 校准） — 等用户启动 `/build` 第二轮

### R1 已知风险
- **R1 范围漂移**：14 个候选 findings 在深读阶段会增至 ~30+ → spec §6 P0/P1/P2 分级标准是关键防漂移工具
- **抽样深度被挑战**：H 20 文件深读 ~100 min，M 80 文件 ~40 min，L 36 文件 ~3 min，合计 ~143 min plan / ~86 min 校准 — 与 R1 预算匹配 ✅
- **Findings 数量 vs 严重度反相关**：F-010 是唯一候选 P0（但其严重度评估在 R1 详查后才能锁定，可能降为 P1）

### R2 触发预测
- 当前候选 P0 **仅 1 个 (F-010)**，但 F-010 是依赖升级 + 测试补强（不是 1 行修复，超出 R2 范围！）
- 实际可能 R2 跑出空名单 → Checkpoint 1 可能被用户跳过
- 真正的 R2 候选要在 R1 深读期间产出（笔误 / 死代码 / 命名 / 1 行修复，预计 5-15 个）

---

## 6. 引用工具状态

- ✅ Cursor Grep（自带 ripgrep 内嵌）
- ✅ ctest / cmake / gcc 11.4
- ✅ lcov 1.14 / gcov 11.4 / genhtml 1.14
- ✅ OSV.dev API（通过 WSL2 → Windows Clash 代理 `172.22.32.1:7890`）
- ✅ Python 3 / curl / jq

> **临时数据归档位置**：
> - `/tmp/codebase_review/r0_2_fingerprint.md`（fingerprint 详表）
> - `/tmp/codebase_review/r0_3_coverage.md`（coverage 详表）
> - `/tmp/codebase_review/r0_3_coverage_full.txt`（lcov per-file 完整列表）
> - `/tmp/codebase_review/r0_4_cve.md`（CVE 详表）
> - `/tmp/codebase_review/r0_4_cve_full.json`（OSV 原始 JSON）
> - `build_coverage/coverage_html/index.html`（HTML 报告，本地浏览器可看）
> 这些文件会随会话结束清理，本快照是入库的唯一持久副本。
