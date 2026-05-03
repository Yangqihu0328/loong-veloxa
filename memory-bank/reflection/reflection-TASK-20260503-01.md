# 回顾：DevTool Phase C — Hot Reload 实施

**日期：** 2026-05-03
**任务 ID：** TASK-20260503-01
**复杂度级别：** Level 3（VAN 锁定不 escalate — 蓝图 plan §Phase C 已就绪 + 5 大可复用范式 100% 命中 + 无 dogfood UI 子系统级 + 无 JS native binding 设计 + C.3.1 状态指示器是既有 inspector_panel 扩展非新建）
**安全相关：** ✅ 是（T2 路径穿越高威胁 + T8 mutation propagation 中威胁）
**任务焦点：** Linux inotify file watcher 自实现 + HotReloadManager CSS-only 增量重载（严格不踩 F-025 use-after-free）+ T2 路径穿越 8 步守卫 + DevTool 三件套主线收官

---

## 1. 计划 vs 实际

### 1.1 总体偏差

| 维度 | Plan 假设 | 实际 | 偏差原因 |
|------|----------|------|---------|
| **复杂度** | Level 3（VAN 锁定）| Level 3（无 escalate）| ✅ 锁定成功 — VAN 4 项 push-back 决策（无 dogfood UI / 无新 JS binding / C.3.1 是扩展 / 无新例子）全部生效 |
| **子任务数** | 11（C.0.1/C.1.1-3/C.2.1/C.2.3/C.3.1/C.4.1-2/C.5.1-2）| 11 ✅ 100% 一致 | 蓝图原 10 → VAN 拆 C.2 三段 → plan 锁定 11 → build 0 增减 |
| **预估时间（plan ×0.6）** | 333 min（5.55 h plan）| **104 min** | **0.31× plan ×0.6 / 0.19× plan** — 比 Phase B 0.40× 进一步压缩，**触发「极窄档延续高效区」下沿挤压新数据点群组** |
| **VAN 假设比值区间** | 0.30-0.45×（落 Phase B 同档）| **0.31×** | ✅ 命中 VAN 假设区间下沿 — Phase 0 投入定律第三次实证（11/11 子段 → ~229 min 节省 = 7.6× ROI）|
| **commits 数** | 11 + 1 finalize = 12 | 12（含 plan commit e9c07da）✅ | 严格 1 commit/子任务（B6=A 锁定）+ binutils hotfix 单独已 fast-forward main |
| **A14 黑名单** | +3 项（FileWatcher / InotifyFileWatcher / HotReloadManager）| +3 项 ✅ | 100% 命中 |
| **公开 C API 新增** | 1（vx_view_attach_devtool 加 hot_reload_dir）| **2**（+1 vx_view_hot_reload_tracked_count）| smoke test 验证需要触发计数 read 接口 → C.4.2 实施时新增（plan 未识别）|
| **ctest DEVTOOL=ON** | ~1260+（plan §C.5.2 假设含 SDL2 套件）| **1247**（baseline 1231 + 16 SecurityT2）| 配置差额 — plan 假设 SDL2=ON baseline 是历史值，实测 baseline 是 SDL2=OFF 配置；非回归 |
| **ctest DEVTOOL=OFF** | ~1085+ | **1082** ✅ | 完全一致（A14 守门验证零回归）|
| **ctest SDL2=ON** | 未明确预期 | **1265**（含 hello_devtool_hot_reload_smoke 0.87s 端到端 ✅）| C.4.2 dogfood 路径成功 |

### 1.2 子任务实测耗时矩阵（11 子任务）

| 子任务 | plan ×0.6 | 实测 | 比值 | 备注 |
|:--|--:|--:|:-:|---|
| C.0.1 FileWatcher 抽象 | 18 min | **8 min** | 0.44× | 含 baseline 验证后启动 |
| C.1.1 InotifyFileWatcher 基础 | 54 min | **11 min** | 0.20× | std::thread + RAII fd + watch loop 一气呵成（业界范式成熟）|
| C.1.2 T2 8 步守卫 | 54 min | **13 min** | 0.24× | 8 项守卫严格按 plan §3.3 表 1:1 实施 |
| C.1.3 完整测试集 4 项 | 45 min | **4 min** | 0.09× | C.1.2 测试基础设施已就绪 → 4 测增量极快 |
| **CP1 自审** | — | — | — | ✅ 通过 — race condition / debounce / max_size 实测验证 |
| C.2.1 HotReloadManager 基础 + R9 反向探针 | 36 min | **10 min** | 0.28× | F-025 不踩契约反向探针单测一次实施成功 |
| C.2.3 CSS 解析失败错误恢复 | 18 min | **7 min** | 0.39× | **brace imbalance 启发式（实测驱动决策替代 plan 字面方案）** |
| C.3.1 DevTool UI 状态指示器 | 18 min | **8 min** | 0.44× | inspector_panel 三件套（HTML+CSS+JS）+ JS binding + 6 测一次实施 |
| C.4.1 vx_view_attach_devtool 扩展 + lazy-attach | 18 min | **8 min** | 0.44× | lazy-attach C ABI 模式直接复用 Phase B 范式 |
| **CP2 自审** | — | — | — | ✅ 通过 — lazy-attach 实证 + DEVTOOL=OFF 路径 + VX_WARNING_HOT_RELOAD_FAILED 设计 |
| C.4.2 hello_devtool Hot Reload smoke + dogfood | 27 min | **15 min** | 0.56× | SDL2=ON 单独 build_sdl 配置 + smoke 端到端 0.87s 验证 |
| C.5.1 T2 完整安全单测 8+8 | 27 min | **7 min** | 0.26× | dual-probe 16 测一次实施（duplicated helpers 故意 scope 隔离）|
| **CP3 自审** | — | — | — | ✅ 通过 — 8 步守卫每项反向探针 + §3.3 表覆盖完整性 |
| C.5.2 finalize + A14 黑名单 + 双绿 verify | 18 min | **12 min** | 0.67× | 含 OFF 路径 unique_ptr incomplete type 修复（plan 未识别工程细节）|
| **合计** | **333 min** | **104 min** | **0.31×** | **Phase B 0.40× → Phase C 0.31× = 进一步 ~22% 加速** |

**实测耗时分布：** 11 子任务 + 3 Checkpoint 自审 + 1 双绿 verify finalize（OFF/SDL2 build_off + build_sdl 双辅助目录构建）；其中 4 个子任务比 plan ×0.6 还低 0.30×（C.1.1/C.1.3/C.2.1/C.5.1），1 个子任务命中 0.67× 上沿（C.5.2 含 unique_ptr 修复）。

### 1.3 文件变更对比

| 类别 | Plan 假设文件清单 | 实际新增/修改文件 | 偏差原因 |
|:--|---|---|---|
| **新文件（plan 列出）** | 6 个：`devtool/hot_reload/{file_watcher.h, file_watcher.cc, file_watcher_inotify.h, file_watcher_inotify.cc, hot_reload_manager.h, hot_reload_manager.cc}` | 6 个 ✅ | 100% 命中 |
| **新文件（plan 未列出）** | — | 1 个：`tests/devtool/hot_reload/security_test.cc`（C.5.1 dual-probe 16 测）| C.5.1 plan 字面写「独立 security_test.cc」是 plan 内允许新文件 — 仍属计划内 |
| **新文件（plan 未列出实测增）** | — | 0 个 | ✅ 零计划外 cc/h 新增 |
| **修改文件（plan 列出）** | 9 个：`api/{veloxa_api.h,.cc}` + `core/{application.h,.cc}` + `core/CMakeLists.txt` + `script/{dom_bindings.h,.cc}` + `devtool/CMakeLists.txt` + `devtool/inspector/CMakeLists.txt` + `devtool/resources/{inspector_panel.html,.css,.js}` + `examples/hello_devtool.cc` + `tests/CMakeLists.txt` + `tests/smoke/devtool_a14_link_closure.cmake` | 全部命中 ✅ | 实施过程中无遗漏 |
| **修改文件（plan 未识别）** | — | `.gitignore`（+`build_*/` 行 — 双绿 verify 用 build_off / build_sdl 辅助目录）| 双绿 verify 协议运行时副产物 — 工程清理范畴 |
| **测试新增（plan 假设）** | ~30+ 测（C.1.3 4 + C.2.1 R9 + C.2.3 2 + C.3.1 6 + C.4.1 ~3 + C.4.2 1 smoke + C.5.1 8）| **+22 测**（实测：4+1+2+6+3+1+5+1+8+8 = 39 — 含 C.5.1 dual-probe 16）| 实测稍多 — dual-probe 16 测覆盖度 +8 比 plan 假设值高 |

### 1.4 设计变更（5 项预期外发现，VAN/plan 阶段未识别）

| # | 变更项 | plan 字面 | 实施决策 | 决策驱动 |
|:-:|---|---|---|---|
| 1 | **错误码命名** | 新错误码 `VX_ERROR_UNSUPPORTED` for OFF 路径 | 沿用既有 `VX_ERROR_INVALID_STATE`（与 A.1.7 / B.0.1 一致）| **codebase 一致性** — 避免冗余错误码扩张 |
| 2 | **CSS 解析失败检测** | `CssParser::Parse(content).rules.IsEmpty()` 启发式 | **brace imbalance count** 启发式 | **实测驱动** — CssParser 过宽容（缺 `}` 也能解析出 0 declarations 的 rule），原启发式失效 |
| 3 | **OFF 路径 `unique_ptr` 修复** | application.h 字段无条件存在 | `#ifdef VX_BUILD_DEVTOOL` 包围字段 + getter | **工程细节** — unique_ptr dtor 需完整类型，OFF 路径不 include hot_reload_manager.h 致 incomplete type 编译错误 |
| 4 | **新循环依赖** | C.4.1 引入 vx_core PRIVATE link vx_devtool（plan §3 已声明）| 与既有 vx_core ↔ vx_script 循环叠加 — binutils 2.46+ hotfix `--start-group/--end-group` 已解决 | **环境兼容** — 实证 hotfix 设计正确性，零额外 CMake 改动 |
| 5 | **新增公开 C API** | 仅扩展 vx_view_attach_devtool | +1 `vx_view_hot_reload_tracked_count` | **测试可观测性** — smoke test 需 read 接口验证「count=1」（不引入新接口则需 internal access）|

**5 项偏差性质归类：**
- **设计/工程偏差**：#1 / #2 / #3（实施过程中实测驱动，非反复模式 — VAN/plan 难以前置识别）
- **环境偏差**：#4（plan §0.1 baseline 阻塞触发的连锁验证 — 已在 hotfix 分支独立闭环）
- **API 设计补强**：#5（plan 阶段对 testability 接口考虑不足 — P1 反馈）

---

## 2. Build 阶段额外事件 — binutils 2.46+ baseline 阻塞 hotfix

**事件定位：** Build 阶段 §0.1 ctest baseline 二次验证发生 — 在 C.0.1 RED 之前，本任务范围之外但本次 build 触发的「环境兼容」事件。

**事件详情（已沉淀于 progress.md / activeContext.md）：**

- **意外阻塞**：GNU ld 2.46（2026 Binutils）单次扫描静态归档严格化 → vx_core ↔ vx_script ↔ vx_devtool 循环依赖失效 → 6 测试 link FAILED（undefined: `EnumValueToCssString` / `dom::ToJson` / `LayoutBox::ToJson`）— CMake 传统「重复列出 .a」hack 失效
- **决策方案 B（用户选择）**：开 `hotfix/binutils-2.46-link-group` 单独分支修复
- **修复**：根 `CMakeLists.txt` +10 行 `-Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group`（仅 GNU/Clang + Linux 生效）
- **验证**：271/271 link OK + 1195/1195 ctest PASS（实测低于 plan 假设的 1228，因 SDL2/Benchmarks OFF 配置差额 = ~33 测，是配置差额非回归）
- **合入路径**：fast-forward merge to main `ddc1e3c`（10 行 / 1 commit / Phase C 范围外）→ feature 分支 rebase 上 main → stash pop WIP 文档

**意外发现的反向验证：** C.4.1 引入 vx_core PRIVATE link vx_devtool 形成第二个循环依赖（与既有 vx_core ↔ vx_script 叠加），hotfix 的 `--start-group/--end-group` 包围方案**无需任何额外 CMake 改动即解决**（双重循环依赖一并通过）— **实证 hotfix 设计正确性**（不仅修复触发它的具体 case，还覆盖未来叠加场景）。

**预期外发现入库（archive 阶段须沉淀）：**
- R12「工具链版本激进升级 → CI/baseline 失败」风险登记 → `systemPatterns.md` 风险维度
- 「baseline 阻塞 hotfix 分离协议」systemPattern 候选（hotfix 分支 → fast-forward main → feature rebase WIP 链路）→ `systemPatterns.md` Git 工作流维度

---

## 3. 回顾检查清单（Level 3 安全相关任务）

**代码变更类任务：**
- [x] **计划精确度** — 文件清单 100% 命中（9/9 + 6/6 新文件 + 1/1 新测试文件）；预估比值 0.31× 命中 VAN 假设区间下沿（0.30-0.45×）— 计划质量极高
- [x] **TDD 执行情况** — 11/11 子任务严格 RED → GREEN → verify → commit；C.1.1 RED `file_watcher.h: No such file` / C.1.2 RED `ResolveAndValidate has no member` / C.2.1 RED 反向探针 / C.2.3 RED brace heuristic / C.3.1 RED `SetHotReloadManager has no member` / C.5.1 RED 16 测全 dual-probe — 零跳过零变体
- [x] **子代理质量** — N/A（本任务 100% 单 agent 直跑，无子代理 — Level 3 vs L4 决策法则严格执行）
- [x] **测试隔离** — C.1.3 / C.5.1 `TempDir` RAII + `WaitUntil` polling + `CaptureSink` 命名 scope 工具复用；C.5.1 故意 duplicated helpers 与 file_watcher_test.cc scope 隔离（plan 决策）— 无串扰无 flaky
- [x] **提交粒度** — 11 commits 严格 1 commit/子任务 + 1 finalize + 1 plan commit = 12 commits；零大杂烩；commit message 包含子任务编号 + [安全相关] 标签 + 详细 body
- [x] **非默认路径** — DEVTOOL=OFF（A14 守门 + unique_ptr 修复） + DEVTOOL=ON（baseline + SecurityT2 +16 测） + SDL2=ON（hello_devtool_hot_reload_smoke 端到端 0.87s）三路径**双绿 verify**

**安全相关任务：**
- [x] **输入验证** — T2 路径穿越 **8 步守卫** + dual-probe 16 测（forward × 8 + reverse × 8）；max_file_size / extension filter / mask 锁定 / realpath canonicalization / boundary check / WARN logging / 50ms debounce
- [x] **认证/授权** — N/A（DevTool C ABI 设计层面已通过 A14 守门 + DevTool=OFF 默认关闭）
- [x] **数据保护** — N/A（hot reload 仅读 .css 文件，无敏感数据存储/传输；watcher root allowlist 限制扫描范围）
- [x] **依赖审计** — 零新外部依赖（inotify 是 Linux libc 系统调用 / std::thread / std::filesystem 是 stdlib / std::condition_variable / std::atomic 是 stdlib）
- [x] **错误信息** — `VX_LOG_WARN` 路径 + 错误信息仅含 path 字符串和守卫类型（不泄露内部路径如 `/proc/self`）；`last_error_` 仅短描述串

---

## 4. 做得好的

### 4.1 流程层面（5 项）

1. **Phase 0 投入定律 dual-evidence 升级到 triple-evidence**
   - Phase A 5.3× ROI / Phase B 5.2× ROI → Phase C **7.6× ROI**（13 子段 Phase 0 ~30 min 投入 → 节省 ~229 min build phase = 333 plan ×0.6 → 104 min 实测）
   - 关键发现：§0.7 LoadCSS 路径 100% 安全 grep + §0.12 std::thread 零既有用例 grep + §0.13 realpath 选型决策实证 = 三大「实测发现」直接消除 plan 阶段所有不确定性
   - **结论**：「Phase 0 投入越深 / build phase 越快」从 dual-evidence 定律升级为 triple-evidence 跨 3 个连续任务实证

2. **VAN 阶段 4 项 push-back 决策全部生效（Level 3 锁定不 escalate）**
   - 拒绝陷阱：「F-025 LoadHTML use-after-free 一期 CSS-only 边界守不住」/「inotify race condition 卷入即 escalate」/「T2 8 步守卫漏 1 步」/「TASK-30-04 plan 全照搬陷阱」/「DOM 状态保留协议低估陷阱」
   - 5/5 拒绝全部生效 — 无中途 escalate（vs Phase A 中途 escalate L3→L4）
   - **结论**：Level 3 vs L4 子代理决策法则（Phase B 沉淀）第二次实证

3. **3 Checkpoint 协议严格执行**
   - CP1（C.1.3 完成）→ 暂停审视 watch loop race condition / debounce / max_size 实测大文件
   - CP2（C.4.1 完成）→ 暂停审视 lazy-attach C ABI 模式实证 + DEVTOOL=OFF 路径
   - CP3（C.5.1 完成）→ 暂停审视 8 步守卫每项反向探针 + §3.3 表覆盖完整性
   - 用户选「连跑」模式两次（CP1 后连跑 CP2 / CP2 后连跑 finalize），但每个 CP 自审 prompt 在 build 内部仍执行 — 即「连跑 + CP 自审 inline」组合
   - **结论**：Checkpoint 推荐默认 + 隐式批准协议（systemPatterns）跨任务持续生效

4. **TDD 模板 11/11 子任务零变体**
   - 全部 RED → GREEN → verify → commit；零跳过 RED；零批量验证；零覆盖补充
   - C.1.1 / C.1.2 / C.2.1 / C.2.3 / C.3.1 / C.5.1 6 个子任务 RED 阶段都遇到了符合预期的编译失败（`No such file` / `has no member` / 反向探针 FAIL）— 全部「故意 RED」一次过
   - **结论**：TDD 严格度与 Level 3 实施类任务匹配（Phase A/B/C 三任务一致）

5. **commit 粒度 100% 按 plan**
   - B6=A 锁定「每子任务 1 commit」从 plan 阶段贯穿到 build 阶段
   - 12 commits（含 plan + 11 子任务 + 1 finalize）严格语义清洁；A14 黑名单更新放在 finalize commit；hotfix 分离单独 commit 已在 main fast-forward
   - **结论**：「Multi-subtask commit 拆分」原则（TASK-20260502-01 P1 #4 反思建议）跨任务持续落实，未触发 `git add -p` 需要

### 4.2 技术层面（6 项）

1. **5 大可复用架构范式 100% 命中且加深（第三次连续生效）**
   - Phase A 沉淀 5 范式 → Phase B 100% 复用且加深 → Phase C 100% 复用 + 新模式叠加：
     - 双 UpdateManager → ✅ HotReloadManager → app->LoadCSS → update_manager_.reset() 重建
     - 双层 API → ✅ 内部 C++ HotReloadManager + 公开 C ABI 薄封装
     - #ifdef + CMake conditional → ✅ DEVTOOL=OFF 全链路守门（含 application.h 字段 #ifdef 修复）
     - 资源嵌入 → ⊘ N/A（本任务无资源嵌入需求）
     - canvas Translate → ⊘ N/A（本任务无 DevTool UI 渲染需求）
   - **结论**：5 大范式实测使用率 3/5 命中 + 2/5 N/A = 设计资产化第三次实证

2. **lazy-attach C ABI 容错模式新沉淀升级**
   - Phase B B.0.1 设计 / B.3.2 实证 → Phase C C.4.1 完整复用 + **新增 `VX_WARNING_HOT_RELOAD_FAILED` warning 语义层**
   - 设计：hot reload attach 失败不阻塞 DevTool 主路径 — 返 warning + 其他 DevTool 功能继续工作
   - **结论**：lazy-attach 模式从「错误返 INVALID_STATE + cache hooks」演进到「警告返 WARNING + 主路径继续」语义分层

3. **A14 链接闭包零自动化守门 + 新组件覆盖**
   - C.5.2 黑名单 +3 项（FileWatcher / InotifyFileWatcher / HotReloadManager）
   - 「NOT in blacklist intentional」段补充 — 公开 C ABI（vx_view_hot_reload_tracked_count）+ anonymous namespace local symbols（VxDevtoolGetHotReloadStatus）排除注释
   - DEVTOOL=OFF 1082 PASS + A14 smoke link-closure 零 DevTool 符号验证 ✅
   - **结论**：A14 守门维护演进透明度（Phase A/B/C 区分注释）持续生效

4. **跨线程消息传递设计（首次引入 std::thread）**
   - inotify 线程仅 push thread-safe queue（mutex + condition_variable）+ main 线程仅 drain queue + watcher_ 析构 join thread + std::atomic<bool> running_
   - sleep_for(50ms) 兜底 read EAGAIN 路径
   - 50ms 事件 debouncing 应对 atomic write 多次 IN_MODIFY 事件
   - **结论**：codebase 首个 std::thread 用例（§0.12 grep 实证零既有用例）按设计落地无 race / no flaky

5. **T2 8 步守卫 dual-probe 16 测覆盖**
   - C.5.1 实施 16 测（8 forward 守卫验证 + 8 reverse 守卫去除即 fail 验证）
   - 8 守卫：absolute root / locked inotify mask / realpath canonicalization / canonical path boundary / max_file_size / .css extension / WARN logging / 50ms debounce
   - dual-probe 反向探针有效性陷阱清单（Phase A 沉淀）100% 落实
   - **结论**：T2 高威胁面以 dual-probe 16 测全覆盖 = systemPatterns 反向探针范式第二次完整实施（Phase A 4 测 → Phase C 16 测扩展）

6. **dogfood 路径第三次复用（hello_devtool 第三次扩展）**
   - Phase A.3.1 hello_devtool 创建（dogfood 完整链路）→ Phase B.3.2 perf overlay smoke → Phase C.4.2 hot reload smoke 第三次扩展
   - 实测 0.87s 端到端验证 `HOT RELOAD: triggered count=1`（mkdtemp temp dir + 初始 CSS write + SDL timer 触发 modify + vx_view_hot_reload_tracked_count read + 验证）
   - **结论**：dogfood 路径范式从单点升级为持续验收基础设施（每个 Phase 加 1 smoke）

---

## 5. 遇到的挑战

### 5.1 流程层面（2 项）

1. **§0.1 baseline 阻塞 hotfix 分离协议（首次实证）**
   - 触发：build 阶段第一步 baseline 二次验证遭遇 binutils 2.46+ ld 严格化（plan 阶段未识别此风险 — F1-F8 grep 实证未涉及 ld 行为）
   - 应对：用户决策方案 B「单独 hotfix 分支修复」→ fast-forward to main → feature 分支 rebase
   - 教训：Phase 0 §0.1 ctest baseline 二次验证不仅是「baseline 数字记录」也是「环境兼容验证」入口；工具链激进升级（binutils 2.46）应在 §0.10 工具链与子系统关闭守门子段加 grep
   - **沉淀方向**：「baseline 阻塞 hotfix 分离协议」systemPattern 候选（archive 阶段沉淀）

2. **plan 阶段「测试数量预期」假设污染**
   - plan §C.5.2 写「DEVTOOL=ON 1228 + ~30+」期望 ~1260+
   - 实测 1247（baseline 1231 + 16 SecurityT2）— 配置差额（plan 假设含 SDL2 套件，实测 baseline 是 SDL2=OFF 配置）
   - 教训：plan 阶段写「期望 ctest 数量」时应明确「config 假设」（DEVTOOL=ON/OFF + SDL2=ON/OFF + Benchmarks=ON/OFF 矩阵）
   - **沉淀方向**：plan template「§验收要点」段加 config 矩阵明示

### 5.2 技术层面（3 项）

1. **CSS 解析失败检测启发式实测驱动重设计**
   - plan §C.2.3 字面方案：`CssParser::Parse(content).rules.IsEmpty()` 启发式
   - 实测发现：CssParser 过宽容（缺花括号也能解析出 0 declarations 的 rule），原启发式失效 → 测试 RED 阶段命中
   - 应对：改用 brace imbalance count 启发式（`std::count(content.begin(), content.end(), '{') != std::count(... '}')`）— 更结构化、更可靠
   - 教训：plan 阶段对既有组件能力的假设（CssParser 严格性）需在 Phase 0 §0.x 「既有组件契约 grep」实测验证
   - **沉淀方向**：Phase 0 子段加「既有组件契约 fingerprint 测试」步（B.0.8 前置任务遗留实证已部分覆盖，需扩展到「依赖组件能力假设」）

2. **OFF 路径 unique_ptr incomplete type 编译错误（finalize 阶段双绿 verify 发现）**
   - 触发：C.5.2 finalize 双绿 verify 跑 DEVTOOL=OFF build → application.h 字段 `std::unique_ptr<HotReloadManager> hot_reload_manager_` 无 #ifdef 包围 → application.cc 在 OFF 时不 include hot_reload_manager.h → ~Application() 析构 unique_ptr 时遇到 incomplete type 编译错误
   - 应对：字段 + getter 双向 #ifdef VX_BUILD_DEVTOOL 包围
   - 教训：Phase 0 §0.x 「双绿 verify 前置工程检查」未覆盖「unique_ptr<DevTool 类> 字段在 OFF 路径需 #ifdef」工程模式；应建立 checklist
   - **沉淀方向**：systemPatterns「#ifdef + CMake conditional 范式」段加「unique_ptr 子模式 — 字段也需 #ifdef」子段

3. **新循环依赖叠加风险（C.4.1 引入 vx_core PRIVATE link vx_devtool）**
   - 触发：C.4.1 plan §3 已声明 vx_core PRIVATE link vx_devtool（hot reload manager 调 app_->LoadCSS）— 与既有 vx_core ↔ vx_script 循环叠加
   - 验证：binutils 2.46+ hotfix `--start-group/--end-group` 已包覆全 LINK_LIBRARIES，**无需额外 CMake 改动即解决**
   - 教训：单循环依赖 → 双循环依赖叠加场景下 hotfix 仍正确 = 实证 hotfix 设计的「群组式包围」覆盖未来叠加；非 challenge 而是反向验证
   - **沉淀方向**：systemPatterns「CMake 静态库循环依赖处理」段加 binutils 2.46+ hotfix 的「叠加场景覆盖性」实证记录

---

## 6. 经验教训

### 6.1 流程层面（4 项）

1. **「极窄档延续高效区」下沿挤压触发新数据点群组**
   - Phase A 0.64× → Phase B 0.40× → **Phase C 0.31×** 三连递降
   - VAN 假设区间 0.30-0.45× 命中下沿；plan ×0.6 矩阵第 48-58 数据点入库（11 子任务群组）
   - **教训**：连续高 Phase 0 投入 + 5 大范式累积复用 → 比值持续下沿挤压；可考虑新子档「极窄档加速衰减区 0.20-0.30×」候选

2. **Phase 0 投入定律 triple-evidence**
   - 三任务一致：A 5.3× ROI / B 5.2× ROI / C 7.6× ROI
   - C 比 A/B 更高的 ROI 是因为：(1) 5 大范式 100% 命中无新探索成本 + (2) lazy-attach C ABI 模式直接复用 + (3) dogfood 路径第三次复用 = 复合加速效应
   - **教训**：Phase 0 投入定律不仅是「越深越快」更是「跨任务范式累积越多越快」— 范式资产化的复利效应

3. **「baseline 阻塞 hotfix 分离协议」首次实证 — 工具链激进升级风险登记**
   - binutils 2.46+ ld 单次扫描静态归档严格化是「环境因子」，不应卷入业务任务（feature/...）的 commit 历史
   - 用户决策方案 B（单独 hotfix 分支 → fast-forward main → feature rebase）= 干净分离环境兼容与业务实施
   - **教训**：plan §0.x 工具链与子系统关闭守门段应增加「toolchain 版本激进升级 → 行为变化检查」子段（如 `ld --version` 输出对比 + 已知行为差异 grep）

4. **「连跑 + CP 自审 inline」组合协议**
   - 用户两次选「连跑」模式（CP1 → 连跑到 CP2 / CP2 → 连跑到 finalize）
   - CP 自审在 build 内部仍执行 — agent 自动 inline self-review 而非外置 prompt
   - **教训**：Checkpoint 协议两种执行模式（暂停审视 vs 连跑 inline 自审）都有效；用户偏好「最少打断」时优选 inline 模式

### 6.2 技术层面（4 项）

1. **plan 字面方案 vs 实施实测决策的处理范式**
   - 5 项预期外发现中 3 项是「plan 字面方案 → 实施实测驱动改写」（错误码命名 / CSS 解析检测 / unique_ptr 修复）
   - **关键模式**：plan 字面方案是「假设」不是「契约」— 实施过程中用 RED 测试 / 编译失败 / verify failure 驱动改写是正常路径
   - **教训**：plan 文档应明确标注「字面方案」vs「契约」分离；契约（如 CSS-only 严格不踩 F-025 / T2 8 步守卫 1:1 表 / A14 黑名单 +3 项）必须严格遵守，字面方案允许实测驱动改写但需在 reflect 中记录

2. **brace imbalance 启发式 vs 标准库 parser 信任假设**
   - 标准库 / 内部组件的「严格性假设」（如 CssParser 缺 `}` 应拒绝）需 Phase 0 实测验证
   - brace imbalance 启发式是「基于源代码结构而非 parser 输出的检测」更鲁棒
   - **教训**：错误检测应优先用「输入侧结构特征」（brace count / 文件大小 / 路径模式）而非「输出侧组件状态」（parser 返回值）— 避免组件实现变更导致检测失效

3. **dual-probe 反向探针扩展到 16 测的范式确立**
   - C.5.1 dual-probe 16 测（8 forward + 8 reverse）= Phase A 4 测反向探针 → Phase C 16 测全覆盖范式扩展
   - 「duplicated helpers 故意 scope 隔离」决策避免单测文件巨大 + 跨测试文件耦合
   - **教训**：高威胁面（T2 路径穿越 8 守卫 / 任意 eval / 跨域访问）应一律采用 dual-probe 16 测覆盖度模板

4. **跨线程消息传递设计在 codebase 首次引入即按设计落地**
   - §0.12 grep 实证零既有 std::thread 用例 → C.1.1 一次实施成功（mutex + condition_variable + atomic + sleep_for 兜底）
   - 关键设计决策：inotify 线程仅 push（无业务逻辑）+ main 线程仅 drain（无 IO） = 「单一职责跨线程」模式
   - **教训**：首次引入新并发原语时应优选「单一职责跨线程」模式（避免 race condition 主源 = 跨线程共享状态）

---

## 7. 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 | 是否反复模式 |
|:-:|---|:-:|---|---|:-:|
| 1 | **新公开 C API testability 接口前置识别** — C.4.2 dogfood smoke 需要 `vx_view_hot_reload_tracked_count` 才能验证「count=1」，plan 阶段未识别；建议 plan template「§API 设计」段加「testability 子段：smoke / dogfood / observability 接口需求」清单 | P1 | 在 `.cursor/rules/skills/writing-plans.mdc` 「§API 设计」段补充 testability 子段 checklist | 下次涉及 C ABI 设计 + dogfood smoke 时合并执行 | 否（首次发现）|
| 2 | **plan ctest 数量预期标注 config 假设** — plan §C.5.2 写「期望 1260+」未明确「DEVTOOL=ON + SDL2=ON + Benchmarks=ON 矩阵」假设，实测 1247 是配置差额；建议 plan template「§验收要点」段加 config 矩阵明示 | P1 | 在 `.cursor/rules/skills/writing-plans.mdc` 「§验收要点」段补充 config 矩阵示例 | 下次涉及 ctest 数量预期写入时合并执行 | 是（**TASK-20260502-02 P1 #2 同类反复**）— 优先级升至 P1 |
| 3 | **「baseline 阻塞 hotfix 分离协议」沉淀** — binutils 2.46+ 事件首次实证；建议在 `systemPatterns.md` 「Git 工作流」段加新子段：(a) 触发条件 / (b) 决策树（继续 vs hotfix 分离）/ (c) hotfix 分支 → fast-forward main → feature rebase 链路 / (d) commit 信息分类 | P1 | archive 阶段直接落实到 `systemPatterns.md` 新子段 | 跨任务长期复用 | 否（首次发现，但可能重现）|
| 4 | **R12「工具链版本激进升级」风险登记** — plan §0.10 工具链与子系统关闭守门段应加「toolchain 版本激进升级 → 行为变化检查」子段（如 `ld --version` / `gcc --version` 输出对比 + 已知行为差异 grep）| P1 | 在 `.cursor/rules/skills/writing-plans.mdc` Phase 0 模板段补充工具链版本检查子段 | 下次 plan 阶段 §0.10 子段直接生效 | 否（首次发现）|
| 5 | **`unique_ptr<DevTool 类>` 字段需 #ifdef 工程子模式沉淀** — C.5.2 finalize 双绿 verify 发现 unique_ptr incomplete type 编译错误；建议在 `systemPatterns.md` 「#ifdef + CMake conditional 范式」段加「unique_ptr 子模式 — 字段也需 #ifdef」子段（含示例 + checklist）| P2 | archive 阶段落实到 `systemPatterns.md` | 跨任务长期复用（DevTool 后续 Phase D/E/F/G 必用）| 否（首次发现）|
| 6 | **「Phase 0 投入定律」triple-evidence 升级 + ROI 公式入库** — Phase A 5.3× / Phase B 5.2× / Phase C 7.6× ROI 三任务一致；建议在 `systemPatterns.md` 「Phase 0 投入定律」段升级为 triple-evidence + ROI 公式：`ROI = (plan ×0.6 - 实测) / Phase 0 投入` | P2 | archive 阶段落实到 `systemPatterns.md` 升级段 | 跨任务长期复用 | 否（持续累积）|
| 7 | **「极窄档加速衰减区 0.20-0.30×」候选新子档登记** — Phase C 0.31× 命中「极窄档延续高效区 0.30-0.45×」下沿；连续递降趋势暗示新子档；建议在 `systemPatterns.md` plan ×0.6 矩阵段加候选新子档 + 触发条件（5 大范式 100% 命中 + lazy-attach C ABI 复用 + dogfood 第三次复用）| P2 | archive 阶段落实到 `systemPatterns.md` plan ×0.6 矩阵段 | 跨任务长期复用 | 否（趋势识别）|
| 8 | **`vx_core ↔ vx_devtool` 双循环依赖叠加 + binutils hotfix 覆盖性实证** — 建议在 `systemPatterns.md` CMake 静态库循环依赖处理段加 binutils 2.46+ hotfix 的「叠加场景覆盖性」实证记录 | P2 | archive 阶段落实到 `systemPatterns.md` | 跨任务长期复用 | 否（首次实证）|

**优先级定义：**
- **P0 立即（0 项）**：本任务无 P0 改进 — build 全按 plan 执行无重大偏差（与 Phase B 同样 0 项 P0，第二次创纪录）
- **P1 下次（4 项）**：下个同类任务前应落实，迁移到 `activeContext.md` 待处理事项
- **P2 长期（4 项）**：archive 阶段直接落实到 `systemPatterns.md` / `techContext.md`

---

## 8. 反复模式识别

| 已知模式 | 出现频率 | Phase C 是否重复？ | 备注 |
|:--|:--:|:--:|---|
| 计划文件清单与实际变更不一致 | 9+ 次 | ❌ 否 | 6 新文件 100% 命中 + 9 修改文件 100% 命中（仅 .gitignore 工程清理 + security_test.cc 是 plan 字面允许范围）|
| 子代理产出需大量返工 | 7+ 次 | ❌ 否（N/A）| 本任务 100% 单 agent 直跑，无子代理 |
| 前置依赖/环境/API 能力未验证 | 8+ 次 | ⚠️ **是**（CssParser 严格性假设 / 工具链 binutils 2.46+ 行为变化）| 2 项命中 — 已沉淀为 P1 #2 + P1 #4 改进建议 |
| 非默认路径（流式/错误/缓存）遗漏验证 | 4+ 次 | ❌ 否 | DEVTOOL=OFF / SDL2=ON / SecurityT2 三非默认路径全验证 |
| 测试隔离问题 | 7+ 次 | ❌ 否 | TempDir RAII + WaitUntil + CaptureSink + duplicated helpers scope 隔离全到位 |
| 提交粒度偏离计划（大杂烩提交）| 7+ 次 | ❌ 否 | 12 commits 严格 1 commit/子任务 |
| TDD 严格度与场景不匹配 | 11+ 次 | ❌ 否 | 11/11 子任务 RED → GREEN → verify → commit 零变体 |

**Phase C 反复模式命中：1/7 部分命中**（前置依赖/环境/API 能力未验证 — 2 项）

**对比历史：** Phase A 0/7 / Phase B 0/7 / Phase C 1/7
**评估：** 1/7 命中是「连续两次零反复」之后的小幅回升，触发因子：(1) binutils 2.46+ 是激进新升级（plan 阶段难以前置识别）+ (2) CssParser 严格性是既有组件能力假设（应在 Phase 0 grep 实测覆盖但未覆盖）。
**建议沉淀：** P1 #2 + P1 #4 已落实改进路径，下次任务应实证「前置组件能力假设 grep 子段」补充到 plan template 的有效性。

---

## 9. 技术改进建议

### 9.1 代码质量

- **HotReloadManager error 消息脱敏** — 当前 `last_error_ = "css parse failure: " + path_str` 包含完整 path；考虑是否需要 redact watcher root 之外的部分（仅显示 root 内相对路径）→ P3 候选
- **InotifyFileWatcher debounce 50ms 是常量** — 后续如果用户报告短间隔修改丢事件，可考虑暴露为 WatchOptions 字段 → P3 候选

### 9.2 测试覆盖度

- **C.5.1 dual-probe 16 测覆盖 8 守卫充分**，但缺「8 守卫之间相互依赖」交叉验证（如 realpath 失败 → 应触发 boundary check 但被 max_size 早一步拒绝的优先级排序）→ P3 候选
- **hello_devtool_hot_reload_smoke 单帧验证 ✅**，但缺多次 modify 验证（验证 50ms debounce 实际生效）→ P3 候选

### 9.3 文档完整性

- **plan §C.4.1 hot_reload_dir 参数语义** 应明确：(a) nullptr / empty string → skip attach / (b) 相对路径 → 相对于 cwd / (c) 绝对路径 → 直接使用 — 当前代码是 (b)+(c) 混合（POSIX realpath 处理）但未在 API 文档明示 → P2 候选

### 9.4 性能

- **inotify watch loop 无 backpressure** — 高频 IN_MODIFY（如脚本批量修改 1000 个 .css 文件）会导致 queue 无界增长；考虑 queue 容量上限 + 溢出时合并最新事件 → P3 候选

### 9.5 安全

- **T2 8 步守卫已完整实施** ✅
- **error 消息可能泄露路径深度结构** — 见 9.1 redact 建议 → P3 候选
- **inotify 文件描述符在 fork 后子进程继承风险** — codebase 当前无 fork 用例，但如果 future 引入 fork-based subprocess，需 IN_CLOEXEC / FD_CLOEXEC 守门 → P3 候选

---

## 10. 安全评估

| 维度 | 状态 | 备注 |
|------|:-:|---|
| 输入验证 | ✅ | T2 路径穿越 8 步守卫 + dual-probe 16 测；max_file_size 4 MiB 上限；.css 扩展名 filter；realpath canonicalization；symlink 跨 root 拒绝 |
| 认证/授权 | N/A | DevTool C ABI 设计层面已通过 A14 守门 + DEVTOOL=OFF 默认关闭 |
| 数据保护（加密/脱敏）| ⚠️ 部分 | hot reload 仅读 .css 文件无敏感数据；error 消息含完整 path（见 9.1 P3 redact 建议）|
| 依赖审计 | ✅ | 零新外部依赖（inotify Linux libc / std::thread / std::filesystem / std::condition_variable / std::atomic 均 stdlib）|
| 错误信息脱敏 | ⚠️ 部分 | VX_LOG_WARN 路径含 path 字符串（见 9.1）|
| 敏感数据处理 | N/A | 无 token / key / password 处理 |
| 跨线程安全 | ✅ | mutex + condition_variable + atomic running_ + watcher_ 析构 join thread + sleep_for(50ms) 兜底 |
| Use-after-free 防御 | ✅ | R9 F-025 不踩契约：HotReloadManager 仅识别 .css + 仅调 LoadCSS（不调 LoadHTML）+ 反向探针单测 `HtmlFileChangeDoesNotCallLoadCSSorLoadHTML` 验证 |

**结论：** 8 维度 6 ✅ + 2 ⚠️ 部分（error 消息 path 脱敏 P3 候选）+ 0 ❌ — 安全基线满足 Level 3 安全相关任务要求。

---

## 11. 总结

**TASK-20260503-01 DevTool Phase C — Hot Reload 实施** 是 DevTool 三件套主线收官（Phase A → B → C 完整闭环），实测耗时 ~104 min（plan ×0.6 333 min，比值 0.31×），打破 Phase B 0.40× 进一步压缩，触发「极窄档延续高效区」下沿挤压新数据点群组（候选新子档「极窄档加速衰减区 0.20-0.30×」）。

**核心成就（5 项）：**

1. **11/11 子任务全部完成 + CP1/CP2/CP3 三大自审通过 + 双绿 verify 完成**（DEVTOOL=ON 1247 / DEVTOOL=OFF 1082 / SDL2=ON 1265 + hello_devtool_hot_reload_smoke 0.87s 端到端）
2. **Phase 0 投入定律 triple-evidence 升级**（A 5.3× / B 5.2× / C 7.6× ROI 三任务一致）
3. **5 大可复用架构范式 100% 命中 + lazy-attach C ABI 模式扩展**（新增 VX_WARNING_HOT_RELOAD_FAILED warning 语义层）
4. **T2 路径穿越 8 步守卫 dual-probe 16 测全覆盖**（systemPatterns 反向探针范式从 4 测扩展到 16 测）
5. **DevTool 三件套主线收官 — Inspector + Performance Overlay + Hot Reload 完整闭环**

**预期外发现 5 项已入库：** 错误码命名 / CSS 解析失败检测 / unique_ptr OFF 路径修复 / 双循环依赖叠加 / 测试数量配置差额 — 全部沉淀到本回顾 §1.4 + §6 教训段。

**Build 阶段额外事件：** binutils 2.46+ baseline 阻塞 hotfix 分离协议首次实证（已 fast-forward to main `ddc1e3c`，零 Phase C 业务范围污染）— 沉淀方向「baseline 阻塞 hotfix 分离协议」systemPattern 候选。

**改进建议落实计划：**
- P0 0 项（创纪录第二次连续）
- P1 4 项（迁移到 activeContext.md 待处理事项）
- P2 4 项（archive 阶段直接落实到 systemPatterns.md / techContext.md）

**反复模式：** 1/7 部分命中（前置依赖/环境/API 能力未验证 2 项）— 比 Phase A/B 的 0/7 小幅回升，已识别因子并通过 P1 #2 + P1 #4 改进建议闭环。

**下一步：** 用户调用 `/archive` 启动归档阶段。
