# 回顾：修复 main Release `-Werror` 编译失败（2 处）

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-07
**复杂度级别：** Level 1
**分支：** `feature/TASK-20260419-07-release-werror-fixes`（3 commits ahead of main `b321482`）
**关联：** TASK-20260419-03 P6 fresh build 副发现；与 TASK-20260419-04 同根（GCC IPA 误报）

---

## 1. 计划 vs 实际

| 维度 | VAN 阶段预估 | 实际 | 偏差原因 |
|------|------|------|---------|
| 文件变更 | `memory_surface_test.cc` + `string.h` | ✅ 一致（2 文件，+9/-3） | — |
| 修复方案 (a) | A1 `ASSERT_NE(fgets,nullptr)` | ✅ 一致 | — |
| 修复方案 (b) | **B3 `__builtin_memcpy`** 推荐 | ❌ 改为 **B2 `[[gnu::noinline]]`** | B3 假设根因 = `__memcpy_chk` fortify wrapper，**未先验证** GCC `-Warray-bounds` 的诊断阶段；B3 试验失败后才确认 IPA 在中端发出，先于 fortify 展开 |
| 验证范围 | Release 单 target + Debug ctest 全量 | ✅ 一致（额外加了 7 bench sanity） | — |
| 时间 | Level 1 预期 < 30 分钟 | ~25 分钟（含一次方案回退） | 仅多 1 次 build round-trip |
| 提交数 | 2-3 | 3（2 fix + 1 MB 收尾） | 一致 |

**核心偏差**：方案选择被一次 build round-trip 修正。无返工浪费（修改隔离在单文件，回退一次即可），但暴露了**方案表设计的根因验证空白**。

---

## 2. 回顾检查清单（代码变更类）

- [x] **计划精确度** — 文件清单 100% 一致；方案候选 2/3 命中（A1 + B2），B3 是预期内误差
- [x] **TDD 执行** — 编译错误本身充当 RED；GREEN 通过单 target build + 单元测试双重验证；REFACTOR 不需要（修改极小）
- [n/a] **子代理质量** — 本任务未使用子代理（Level 1）
- [x] **测试隔离** — 修改 `memory_surface_test.cc` 仅强化已有断言（之前隐式依赖后续 EXPECT_STREQ 兜底），无新增 flaky 风险
- [x] **提交粒度** — 2 个 fix 各自独立 commit，按修复目标分离；MB 收尾另起 1 commit；body 含完整诊断+决策链
- [x] **非默认路径** — fgets 失败路径之前是「悄无声息」，新方案通过 ASSERT_NE 把异常路径明确化
- [x] **代码审查（自查）**：
  - `[[gnu::noinline]]` 用 `#if defined(__GNUC__) && !defined(__clang__)` 守卫，clang 不受影响 ✅
  - `string.h` 仅改动拷贝构造（非热路径，move ctor 不受影响）✅
  - commit body 留有未来 GCC 升级回归的迁移指南 ✅

---

## 3. 做得好的

1. **VAN 阶段「无需 FetchContent」预判准确** — 显式注明 `build-bench/` 复用路径绕开 git proxy，避免了已识别的 P0 反复模式（TASK-03 P6 第 9+ 次踩坑），本任务零相关时间损耗
2. **错误实地复现先于编码** — VAN 末尾 `cmake --build build-bench --target string_test -j` 跑通后才进入修复，确保「修了什么」与「报了什么」同源
3. **提交 body 信息密度** — `51d6ff1` body 完整记录了 B3→B2 的决策链（probe/reject/accept），未来阅读者无需翻 reflection 即可理解为何不用 `__builtin_memcpy`
4. **Release + Debug + bench sanity 三重验证** — 同时确认了「修了原问题 + 没破坏 main 线 + 不影响 TASK-03 baseline」
5. **副发现主动记录** — 注意到 string.h 还有 3 处同根 runtime-size memcpy（line 45/150/230）当前未触发但共享架构风险，commit body + tasks.md 都留下了未来回归的迁移指南，避免「现在能跑就忘」

---

## 4. 遇到的挑战

### 4.1 B3 `__builtin_memcpy` 假设失败（**关键教训**）

**预期**：替换 `std::memcpy` → `__builtin_memcpy` 绕过 `__memcpy_chk` fortify wrapper，消除 IPA 对 fortify chk 路径的对象大小推导。

**实际**：`__builtin_memcpy` 同样触发 `error: ‘void* __builtin_memcpy(...)’ forming offset [32, 41] is out of the bounds [0, 32]`。差别只是错误消息函数名从 `__builtin___memcpy_chk` 变成 `__builtin_memcpy`。

**根因诊断（事后）**：
- GCC `-Warray-bounds` 由**中端 IPA pass** 发出（具体是 `-fipa-modref` + range derivation）
- IPA 内联 `BasicString` 拷贝构造 → 内联 `other.data()` → 看到该函数可以返回 `&storage_.sso.data[0]`（对象内 pointer）→ 把 `other.data()` 的可能上界用 `&storage_ + sizeof(Storage)` 推导
- 当 `sz=42`（`String b(a)` 测试值），IPA 算出 `data() + sz` 的偏移 [32, 41] 超出 [0, 32]
- 这一推导**完全独立于** `__memcpy_chk` 的 `__glibc_objsize0`；fortify wrapper 只是把已经成立的 IPA range 串到诊断消息里
- 改用 `__builtin_memcpy` 跳过 fortify 但 IPA 已经先发出诊断 → 同样失败

**B2 为什么有效**：`[[gnu::noinline]]` 阻止 GCC 把拷贝构造内联到测试函数体，IPA 只看到 noinline 函数边界 → `other.data()` 的范围在调用点不可见 → IPA 无法把"对象 size 32 字节"与"runtime sz=42"关联 → 误报消失。这与 TASK-04 detemplatize 是**同一元模式**：阻断 IPA 跨函数/跨实例化关联。

### 4.2 方案表设计的根因验证空白

**问题**：VAN 阶段我把 B3 描述为「**根因消除** + 防回归」，把 `__memcpy_chk` 当根因；这是基于错误消息表面（`__builtin___memcpy_chk` 字样）的快速假设，没有先做 1 行验证（`grep -B5 'array-bounds' include/x86_64-linux-gnu/bits/string_fortified.h` 即可看出 chk 只是诊断消息的"展示层"）。

**代价**：1 次 build round-trip（~2s 编译 + 30s 决策思考）。本次因为修改隔离在 1 个文件 + 单 target build 极快，回退成本低；但若未来涉及多文件 / 长 build 任务，类似假设失败可能放大数倍。

**反复模式核对**（参照 reflect 步骤 3.5 表）：
- ❌ 不匹配「计划文件清单与实际变更不一致」
- ❌ 不匹配「子代理产出需返工」
- ⚠️ **匹配「前置依赖/环境/API 能力未验证」** — 不是依赖未验证，是**根因诊断未验证**就推荐方案。属于该模式的子类「方案-根因绑定假设未验证」。本次首次明确识别为新子类。

---

## 5. 经验教训

### 5.1 GCC `-Warray-bounds` 修复方法论（**新模式**）

诊断这类警告时**先定位发出阶段**再选方案：

| 发出阶段 | 特征 | 适用方案 |
|---------|------|---------|
| **前端 / 解析期** | 数组字面量定义点的明显越界 | 改下标 / 改大小 |
| **中端 IPA** | 错误消息含「forming offset / out of the bounds of object」+ 在调用点而非定义点报错 | **阻断 IPA 关联**：`[[gnu::noinline]]`、去模板化、隐藏指针来源 |
| **后端 fortify** | 错误消息含 `__memcpy_chk` / `__strncpy_chk` 字样 + `__glibc_objsize0` | `__builtin_xxx` 绕过 fortify wrapper |

**判定方法（30 秒）**：
1. 查错误消息函数名 — 含 `_chk` 后缀 → fortify；不含 → 中端 IPA
2. 查报错位置 — 在 dst/src 定义点 → 前端；在调用点（且 dst/src 来自上层调用）→ IPA
3. **试做 1 步验证**：把 `std::memcpy` 改 `__builtin_memcpy`：消息变为 `__builtin_memcpy` 且仍报 → 100% IPA；消息消失 → fortify

### 5.2 GCC IPA 误报的「阻断关联」元模式（横向归纳）

**TASK-04 + TASK-07 是同一元模式的两个实例**：

| | TASK-04 | TASK-07 |
|---|---------|---------|
| 触发器 | `template<usize N>` clone 跨 N 值 IPA 传播 | 拷贝构造内联到调用点后 IPA 关联对象 size |
| 阻断手法 | 去模板化（`std::size()` 运行时取大小） | `[[gnu::noinline]]`（边界化函数视野） |
| 共同本质 | **强制 IPA 在某个边界停止跨函数推导** | 同左 |

**适用判定**：当看到 GCC `-Warray-bounds` / `-Wstringop-overflow` / `-Wstringop-overread` 等"中端范围派生"警告，先选「阻断关联」方案族（noinline / detemplatize / 隐藏指针来源），再选「警告抑制」方案族（pragma / `-Wno-`）。前者治本，后者治标。

### 5.3 方案表必须附「根因验证步骤」

VAN / BUILD 阶段写候选方案表时，每个方案应附**一行根因验证操作**（grep / 读 1 个头 / 跑 1 个 build），而不是直接靠错误消息表面假设根因。本次若 B3 行附「先 grep `string_fortified.h` 确认 `__memcpy_chk` 是诊断展示层还是诊断源头」，可在 30 秒内排除 B3。

### 5.4 副发现的强制记录习惯继续有效

`string.h` 行 45/150/230 共享根因但当前未触发 — 在 commit body 留迁移指南、在 tasks.md 留待跟踪条目，是 TASK-03 反思 P2 #「Fresh build 副发现强制记录」的延续应用。本次进一步证明该模式低成本高回报。

---

## 6. 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | **GCC `-Warray-bounds` 修复方法论 3 阶段诊断表**沉淀到 `systemPatterns.md` | P1 | `systemPatterns.md` 加新段「GCC `-Warray-bounds` 误报诊断与修复模式」，含 §5.1 表格 + §5.2 元模式 + 双任务实例链接 | `systemPatterns.md` |
| 2 | **方案表附「根因验证步骤」要求**沉淀到 `writing-plans.mdc` | P1 | `writing-plans.mdc` 加段「候选方案表要求 — 每方案 ≤ 30 秒根因验证操作」 | `.cursor/rules/skills/writing-plans.mdc` |
| 3 | **`string.h` 剩余 3 处 runtime-size memcpy 防御性 noinline 化** — 已在 commit body 注明，但未立项 | P3（防回归，触发条件：未来 GCC 升级回归同类警告） | 触发时立 Level 1 task；目前不主动改（避免引入不必要的内联开销） | tasks.md 待立项候选区 |
| 4 | **「方案-根因绑定假设未验证」识别为反复模式新子类** — 加入 `/reflect` 反复模式表 | P2 | `reflect.md` 命令文档 §3.5 表加新行「方案根因假设未先验证」，频率初始 1 次（本次首识别） | `.cursor/commands/reflect.md` |
| 5 | **TASK-04 ↔ TASK-07 横向同源链接**写入两份归档之间的「相关任务」段 | P2 | TASK-07 archive doc 「相关任务」段引用 TASK-04；TASK-04 archive 暂不回填（保持归档不可变性） | TASK-07 archive |

---

## 7. 技术改进建议

1. **noinline attribute 性能影响实测（可选）**：本次基于「拷贝堆 String 必伴随分配（~1-2ns indirect call ≪ allocator 开销）」的理论判断未做实测。若未来 String copy 进入热路径，可补 1 个 `BM_StringCopyHeap` 微基准量化（当前 `bench_strings.cc` 主要测 SSO，无 heap copy 用例）— 列入 TASK-05 候选 / TASK-06 替代任务清单
2. **在 CI 加 Release `-O2 -Werror` 通路**（与 TASK-04 反思 P1 #1、TASK-03 反思 P1 同源）：本任务是 main 上的 -Werror 失败被 TASK-03 fresh build 偶然发现的第 2 个实例。若 CI 有 Release 构建路径，这两个错误会在合入时立即拦截而不是潜伏到下次 fresh rebuild。已是反复 P1 项，本次不重复升级，仅再次复确

---

## 8. 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | 修改 1 行 attribute + 3 行 ASSERT_NE 包装，无外部输入 |
| 认证/授权 | N/A | |
| 数据保护 | N/A | |
| 依赖审计 | N/A | 无新依赖 |
| 错误信息脱敏 | N/A | |
| 敏感数据处理 | N/A | |

**结论**：本任务不涉及安全变更。

---

## 9. 关键发现总结

1. **B3 假设失败暴露方法论盲点** — GCC `-Warray-bounds` 在 IPA 中端发出，先于 fortify 展开，`__builtin_memcpy` 不绕过 IPA。诊断阶段判定先于方案选择
2. **TASK-04 ↔ TASK-07 是 GCC IPA 误报「阻断关联」元模式的两个实例** — 不同手法（detemplatize vs noinline）背后是同一元原理；可固化为 `systemPatterns.md` 可复用判定表
3. **VAN「无需 FetchContent」预判完美绕开 P0 反复模式** — 累计 9+ 次的 git proxy 踩坑本次零损耗，证明 P0 升级 + 检查表设计有效
4. **副发现主动记录习惯持续低成本高回报** — string.h 行 45/150/230 同根但未触发，commit body + tasks.md 双留迁移指南
5. **完成验证 3 重证据链** — Release 全量 build 0 errors + Debug ctest 890/890 + bench sanity exit 0，零回归保证
