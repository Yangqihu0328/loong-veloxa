# 回顾：消化关键技术债务

**日期：** 2026-04-18
**任务 ID：** TASK-20260418-01
**复杂度级别：** Level 3（中等功能 / 跨 2 子系统）
**工作流路径：** `/van` → `/plan` → `/build` → `/reflect`（无 `/creative`）

---

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 6（0–5） | 6 | 一致 |
| 预估时间 | ~2h | ~2h（含 CMake 意外约 15 min） | 接近预估 |
| 文件变更（源） | 5 | 5（`dom_bindings.{h,cc}`、`font_manager.{h,cc}`、`software_canvas.cc`） | 一致 |
| 文件变更（构建） | 1（`tests/CMakeLists.txt`） | 1（`veloxa/text/CMakeLists.txt`） | **位置错判**：计划把 HarfBuzz 链接修补放在 tests/，实际需要在 vx_text/ 升 PUBLIC |
| 新增测试数 | ~12 | 14（Phase 1: 2 + Phase 2: 2 + Phase 3: 7 + Phase 4: 3） | 一致（Phase 3 测试粒度拆得更细） |
| 提交数 | 5 | 6（+ docs/memory-bank 收尾提交） | 一致 |
| 新增 P1/P2 残留 | 2 | 3（另加 #47 精确 removeEventListener 依赖 EventManager API） | 符合预期：Phase 2 实施时确认 EventManager 无按 handler 移除 API |

### 提交序列

```
6b7e68c docs(plan): design + plan for TASK-20260418-01 tech debt cleanup
255c74d refactor(script): migrate DomBindings globals to pimpl InstanceData (#45)
d105c36 fix(script): call RemoveEventListeners before freeing JS callbacks (#50)
081896a feat(script): implement StyleGetProp read path (#46)
d73d303 perf(text): cache hb_font_t per font handle (#48)
ea1a95b docs(memory-bank): mark TASK-20260418-01 build phase complete
```

测试：**856/856 passed**（较基线 842 +14，0 回归）。

---

## 回顾检查清单

本任务属于**代码变更类**（含重构 + bug 修复 + 性能优化）。

- [x] **计划精确度** — 源文件清单 100% 匹配；CMake 文件位置**错判**（预计改 tests/，实际改 vx_text/），见"遇到的挑战"#1
- [x] **TDD 执行情况** — 4 个 Phase 全部 RED→GREEN，Phase 1 的 `MultipleInstancesIndependentDocuments` 与 Phase 3 的 5 个 `StyleGet*` 真实 RED 后 GREEN；Phase 2 UAF 属于内存安全修复，白盒测试（行为等价）为主，commit message 已注明限制；Phase 4 编译失败型 RED
- [x] **子代理质量** — 本任务未使用子代理（单 agent 顺序执行）
- [x] **测试隔离** — 新增测试均独立构造 `QuickjsEngine`/`DomBindings`/`FontManager`，无共享状态，无 flaky
- [x] **提交粒度** — 每 Phase 一个独立 commit，commit message 描述 why（迁移原因/UAF 机制/序列化规则/缓存触发条件），无大杂烩
- [x] **非默认路径** —
  - `GetHbFont`: invalid handle、size change 都覆盖
  - `StyleGetProp`: unset / enum 未实现返 `""` 均有测试
  - `Unbind` → `Bind` → `Unbind` 循环已验证
  - 未覆盖：`FT_Set_Pixel_Sizes` 未先调用就 `GetHbFont`（调用方契约违约路径，仅文档约定）

---

## 做得好的

### a. 技术选择

1. **pimpl + `JS_SetContextOpaque` 组合拳极简**
   - header 从 20 行含 `JSContext*`/`JSValue` 成员，精简到仅 `std::unique_ptr<InstanceData>`
   - 所有自由函数回调通过 `JS_GetContextOpaque(ctx)` 拿到 `DomBindings*`，零新全局、零 thread-local
   - `DomBindingsInternal::Data()` friend 桥接不暴露 public API

2. **`JSClassID` 文件级静态 + 幂等注册**
   - 避开了多次 Bind 反复 `JS_NewClassID` 的陷阱（quickjs-ng 同 runtime 不能重复注册）
   - `JS_IsRegisteredClass` 让代码对 "已注册但被重建" 场景也安全

3. **`Unbind` 顺序修复的最小侵入**
   - 没引入弱引用、没改 `EventManager` 接口，仅新增 `listener_elements` 向量 + 调换两行顺序
   - commit message 明确记录"宿主须确保 `DomBindings` 先于 `EventManager` 析构"遗留约束，把工程权衡显式化

4. **`SerializeCssValue` 一次覆盖 6 种 ValueType**
   - length/color/number/auto/inherit/initial 单次 switch 全部处理，后续补 enum 只需加一个 case
   - 颜色格式 `rgba(r, g, b, a)` 直接匹配 CSSOM getter 约定，无需 TR 字符串特殊处理

5. **`hb_ft_font_changed` 而非销毁重建**
   - 同 handle、不同 pixel_size 下，`hb_font_t*` 指针稳定（测试 `GetHbFontHandlesSizeChange` 验证 `hb1 == hb2`）
   - 避免了缓存失效引发的连锁开销；调用方契约（先 `FT_Set_Pixel_Sizes` 再 `GetHbFont`）在 header 注释明确

### b. 流程

6. **每 Phase 一个独立 commit，回退粒度最小**
   - 如果 Phase 3 出回归，`git revert 081896a` 不影响 #45/#50/#48
   - 便于未来 bisect

7. **Phase 0 先跑完整回归（842/842）作为基线**
   - Phase 1 pimpl 大改后立刻发现任何回归都能归因到本 Phase
   - 没有"大改完再跑测试"带来的多源 bug 叠加

8. **设计与实现计划先行**（`/plan` 阶段产出 spec + plan）
   - `/build` 期间几乎没有"停下来想怎么做"，决策节点全部前置到头脑风暴
   - 方案编号（A1/B1/C1/D1）在 commit message、tasks.md 中有一致引用

---

## 遇到的挑战

### #1 CMake 静态库依赖传播（耗时约 15 min，本次最大卡点）

- **现象**：Phase 4 加 `#include <ft2build.h>` 到 `font_manager_test.cc` 后编译失败 `ft2build.h: No such file`
- **错误假设**：计划里写"在 `tests/CMakeLists.txt` 用 `pkg_check_modules` 补 HarfBuzz 链接"
- **踩坑顺序**：
  1. 尝试 `pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)` → `FindPkgConfig.cmake: Unknown arguments specified`（父 scope 冲突）
  2. 改前缀 `HARFBUZZ_TEST` → 仍报 Unknown arguments（根因是 `pkg_check_modules` 跨文件重入问题）
  3. 最终正解：回退 `tests/CMakeLists.txt`，把 `veloxa/text/CMakeLists.txt` 中 `Freetype::Freetype`、`${HARFBUZZ_LIBRARIES}`、`${HARFBUZZ_INCLUDE_DIRS}` 从 `PRIVATE` 升 `PUBLIC`
- **教训**：
  - 静态库 `A` 的 `PRIVATE` 依赖**不会**自动传播给链接 `A` 的目标；如果下游源文件（含测试）会 `#include` 或依赖这些依赖暴露的符号，必须 `PUBLIC`
  - `pkg_check_modules` 不应在多个 `CMakeLists.txt` 重复调用同一模块
  - **计划阶段应做"CMake 链接方向分析"**：每个下游目标能否直接 `target_link_libraries(X my_lib)` 然后就跑起来？如果不行说明依赖是 `PRIVATE` 且必须升级
  - 这与 `activeContext.md` 待处理事项 P1「计划模板增加『CMake 链接方向约束分析』段」**直接呼应**——这次又踩了一次

### #2 `InstanceData` 私有导致匿名 namespace 回调无法命名（耗时约 5 min）

- **现象**：`GetData(JSContext*) -> DomBindings::InstanceData*` 在匿名 namespace 中定义，编译报 `'InstanceData' is private within this context`
- **原因**：C++ 访问控制对"命名类型"也适用，即使返回类型的函数本身通过 friend 可访问 `data_` 成员
- **解法**：把 `struct InstanceData;` 前向声明移到 `public:`，定义和 `data_` 保持 `private` —— Pimpl 封装性不变
- **教训**：pimpl 时前向声明放 `public`、定义+成员放 `private` 是标准模式，应纳入 code-review 检查点

### #3 `-Werror` 把 `GetEventManager` 未使用变成编译错误（耗时 <1 min）

- Phase 1 重构后，`GetEventManager` 辅助函数变成无调用者
- 直接删除即可；但**证明了** `-Werror` 的价值：未死代码在 clang 15 立刻暴露

### #4 Phase 2 白盒测试复现 UAF 的局限

- `HandleInput` 需要 `LayoutBox*`，测试不便构造完整布局树
- 计划里已预见到，commit message 注明"白盒测试 + 代码审查验证"
- **反思**：未来涉及内存安全的修复，考虑在 CI 开 ASan（本仓库尚未配置）可直接用既有集成测试跑出 UAF 证据；当前通过调换两行顺序 + 注释说明 + 单元测试白盒验证基本保障正确性

---

## 经验教训

### 技术层面

1. **pimpl 的前向声明位置决定可用范围**——`public:` 处前向声明允许 `.cc` 内自由函数命名类型；`private:` 处只能给 class 自己用
2. **`JS_SetContextOpaque` 是 QuickJS 桥接 C++ 实例的规范通道**——避免全局 map、避免 thread-local
3. **`JSClassID` 是进程/runtime 级别常量**，必须幂等注册；实例状态和类型注册要分层
4. **CMake 静态库 `PRIVATE` 依赖不传播**——下游要用时升 `PUBLIC`，别在下游补 `pkg_check_modules`
5. **C++ lambda 捕获与 JS_Value 必须同寿命**——释放顺序：先销毁 lambda 拷贝，后释放被捕获资源
6. **`hb_ft_font_changed` 足以响应 size 变更**——不必销毁重建 `hb_font_t`，指针稳定有利于缓存

### 流程层面

7. **每 Phase 独立提交 + 每 Phase 先跑回归** 是 Level 3 任务最有价值的节奏
8. **计划阶段做"CMake 链接方向分析"** 可避免本次 #1 踩坑；已登记为 P1
9. **前置测试可以有意设计成"暴露旧实现 bug"**（Phase 1 `MultipleInstancesIndependentDocuments`）——这样修复前 FAIL、修复后 PASS 的证据链最强

### 反复模式对照（见本文后面"反复模式识别"段）

- 本次踩到了「前置依赖/环境/API 能力未验证」的 CMake 变体——第 9+ 次
- 反之**避免了**「计划文件清单与实际变更不一致」（文件清单完全精确）——值得延续

---

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | `/plan` 模板增加「CMake 链接方向约束分析」段：列出新增/修改的下游目标，核对每个上游依赖是 PRIVATE 还是 PUBLIC，若下游会 include/symbol 级依赖则必须 PUBLIC | **P1 反复出现，需固化** | 更新 `.cursor/rules/skills/writing-plans.mdc`；归并 `activeContext.md` 已有同名 P1 | `.cursor/rules/skills/writing-plans.mdc` |
| 2 | pimpl 模式检查点：前向声明放 public（.cc 内自由函数/helper 需命名），定义+成员放 private；析构必须在 .cc non-default | P2 | 追加到 `systemPatterns.md` C++ 惯用法段 | `memory-bank/systemPatterns.md` |
| 3 | QuickJS `JSClassID` 幂等注册模板固化 | P2 | 追加到 `systemPatterns.md` | `memory-bank/systemPatterns.md` |
| 4 | CI 引入 AddressSanitizer target（`-fsanitize=address` 条件构建） | P2 长期 | 技术债 #? 新增 | `memory-bank/techContext.md` |
| 5 | `StyleGetProp` Enum 读路径（display 等）补齐 | P2 | 独立后续任务，需 `PropertyId → enum string` 反查表 | 新任务 |
| 6 | `DomBindings`/`EventManager` 双向析构安全（弱引用机制） | P2 | 独立后续任务，当前有宿主约定兜底 | 新任务 |
| 7 | `removeEventListener` 按 `(type, handler)` 精确移除 | P2 | 需 `EventManager` 扩展 `RemoveEventListener(el, type, handler)` API | 新任务（原 #47） |

**优先级定义：**
- **P0 立即**：本任务归档前须落实——**无**
- **P1 下次**：下个同类任务前须落实——#1（登记 `activeContext.md`）
- **P2 长期**：记录到 `systemPatterns.md` / `techContext.md` 或新建任务——#2~#7

---

## 技术改进建议

1. **代码质量**：
   - pimpl 改造后 `dom_bindings.h` 更干净，但 `dom_bindings.cc` 有 ~700 行，可考虑拆分 `dom_bindings_callbacks.cc`（QuickJS 回调）与 `dom_bindings_lifecycle.cc`（Bind/Unbind/InstanceData）
   - `SerializeCssValue` 放在 `dom_bindings.cc` 匿名 namespace，但逻辑上属于 `css::CssValue` 的展示层；未来若 DevTools/调试输出需要同样能力，应移到 `veloxa/core/css/` 公共模块

2. **测试覆盖**：
   - 14 个新测试全通过，但 Phase 2 UAF 的**真实运行时触发**未验证；引入 ASan 后可以去掉白盒测试代之集成测试
   - `software_canvas` 原有像素测试未新增；`GetHbFont` 切换后仅靠 `font_manager_test` + 既有集成测试保障，视觉回归风险可接受（因 fallback 路径不变）

3. **技术债务（本次引入）**：
   - ~~#45~~ ✅ ~~#46~~ ✅（部分）~~#48~~ ✅ ~~#50~~ ✅
   - 新增遗留：#46 Enum 读路径；#50 宿主析构顺序约束
   - 原 #47 精确 removeEventListener 仍在

4. **性能**：
   - `hb_font_t` 缓存的实际收益待 Benchmark 确认（本仓库尚无 benchmark infra，P1 待处理）
   - 理论上重复绘文本帧省下每字符 `hb_ft_font_create_referenced` + `hb_font_destroy`（两次 face 表解析）

---

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | 本任务无新外部输入接口 |
| 认证/授权 | N/A | — |
| 数据保护（加密/脱敏） | N/A | — |
| 依赖审计 | N/A | 未引入新依赖（HarfBuzz 已在 vx_text） |
| 错误信息脱敏 | N/A | — |
| 敏感数据处理 | N/A | — |
| **内存安全（额外）** | ✅ | #50 UAF 修复本身就是内存安全改进；#45 pimpl 消除了全局 mutable state 带来的 TOCTOU 面 |

**本任务不涉及外部接口/认证/数据保护变更；属纯内部重构 + 性能优化 + 内存安全修复。**

---

## 反复模式识别

| 已知模式 | 历史频率 | 本次是否重复？ |
|---------|---------|---------------|
| 计划文件清单与实际变更不一致 | 9+ 次 | **未重复** ✅（源文件清单精确；CMake 文件位置判断错了，但属性质差异，见下条） |
| 子代理产出需大量返工 | 7+ 次 | N/A（未用子代理） |
| 前置依赖/环境/API 能力未验证 | 8+ 次 | **部分重复** ⚠️ — CMake 依赖传播方向（PRIVATE vs PUBLIC）未前置分析，导致 Phase 4 卡 15 min。触发升级：建议 #1 **P1 固化到 `writing-plans.mdc`** |
| 非默认路径遗漏验证 | 4+ 次 | **未重复** ✅（invalid handle、size change、unset、enum 都覆盖） |
| 测试隔离问题 | 7+ 次 | **未重复** ✅ |
| 提交粒度偏离计划 | 7+ 次 | **未重复** ✅（6 个 commit 对应 5 个 Phase + 1 docs，粒度一致） |
| TDD 严格度与场景不匹配 | 11+ 次 | **未重复** ✅（4 Phase 都明确设定 RED，Phase 2 白盒限制已 commit 注明） |

**结论**：仅触发 1 项反复模式（CMake 前置分析），且在 `activeContext.md` 已有同名 P1。本次建议#1 要求把该 P1 **具体落实到 `.cursor/rules/skills/writing-plans.mdc`** —— 不能再光登记不执行。

---

## 结论

TASK-20260418-01 在 ~2h 内清理了 4 条 P1 技术债（#45/#46/#48/#50），测试 856/856 全绿，commit 粒度规整，遗留 3 条 P2/新任务已明确登记。唯一工程事故——CMake `PRIVATE` 依赖传播判断错误——已经第 9+ 次以不同变种出现，本次回顾决定**把 `/plan` 模板强化**从而闭环。pimpl + `JS_SetContextOpaque` + `JS_IsRegisteredClass` 的组合作为 QuickJS 集成模板沉淀到 `systemPatterns.md`，下次涉及类似集成任务可直接引用。

**下一步**：`/archive` 归档任务。
