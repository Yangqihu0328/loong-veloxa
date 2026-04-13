# 回顾：QuickJS 脚本引擎集成

**日期：** 2026-04-13  
**任务 ID：** TASK-20260413-01  
**复杂度级别：** Level 4  

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| 任务分段 | 任务 1 骨架 → 2 Fetch → 3–6 测试与实现 | 合并为一次功能提交 + Memory Bank 收尾提交 | 环境阻塞（DNS/代理）后集中打通；仍覆盖计划中的 API 与安全点 |
| 文件变更 | 计划列出的路径 | 一致：`veloxa/script/*`、`tests/script/*`、根与 `tests/CMakeLists.txt` | 额外变更根 `CMakeLists.txt` 将严格告警**限定为 CXX**（计划未写，属必要修正） |
| 上游 | quickjs-ng v0.14.0 + FetchContent | 一致 | — |
| 创意对齐 | `JS_SetMemoryLimit`、禁 `js_std_*`、无 interrupt | 一致 | — |
| TDD | 严格 RED→GREEN 每步 | 桩→异常顺序修正→全绿；非「每一步单独提交」 | 与网络/编译迭代耦合 |

## 回顾检查清单（节选）

**代码变更类**

- [x] 计划精确度 — 文件清单一致；CXX-only 为实施中发现缺口  
- [x] TDD — 有失败用例驱动（Eval、`throw`）；合并提交略粗  
- [ ] 子代理 — 未使用  
- [x] 测试隔离 — 无 flaky；全量 `ctest` 796 通过  
- [x] 非默认路径 — 语法错误、`throw`、超长源码已测  

**安全相关**

- [x] 输入验证 — 源码最大字节数在进入 `JS_Eval` 前校验  
- [x] 错误信息 — 返回 QuickJS 异常字符串；未接 `js_std_*` 缩小面  
- [ ] 依赖审计 — 未跑自动化 CVE 工具；上游固定 tag，**建议**后续随发布周期人工对照  

## 做得好的

- **规格—创意—实现链路清晰**：`/van`/`plan`/`creative` 文档与 `JS_SetMemoryLimit`、禁 `js_std_*`、分阶段 interrupt 对齐良好。  
- **quickjs-ng + CMake 自带构建**：避免自维护 bellard 源码的 C 文件列表。  
- **异常路径对照上游**：`JS_IsException` → `JS_FreeValue` → `JS_GetException` 与 `api-test.c` 一致，修复了 `throw` 用例。  
- **根告警语言隔离**：`$<COMPILE_LANGUAGE:CXX>` 使 Veloxa 保持严格 C++ 而不误伤 FetchContent 的 C 代码。

## 遇到的挑战

- **WSL DNS**：无法解析 `github.com`，FetchContent 失败；经用户提供的 **HTTP 代理** 解决。  
- **全局 `-Wpedantic -Werror` 泄漏到 C 子项目**：quickjs.c 中 `void` 函数 `return expr` 触发失败；通过根 CMake 限定语言修复。  
- **首版异常字符串化**：在错误顺序下 `EvalThrownError` 失败；对照上游测试修正。

## 经验教训

- **任何 `add_compile_options(-Werror …)` 在含 C 子项目的 monorepo 中应默认按语言过滤**，或在 `/plan` 中列为「FetchContent 前置检查」。  
- **含 Git 拉取的任务**：在计划或 README 中预写 **代理 / 离线缓存 / `FETCHCONTENT_FULLY_DISCONNECTED`** 策略，减少环境排障时间。  
- **QuickJS 异常处理**：以 **上游 `api-test.c`** 为权威，而非凭直觉 stringify `JS_Eval` 返回值。

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | `/plan` 模板：凡 FetchContent 引入 **C 或第三方编译** 时，增加 checklist「根目录全局告警是否仅限 CXX/目标级」 | P1 | 计划模板 + 可选 `activeContext` 待办 | 避免再次全仓污染 |
| 2 | 文档：`docs/plans` 或 `techContext` 增加 **CI/本地：代理与首次 cmake** 一句操作说明 | P1 | `techContext.md` 或根 README 小节 | 新成员可复制 |
| 3 | 依赖：对 **quickjs-ng 固定 tag** 做发布说明/CVE 季度扫（无现成 `npm audit`） | P2 | `techContext` 技术债或定期 chore | 供应链卫生 |
| 4 | Phase 2：`SetEvalInterruptBudget` 与单测「死循环可被中止」 | P2 | 后续 TASK 引用 `creative-quickjs-host.md` | 安全硬化 |

## 反复模式识别

| 已知模式 | 本次是否重复？ |
|---------|---------------|
| 前置依赖/环境/API 能力未验证 | **是**（DNS、代理、C 与 `-Werror` 交互）→ 建议 **P1 固化到计划 checklist** |
| 非默认路径遗漏验证 | **否**（异常与超长源已覆盖） |
| TDD 严格度与场景不匹配 | **部分**（合并提交；核心行为仍有单测） |
| 计划文件未列 CXX 语言隔离 | **是**（实施中发现）→ 归入「计划精确度」改进 |

## 技术改进建议

- 后续为 `QuickjsEngine` 增加 **可选 interrupt** 与 **可配置内存上限** API（创意文档已框定）。  
- 考虑 `VX_QUICKJS_ROOT` 或 `third_party/quickjs-ng` **本地路径优先**，减轻 CI 对外网硬依赖（可选）。

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | ✅ | 256KiB 源码上限 |
| 认证/授权 | N/A | 无宿主 API 暴露 |
| 数据保护 | N/A | |
| 依赖审计 | ⚠️ | 固定 v0.14.0；建议周期性人工/CVE 跟踪 |
| 错误信息脱敏 | ⚠️ | 返回引擎异常文本，可能含栈片段；当前无宿主路径注入 |
| 敏感数据处理 | N/A | |

本任务为 **脚本执行面** 的基础合入；**未**实现 CPU 中断预算（创意 Phase 2），对不可信脚本仍须在集成层控制调用策略。
