# 活跃上下文

## 当前阶段
归档中

## 当前任务

- **任务 ID**：TASK-20260418-01
- **描述**：消化关键技术债务（#45 dom_bindings 全局状态 / #46 StyleGetProp 读路径 / #48 HarfBuzz font 缓存 / #50 addEventListener 生命周期）
- **复杂度**：Level 3（中等功能）
- **工作流**：`/van` ✅ → `/plan` ✅ → (无需 /creative) → `/build` → `/reflect` → `/archive`
- **来源**：TASK-20260414-01 归档遗留的 P1 技术债
- **涉及子系统**：`veloxa/script`（dom_bindings）、`veloxa/text`（font_manager）、`veloxa/graphics/software`（software_canvas）
- **基线分支**：`main`
- **特性分支**：`feature/TASK-20260418-01-tech-debt`（已创建）
- **设计规格**：`docs/specs/2026-04-18-tech-debt-dom-bindings-design.md` ✅
- **实现计划**：`docs/plans/2026-04-18-tech-debt-dom-bindings.md` ✅

### 已确认方案（头脑风暴阶段通过）

- **#45** → **A1**：`JSClassID` 保留 `s_` 静态 + 幂等注册（`JS_IsRegisteredClass`）；`g_bound_doc`/`g_tracked_callbacks` 迁移到 `DomBindings::InstanceData`（pimpl）
- **#46** → **B1**：实现 length/color/auto/number/inherit/initial 的序列化；Enum（display）暂返回 `""`
- **#48** → **C1**：`hb_font_t*` 嵌入 `FontManager::FontEntry`，新增 `GetHbFont(handle, pixel_size)` + `hb_ft_font_changed` 响应 size 变化
- **#50** → **D1**：`InstanceData::listener_elements` 记录已绑定元素；`Unbind` 顺序改为先 `RemoveEventListeners` 再 `FreeAll callbacks`

### Phase 划分

| Phase | 内容 | 独立提交 |
|-------|------|---------|
| 0 | 基线验证 + 创建特性分支 | ✅ `6b7e68c` |
| 1 | #45 DomBindings pimpl 改造 + 全局状态迁移 | ✅ `255c74d` |
| 2 | #50 Unbind 顺序修复 | ✅ `d105c36` |
| 3 | #46 StyleGetProp 读路径 | ✅ `081896a` |
| 4 | #48 FontManager hb_font 缓存 | ✅ `d73d303` |
| 5 | 文档更新 + 最终验证 | ✅ `ea1a95b` |

**不需要创意阶段**（所有设计决策已在头脑风暴定案）。

### 构建验证

- `cmake --build build -j` → clean
- `ctest --output-on-failure` → **856/856 passed**（新增 `dom_bindings_test` 11 条、`font_manager_test` 3 条均通过）

## 待处理事项
- ~~**P1**：计划模板增加「CMake 链接方向约束分析」段——新模块涉及两个已有库时预画依赖图检测循环（来源 TASK-20260414-01）~~ ✅ 已落实到 `.cursor/rules/skills/writing-plans.mdc`（TASK-20260418-01 反思）
- **P1**：`/plan` checklist：FetchContent 引入 C/第三方编译时，校验根目录 `add_compile_options(-Werror…)` 是否仅限 `$<COMPILE_LANGUAGE:CXX>` 或目标级（来源 TASK-20260413-01，反复模式「环境/编译前置未验证」）
- **P1**：含 Git 拉取依赖的文档（`techContext` 或 README）写明代理 `http_proxy`/`HTTPS_PROXY` 与首次 `cmake` 注意点（来源 TASK-20260413-01）
- **P1**：计划模板增加「测试基础设施审计」段——列出测试需要访问的内部状态及其访问路径（来源 TASK-11，反复出现）
- **P1**：补充 Benchmark（网络恢复后，来源 TASK-01）
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`，并在该头注释示例 hex→RGBA（来源 TASK-20260413-02）
- **P1**：子代理 prompt 模板增加「跨模块数据格式」段（来源 TASK-02）— 已验证有效
- **P1**：集成测试优先验证数据格式一致性（来源 TASK-02）
- **P1**：存根文件预创建策略固化到子代理开发规则（来源 TASK-04）
- **P1**：合并 Phase 给子代理的策略固化到计划模板（来源 TASK-04）
- **P1**：计划模板增加「边界输入清单」段——每个 Phase 列出非默认路径（来源 TASK-06，反复出现）
- **P1**：集成测试必须使用真实 HTML/CSS 解析器，禁止仅用手动 DOM 构建（来源 TASK-06，反复出现）
- **P1**：子代理 prompt 涉及 LayoutBox 坐标计算时须包含 x/y 语义定义（content origin vs border box origin）（来源 TASK-07）
- **P1**：集成测试像素验证优先用 DisplayList 检查和区域扫描，避免硬编码坐标（来源 TASK-07）
- **P1**：CSS 颜色测试禁止与 gfx::Color 编程常量直接比较，必须通过 CssColorToGfx 转换（来源 TASK-07）
- **P1**：集成测试禁止使用 HTML inline style（BuildTree 不解析），必须用外部 CSS 选择器（来源 TASK-08，API 能力假设错误第三次出现）
- **P1**：并行子代理可行条件：无共享 .cc + 共享 .h 已创建 + CMakeLists.txt 已更新（来源 TASK-08）
- **P1**：跨模块参数透传修改时，计划模板增加「调用链端到端验证」段（来源 TASK-09）
- **P1**：设计文档管线注入点须附代码级可行性验证（来源 TASK-13）
- **P1**：集成测试模板增加 API 备忘清单：html::Parser / FindElement / HandleInput（来源 TASK-13，反复出现第 4 次）

### 本任务遗留（`/reflect` 需登记）

- **P2**：`StyleGetProp` Enum 读路径（display 等）——需要 `PropertyId→enum string` 反查表（来源 TASK-20260418-01 #46）
- **P2**：DomBindings/EventManager 析构顺序硬约束——当前仅保证 `DomBindings` 先析构；反向场景需弱引用机制（来源 TASK-20260418-01 #50）
- **原 #47**：`removeEventListener` 按 `(type, handler)` 精确移除，仍待 EventManager 扩展 API
