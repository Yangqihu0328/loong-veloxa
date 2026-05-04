# 创意设计：MVP-A 最小可运行 demo（基线档）

**日期：** 2026-05-04
**状态：** 已批准（V1=D 三档分级 + B2=3 篇拆分粒度 user all_recommended 锁定）
**关联任务：** [TASK-20260504-01](../../docs/specs/2026-05-04-mvp-scope.md)
**对应 spec 段：** [`§3.1 MVP-A`](../../docs/specs/2026-05-04-mvp-scope.md) + 附录 A.1-A.6 + §6.1 三档总览矩阵

---

## 1. 设计挑战

**问题陈述：**

MVP-A 已 100% 达成（A.1-A.6 全 ✅ / ctest 1284 ON + 1091 OFF / A14 0 字节增长）。**核心挑战不是"如何达成"，而是"如何防止退化"** —— Veloxa 项目持续演进 / 后续 30-50+ 个子任务（MVP-B 4 gap / MVP-C 9 gap / DevTool Phase E/F/G plus / 性能优化 R3+ 13 项 / etc）将不断改动 codebase；如何确保每个 commit 都不破坏 MVP-A 基线？

**约束条件：**

1. **零运行时开销** — baseline 守门机制不能引入任何 production 运行时成本
2. **CI 兼容** — 必须能在现有 GitHub Actions / 本地 ctest 流程中工作
3. **低维护成本** — 不能要求每个 contributor 手动报告 ctest 数量
4. **可被工作流系统强制** — 沿用 [memory-bank/systemPatterns.md «commit body Source 溯源 + 实测数据 quad-evidence» 范式](../../memory-bank/systemPatterns.md)

**成功标准：**

1. **检测时延** — baseline 退化必须在 commit 时（最迟在下一次 ctest 运行时）被检测
2. **零误报** — 不能因正常的 ctest 数量增加（B-G/C-G 补全）而误报为退化
3. **零漏报** — 任何 commit 删除 ctest 或绕过 ctest 必须被发现
4. **强制守门 sept-evidence** — 沿用 [Phase 0 投入越深 / build phase 越快定律 sept-evidence](../../memory-bank/systemPatterns.md) 范式

---

## 2. 探索的方案（3 方案）

### 方案 A：被动 baseline（仅在 ctest 数量退化时被动发现）

**描述：** 不做任何主动守门 / 依赖 contributor 自觉运行 ctest / 退化在 review 时被发现。

**优点：**
- 零开发成本（无需新机制）
- 零文档约定（无需培训 contributor）

**缺点：**
- 漏报概率极高（review 时容易 miss ctest 输出）
- 退化追溯困难（不知道是哪个 commit 引入的退化）
- 与既有 [«commit body 实测数据 quad-evidence» 范式](../../memory-bank/systemPatterns.md) 不一致

**复杂度：** 极低
**风险：** 极高（违反 sept-evidence 文化 / 与项目工作流系统脱节）

---

### 方案 B：commit body Source 段强制守门（推荐 ✅）

**描述：** 沿用既有 [«commit body 实测数据 quad-evidence» 范式](../../memory-bank/systemPatterns.md)，强制要求**每个**涉及 codebase 改动的 commit 在 body 中必含一行 `MVP-A baseline X/X PASS`（X = 当前 baseline ctest 数量 + 任何新增允许数 / 双 config DEVTOOL=ON / OFF）。

**优点：**
- 复用既有范式（零学习成本 / contributor 已熟悉）
- 退化追溯精确到 commit（git log 可查）
- 与 Memory Bank archive / progress / tasks 文档自然对齐
- 强制 contributor 在 commit 前实测（培养 ctest-first 习惯）
- 写入 systemPatterns 成第 12 个范式

**缺点：**
- 仍依赖 contributor 自觉（无机器强制）
- contributor 可能 fake 数据（但与 archive quad-evidence 范式相同 — 信任问题不是新的）

**复杂度：** 低（仅文档约定 + writing-plans.mdc audit 子条强化）
**风险：** 中（依赖纪律 / 但项目内已有 32-33 commits 累计 quad-evidence 实证范式可行）

---

### 方案 C：CI lint hook（自动检测 commit body 是否含 baseline 报告）

**描述：** 在 GitHub Actions / pre-commit hook 中加 lint 脚本，扫描每个 commit body 是否含 `MVP-A baseline \d+/\d+ PASS` 模式 + 与实际 ctest 输出对照（额外执行一次 ctest 抽样 verify）。

**优点：**
- 机器强制（contributor 无法绕过）
- 退化在 PR 阶段就被 block
- baseline 数字自动追踪（可生成 dashboard）

**缺点：**
- 开发成本高（需写 lint 脚本 + CI 集成）
- 假阳性风险（lint 脚本本身可能有 bug）
- ctest 抽样 verify 显著增加 CI 时间（DEVTOOL=ON 1284 测全跑约 5-10 min）
- 与本任务范围（**纯蓝图 / V2=a 不含 build**）矛盾 — 需另立子任务

**复杂度：** 中（需新立 Level 2-3 子任务实施）
**风险：** 中（CI 时间膨胀 / lint 脚本维护负担）

---

### 2.x 方案对比

| 维度 | 方案 A 被动 | 方案 B commit body 守门 | 方案 C CI lint hook |
|---|:-:|:-:|:-:|
| 复杂度 | 极低 | 低 | 中 |
| 可维护性 | 极差（无追溯）| 高（git log 可查）| 中（lint 脚本维护）|
| 检测时延 | 长（review 阶段）| 短（commit 阶段）| **极短（PR 阶段 block）** |
| 漏报概率 | 极高 | 低（依赖纪律）| **极低（机器强制）** |
| 误报概率 | 0 | 0 | 中（lint 脚本 bug）|
| 与既有范式协同 | ❌ 违反 | ✅ 完全一致（quad-evidence）| ⚠️ 需扩展 |
| 与本任务范围 | ✅ 零额外工作 | ✅ 仅文档约定 | ❌ 需另立子任务 |
| 开发成本 | 0 | ~30 min（writing-plans.mdc 加 audit 子条）| ~3-8 h plan ×0.6（Level 2-3）|
| CI 时间影响 | 0 | 0 | ⚠️ +5-10 min |
| **推荐度** | ❌ | **⭐ 主推** | **🌟 长期增量** |

---

## 3. 选定方案：B 主推 + C 长期增量

**推荐方案：B + C 混合 — 短期立即采用 B，长期可选 C 增强**

### 3.1 选择理由

1. **B 完全契合本任务范围（V2=a 蓝图 / 纯文档）** — 仅需在 [writing-plans.mdc](../../.cursor/rules/skills/writing-plans.mdc) 加一条 audit 子条 + 在本 creative 文档落地标准 commit body 范本，零 codebase 改动
2. **B 复用既有 [«commit body 实测数据 quad-evidence» 范式](../../memory-bank/systemPatterns.md)** — 已在 32-33 commits 中实证可行 / contributor 已熟悉
3. **B 的纪律风险可接受** — 项目内 [TASK-20260503-04 archive §6 评估范式](../../memory-bank/archive/archive-TASK-20260503-04.md) 已显示 contributor 自觉率 100%
4. **C 是长期增量方向** — 当 contributor 数量 ≥ 5 / fake 数据风险上升时再考虑投资
5. **风险缓解** — B 方案的"依赖纪律"风险通过 [reflection 沉淀回流模式](../../memory-bank/systemPatterns.md) 的反复模式 #1 抑制守门已部分缓解（commit body audit 是 reflection 阶段强制检查项）

### 3.2 风险缓解措施

| 风险 | mitigation |
|---|---|
| Contributor 忘记加 baseline 报告 | writing-plans.mdc audit 子条 + reflection 阶段强制检查 / 累积反复模式 #N+1 候选定型 |
| baseline 数字写错（typo / 复制旧数）| 按 [TASK-20260502-02 P3 #N audit 范式](../../memory-bank/archive/archive-TASK-20260502-02.md) — 至少 1 个 reviewer 交叉对照 |
| 数据 fake | quad-evidence 文化已实证 / 长期切到方案 C 机器强制 |

---

## 4. 算法/坐标约定一图（§d.1 audit）

**N/A** — 本设计无算法 / 无 ≥2 坐标系 / 无 ≥2 方向多锚点偏移 → [`/creative` §d.1 P0 强制项](../../.cursor/commands/creative.md) 不适用本 creative。

---

## 5. 算法伪码累积语义 explicit method（§d.2 audit）

**N/A** — 本设计无算法伪码 / 无累积量 / 无 chain / accumulator / state machine → [`/creative` §d.2 P1 强制项](../../.cursor/commands/creative.md) 不适用本 creative。

---

## 6. 实现指导（方案 B 落地）

### 6.1 commit body Source 段标准范本

任何涉及 codebase 改动的 commit（**非纯文档 commit 除外**）必须在 body 中含以下段：

```
DEVTOOL=ON 1284/1284 PASS / DEVTOOL=OFF 1091/1091 PASS
A14 link closure: nm verify 0 byte growth (vs baseline)
MVP-A baseline 不退化 ✅
```

**baseline 数字更新协议：**

- 任何 commit **新增** ctest（非删除 / 非绕过）→ 新 baseline = 旧 baseline + 新增数（写入 commit body 同时**主动**更新 [`memory-bank/productContext.md`](../../memory-bank/productContext.md) 「ctest baseline」段 — 待后续子任务 sync）
- 任何 commit **删除** ctest（如重构 / 测试合并）→ 必须在 commit body 中**显式**说明 `removed N tests due to <reason>`，并展示新 baseline；reflection 阶段必检 / archive 阶段必沉淀

### 6.2 baseline 红线（不可退化）

| Config | baseline | 验证方式 |
|---|:-:|---|
| `DEVTOOL=ON` | **1284** | `ctest --output-on-failure` |
| `DEVTOOL=OFF` | **1091** | `ctest --output-on-failure -DVX_BUILD_DEVTOOL=OFF` build |
| A14 链接闭包零字节增长 | **0 byte** | `nm libveloxa.a \| grep -c devtool::` 对比 |

### 6.3 与 writing-plans.mdc audit 子条的对应（已 sept-evidence 沉淀）

[writing-plans.mdc «ctest 数量预期 config 矩阵» + add_test config guard 边界假设审计](../../.cursor/rules/skills/writing-plans.mdc) 已是 **P0 强制 audit 子条**（[TASK-20260503-04 P0 #1 升级落实 commit `33afb7c`](../../memory-bank/archive/archive-TASK-20260503-04.md)）。本 creative 不重复定义 / 仅引用 + 强化「**MVP-A 视角的 baseline 数字红线**」语义。

### 6.4 后续 example 范例（hello_mvp_a.cc 占位 / 非本任务实施）

**建议**（由后续 MVP-A baseline 守门 example 子任务实施）：

```cpp
// examples/hello_mvp_a.cc — MVP-A 基线档示范
// 验收：headless MemorySurface 渲染 1 帧 → exit 0

#include <veloxa/veloxa_api.h>

int main() {
  auto* event_loop = vx_event_loop_create_headless();
  auto* view = vx_view_create();
  vx_view_load_html(view, "<h1>Hello, MVP-A.</h1>");
  vx_view_load_css(view, "h1 { color: red; }");
  auto* surface = vx_surface_create_memory(800, 600);
  vx_view_update(view, surface);
  // 假设可保存 PPM 验证或与黄金图对比
  return 0;
}
```

### 6.5 后续 smoke test 建议（label `mvp-a-baseline`）

| smoke test | 验证 |
|---|---|
| `vx_view_load_html_smoke` | A.1 |
| `vx_view_load_css_smoke` | A.1 |
| `css_engine_smoke` | A.2 |
| `layout_engine_smoke` | A.2 |
| `software_canvas_smoke` | A.3 |
| `c_api_smoke` | A.4 |
| `hello_sdl2_smoke` | A.5 |
| `devtool_a14_link_closure` | A.6 |

> **注：** 上述部分 smoke test 已存在（`hello_sdl2_smoke` / `devtool_a14_link_closure`），其余建议在后续 MVP-A baseline 守门 example 子任务中以 `add_test` + `set_tests_properties(... LABELS mvp-a-baseline)` 方式落地。

---

## 7. 与既有 example 关系

| example | 对应 MVP-A 验收 | 状态 |
|---|:-:|:-:|
| `examples/hello.cc` | A.1（headless 渲染 / 入门 demo）| ✅ 已落地 |
| `examples/hello_sdl2.cc` | A.5（SDL2 真窗口后端）| ✅ 已落地 |
| `examples/hello_devtool.cc` | 不属于 MVP-A（属于 MVP-B B.7+B.8 dogfood）| ✅ 已落地 |
| **`examples/hello_mvp_a.cc`**（建议）| 基线档完整 demo（A.1+A.2+A.3+A.4 一站验证）| 🟡 占位（非本任务）|

---

## 8. 安全考量

### 8.1 威胁面（与 MVP-A 范围一致）

MVP-A 是基线档 / **无新外部输入** / **无新威胁面引入**。完全沿用既有：

| Threat | mitigation 来源 | 状态 |
|---|---|:-:|
| T2 路径穿越（HTML / CSS 文件加载）| `2026-04-05-foundation-design.md` 文件系统 sandbox | ✅ |
| T3 redaction（C ABI 错误信息不泄漏路径）| `c_api_test` audit | ✅ |
| T5 process 隔离（Surface vs window）| `2026-04-25-sdl2-window-backend-design.md` | ✅ |
| T6 callback budget（QuickJS interrupt handler — 仅在 MVP-B B.3 启用）| TASK-20260503-05 | N/A（A 不依赖 JS）|
| T7 buffer overflow（SoftwareCanvas 边界检查）| `software_canvas_test` 边界用例 | ✅ |

### 8.2 baseline 守门机制本身的安全考量

- ✅ commit body baseline 报告**不**含敏感数据 / 仅含 ctest 数量
- ✅ baseline 数字本身**非**机密（已在 README + productContext + archive 公开）
- ✅ A14 nm verify 命令本身只读 / 无副作用

---

## 9. 与既有 systemPatterns 协同度自检

| # | systemPattern | 本 creative 对应 |
|:-:|---|---|
| 1 | commit body Source 溯源 + 实测数据 quad-evidence | ✅ 主路径 — 方案 B 直接复用 |
| 2 | reflection 沉淀回流模式 | ✅ §6.1 baseline 数字更新协议中明示 reflection 阶段必检 |
| 3 | Phase 0 投入越深 / build phase 越快定律 sept-evidence | 引用（baseline 守门 = Phase 0 守门最小成本案例）|
| 4 | A14 链接闭包零字节增长守门 | ✅ §6.2 baseline 红线第 3 行 |
| 5 | writing-plans.mdc «ctest 数量预期 config 矩阵» audit | ✅ §6.3 直接引用 |

**协同度自评：** 5/5（100% — 完全契合既有范式 / 0 新模式入库 / 仅强化 MVP-A 视角）

---

## 10. 验收标准（本 creative 文档级）

- [x] 设计挑战清晰（§1 问题陈述 + 4 项约束 + 4 项成功标准）
- [x] ≥2-3 个方案探索（§2 / 3 方案 A/B/C 全面对比）
- [x] 方案对比表（§2.x / 9 维度对比）
- [x] 推荐方案 + 论证（§3 / B 主推 + C 长期增量 + 5 项理由 + 3 项风险缓解）
- [x] §d.1 / §d.2 audit（§4 + §5 / 全标 N/A 已 audit）
- [x] 实现指导（§6 / commit body 范本 + baseline 红线 + 与 writing-plans.mdc 引用 + 后续 example 占位）
- [x] 与既有 example 关系映射（§7）
- [x] 安全考量（§8 / 5 项 T 威胁面 + baseline 守门自身安全）
- [x] systemPatterns 协同度自检 ≥ 5 模式（§9 / 5/5 100%）

---

**文档结束。** 本 creative 由 [TASK-20260504-01 plan §4.2 子任务 2](../../docs/plans/2026-05-04-mvp-scope.md) 直接产出。下一步：commit 2 落盘 + 进入 creative-mvp-B.md。
