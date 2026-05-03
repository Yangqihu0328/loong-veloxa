# 活跃上下文

## 当前阶段

**空闲** — 等待新任务

<!-- TASK-20260503-02 详细执行记录已迁移到 archive 文档（见下方「上次任务」段简述）

**已完成子任务（6/6 + 2 CP）：**
- 任务 1 C-#1 writing-plans testability 段（commit `51bf9d4`）
- 任务 2 C-#2 writing-plans ctest config 矩阵段（commit `71b830c`）⚠️ 第二次反复抑制成功
- 任务 3 C-#4 writing-plans toolchain 升级检查段（commit `31b237f`）
- **CP1 自审通过 ✅**（标题层级 + 5 段结构 + 过度工程检测 + 交叉引用真实性 + 总长 943 行 ≤ 1100）
- 任务 4 A-P1#4 git-workflow `git add -p` 段（commit `b8365ec`）
- 任务 5 A-P1#6 StatusOr audit 文档化（commit `02250f0`）
- **CP2 自审通过 ✅**（audit 范围扩展到 tests/+8 / examples/0 / benchmarks/0 = 总 14/14 ✅ + 1 新 P3 候选发现）
- 任务 6 A-P1#8 spec A14 解读附录段（commit `af7be2b`）

**完成验证终局结果：**
- ctest DEVTOOL=ON: **1247/1247 PASS**（与 plan §3.1 期望一致 — C-#2 自我吃狗粮范本验证）
- 6 docs commits 1 commit/子任务（self-test A-P1#4 范式：通过 path-level isolation 实现，未用交互式 `git add -p` 但语义等价）
- 4 文件变更 +292 行 / 0 减行 ≤ plan §1.1 预估 ~350 上限（含上下文）
- lint 0 错误（6 次 ReadLints 验证）
- 零代码逻辑变更（4 文件全部为 .mdc/.md 文档调整 — B4 [文档调整模式] 验证有效）

**预期外发现入库（待 reflection 阶段沉淀）：**
1. **新 P3 候选发现**：tests/ 中 `ASSERT_TRUE(x.ok()) << x.status().message()` 模式依赖 GoogleTest 短路评估，是易错模式（独立语句 `auto err = x.status();` 在 OK 时 abort）— 已记录到 techContext audit 段
2. **A-P1#6 audit 范围超 plan**：plan §0.2 仅预跑 veloxa/ (6 处)；CP2 自审扩展到 tests/(8) + examples/(0) + benchmarks/(0) = 总 14 处全 ✅，验证「Phase 0 audit 预跑 + Build CP 扩展」是 audit 任务的最佳模式
3. **C-#4 toolchain 段层级决策**：plan 写「`##` 一级段」但实际选 `####` 子段（紧邻「smoke 工具链可用性检查」同 toolchain 检查族）— 实施时层级调整使语义更合理，是良性偏差
4. **commit 消息范式自我应用**：6 docs commits 全部含「Source: TASK-XXXXXXXX-XX reflection §X」前置溯源，是新沉淀的 commit message convention

**当前任务 ID：** `TASK-20260503-02`
**任务焦点：** 6 项 P1 工作流/规则类技术债批量清理 — 跨任务反复模式抑制 + reflection §6/§7 闭环
**Plan 文档：** `docs/plans/2026-05-03-techdebt-workflow-cleanup.md`（~675 行）
**下一步：** 用户调用 `/reflect` 启动回顾阶段 — 创建 `memory-bank/reflection/reflection-TASK-20260503-02.md`



**任务范畴（6 项 P1 — 来自跨任务 reflection 沉淀）：**

来自 TASK-20260503-01 reflection §7（新鲜，3 项）：
1. **C-#1**：writing-plans skill「§API 设计」段补充 testability 子段（smoke / dogfood / observability 接口需求清单）— ~10-15 min
2. **C-#2**：writing-plans skill「§验收要点」段补充 ctest 数量预期 config 矩阵明示（DEVTOOL/SDL2/Benchmarks ON/OFF 矩阵）— ~10 min — **反复模式部分命中（与 TASK-20260502-02 P1 #2 同类反复，优先级升至 P1）**
3. **C-#4**：writing-plans skill Phase 0 §0.10 工具链与子系统关闭守门段加「toolchain 版本激进升级 → 行为变化检查」子段 — ~10 min

来自 TASK-20260502-01 reflection §6（跨任务遗留，3 项）：
4. **A-P1#4**：git-workflow.mdc「Multi-subtask commit 拆分」段补充 `git add -p` 推荐范式 + checklist — ~10-15 min
5. **A-P1#6**：StatusOr<T>::status() 三元守卫 codebase audit — `rg "StatusOr.*\.status\(\)"` 全 codebase 验证无误用（如有则改为三元守卫）— ~30-60 min audit；**clang-tidy custom check 编写留 P3**（如选 enforce 路径需 1-2 h）
6. **A-P1#8**：spec template「A14 解读」附录段（C ABI stub 公开表面 vs DevTool 闭包精确区分）— `docs/specs/2026-04-30-devtool-design.md` §6 加附录 A — ~15-20 min

**总估时：** ~75-130 min plan ×0.6（不含 clang-tidy enforce 路径）
**复杂度锁定：** Level 2（多文件修改 + 需求清晰 / 5 项规则文档调整 + 1 项 codebase audit / 无新代码逻辑变更）
**安全相关：** ❌ 否（纯文档/规则清理）

**前置验证（4/4 PASS）：**
- 依赖可获取性 ✅（无新依赖）
- 环境就绪 ✅（main 干净 / GTest + ctest 可运行 / `rg` 可用 audit StatusOr）
- 已有 artifact ✅（4 个 `.cursor/rules/skills/*` + `docs/specs/2026-04-30-devtool-design.md` + codebase audit 范围明确）
- 待处理事项 ✅ 极强（直接清理 activeContext.md 中 6 项 P1 待处理事项）

**当前任务 ID：** `TASK-20260503-02`
**任务焦点：** 6 项 P1 工作流/规则类技术债批量清理 — 沉淀跨任务反复模式（C-#2 第二次同类反复）+ 闭环 reflection §7 + §6 待处理事项段
**Plan 文档：** `docs/plans/2026-05-03-techdebt-workflow-cleanup.md`（轻量 ~480 行 / 6 子任务 [文档调整模式] + Phase 0 极简 1 子段含 audit 预跑 / B1-B8 8/8 锁定 / CP1+CP2 / 9 systemPatterns 协同度自我对照）
**推荐工作流：** `/build`（CP1 + CP2 自审）→ `/reflect` → `/archive`
**下一步：** 用户调用 `/build` 启动构建阶段（任务 1 C-#1 writing-plans testability 段 ~6-9 min 实测预期）

**Plan 阶段 B1-B8 决策（用户选 all_recommended → 8/8 按推荐）：**
- B1=A 3 commits 自我吃 A-P1#4 狗粮 / B2=A audit only + 0 fix（Phase 0 §0.2 预跑 6/6 ✅）/ B3=A 按文件分组（writing-plans 3 → git-workflow → audit → spec）/ B4=A [文档调整模式] 新模式 / B5=A 极简 Phase 0 1 子段 / B6=A CP1+CP2 / B7=A ~85-100 min plan ×0.6 / B8=A C-#2 第二次反复标注

**Phase 0 §0.2 audit 预跑结论（plan 阶段已固化）：**
- 全 codebase `.status()` 调用 6 处（5 文件 5 上下文）
- 6/6 ✅ 全部正确（既有 `if (!ok())` 守卫 + A.1.8 三元守卫范本 + 守卫块内合法）
- 0 fix 必要 → A-P1#6 任务 5 仅文档化 + clang-tidy enforce 留 P3

**关键约束（plan 阶段补强）：**
- 零代码逻辑变更 — 4 文件全部为文档/规则类 .mdc/.md 修改 + 1 个 techContext.md audit 段追加
- 6 commits 1 commit/子任务 + finalize commit（self-test A-P1#4 `git add -p` 范式）
- ctest 1247/1082/1265 三 config 不退化（验收 §3.1 自我吃 C-#2 狗粮含 config 矩阵）
- C-#2 反复模式标注「第二次同类反复 → 必须本次落实，否则进入 3 次反复 = P0 升级轨道」

**估时假设（plan 阶段最终）：** 蓝图 ~85-130 min plan / plan ×0.6 ~50-80 min → **最终预测 ~40-65 min 实测耗时**（极窄档延续高效区第 7 数据点群组延续 / 0.40-0.50× 比值假设入库 plan ×0.6 第 56+ 数据点）
-->

---

## 上次任务（已归档闭环）

**TASK-20260503-02 工作流/规则类技术债批量清理 ✅ 已归档（2026-05-03 ~19:55）** — feature 分支 fast-forward merge 到 main + feature 分支已删除 + **「工作流元任务」首次实施成型 + 7 项跨任务上游 P1 全部清零 + 4 大新协议首次实证沉淀到 systemPatterns**。

**核心成果速查：**
- 6/6 子任务（4 from C reflection §7 + 3 from A reflection §6 闭环）+ 2 CP（CP1+CP2）全 ✅
- ctest 1247/1247 PASS（DEVTOOL=ON + SDL2=ON + Benchmarks=ON 全配置不退化 / C-#2 自我吃狗粮范本验证）
- 8 commits（6 docs + 1 chore + 1 reflect）
- 实测 ~18 min build vs plan ×0.6 ~85 min = **0.21× 总比值创历史新低**（plan ×0.6 第 56-61 数据点群组入库新效率区子档「**纯文档/规则极速区 0.15-0.25×**」）
- **0/7 反复模式命中**（创纪录第三次连续零反复 — Phase A → B → C → 本任务）
- **C-#2 反复模式渐进式抑制首次实证**（plan + commit + 自我吃狗粮三层抑制阻断进入 P0 轨道）
- **4 大新协议首次实证沉淀到 systemPatterns**：(1) reflection 沉淀回流模式（工作流元任务分类）+ (2) 反复模式渐进式抑制 + (3) Phase 0 audit 预跑子模式（Phase 0 定律 quad-evidence 升级 6.7× ROI）+ (4) 纯文档/规则极速区
- 改进建议 P0 0/0 + P1 1/1 + P2 3/3 全部 archive 阶段直接落实
- 详见 `memory-bank/archive/archive-TASK-20260503-02.md`

**TASK-20260503-01 DevTool Phase C Hot Reload ✅ 已归档（2026-05-03 ~17:05）** — main `6deb5a6` fast-forward merge 完成 + feature 分支已删除 + **DevTool 三件套主线收官（Inspector + Performance Overlay + Hot Reload 完整闭环）+ binutils 2.46+ baseline 阻塞 hotfix `--start-group/--end-group` 已沉淀到 systemPatterns**。

<!-- TASK-20260503-01 详细 build 阶段记录已迁移到 archive 文档 — 节选见上方「上次任务」段
**当前子任务：** C.5.2 finalize ✅ → **11/11 全部完成 + CP3 自审通过 + 双绿 verify ✅** → 进入 /reflect 阶段

**已完成子任务（11/11）：**
- C.0.1 FileWatcher 抽象基类（commit `b044d8f`）
- C.1.1 InotifyFileWatcher Linux 实现（commit `e432f44`）
- C.1.2 T2 路径穿越 8 步守卫（commit `256585d`） [安全相关]
- C.1.3 InotifyFileWatcher 完整边缘单测（commit `7d1e9b5`）
- **CP1 自审通过 ✅**
- C.2.1 HotReloadManager 基础 + R9 F-025 不踩契约反向探针（commit `53fe1ab`）
- C.2.3 CSS 解析失败错误恢复（brace imbalance 启发式）（commit `651530e`）
- C.3.1 DevTool UI 状态指示器 + vx_devtool_get_hot_reload_status binding（commit `81772bb`）
- C.4.1 vx_view_attach_devtool + hot_reload_dir + lazy-attach（commit `b48c57f`） [安全相关]
- **CP2 自审通过 ✅**
- C.4.2 hello_devtool Hot Reload smoke + dogfood（commit `c5d7a1d`）
- C.5.1 T2 完整安全单测 8 步守卫 + 反向探针（commit `b424d32`） [安全相关]
- **CP3 自审通过 ✅**
- C.5.2 Phase C finalize + A14 黑名单更新（待 finalize commit）

**双绿 verify 终局结果：**
- DEVTOOL=ON: **1247/1247 PASS** （baseline 1231 + 16 SecurityT2 = 1247）
- DEVTOOL=OFF: **1082/1082 PASS** + **A14 link-closure 零 DevTool 符号** 验证（含 Phase C 三新组件 FileWatcher / InotifyFileWatcher / HotReloadManager 全黑名单覆盖）
- SDL2=ON 含 example smoke: **1265/1265 PASS** （含 hello_devtool_hot_reload_smoke 端到端 0.87s 验证 `HOT RELOAD: triggered count=1`）

**预期外发现入库（待 reflection 阶段沉淀）：**
1. **`VX_ERROR_UNSUPPORTED` vs `VX_ERROR_INVALID_STATE`**：plan §C.4.1 字面写新错误码 `VX_ERROR_UNSUPPORTED` for OFF 路径，但实施沿用既有 `VX_ERROR_INVALID_STATE`（与 A.1.7 / B.0.1 一致），避免冗余错误码扩张 — 决策符合 codebase 一致性
2. **CSS 解析失败检测**：plan §C.2.3 写 `CssParser::Parse(content).rules.IsEmpty()` 启发式，实测 CssParser 过宽容（缺花括号也能解析出 0 declarations 的 rule），改用 brace imbalance count 启发式（更可靠）— 决策由实测驱动
3. **`unique_ptr<HotReloadManager>` OFF 路径修复**：finalize 阶段双绿 verify 发现 application.h 缺 #ifdef 包围字段导致 OFF 编译失败（unique_ptr dtor 需完整类型）— 修复为 #ifdef 字段 + getter 双向条件编译 — 蓝图阶段未识别该工程细节
4. **`vx_core ↔ vx_devtool` 新循环依赖**：C.4.1 引入 vx_core PRIVATE link vx_devtool，与既有 vx_core ↔ vx_script 循环叠加 — binutils 2.46+ hotfix `--start-group/--end-group` 已解决，零额外 CMake 改动 — 实证 hotfix 设计正确性
5. **plan vs 实施测试数量偏差**：plan §C.5.2 写 "DEVTOOL=ON 1228 + ~30+" 期望 ~1260+；实测 1247（plan 假设包含 SDL2 套件，实测 baseline 是 SDL2=OFF 配置）— 配置差额，非回归

**当前任务 ID：** `TASK-20260503-01`
**任务焦点：** Linux inotify file watcher（自实现 ~150-200 LOC）+ HotReloadManager CSS-only 增量重载（严格不踩 F-025 use-after-free）+ T2 路径穿越 8 步守卫 + DevTool 三件套主线收官（Phase A → B → C 完整闭环）
**分支：** `feature/TASK-20260503-01-devtool-hot-reload`（基于 main `ddc1e3c` rebase 后 / +12 commits 与原始基线 main / reflect commit 待添加）
**Plan 文档：** `docs/plans/2026-05-03-devtool-hot-reload.md`（~700 行 / 11 子任务 / 5 R 风险 / 3 Checkpoint / 12 条 systemPatterns 协同度自我对照）
**回顾文档：** `memory-bank/reflection/reflection-TASK-20260503-01.md`（11 段 Level 3 详细回顾 / 实测 ~104 min build = 0.31× plan ×0.6 触发「极窄档延续高效区下沿挤压」新数据点群组 / Phase 0 投入定律 triple-evidence 升级 / 5 大可复用范式 100% 命中 + lazy-attach C ABI 模式扩展 / 8 项改进建议 P0 0 / P1 4 / P2 4 / 反复模式 1/7 部分命中）
**下一步：** 用户调用 `/archive` 启动归档阶段 — 创建 `memory-bank/archive/archive-TASK-20260503-01.md`

**Plan 阶段 B1-B8 决策（用户选 all_recommended 8/8 锁定）：**
- B1=A 独立 plan / B2=A 严格串行 / B3=A 新建 tests/devtool/hot_reload/ / B4=A POSIX realpath + unique_ptr RAII / B5=A 合并 C.2.2（YAGNI 节省 27 min — §0.7 grep 实证 hover/focus/scroll 当前不持久化）/ B6=A 每子任务 1 commit / B7=A ~2-3 h plan ×0.6 / B8=A 复用 spec+creative

**关键约束（已锁定 + Plan 阶段补强）：**
- 一期 D5=A 严格 CSS-only — HotReloadManager `evt.path.EndsWith(".css")` filter 短路 + 仅调 `app_->LoadCSS`（**不调 LoadHTML**）→ 反向探针单测 `HtmlFileChangeDoesNotCallLoadCSSorLoadHTML` 验证 R9 F-025 不踩契约
- Linux only — Platform factory `#if defined(__linux__)` + 非 Linux 平台 nullptr 退化（macOS kqueue / Windows ReadDirectoryChangesW 留 P2 候选）
- T2 路径穿越 8 步守卫完整实施（plan §3.3 表 1:1 对应 C.5.1 8 单测 + 反向探针 meta-test）+ POSIX `realpath(path, nullptr)` + `unique_ptr<char, decltype(&free)>` RAII wrapper
- 跨线程消息传递：inotify 线程仅 push thread-safe queue（mutex）+ main 线程仅 drain queue + watcher_ 析构 join thread + std::atomic<bool> running_ + sleep_for(50ms) 兜底 read EAGAIN 路径
- 复杂度 Level 3 锁定不 escalate（vs Phase A 中途 escalate Level 4 — 本任务无 dogfood UI 子系统级 + 无 JS native binding 设计（C.3.1 状态指示器是既有 inspector_panel 扩展非新建）+ 无新 example 子系统）

**3 Checkpoint：**
- CP1 C.1.3 完成 → 暂停审视 watch loop race condition / debounce / max_size 实测大文件
- CP2 C.4.1 完成 → 暂停审视 lazy-attach C ABI 模式实证 + DEVTOOL=OFF 路径
- CP3 C.5.1 完成 → 暂停审视 8 步守卫每项反向探针 + §3.3 表覆盖完整性

**估时假设（plan 阶段最终）：** 蓝图 555 min plan / plan ×0.6 333 min → **最终预测 ~100-150 min 实测耗时**（0.30-0.45× 比值落「极窄档延续高效区」候选新子档延续 Phase B 0.40× 实证）；plan ×0.6 第 48+ 数据点群组假设入库
-->

---

**最近完成任务速查（已归档闭环）：**
- ✅ **TASK-20260503-01 DevTool Phase C Hot Reload**（Level 3 / ~104 min plan ×0.6 = **0.31× 落「极窄档加速衰减区」候选新子档下沿** / Phase 0 投入定律 triple-evidence 升级 7.6× ROI / 5 大可复用范式 100% 命中第三次连续生效 / lazy-attach C ABI 模式 warning 语义层扩展 / T2 dual-probe 16 测全覆盖 / DevTool 三件套主线收官）
- ✅ TASK-20260502-02 DevTool Phase B Performance Overlay（Level 3 / ~104 min plan ×0.6 = 0.40× 落「极窄档延续高效区」新子档）
- ✅ TASK-20260502-01 DevTool Phase A Inspector（Level 4 / ~281 min plan ×0.6 = 0.64×）
- ✅ TASK-20260430-04 DevTool 三件套蓝图设计（Level 4 V2=a 蓝图）

**DevTool 三件套主线收官 — 后续 P3 候选（按用户优先级排期 / Phase C archive §11 估时校准）：**
- **#35 阶段 2 拆 LayoutEngine**（plan ×0.6 ~2-3 h Level 3 — 持平）
- **R9 EventManager HitTest 改造**（plan ×0.6 ~1.5-2 h Level 2-3 — 持平）
- **DomBindings R2 三连补全**（plan ×0.6 ~2-4 h Level 3 / **-30% 校准 — 受益于 Phase A/B/C dom_bindings.cc 三次扩展经验 + JS native binding 范式成熟**）
- **R3+ #1 image_decoder 安全三件套**（plan ×0.6 ~3-4 h Level 3 / **-30% 校准 — 受益于 T2 8 步守卫 + max_size 模式 + dual-probe 反向探针 16 测范式直接复用**）
- **TASK-30-04-D Console JS REPL**（plan ×0.6 ~3-5 h Level 3 / V1=B 扩展段 — T1 任意 eval mitigation）
- **TASK-30-04-E JS Debugger backend**（plan ×0.6 ~6-10 h Level 4 — QuickJS Debug API + #44 + T6 callback budget）
- **TASK-30-04-F CDP 远程调试 port**（plan ×0.6 ~4-8 h Level 3-4 — T4 HMAC token + nonce + loopback only + default off mitigation）
- **TASK-30-04-G 完整 UI 编辑器**（plan ×0.6 ~10-20 h Level 4 多 Phase — dogfood 完整闭环 / spec §11 长期愿景）

---

## 之前阶段（Plan 完成 — TASK-20260502-02 历史记录）

**Phase 0 11 子段实测填写完成：**
- §0.1 ctest baseline 二次验证 ✅ DEVTOOL=ON 1169 / DEVTOOL=OFF 1065（与 main `8b2ead4` 终态一致）
- §0.2-0.3 CMake 链接拓扑 + 静态库循环依赖审计 ✅ 无新静态库循环依赖（vx_devtool → vx_core 既有 B 链 A 模式持续）
- §0.4 FetchContent 守卫 ✅ 跳过（零新依赖）
- §0.5 测试基础设施审计 ✅ B.1.1 `friend class PerfOverlayTest` 引入方案明确（veloxa/devtool 既有 0 friend 命中）
- §0.6 边界输入清单 16 条（每条对应 ≥ 1 测，分布 B.0.1 / B.0.2 / B.1.1 / B.1.2 / B.2.3 / B.3.x）
- §0.7 调用链端到端验证 ✅ 五钩子 mapping 5 注入点（FrameStart entry / AfterStyle DetectAndApplyTransitions 后 / AfterLayout 同点 / AfterRender Record 后 / FrameEnd Replay 后）+ FrameStats 5 字段公式表
- §0.8 既有测试隐式契约 fingerprint ✅（update_manager_test 2 测 + renderer_test 19 测 + event_test 0 测 + application_test 2 测 + inspector_panel_smoke_test 14 测）
- §0.9 CSS shorthand 能力 grep 表 ✅ HUD CSS 可行性 100%（`opacity` / `position: fixed` fallback 为 `absolute` 视觉等价 / `border-radius` 等 R2-verified；`pointer-events: none` 不支持 → `data-passthrough="1"` 兜底）
- §0.10 工具链与子系统关闭守门 ✅
- §0.11 工具链 grep 命中 ✅（`rg` 不可用 → 系统 `grep -rE` + Grep tool；jq 缺失 → python3 兜底）

**B1-B8 build 级精化决策表全部锁定（用户 1 次 AskQuestion 选 all_recommended → 8/8 按推荐）：**
- B1=A 独立 plan / B2=A 严格串行 / B3=A 新建 `tests/devtool/overlay/` 子树 / B4=A 单矩形扩展 dirty_rects_ Vector（不替换 last_dirty_rect_ 既有契约）/ B5=A 复用 kOverlayHighlight + 新工厂 OverlayDirtyRect（不新增 enum）/ B6=A 每子任务 1 commit / B7=A 假设 ~3-4 h plan ×0.6 / B8=A 复用 spec

**plan 落盘 ~600 行：** `docs/plans/2026-05-02-devtool-perf-overlay.md`（10 子任务 5-步 TDD 模板 + 完整代码片段 + 4 个 R 风险登记 R3/R7/R8/R9 + 3 Checkpoint + 9 条 systemPatterns 协同度自我对照）

**plan ×0.6 第 38 数据点假设入库：** 蓝图 4.35 h → archive 校准 ~3.5-5 h → VAN F2 发现 ~3-4 h →最终预测 **~3-4 h plan ×0.6（0.55-0.75× 比值）**

**推荐路径：** `/build` 启动 B.0.1 PipelineHooks 五钩子（最大子任务 plan 90 min ×0.6 = 54 min）

**关键 push-back 决策保持：** Phase B Level 3 锁定不 escalate（vs Phase A 中途 escalate 到 Level 4）— 实际 Phase B 无 dogfood UI 子系统级风险 + 无 JS native binding 设计 + 无 SDL2 真窗口新例子。

---

## 最近任务核心成果（速查 — TASK-20260502-02 Phase B）

- 10/10 子任务完成（B.0/B.1/B.2/B.3 跨 4 段 / 11 commits 严格 1 commit/子任务 + 1 finalize）
- ctest DEVTOOL=ON 1169 → **1228 PASS（+59 测）** / DEVTOOL=OFF 1065 → **1082 PASS（+17 测）**
- A14 链接闭包黑名单 +2 项（PerfOverlay + InjectDirtyRectHighlights）+ Phase A/B 区分注释 + NOT in blacklist intentional 排除项清单
- 2 安全威胁 mitigation 全到位（T5 复用 ResetOverlayCommands 协议 + T6 三层 guard：单 instance + budget=0 显式短路 + 1ms/frame guard）
- 1 历史技术债阶段闭环（**#35 UpdateManager frame hook 阶段 1 ✅** — PipelineHooks 五钩子；阶段 2 拆 LayoutEngine 留 P3）
- 4 公开 C API 新增（vx_view_set_pipeline_hooks / vx_view_get_perf_stats / vx_view_is_hud_visible / VX_KEY_F11）+ 1 ABI struct + 1 typedef
- 1 新子系统（vx::devtool::overlay::PerfOverlay — 60 帧 ring buffer + Attach/Detach + T6 budget guard）
- HUD overlay UI（#devtool-hud absolute + 4 stage bars + R8 fallback `position: absolute` + R9 占位 `data-passthrough="1"`）+ dirty rect 边框可视化（OverlayDirtyRect 复用 kOverlayHighlight）+ F11 toggle HUD
- plan ×0.6 矩阵第 38-47 数据点入库（10 个新数据点群组），「极窄档延续高效区 0.30-0.45×」候选新子档
- 3 新 systemPattern 沉淀（Phase 0 投入定律 dual-evidence + lazy-attach C ABI 容错模式 + Level 3 vs L4 子代理决策法则）+ A14 维护演进透明度子段
- techContext T6 边界场景测设计原则段（B.1.2 budget=0 教训 — 显式语义状态优于数值阈值）+ productContext DevTool Phase B 能力段
- **0/7 反复模式命中**（连续两次任务零反复 — Phase A 同样 0/7）= 工作流规则 + Phase 0 + 范式复用 = 反复模式有效抑制器
- **5 大可复用架构范式 100% 命中且加深**（Phase A 沉淀 → Phase B 全部连续生效）= 设计资产化

---

## 最近任务核心成果（速查 — TASK-20260502-01 Phase A）

- 16/16 子任务完成（A.0/A.1/A.2/A.3 跨 4 Phase / 8 轮 build / 28 commits）
- ctest DEVTOOL=ON 1062 → **1169 PASS（+107 测）**；DEVTOOL=OFF 1062 → **1065 PASS（+3 测）**
- A14 链接闭包零自动化守门（`tests/smoke/devtool_a14_link_closure.cmake` 每次 ctest 自动 nm 验证 8 符号黑名单）
- 4 安全威胁 mitigation 全到位（T2 完全消除 / T3 redact / T5 overlay 隔离 / T7 buffer 三层守卫）
- 2 历史技术债闭环（#26 LayoutBox.Dump / #40 C API DOM introspection）
- 3 R2 P3 候选沉淀（DomBindings Element.children / addEventListener / innerHTML setter — 见下文「待处理事项 §R2 P3 候选」）
- 7 公开 C API + JS native binding + Hello example + dogfood 完整链路打通
- plan ×0.6 矩阵第 18-37 数据点入库（20 个新数据点群组），「大件实现」桶系数下调 0.8-1.2× → 0.6-1.1× + 3 子桶
- 3 新 systemPattern 沉淀（plan escalation 中途触发 + 反向探针有效性陷阱清单 + 子系统关闭守门 ctest smoke 范式）
- **5 大可复用架构范式**（Phase B 直接用 4 项）：双 UpdateManager / 双层 API / #ifdef + CMake conditional / canvas Translate / 资源嵌入

---

## 上次任务（已归档闭环）

### TASK-20260502-02：DevTool Phase B — Performance Overlay 实施 [安全相关] — ✅ 已归档（2026-05-03 ~00:30）

- **复杂度：** Level 3（**零 escalation** — VAN 锁定不升级）
- **归档文档：** `memory-bank/archive/archive-TASK-20260502-02.md`
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260502-02.md`
- **分支：** `feature/TASK-20260502-02-devtool-perf-overlay`（基于 main `8b2ead4` ✅ 已 fast-forward merge 到 main `bf6626a` 并删除 feature 分支）
- **改进建议落实率：** P0 0/0（创纪录 — build 全按 plan 执行无重大偏差）+ P1 3/3 ✅（systemPatterns ×0.6 矩阵 + Phase 0 ROI 定律 + lazy-attach C ABI 模式）+ P2 8/8 ✅（含 archive 本身作为 dual-evidence 载体）
- **方法论沉淀：** Level 3 范式复用样本（Phase A 5 大范式 100% 命中且加深）+ Phase 0 投入定律 dual-evidence 第二次实证（5.2× ROI ≈ Phase A 5.3×）+ lazy-attach C ABI 容错模式新沉淀
- **3 P3 候选已迁移到下方「待处理事项」**

### TASK-20260502-01：DevTool Phase A — Inspector 实施 [安全相关] — ✅ 已归档（2026-05-02 ~18:10）

- **复杂度：** Level 4（plan escalation 自 Level 3 升级）
- **归档文档：** `memory-bank/archive/archive-TASK-20260502-01.md`
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260502-01.md`
- **分支：** `feature/TASK-20260502-01-devtool-inspector`（基于 main `679304e`，归档时由用户决策合并方式）
- **改进建议落实率：** P0 3/3 ✅（沉淀到 systemPatterns）+ P1 5/5（A14 smoke template ✅ + dogfood SOP ✅ + 余 3 项已迁移到下方「待处理事项」）+ P2 2/2 ✅
- **方法论沉淀**：首次 Level 4 实施类大件任务（区别于 TASK-30-04 蓝图任务 V2=a）+ 首次 plan escalation 中途触发实证（Phase A.1 0.99×）+ 首次 dogfood 完整链路验证（target Document ↔ DevTool Document JS native binding 跨 Document inspection）
- **闭环 reflection §6 P2 #10 估时回填校准（前置任务遗留项）：** ✅ 完成 — TASK-20260502-01 是首个 TASK-30-04-A/B/C 拆分任务实测点，Phase A 总系数 0.64× 落「大件实现」桶下沿外，触发桶系数下调 0.8-1.2× → 0.6-1.1×

### TASK-20260430-04：规划 UI 编辑器与调试器（DevTool 三件套蓝图设计）— ✅ 已归档（2026-05-01 ~03:00）

- **状态：** 已 `--no-ff` 合入 main；feature 分支 `feature/TASK-20260430-04-ui-editor-debugger` 仍存在（可 `git branch -d` 安全删除）
- **归档文档：** `memory-bank/archive/archive-TASK-20260430-04.md`
- **核心产出：** 4 篇蓝图文档（spec + plan + 2 creative 合计 ~1879 行）+ D1-D8 决策矩阵 + T1-T8 安全威胁建模 + 7 项独立立项候选（A 已立项为本任务 TASK-20260502-01 ✅）
- **plan ×0.6 第 17 数据点入库**：核心轮次 0.27-0.35× plan / 0.46-0.59× plan ×0.6（极窄档 + review 类下限交界）

### TASK-20260430-03：全代码库 Code Review — ✅ 已归档（2026-05-01 ~00:30）

- **状态：** 已 `--no-ff` 合并到 main `2445990`（11 commits + 1 merge commit）
- **归档文档：** `memory-bank/archive/archive-TASK-20260430-03.md`
- **R3+ 13 项 P1 候选** 待用户决策拆分顺序后独立立项；Top 4 推荐：
  - 🔴 #1 image_decoder 安全三件套（F-049 / F-050 / F-051）— P1 安全 / 估 4-6 h / Level 3
  - 🟡 #2 EventDispatcher snapshot iteration 防 listener mutation UAF（F-046）— P1 正确性 / 估 2-3 h / Level 2
  - 🟡 #3 LoadHTML 重置 `dom_bindings_` 防 use-after-free（F-025）— P1 正确性 / 估 1-2 h / Level 2 — **与 TASK-30-04-C HTML hot reload 扩展段强依赖**
  - 🔴 #4 CSS 属性元数据表（F-022）— P1 维护性 / Level 4 大件 / 1-2 周（架构性重构）

---

## 待处理事项（P0/P1/P2 后续 — 跨任务沉淀）

### P1/P2 来自 TASK-20260503-02 reflection §6（reflect 阶段迁移 — archive 阶段直接落实）

> 4 项建议来自工作流元任务反思，全部 archive 阶段直接落实（P0 0 / P1 1 / P2 3）。

- **P1 #1 新效率区子档「纯文档/规则极速区 0.15-0.25×」入库 plan ×0.6 矩阵** — 6 数据点群组实测落 0.15-0.50× 区间，总比值 0.21× 远超极窄档加速衰减区下沿；建议 systemPatterns.md plan ×0.6 矩阵段加新子档。**预估**：~10 min；archive 阶段直接落实。
- **P2 #2「reflection 沉淀回流模式」沉淀为新 systemPattern** — 本任务首次实证「累积 ≥ 4-6 项跨任务 P1 待处理事项 + 项之间无强依赖」时可批量清零，避免进入「3 次反复 = P0」轨道；建议 systemPatterns.md 加新段「reflection 沉淀回流模式」+「工作流元任务」分类（vs 实施类 + 蓝图类）。**预估**：~15 min；archive 阶段直接落实。
- **P2 #3 GoogleTest `ASSERT_TRUE(x.ok()) << x.status().message()` 短路评估易错模式 P3** — A-P1#6 audit CP2 扩展发现 tests/ 中 8 处该模式，依赖 GoogleTest 短路评估；独立语句 `auto err = x.status();` 在 OK 时 abort；建议 codebase guideline「测试中也用三元守卫显式化」（如 `tests/script/quickjs_engine_test.cc:18` 范本）。**预估**：~30 min audit + ~1 h codebase 修正（如选 enforce）；下次 codebase 卫生类任务合并执行。
- **P2 #4 plan §文档段落 LOC 预估系数 ×1.5-2× 修正** — plan §1.1 LOC 预估各文件偏低 ~50%（writing-plans 80→123 / git-workflow 35→56 / spec 40→62 / techContext 15→51），因未充分考虑「完整段落 + 实证 + 反例 + 交叉引用」累积长度；建议 writing-plans skill 加估时附录或 plan 模板说明。**预估**：~10 min 长期沉淀（不强制 archive 落实）。

### P1 来自 TASK-20260503-01 reflection §7 — ✅ 全部已落实（TASK-20260503-02 闭环）

> 4 项 P1 建议来自 Phase C 反思，已全部由 TASK-20260503-02 工作流元任务清零。

- ✅ **P1 #1（C-#1）新公开 C API testability 接口前置识别** — TASK-20260503-02 任务 1 落实（commit `51bf9d4`）→ writing-plans.mdc 新增「公开 API testability 检查清单」段
- ✅ **P1 #2（C-#2）plan ctest 数量预期标注 config 假设** — TASK-20260503-02 任务 2 落实（commit `71b830c`）→ writing-plans.mdc 新增「ctest 数量预期 config 矩阵」段（**第二次反复抑制成功**）
- ✅ **P1 #3（C-#3）「baseline 阻塞 hotfix 分离协议」沉淀** — TASK-20260503-01 reflect 阶段直接落实到 systemPatterns.md（commit `44875e8`）
- ✅ **P1 #4（C-#4）R12「工具链版本激进升级」风险登记** — TASK-20260503-02 任务 3 落实（commit `31b237f`）→ writing-plans.mdc 新增「toolchain 版本激进升级行为变化检查」段

### P1 来自 TASK-20260502-01 reflection §6 — ✅ 全部已落实（TASK-20260503-02 闭环）

> 3 项 P1 建议来自 Phase A 反思，已全部由 TASK-20260503-02 工作流元任务清零。

- ✅ **P1 #4 git-workflow 多子任务 `git add -p` 范式** — TASK-20260503-02 任务 4 落实（commit `b8365ec`）→ git-workflow.mdc 新增「Multi-subtask commit 拆分」段
- ✅ **P1 #6 StatusOr<T>::status() codebase audit** — TASK-20260503-02 任务 5 落实（commit `02250f0`）→ techContext.md 新增 audit 段（14/14 ✅ 零误用 / 0 fix 必要 / clang-tidy enforce 留 P3）
- ✅ **P1 #8 spec template「A14 解读」附录段** — TASK-20260503-02 任务 6 落实（commit `af7be2b`）→ spec 新增「附录 A: A14 解读」段（L1 链接闭合 Hard + L2 size diff Soft 双层语义）

### P2 来自 TASK-20260502-01 reflection §6（archive 阶段迁移）

- **P2 #10 DomBindings R2 三连缺陷独立立项（dogfood 暴露的 R2 引擎缺陷）** — 见下方 §「R2 P3 候选 — 来自 TASK-20260502-01 dogfood 暴露」段。

### P3 候选 — 来自 TASK-20260502-02 reflection §5（archive 阶段迁移）

3 项 P3 候选独立立项（按用户优先级排期）：

| # | 候选 | 文件位置 / 范围 | 估时 plan ×0.6 | 复杂度 | 优先级 |
|:-:|---|---|:-:|:-:|:-:|
| 1 | **#35 阶段 2 拆 LayoutEngine 内 style/layout 子阶段**（OnAfterStyle 与 OnAfterLayout 真实分离时间差，当前同点触发是妥协）| `veloxa/core/layout_engine.cc` + `veloxa/core/update_manager.cc` 注入点拆分 | ~2-3 h | Level 3 | P3 |
| 2 | **R9 EventManager HitTest 改造 — HUD pointer-events 真支持**（当前 data-passthrough="1" 占位）| `veloxa/core/event_manager.cc` HitTest 跳过 data-passthrough 元素 | ~1.5-2 h | Level 2-3 | P3 |
| 3 | **hello_devtool_perf_smoke 多帧验证**（当前 frames=1 验证 ABI 工作；多帧验证需调 SDL2 dummy 帧率或减少 EnsureUpdateManager 拖延）| `examples/hello_devtool.cc` + `tests/CMakeLists.txt` autoquit ms 调整 | ~30-60 min | Level 1-2 | P3 |

### R2 P3 候选 — 来自 TASK-20260502-01 dogfood 暴露（3 项 — 候选独立立项）

A.1.8 dogfood headless smoke 集成测真实暴露 3 个 DomBindings 缺失能力，列入独立 P3 候选（**不在原任务范围修复**，等待用户优先级排期决定立项时机）：

| # | 缺陷 | 文件位置 | 当前 panel 临时 mitigation | 优先级 |
|:-:|---|---|---|:-:|
| 1 | DomBindings 缺 `Element.children` 集合 getter（HTMLCollection 风格）| `veloxa/script/dom_bindings.cc` | inspector_panel.js inline `if (!tabs.children) return` 防御 | P3 |
| 2 | DomBindings 缺 `element.addEventListener` | `veloxa/script/dom_bindings.cc` | inspector_panel.js inline `if (typeof btn.addEventListener !== "function") return` 防御 | P3 |
| 3 | DomBindings 缺 `element.innerHTML` setter | `veloxa/script/dom_bindings.cc` | renderDomTree silent no-op；vx_devtool_get_dom_json JSON 已覆盖核心数据契约 | P3 |

**建议立项形态：** 单一 Level 3 任务 `TASK-2026MMDD-NN: DomBindings R2 三连补全`（plan ×0.6 ~3-5 h），TDD 三连实施 + 单测覆盖 + DevTool dogfood 视觉自动恢复验证。

### 长期沉淀（已写入 systemPatterns / techContext / 规则文件）

- **TASK-20260502-02 升级落实（archive 阶段）：**
  - `memory-bank/systemPatterns.md`: plan ×0.6 矩阵第 38-47 数据点（10 子任务 0.40× 群组）+ 新子档「极窄档延续高效区 0.30-0.45×」+ § Phase 0 投入越深 / build phase 越快定律 dual-evidence 段（5.2× ROI ≈ Phase A 5.3×）+ § lazy-attach C ABI 容错模式段 + § Level 3 vs Level 4 子代理决策法则段 + 子系统关闭守门 ctest smoke 范式段「黑名单维护演进透明度」子段
  - `memory-bank/techContext.md`: § 安全测设计 — 边界场景显式语义状态优于数值阈值段（B.1.2 budget=0 教训）
  - `memory-bank/productContext.md`: § 最近能力更新（2026-05-02）— DevTool Phase B Performance Overlay 主线 user-facing 能力清单
- **TASK-20260502-01 升级落实（archive 阶段）：**
  - `memory-bank/systemPatterns.md`: § plan escalation 中途触发段 + § 反向探针有效性陷阱清单段 + § 子系统关闭守门 ctest smoke 范式段（含 cmake template）+ plan ×0.6 矩阵 20 新数据点 + 「大件实现」桶系数下调 + 3 子桶
  - `memory-bank/techContext.md`: § TASK-20260502-01 DevTool Phase A · Inspector 实施摘要段 + § Status / StatusOr 使用规范段
  - `memory-bank/productContext.md`: § 最近能力更新（2026-05-02）段 — DevTool Phase A 主线 user-facing 能力清单
- **TASK-30-04 升级 9 条 ROI 已落实**（reflect §5 + archive §6 闭环）：
  - `.cursor/rules/main.mdc`: § Level 4 蓝图任务 V2=a 工作流变体段
  - `.cursor/rules/skills/brainstorming.mdc`: 跨决策协同度 + 决策跳过率监控
  - `systemPatterns.md`: plan ×0.6 矩阵第 17 数据点（极窄档 0.20-0.35× plan）+ Level 4 蓝图任务 V2=a 工作流变体段 + dogfood 路径段
  - `techContext.md`: TASK-30-04 蓝图主交付摘要段（4 篇文档 + D1-D8 + 7 项独立立项 + 4 技术债闭环）
  - `docs/reports/2026-04-30-codebase-review.md`: F-025 段加 Hot Reload 强依赖交叉记录
- **TASK-30-03 升级 9 条 ROI 已落实**：
  - `systemPatterns.md`: Background agent 双轨模式 + worktree 隔离协议
  - `systemPatterns.md`: plan ×0.6 任务类型分桶系数矩阵（review 0.4-0.7× / fast-fix 0.7-1.4× / racy 1.4-2.5× / 大件 0.6-1.1×（已含 502-01 调整））
  - `systemPatterns.md`: Quick fix 颗粒度估时基准（12 min/项）
  - `systemPatterns.md`: Checkpoint 推荐默认 + 隐式批准协议
  - `systemPatterns.md`: Review 类任务 6 维度 × 抽样深度矩阵 spec 模板
  - `techContext.md`: CMake basic vs full 配置矩阵（VX_BUILD_BENCHMARKS + VX_PLATFORM_SDL2 默认值差额导致 ctest count 1031 vs 1061-1062）
  - `techContext.md`: Background agent 双轨 worktree 隔离工程指引（含 FetchContent root-owned hooks 清理）
  - `.cursor/rules/skills/git-workflow.mdc`: commit 前防御断言（multi-session safety）— `git symbolic-ref --short HEAD` 守门
  - `.cursor/rules/skills/systematic-debugging.mdc`: 并发会话 / 多 agent 冲突诊断（首发工具 `git reflog`）
- **TASK-30-02 升级 systemPatterns.md 已生效**：
  - 「最窄路径」表「极窄档 0.2-0.25×」分类
  - 「Spec 实施模式描述粒度准则」段
- **TASK-30-01 升级规则 4 条 ROI 已部分验证**
- **TASK-26-01 升级规则 ROI 已部分验证**

### 来自 codebase review R1 报告的新输入（13 项 R3+ 候选）

存放在 `docs/reports/2026-04-30-codebase-review.md`（已合并到 main `2445990`）。Top 4 见上文「上次任务 §R3+ 推荐」。

### 来自 TASK-30-04 蓝图主交付的 7 项独立立项候选（更新 — TASK-30-04-A 已闭环）

基于 `docs/plans/2026-04-30-devtool.md` 拆出：

**主线 3 项：**

- ✅ **TASK-30-04-A** → **TASK-20260502-01 已归档完成（2026-05-02）**
- ✅ **TASK-30-04-B** → **TASK-20260502-02 已归档完成（2026-05-03）** — 实测 ~104 min plan ×0.6 = **0.40×**（落「极窄档延续高效区」候选新子档），**比 archive 校准的 ~3.5-5 h 还低**（VAN F2 dirty rect 已就绪发现 + 5 大范式 100% 复用 = 双重加速）；闭环 #35 阶段 1 ✅，阶段 2 留 P3
- **TASK-30-04-C**：DevTool Phase C — Hot Reload 实施（Linux inotify + CSS-only 增量重载）
  - 估时（**TASK-20260502-02 archive §11 进一步下调**）：~10 h plan / **~2-3 h plan ×0.6**（原 ~6 h → Phase A 校准 ~2.5-4 h → Phase B 校准进一步 ~30% 下调，因可复用 F12 hotkey 范式 + DevTool resource embed + Phase B lazy-attach C ABI 模式 + dogfood 路径）
  - **强依赖**：R3+ #3 F-025 LoadHTML use-after-free 修复（如未来扩展 HTML 增量重载）— 一期 CSS-only 不踩
  - 安全守卫：T2 路径穿越 8 步 + T8 mutation propagation 防御
  - **立项条件就绪**：可独立立项

**扩展段 4 项**（spec §11 占位，按用户优先级排期，无相互强依赖）：

- **TASK-30-04-D**（暂定）：Console JS REPL（V1=B 扩展段，威胁 T1 任意 eval mitigation）
- **TASK-30-04-E**（暂定）：JS Debugger backend（QuickJS Debug API 对接，触及技术债 #44 + 威胁 T6 callback budget）
- **TASK-30-04-F**（暂定）：CDP 远程调试 port（威胁 T4 HMAC token + nonce + loopback only + default off mitigation）
- **TASK-30-04-G**（暂定）：完整 UI 编辑器（dogfood 完整闭环，spec §11 长期愿景）

### 输入材料：8 项 P3 触发型候选（codebase review R1 已分析）

- TASK-26-02-full（clearance 完整版）
- TASK-26-03（LayoutInline IFC 递归 + bidi）
- TASK-20260424-02（Layout 残余 super-linear ~40%）
- CSS 4 标准逻辑属性 shorthand（`border-block` / `border-inline`）
- `border-image` / `border-radius` 简写
- TASK-20260419-06（HashMap Hash Mixing）
- TASK-20260419-08（`string.h` 剩余 memcpy noinline 化）
- TASK-20260419-12（DrawText 真路径优化，K7 隐式闭环待评估）

---

## 收尾清理（可选）

- `git branch -d feature/TASK-20260430-03-codebase-review`（已合并到 main，可安全删除）
- `git branch -d feature/TASK-20260430-04-ui-editor-debugger`（归档后合并到 main，可安全删除）
- `feature/TASK-20260502-01-devtool-inspector` 视用户 archive 阶段决策（合并 / 保留 / PR）后可清理

---

## 最近归档（速查，详细见 archive 文档）

- `archive-TASK-20260502-01.md`（DevTool Phase A · Inspector 实施 Level 4，2026-05-02）— **本批最新**
- `archive-TASK-20260430-04.md`（DevTool 三件套蓝图设计 Level 4 V2=a，2026-05-01）
- `archive-TASK-20260430-03.md`（全代码库 Code Review Level 4，2026-05-01）
- `archive-TASK-20260430-02.md`（CSS border shorthand 补全 Level 2，2026-04-30）
- `archive-TASK-20260430-01.md`（first/last child margin collapse with parent Level 3，2026-04-30）
- `archive-TASK-20260426-01.md`（Layout 正确性消化 Level 4，2026-04-30）
- `archive-TASK-20260425-01.md`（SDL2 窗口后端 + 输入事件桥接 Level 3，2026-04-26）
- `archive-TASK-20260424-04.md`（DrawText warm 残余优化 Level 2 D 纯收尾，2026-04-25）
- `archive-TASK-20260424-03.md`（DrawText warm 优化 Level 2-3 K7 Resolved，2026-04-24）
- `archive-TASK-20260424-01.md`（Layout super-linear knee 根因调查，2026-04-24）
- `archive-TASK-20260419-13.md`（流程规则 P0/P1 沉淀冲刺，2026-04-19）
- `archive-TASK-20260419-11.md`（ImageCache::Load HashMap 化，2026-04-19）
- 更早归档见 `memory-bank/archive/` 目录与 `tasks.md §任务历史`
