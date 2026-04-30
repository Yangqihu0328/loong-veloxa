# 进度记录

## 当前任务

**TASK-20260430-03：全代码库 Code Review** — Level 4 [安全相关]，**BUILD R1 完成**（2026-04-30 ~24:39），Checkpoint 1 等待用户决策。

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

## 最近归档完成

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
