# 任务跟踪

## 当前任务

### TASK-20260419-01 — 流程规则沉淀 + P2 功能技术债收口

- **复杂度级别：** Level 3（中等功能；跨「.cursor 规则」+ 「Script/CSS/Event」子系统）
- **当前阶段：** 回顾中（待归档）
- **分支：** `feature/TASK-20260419-01-rules-and-debt`（基线 `main`）
- **创建日期：** 2026-04-19
- **设计规格：** `docs/specs/2026-04-19-rules-and-debt-design.md`
- **实现计划：** `docs/plans/2026-04-19-rules-and-debt.md`
- **预估提交：** 21 个；预估新增测试：+22（基线 856 → ≥ 878）

#### 需要创意阶段的组件

**无。** 头脑风暴阶段已为 Part B 三个设计点（B5 Enum 反查表 / B6 反向析构机制 / B7 精确移除 API）固化方案：

| 子项 | 决策 | 模式来源 |
|------|------|---------|
| B5 | 完整反查表 `enum_serialization.{h,cc}` | 枚举 + 元数据表驱动模式（property.cc 同构） |
| B6 | EventManager `AddDestructionObserver` 回调 | Push 回调失效触发模式（SetInvalidationCallback 同构） |
| B7 | u64 ListenerToken + `RemoveByToken` API | 不透明句柄模式（与现有 `JSValue` 句柄风格一致） |

无需进一步探索 → 跳过 `/creative`，直接 `/build`。

#### Phase 概览

| Phase | 内容 | 预计耗时 |
|-------|------|---------|
| 0 | 基线验证（856 测试全绿） | 5 min |
| A1 | writing-plans.mdc 增 5 段 | 30 min |
| A2 | subagent-development.mdc 增 3 段 | 20 min |
| A3 | 新建 integration-testing.mdc + 注册 | 20 min |
| A4 | techContext.md「FetchContent 与代理」段 | 10 min |
| B5 | enum_serialization.{h,cc} + dom_bindings 接入 + 测试 | 45 min |
| B6 | EventManager DestructionObserver + DomBindings 注册 | 30 min |
| B7 | Listener Token API + 精确 removeEventListener | 60 min |
| 8 | 收尾验证 + Memory Bank 更新 | 20 min |
| **合计** | | **~4 h** |

#### 任务范围

**Part A — 流程规则沉淀（14 条 P1 待办，纯 .mdc 文件修改）**

1. 计划模板（`.cursor/rules/skills/writing-plans.mdc`）增段：
   - FetchContent 引入第三方编译时校验根 `add_compile_options(-Werror...)` 是否仅限 `$<COMPILE_LANGUAGE:CXX>` 或目标级（来源 TASK-20260413-01）
   - 测试基础设施审计——列出测试需访问的内部状态及访问路径（来源 TASK-11）
   - 边界输入清单——每个 Phase 列出非默认路径（来源 TASK-06）
   - 跨模块参数透传修改时调用链端到端验证（来源 TASK-09）
   - 设计文档管线注入点须附代码级可行性验证（来源 TASK-13）
2. 子代理 prompt（`.cursor/rules/skills/subagent-development.mdc`）增段：
   - 跨模块数据格式段（来源 TASK-02）
   - LayoutBox 坐标计算时须包含 x/y 语义定义 content origin vs border box origin（来源 TASK-07）
   - 并行子代理可行条件：无共享 .cc + 共享 .h 已创建 + CMakeLists.txt 已更新（来源 TASK-08）
   - 存根文件预创建策略（来源 TASK-04）
   - 合并 Phase 给子代理的策略（来源 TASK-04）
3. 集成测试规范——新建 `.cursor/rules/skills/integration-testing.mdc`：
   - 集成测试优先验证数据格式一致性（来源 TASK-02）
   - 必须使用真实 HTML/CSS 解析器，禁止仅用手动 DOM 构建（来源 TASK-06）
   - 像素验证优先用 DisplayList 检查和区域扫描，避免硬编码坐标（来源 TASK-07）
   - CSS 颜色测试禁止与 gfx::Color 编程常量直接比较，必须通过 CssColorToGfx 转换（来源 TASK-07）
   - 禁止使用 HTML inline style（BuildTree 不解析），必须用外部 CSS 选择器（来源 TASK-08）
   - 集成测试模板增加 API 备忘清单：html::Parser / FindElement / HandleInput（来源 TASK-13）
4. 文档：含 Git 拉取依赖的文档（`techContext.md` 或 README）写明代理 `http_proxy`/`HTTPS_PROXY` 与首次 `cmake` 注意点（来源 TASK-20260413-01）

**Part B — P2 功能技术债（含代码与测试）**

5. **#46 续作**：`StyleGetProp` Enum 读路径（display 等）——构建 `PropertyId→enum string` 反查表，覆盖 display/position/visibility 等 Enum 类型 CSS 属性
6. **#50 续作**：`DomBindings`/`EventManager` 析构顺序硬约束——目前仅保证 `DomBindings` 先析构；反向场景（`EventManager` 先销毁）需弱引用机制
7. **#47 续作**：`removeEventListener` 按 `(type, handler)` 精确移除——扩展 `EventManager::RemoveEventListenersByHandler` API 并在 `dom_bindings` 调用

#### 安全相关

否（纯内部技术债 + 流程规则；不涉及外部输入/认证/存储/部署）

## 任务历史

| ID | 描述 | 最终状态 | 归档日期 |
|----|------|---------|---------|
| TASK-20260418-01 | 消化关键技术债务（#45 dom_bindings 全局状态 / #46 StyleGetProp 读路径 / #48 hb_font 缓存 / #50 addEventListener 生命周期） | ✅ 已完成 | 2026-04-18 |
| TASK-20260414-01 | 功能补全（border-radius / 字体渲染 / 图片支持 / JS-DOM 绑定） | ✅ 已完成 | 2026-04-14 |
| TASK-20260413-02 | 消化技术债务子集（#41 `HashMap` const 迭代、#39 测试像素头、`active_count_` 移除） | ✅ 已完成 | 2026-04-13 |
| TASK-20260413-01 | QuickJS 脚本引擎集成（quickjs-ng、`vx_script`、`QuickjsEngine`） | ✅ 已完成 | 2026-04-13 |
| TASK-20260405-01 | 构建 Foundation 基础库（内存管理/容器/字符串/日志） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-02 | 构建 Graphics HAL 图形抽象层与 Platform HAL 平台抽象层 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-03 | 构建 DOM 树 + HTML 解析器 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-04 | 构建 CSS 引擎（Tokenizer/Parser/选择器/属性/层叠） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-05 | 消化技术债务（Arena/HashMap/PPM/Parser Error） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-06 | 构建 Layout Engine 布局引擎 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-07 | 构建渲染管线（Render Pipeline） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-08 | 构建事件系统（Event System） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-09 | 脏区更新与重绘机制 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-10 | 事件循环与应用壳（EventLoop / Application Shell） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-11 | C API 层（对外嵌入接口） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-12 | 示例应用（examples/hello） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-13 | CSS 动画系统（CSS Transitions） | ✅ 已完成 | 2026-04-05 |
