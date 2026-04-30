# 进度记录

## 当前任务

**TASK-20260430-04：规划 UI 编辑器 + 调试器（DevTool 三件套蓝图设计）[安全相关]** — Level 4，**`/plan` 阶段完成**（2026-05-01 ~01:50），4 篇产出文档全部落盘（spec + plan + 2 creative），D1-D8 决策矩阵全部锁定（按 VAN 推荐默认）。下一步路径三选一：A 进入 `/reflect`（推荐，与 V2=a 主线一致）/ B 改路径 V2 → b 进入 `/build` / C 暂停审查（用户先 review 4 篇文档）。

### `/plan` 阶段产出快照（2026-05-01 ~01:50）

- **头脑风暴 D1-D8 全部锁定（用户两次跳过 AskQuestion 后按 VAN 推荐默认锁定 6 次决策）：**
  - D1 三件套实施优先级 = **B Inspector → Overlay → Hot Reload**（Inspector 优先做 UI 渲染样板 + 闭环 #26/#40/#4 三大技术债）
  - D2 Inspector 数据采集协议 = **B 半结构化（JSON tree + DisplayList overlay + C API JSON）**（C API 边界清晰未来对接 CDP + DisplayList overlay 不污染目标 DOM）
  - D3 DevTool UI 主屏布局 = **B 同窗口 splitter dock + Overlay HUD 子模式**（→ creative #1 详化）
  - D4 DevTool 隔离边界 = **B 单进程共享容器（双 Document + 共享 EventLoop / Application / ImageCache）**（嵌入式硬约束 + 与 D2 注入语义一致）
  - D5 Hot Reload file watcher + 增量策略 = **A 嵌入式专注（Linux inotify + CSS-only 增量重载）**（→ creative #2 详化）
  - D6 Performance Overlay 数据采集点 = **B Chrome DevTools 风格（五钩子 + 滑动 60 帧 + dirty rect 边框高亮）**（闭环技术债 #35）
  - D7 C API 扩展边界 = **C 双层 API（内部 C++ 核心 + 公开 C API 薄封装）**（兼顾性能 + 扩展性 + 与 D2 协议一致；闭环技术债 #40）
  - D8 安全威胁建模 = **A T2/T3/T5/T6/T7/T8 完整 + T1/T4 扩展段占位**（与 V5=✅ + 三件套实际威胁面对齐）

- **4 篇产出文档：**
  - **spec** `docs/specs/2026-04-30-devtool-design.md`（12 段 / 三件套验收 A1-A14 / D1-D8 决策矩阵 / 注入点 I1-I8 核对表 / T1-T8 威胁建模 / R1-R6 风险登记 / ≥ 30 systemPatterns 自我对照）
  - **plan** `docs/plans/2026-04-30-devtool.md`（Phase 0 全局约束 + CMake 链接审计 + 静态库循环审计 + 测试基础设施审计 + 边界输入清单 16 项 + 既有测试隐式契约 fingerprint + CSS shorthand 能力 grep 表 / Phase A/B/C/D 子任务 ~40 项 + plan ×0.6 估时矩阵）
  - **creative #1** `memory-bank/creative/creative-devtool-screen-layout.md`（5 决策：整体布局双层结构 / dock 模式切换 / HUD 透明合成 / overlay 渲染顺序双线宽 / F12-F11 toggle）
  - **creative #2** `memory-bank/creative/creative-devtool-hot-reload.md`（5 决策：FileWatcher 抽象 / CSS-only 增量 / DOM 状态保留 / watcher root 边界 / 错误恢复）

- **plan ×0.6 估时（主线 V2=a 蓝图任务自身）：**
  - VAN ~25 min（实测）
  - Plan ~180-240 min plan / ~108-144 min plan ×0.6（待实测 — 预期落 Level 4 蓝图任务区间 0.4-0.7×）
  - Reflect ~60 min plan / ~36 min plan ×0.6（待）
  - Archive ~45 min plan / ~27 min plan ×0.6（待）
  - **主线合计** ~315-375 min plan / ~189-225 min plan ×0.6（plan ×0.6 第 17 数据点候选）

- **用户后续独立立项候选（基于 plan 拆出）：**
  - TASK-30-04-A：DevTool Phase A — Inspector 实施（Level 3，~12.25 h plan / ~7.35 h plan ×0.6）
  - TASK-30-04-B：DevTool Phase B — Performance Overlay 实施（Level 2-3，~7.25 h / ~4.35 h）
  - TASK-30-04-C：DevTool Phase C — Hot Reload 实施（Linux only，Level 3，~10 h / ~6 h）
  - 4 项扩展段（Console / JS Debugger / CDP / 完整 UI Editor）— spec §11 占位

- **触及技术债 4 项闭环 ROI 路径：** #26 LayoutBox.Dump → Inspector Layout / #35 UpdateManager frame hook → Performance Overlay / #40 C API introspection → Inspector 全子系统 / #4 ImageCache 命名空间 → DevTool icon 隔离

- **下一步路径三选一：** A 进入 `/reflect`（推荐）/ B V2 → b 进 `/build` / C 暂停审查

<details>
<summary>VAN 阶段产出快照（2026-04-30 23:40，已闭环，点开查看）</summary>

### VAN 阶段产出快照

- **意图判读：** 用户用「**规划**」二字（非「实现」） → 主交付物预期为蓝图级 spec + plan + 子任务 ID 列表，可选 MVP 子集实施
- **grep 实证（F1-F9）：**
  - ❌ 0 处 inspector / devtool / debugger / hot reload / overlay 实现代码
  - ❌ JS Debug API 未集成（技术债 #44 `JS_SetInterruptHandler` 仅 spec 提及）
  - ❌ C API 缺 introspection 接口（技术债 #40 `vx_view_get_document` 等不存在）
  - ❌ LayoutBox / DisplayList 无 `Dump()`（技术债 #26）
  - ❌ UpdateManager 无帧生命周期钩子（技术债 #35）
  - ✅ DOM `Serialize(node)` 已可用（Inspector DOM 树文本化复用基础）
  - ✅ `Application::document() / event_manager() / update_manager() / event_loop()` 已暴露
  - ✅ SDL2 后端已就绪（`hello_sdl2` 范本，DevTool UI 渲染主线起点）
  - ✅ HitTest + EventManager hover/active/focus（Inspector 元素高亮选取复用基础）
- **复杂度初判：** Level 4（多子系统 — Inspector + Hot Reload + Overlay + Console + 编辑器 + JS 调试器 6 候选）
- **基础设施成熟度三色分级：** 🟢 已就绪 5 项 / 🟡 需扩展（小工程）4 项 / 🔴 需新建（大工程）6 项
- **V1-V5 锁定（用户两次跳过 AskQuestion → 按 VAN 推荐默认）：**
  - V1 = **B 三件套**（Inspector DOM/Style/Layout + Hot Reload + Performance Overlay）— Console / JS Debugger / 完整 UI Editor 标为「扩展候选」入 spec §扩展段
  - V2 = **a 纯蓝图**（spec + plan + creative ×N，不强制实施代码）— 与「规划」语义最匹配
  - V3 = **A Veloxa 自渲染**（dogfood 模式 — DevTool 即引擎自我应用样板）
  - V4 = **Level 4**（多子系统蓝图 + 架构决策矩阵 → /plan + /creative 强制路径）
  - V5 = ✅ **是 [安全相关]**（JS REPL / Hot Reload 路径 / 远程调试 port / Inspector 敏感数据 4 个威胁面）
- **触及技术债 4 项映射：** #26 LayoutBox.Dump → Inspector Layout / #35 UpdateManager frame hook → Performance Overlay / #40 C API introspection → Inspector 全子系统 / #44 QuickJS Debug API → 扩展候选 JS Debugger
- **前置验证清单：** 6/6 全 PASS（依赖 / 环境 / artifact / ctest 1062 基线 / FetchContent 跳过 / 待处理事项关联极强）
- **VAN push-back 5 项风险闭环：** 「6 子系统全做」陷阱 → V1=B 收敛 / 「规划」语义模糊 → V2=a 明确 / UI 渲染层选型 → V3=A 锁定 / Spec 主文档过长 → 12 段式样 + creative 拆分 / systemPatterns 重叠 → spec 阶段强制对照 ≥ 30 模式
- **下一步：** 路由到 `/plan` — 进入头脑风暴阶段（预期 D1-D6 决策矩阵 + spec 主文档 12 段式样 + ≥ 2 篇 creative）；不进入 `/build`（V2=a 纯蓝图，build 由用户基于产出 plan 拆出独立 build 任务）

</details>

## 上次任务（已归档闭环）

**TASK-20260430-03：全代码库 Code Review** — ✅ 已归档于 2026-05-01 ~00:30，已 `--no-ff` 合并到 main `2445990`（11 commits + 1 merge commit）。

- **归档文档：** `memory-bank/archive/archive-TASK-20260430-03.md`
- **核心成果：** R0 prep + R1 报告（55 findings / 6 维度归集）+ R2 P0 quick fix 6 项 + Reflect + Archive 全闭环；改进建议落实率 90%（P0 1/1 + P1 4/4 + P2 4/5）
- **plan ×0.6 第 16 数据点入库**：核心轮次 0.85-1.00× ×0.6 阈内 ✅
- **R3+ 13 项 P1 候选** 待用户决策拆分顺序后独立立项（详见 `docs/reports/2026-04-30-codebase-review.md`）

<details>
<summary>TASK-20260430-03 历史阶段快照（archive 已落盘，点开查看会话内进度记录）</summary>

### REFLECT 阶段产出快照（2026-05-01 ~00:08）

- **Reflection 主文档：** `memory-bank/reflection/reflection-TASK-20260430-03.md`（10 段 + 2 附录 / Level 4 全面回顾）
- **关键发现：**
  1. Background agent 双轨模式首次实战暴露并发会话切分支 race condition（reflog 显示 23:41/23:45 两次双切，R2.1 commit 混入 04 状态文件 + R2.3 改动丢失）
  2. plan ×0.6 估时模型有效（总 ~177 min / plan ×0.6 0.85-1.00× 阈内）— 第 16 数据点入库
  3. 任务类型决定 ×0.6 系数比复杂度更显著（review 0.4-0.7× / fast-fix 0.7-1.4× / racy 1.4-2.5× / 大件 0.8-1.2×）
- **改进建议 10 项（P0×1 / P1×4 / P2×5）：** 详见 reflection §5 + activeContext 待处理事项闭环段
- **systemPatterns 新增 5 段：** Background agent 双轨模式 + worktree 隔离协议 / plan ×0.6 任务类型分桶矩阵 / Quick fix 12 min/项基准 / Checkpoint 推荐默认协议 / Review 类 spec 模板
- **techContext 新增 3 段：** TASK-30-03 R1 55 项 findings 引用 + R2 已修复 6 项 + R3+ 13 项拆分；CMake basic vs full 矩阵；Background agent worktree 隔离工程指引
- **实测耗时：** ~30 min（reflection 文档 + 4 MB 文件更新 + 2 commit）

### BUILD R2 阶段产出快照（2026-04-30 ~24:55）

- **R2 全过 ctest 1062/1062 PASS**（基线 1061 + R2.5 新增守卫单测 = 1062；1 Wpt001 Skip 沉淀状态保持）
- **6 commits 链：**
  | # | Commit | Finding | 类型 | 行为变化 |
  |---|---|---|---|---|
  | R2.1 | `3b4b2e7` | F-020 selector_matcher dead return | 安全防御 | ❌ 注释 + VX_DCHECK |
  | R2.2 | `1467207` | F-033 ProcessEndTag isize 净化 | 重构 | ❌ 等价循环 |
  | R2.3 | `ddea78d` | F-040 rasterizer 阈值注释 | 文档 | ❌ 纯注释 |
  | R2.4 | `95ae814` | F-026 LayoutEngine arena thread_local | 线程安全 | ✅ static → thread_local |
  | R2.5 | `9c6ad5f` | F-053 image_decoder max_size 守卫 | 安全 | ✅ 新参数 + 守卫 + 单测 |
  | R2.6 | `668a9fe` | F-055 vx_version() configure_file | 维护 | ✅ hardcode → CMake gen |
- **执行环境：** 独立 git worktree `.worktree-03-r2`（隔离主 worktree 并发会话分支切换冲突，reflog 显示 23:41:23/30 + 23:45:08 双切）
- **新增文件：** `veloxa/api/version.h.in`（CMake configure_file 模板）
- **修改文件：** 6 文件（4 src + 1 header + 1 CMakeLists + 1 test）
- **实测耗时：** ~70 min（plan 55 min ×0.6=33 min；实测 1.27× plan / 2.12× ×0.6）；扣除 worktree 隔离 + 冲突修复 ~25 min 后 ≈ 0.82× plan / 1.36× ×0.6
- **关键观察（reflect 阶段 systemPatterns 候选）：**
  - 「并发会话切分支冲突」检测信号：activeContext.md 跨 commit 莫名变化 + git reflog 显示外部 checkout
  - 应对模式：git worktree 隔离 + 每 commit 前 `git symbolic-ref --short HEAD` 防御
  - background agent 模式（用户主线 04 + 我后台跑 03）= 双 worktree 设计，两轨独立演进

### BUILD R1 阶段产出快照（2026-04-30 ~24:39）

- **R1 报告主文档：** `docs/reports/2026-04-30-codebase-review.md`（11 段 / 55 项 findings / 6 维度归集 / R2 候选 6 项 / R3+ 13 拆分任务建议）
- **R1.1 H 层深读 ✅：** 实际深读 25+ 文件（含附带头文件），按 7 子系统 a-f 分批：
    - R1.1a CSS 5 文件 → 11 项 findings
    - R1.1b Layout 3 文件 → 5 项 findings
    - R1.1c DOM/HTML/App 4 文件 → 9 项 findings
    - R1.1d Rendering 3 文件 → 5 项 findings
    - R1.1e Script/Event/Update 4 文件 → 4 项 findings（含 F-046 真 P1 bug）
    - R1.1f Foundation/Text/Image/API 6 文件 → 11 项 findings（含 F-049/F-050/F-051 image 安全三件套）
- **R1.2 M 层 grep 一过 ✅：** static / memcpy / magic numbers / delete 4 模式扫描（5 个 delete 全部合理 → 嵌入式 arena 策略实施完美）
- **R1.3-R1.4 归集 + 分级 ✅：** 0 P0 + 28 P1 + 19 P2 + 8 P3
- **关键 finding（Top 5）：**
    1. F-051 P1 安全：JPEG decoder 默认 `error_exit` 调 `exit()` 杀进程
    2. F-050 P1 安全：PNG/JPEG `width × height` 无溢出检查（CVE 触发条件）
    3. F-046 P1 正确：EventDispatcher dispatch 中 listener mutation iterator 失效
    4. F-025 P1 正确：`LoadHTML` 不重置 `dom_bindings_` use-after-free
    5. F-022 P1 维护：CSS 属性元数据表缺失 shotgun surgery 跨 8+ 处（最大杠杆点）
- **R2 候选清单（6 项 quick fix / 55 min）：** F-020 dead return / F-026 thread_local / F-033 isize cast / F-040 阈值注释 / F-053 文件大小上限 / F-055 version configure_file
- **R3+ 拆分任务建议（13 项）：** image_decoder 安全 / DOM lifecycle / LoadHTML use-after-free / CSS 元数据表 / border shorthand / 现代选择器 / Layout 边角 / HTML5 实体表 / Rasterizer 性能 / 12 个低覆盖模块测试补充 / libpng 升级
- **实测耗时：** ~85 min（plan 150-200 min × 0.6 = 90-120 min；实测 0.50× plan / 0.78× ×0.6）→ reflect 阶段 plan ×0.6 第 16+ 数据点


### BUILD R0 阶段产出快照（2026-04-30 23:08）

- **R0 综合数据：** `docs/reports/2026-04-30-codebase-review-r0-data.md`（入库唯一持久副本）
- **R0.1 ctest 基线：** 1061/1061 PASS in 2.36s + 1 Skip（Wpt001 沉淀）✅
- **R0.2 fingerprint grep：** 18 项反模式 / 30 关键字覆盖 → veloxa/ 全代码库 0 TODO/FIXME/XXX/HACK + 0 危险 C 函数 + 0 throw + 0 不安全字符串转换
- **R0.3 lcov 覆盖率：** 行 85.4% / 函数 95.4% / 分支 57.6%；薄弱模块 12 项（image_decoder.cc 57.1% + rasterizer.cc 60.4% 是热点）
- **R0.4 CVE 审计：** 0 CRITICAL/HIGH（plan §4 D9 acceptance ✅）+ 5 Medium/Low；**关键发现**：libpng 1.6.37 命中 3 个 2026 新公布 Medium CVE，与 image_decoder.cc 57.1% 覆盖薄弱形成威胁链路（候选 P0 F-010）
- **R0.5 抽样 + commit：** R1 抽样名单 H 20 / M 80 / L 36 三层就绪 + R0 数据 commit 落地
- **候选 findings：** 14 项（F-001 ~ F-014 跨 6 维度），R1 验证 + 分级 + 写方案
- **实测耗时：** ~22 min（plan 90 min × 0.6 = 54 min 校准；实测 0.41× plan / 0.69× ×0.6） → reflect 阶段 plan ×0.6 校准协议第 16 数据点
- **工具补强：** OSV.dev 走 WSL2 → Windows Clash 代理 `172.22.32.1:7890`（沙箱直连 DNS 失败 → 宿主代理 fallback，候选 systemPattern）

### PLAN 阶段产出快照

- **决策矩阵 D1-D10：** B/C/B/B/spec/15/C/C/B/❌不用子代理
- **设计 spec：** `docs/specs/2026-04-30-codebase-review-design.md`（12 段 + T1-T10 安全威胁建模 + R0/R1/R2 多轮次协议）
- **实现 plan：** `docs/plans/2026-04-30-codebase-review.md`（10 段 + R0 + R1.1-R1.4 + R2 + Checkpoint 1/2 + 风险预案表）
- **抽样名单：** TOP-3 深扫 = foundation/{containers,strings} + script/ + core/{dom,html}（35 文件）；其他 4 子系统 grep 模式
- **工具链：** lcov 1.14 ✅ / gcov 11.4 ✅ / FetchContent 三处离线 ✅
- **估时：** plan ~6-10h / plan × 0.6 ~3.6-6h（上限 R0+R1+R2 封顶）
- **预期 plan × 0.6 数据点：** Level 4 review 类首数据点，预期落 0.5-0.6× 中位档

### VAN 阶段产出快照

- **决策矩阵 V1-V5：** A 全代码库 / all 6 维度 / C 完整实施（多轮次 + Checkpoint）/ D 混合视角 / ✅ 安全相关
- **策略 X：** R0+R1 必然（报告必产）/ R2 条件触发（P0 quick fix）/ R3+ 拆出独立后续任务
- **分支：** `feature/TASK-20260430-03-codebase-review`（基于 main `9411584`）
- **前置验证：** ✅ 全过（无新依赖 / `_deps/` 三处离线 / ctest 1061/1061 隐式继承 / 8 项 P3 候选作为 R1 输入材料）
- **Push back 决策：** 强制 R3+ 拆出 + Checkpoint 1/2 + Spec 主文档拆分附录（避免「样样不深」+「不可估时」+「主文档过长」3 风险）

</details>

## 最近归档完成

- **TASK-20260430-03：全代码库 Code Review（6 维度 × 7 子系统 + 多轮次 Build + Checkpoint）[安全相关]** — Level 4 ✅（2026-05-01）
  - 归档文档：`memory-bank/archive/archive-TASK-20260430-03.md`
  - 55 项 findings（28 P1 + 19 P2 + 8 P3）+ 6 项 P0 quick fix 实施 ctest 1062/1062 PASS + 13 R3+ 拆分任务建议
  - 改进建议落实率 90%（P0 1/1 + P1 4/4 + P2 4/5）；P0 #3 git symbolic-ref commit 守门 → `git-workflow.mdc` 落实；P1 #4 reflog 诊断 → `systematic-debugging.mdc` 落实
  - plan × 0.6 第 16 数据点入库（核心轮次 0.85-1.00× ×0.6 阈内 ✅）；首发 background agent 双轨模式 + worktree 隔离协议沉淀
  - feature 分支已 `--no-ff` 合并到 main `2445990`（合并发生于 04 任务推进过程中）
- **TASK-20260430-02：CSS border shorthand 补全（4 方向 + 3 属性级）[安全相关]** — Level 2 ✅
  - 归档文档：`memory-bank/archive/archive-TASK-20260430-02.md`
  - 22 新单测 + 双反向探针完整三态；ctest Debug 1061/1061 + Release 1030/1030
  - 3 改进建议全部闭环（P1 #1「极窄档 0.2-0.25×」+ P2 #2「Spec 描述粒度准则」+ #3 ROI 评估）→ systemPatterns.md
  - A8 ROI 验证 ✅：TASK-30-01 §0 升级规则首次外部任务 2/4 触发 + 触发部分均高/中 ROI
  - plan × 0.6 第 15 数据点 0.22× — 与 TASK-30-01 P6（0.21×）一同定型「极窄档」
- **TASK-20260430-01：first/last child margin collapse with parent（CSS 2.1 §8.3.1）** — Level 3 ✅
- **TASK-20260426-01：Layout 正确性消化（#25 + #28 + #20 + #21）** — Level 4 ✅
- **TASK-20260425-01：SDL2 窗口后端 + 输入事件桥接** — Level 3 ✅

## 说明

详细实现过程、测试细节与经验教训均已沉淀至各任务的 `archive-`* / `reflection-*` 文档。
