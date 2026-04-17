# 进度记录

## 当前任务

**TASK-20260418-01 — 消化关键技术债务**

- ✅ 2026-04-18：`/van` 初始化完成（Level 3，特性分支规划为 `feature/TASK-20260418-01-tech-debt`）
- ✅ 2026-04-18：`/plan` 头脑风暴完成，4 条债务方案确定（A1 / B1 / C1 / D1）
- ✅ 2026-04-18：设计规格保存至 `docs/specs/2026-04-18-tech-debt-dom-bindings-design.md`
- ✅ 2026-04-18：实现计划保存至 `docs/plans/2026-04-18-tech-debt-dom-bindings.md`
- ✅ 2026-04-18：`/build` Phase 0 — 基线验证（856/856 通过），创建特性分支 `feature/TASK-20260418-01-tech-debt`，提交设计 + 计划文档
- ✅ 2026-04-18：`/build` Phase 1 (#45) — DomBindings pimpl 改造，全局状态迁移至 `InstanceData`，`JSClassID` 改文件级 static + 幂等注册（`JS_IsRegisteredClass`）；新增 2 个测试；commit `255c74d`
- ✅ 2026-04-18：`/build` Phase 2 (#50) — `Unbind` 顺序修复（先 `RemoveEventListeners` 再 `FreeAll`），防止 JSValue-释放后 C++ lambda 仍被调用的 UAF；新增 2 个测试；commit `d105c36`
- ✅ 2026-04-18：`/build` Phase 3 (#46) — `StyleGetProp` 读路径实装，新增 `SerializeCssValue`（length/color/auto/number/inherit/initial），Enum 按约定暂保留空串；新增 7 个测试；commit `081896a`
- ✅ 2026-04-18：`/build` Phase 4 (#48) — `FontManager::GetHbFont` 引入 `hb_font_t` 每 FontEntry 缓存；`SoftwareCanvas::DrawText` 改借用不释放；`vx_text` CMake 依赖从 PRIVATE→PUBLIC 以支持测试链接；新增 3 个测试；commit `d73d303`
- ⏳ Phase 5：文档收尾（本次更新）+ 进入 `/reflect`

**关键决策落点（落地确认）**：
- ✅ `JS_IsRegisteredClass` API 存在于 quickjs-ng v0.14.0，`DomBindings` 构造函数做一次性幂等注册
- ✅ `DomBindings` pimpl 化：header 零 `quickjs.h` 依赖，`InstanceData` 前向声明公开（供 `.cc` 内自由函数命名），定义私有
- ✅ `hb_font_t*` 嵌入 `FontManager::FontEntry`，`pixel_size` 变更用 `hb_ft_font_changed`
- ✅ HarfBuzz/FreeType 在 `vx_text` CMakeLists 改 `PUBLIC`（非 tests/CMakeLists 加 pkg_check_modules）

**验证状态**：
- `cmake --build build -j` → 0 errors / 0 warnings
- `ctest --output-on-failure` → 856/856 passed（较基线 +14 个新增测试均过）

## 已完成任务

- TASK-20260414-01：功能补全 → 归档 `memory-bank/archive/archive-TASK-20260414-01.md`

- TASK-20260413-02：消化技术债务子集 → 归档 `memory-bank/archive/archive-TASK-20260413-02.md`
- TASK-20260413-01：QuickJS 脚本引擎集成 → 归档 `memory-bank/archive/archive-TASK-20260413-01.md`
- TASK-20260405-01：Foundation 基础库 → 归档 `memory-bank/archive/archive-TASK-20260405-01.md`
- TASK-20260405-02：Graphics HAL + Platform HAL → 归档 `memory-bank/archive/archive-TASK-20260405-02.md`
- TASK-20260405-03：DOM 树 + HTML 解析器 → 归档 `memory-bank/archive/archive-TASK-20260405-03.md`
- TASK-20260405-04：CSS 引擎 → 归档 `memory-bank/archive/archive-TASK-20260405-04.md`
- TASK-20260405-05：消化技术债务 → 归档 `memory-bank/archive/archive-TASK-20260405-05.md`
- TASK-20260405-06：Layout Engine 布局引擎 → 归档 `memory-bank/archive/archive-TASK-20260405-06.md`
- TASK-20260405-07：渲染管线（Render Pipeline） → 归档 `memory-bank/archive/archive-TASK-20260405-07.md`
- TASK-20260405-08：事件系统（Event System） → 归档 `memory-bank/archive/archive-TASK-20260405-08.md`
- TASK-20260405-09：脏区更新与重绘机制 → 归档 `memory-bank/archive/archive-TASK-20260405-09.md`
- TASK-20260405-10：事件循环与应用壳 → 归档 `memory-bank/archive/archive-TASK-20260405-10.md`
- TASK-20260405-11：C API 层 → 归档 `memory-bank/archive/archive-TASK-20260405-11.md`
- TASK-20260405-12：示例应用 → 归档 `memory-bank/archive/archive-TASK-20260405-12.md`
- TASK-20260405-13：CSS 动画系统（CSS Transitions） → 归档 `memory-bank/archive/archive-TASK-20260405-13.md`
