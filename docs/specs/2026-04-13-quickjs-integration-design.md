# QuickJS 脚本引擎集成 — 设计规格

**任务 ID：** TASK-20260413-01  
**日期：** 2026-04-13  
**状态：** 草案（请审查；批准前不进入 `/build`）

---

## 1. 目标与成功标准

### 1.1 目标

在 Veloxa 中引入 **可构建、可测试、边界清晰** 的 QuickJS 运行时层，使 C++ 代码能以统一封装执行受控的 JavaScript 片段，并为后续 DOM / 事件绑定预留挂载点（**本阶段不实现绑定**）。

### 1.2 成功标准

- 配置阶段可 **固定版本** 拉取上游源码并成功链接（CI 与本地一致）。
- 新增静态库 `vx_script`（命名与 `systemPatterns.md` 中 `veloxa/script/` 一致），**默认不**被 `vx_core` 或 `vx_api` 依赖，避免扩大链接面。
- 至少一个 GTest：合法脚本求值返回预期；异常脚本返回非 `ok` 的 `Status` 且消息非空。
- 设计文档与实现计划中的 **安全需求** 在首版中有对应实现或明确为「后续任务」的缺口。

---

## 2. 方案对比与推荐

| 方案 | 说明 | 优点 | 缺点 |
|------|------|------|------|
| **A. bellard/quickjs + 自维护 CMake** | FetchContent / 子模块 + 手写 `add_library` 列举 `.c` | 与 Fabrice Bellard 主线一致 | 无官方 CMake，升级与交叉编译维护成本高 |
| **B. quickjs-ng + FetchContent** | 使用上游自带 `CMakeLists.txt`，目标 `qjs` / `qjs-libc` | 可复现构建、选项清晰、社区活跃 | 与 bellard 仓库分叉，API 高度兼容但非「原版」 tarball |
| **C. vcpkg 端口** | 增加 `vcpkg.json` 声明依赖 | 与包管理统一 | 当前仓库无 vcpkg 基线，引入全局策略变更 |

**推荐：方案 B（quickjs-ng + FetchContent）**，固定 **`GIT_TAG v0.14.0`**（与 GitHub `releases/latest` 一致，可随归档更新）。

**缓解：** 在 `techContext.md` 归档时注明引擎上游为 quickjs-ng，与产品文档中「QuickJS」指 **QuickJS 家族实现** 对齐。

---

## 3. 架构与信任边界

### 3.1 分层

```
宿主 / 未来 Application
        │
        ▼
  vx::script::QuickjsEngine  (C++17, RAII, Status/StatusOr)
        │
        ▼
  libqjs + libqjs-libc       (C11, quickjs-ng)
```

- **信任边界：** 穿越 `QuickjsEngine::Eval*` 边界的 `source` 字符串视为 **不可信**（与将来来自网络或文件的脚本同源）。
- **本阶段范围：** 仅内存字符串求值；**无**文件系统模块暴露、**无** `vx::dom::*` 指针传入 QuickJS。

### 3.2 与现有模块关系

- `vx_script` 仅依赖 `vx_foundation`（`Status` / `StringView` / 断言）与 QuickJS 头文件。
- **不**修改 `vx_core` / `Application` 的默认链接（避免 `vx_core` 全量用户链接脚本栈）。后续任务再增加「可选脚本宿主」CMake 选项。

---

## 4. 数据流与错误处理

1. 宿主构造 `QuickjsEngine` → 创建 `JSRuntime` / `JSContext`（失败 → `Status::kOutOfMemory` 或 `kInternal`）。
2. `EvalGlobal(source, filename)` → 可选 **源长度上限**（见安全）→ `JS_Eval(..., JS_EVAL_TYPE_GLOBAL)`。
3. 若 `JS_IsException` → 读取 `JS_GetException` 并转为字符串 → 返回 `Status(kInternal, message)`，并 `JS_FreeValue` 清理栈上值。
4. 成功 → 将结果 `JS_ToCString` / 等价 API 转为 `std::string` 返回 `StatusOr<std::string>`（或仅数值场景的窄化返回，见实现计划）。

---

## 5. 测试策略

- **单元测试：** `quickjs_engine_test.cc`，覆盖：算术表达式、语法错误、`throw`、（可选）UTF-8 非 ASCII 字面量。
- **安全相关测试：** 超长源码在 **进入** `JS_Eval` 之前被拒绝（`kInvalidArgument`），不依赖引擎内部行为。
- **不**在本阶段引入网络或 golden bytecode。

---

## 6. 安全分析（STRIDE 摘要）

**资产：** 宿主进程内存、CPU、未来 DOM 状态（本阶段仅前两者）。  
**信任边界：** `QuickjsEngine` 的公开方法 = 不可信输入入口。

| 威胁 | 说明 | 首版缓解 |
|------|------|----------|
| **D — DoS（CPU/内存）** | 死循环、大数组分配 | 封装层 **源码最大长度**（默认 256KiB，可调）；**执行中断 / 时间片** 在 `/creative` 定默认策略与数值 |
| **D — DoS（栈）** | 递归深度过大 | 依赖 QuickJS 默认栈限制；文档记录缺口 |
| **I — 信息泄露** | 异常对象暴露内部路径 | 使用固定逻辑文件名（如 `"<eval>"`）或调用方传入的短名；不向 JS 注入宿主路径 |
| **E — 权限提升** | 未来绑定原生指针 | 本阶段不提供任何 `JS_NewCFunction` 宿主 API |
| **T — 篡改 / 供应链** | 依赖被劫持 | FetchContent 固定 `GIT_TAG` + SHA 可选校验（在 CI 文档中说明） |

**安全需求清单（首版必须）：**

- [ ] 对 `source` 实施 **最大字节数** 校验（白名单式上限）。
- [ ] 所有 `JSValue` 路径 **无泄漏**（`JS_FreeValue` / RAII）。
- [ ] 异常路径返回 **无堆栈转储** 的简洁 `Status::message`（仅引擎错误文本）。

---

## 7. 需移交 `/creative` 的开放决策

以下项影响后续车载场景默认值，不在本规格中强行定死：

1. **是否默认链接 `qjs-libc` 并调用 `js_std_add_helpers`**（决定 `import std` / 信号与计时器行为）。
2. **默认执行预算：** 是否启用 `JS_SetInterruptHandler`、默认最大「指令步数」或 wall-clock（与 `EventLoop` 协同方式）。
3. **与 Foundation 分配器的关系：** QuickJS 仍使用其内部 `malloc` 首版；是否长期提供 `JS_SetRuntimeOpaque` + 自定义 allocator 由创意阶段定。

---

## 8. 审查确认

请确认：

- [ ] 接受 **quickjs-ng** 作为上游而非 bellard 源码直编。
- [ ] 接受首版 **仅 `vx_script` + Eval**，不接入 `Application` / C API。
- [ ] 安全缺口（死循环中断等）允许在 **TASK-20260413-01 后续迭代** 或子任务中补齐，但必须在 `docs/plans` 中显式列出。

**批准方式：** 回复「批准设计」或列出修订要点；修订后更新本文件版本记录。

---

## 9. 版本记录

| 版本 | 日期 | 说明 |
|------|------|------|
| 0.1 | 2026-04-13 | 初稿（`/plan`） |
