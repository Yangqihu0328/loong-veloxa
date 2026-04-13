# 回顾：消化技术债务（子集 #39 / #41）

**日期：** 2026-04-13  
**任务 ID：** TASK-20260413-02  
**复杂度级别：** Level 2  

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| 任务数 | 4 组 | 4 组 | 一致 |
| 提交粒度 | 每任务独立提交 | `feat` / `refactor` / `test` / `docs(memory-bank)` 四分，与计划一致 | — |
| 文件清单 | `hash_map.h`、`transition.*`、`test_pixel_utils.h`、2 测、`techContext`/`systemPatterns` | 一致 | — |
| TDD | 先测后实现（ConstIteration） | 测试与实现同一轮合并通过；非「先看到 RED 再绿」的严格分步 | 头文件迭代器为机械扩展，风险低 |
| 像素注释 | — | 修正 `0x00FF00FF` 通道语义（易误解为「纯绿」） | 引入 `PixelR/G` 断言时发现与 RGBA32 位域不一致 |

## 回顾检查清单（节选）

- [x] 计划精确度 — 与 `docs/plans/2026-04-13-tech-debt-subset.md` 一致  
- [x] TDD — `HashMapTest.ConstIteration` 新增；其余为重构 + 工具提取  
- [x] 测试隔离 — 无 flaky；全量 **797** 通过  
- [x] 非默认路径 — `HasActive` 经 `css_transition_test` / `transition_integration_test` 覆盖  

## 做得好的

- **范围锁定有效**：仅 #39、#41 及派生的 `active_count_` 移除，未蔓延到 parser 等大块债务。  
- **`ConstIterator` 与 `Iterator` 对称实现**：维护成本低，符合现有开放寻址表结构。  
- **删除 `active_count_`**：消除与 `OnStyleChange`/`Tick` 双轨维护风险，行为由现有单测背书。  
- **公共像素头**：`vx::test::Pixel*` 与 `techContext` RGBA32 约定对齐，便于后续渐进迁移。

## 遇到的挑战

- **`api_integration_test.cc`** 中一处 `PixelR` 未改为 `vx::test::PixelR`，编译期才发现（局部替换遗漏）。  
- **`0x00FF00FF` 字面量**：十六进制与「通道名」直觉易错位，首版误写 `PixelG==255` 导致单测失败。

## 经验教训

- 迁移内联辅助函数后，用 **`grep PixelR(`** 或命名空间限定全文替换，避免漏网。  
- 测试中写 **hex 常数**时，在旁注 **RGBA 四通道数值** 或改用 `gfx::Color`/`CssColorToGfx` 再转 `u32`，减少误读。  
- **小步提交**（foundation → css → test → docs）使 `git bisect` 与回滚边界清晰。

## 改进建议

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | 将其余仍手写通道位移的测试（如 `renderer_test`、`render_integration_test`）分批迁到 `test_pixel_utils.h` | P2 | 后续小 TASK 或 opportunistic PR | 彻底落实 #39 |
| 2 | 在 `test_pixel_utils.h` 文件头增加 **「与 `gfx::Color::ToRGBA32` 一致」** 及一行示例 hex 解码 | P2 | 同文件注释 | 降低误读 |

## 反复模式识别

| 已知模式 | 本次是否重复？ |
|---------|---------------|
| 计划清单与实际不一致 | 否 |
| 非默认路径遗漏 | 否（像素边界经失败用例补全） |
| TDD 严格度与场景不匹配 | **部分**（合并 RED/GREEN） |

## 技术改进建议

- `HasActive()` 现为 O(元素×每键向量长度)；过渡数量在 HMI 规模可接受，若未来暴增可缓存「脏标记」再评估。  

## 安全评估

本任务不涉及安全敏感变更（无新外部输入面、无新依赖）。

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | |
| 依赖审计 | N/A | 无新依赖 |

## 版本记录

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0 | 2026-04-13 | `/reflect` 初稿 |
