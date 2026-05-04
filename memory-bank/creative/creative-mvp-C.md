# 创意设计：MVP-C 真嵌入式部署档

**日期：** 2026-05-04
**状态：** 已批准（V1=D 三档分级 + B2=3 篇拆分粒度 user all_recommended 锁定）
**关联任务：** [TASK-20260504-01](../../docs/specs/2026-05-04-mvp-scope.md)
**对应 spec 段：** [`§3.3 MVP-C`](../../docs/specs/2026-05-04-mvp-scope.md) + 附录 C.1-C.10 + §3.3.1 9 项 gap 表 + §6.2 推荐立项顺序 3-10

---

## 1. 设计挑战

**问题陈述：**

MVP-C ~70% 达成（继承 MVP-B 90% + 缺核心 P0 主线 G1 OpenGL ES + G2 DRM/KMS 共 ~40-80 h 主线 + 7 项次线 P1/P2/P3 gap）。**核心挑战：如何规划 9 项 gap 的推进路径，平衡核心目标 #1 嵌入式 Linux + #2 高性能渲染（硬件加速）的硬指标，与项目长期可持续推进的资源节奏？**

**约束条件：**

1. **核心 P0 主线不可绕过** — C-G1 OpenGL ES + C-G2 DRM/KMS 是 [`projectbrief.md` 5 大核心目标 #1+#2 硬指标](../../memory-bank/projectbrief.md) / 必须达成
2. **不破坏 MVP-A/B baseline** — A14 + 1284 ON / 1091 OFF + MVP-B 4 gap 已修复后的扩展 baseline
3. **资源节奏可持续** — 单子任务 plan ×0.6 ≤ 30 h（避免 Level 4 单 Phase 过大无法 reflection）
4. **架构占位优先** — G1/G2 在实施前必须有架构占位（spec + creative + 接口预留），降低 build 阶段风险
5. **次线 gap 不能阻塞主线** — C-G3-G9 不能在 C-G1/G2 完成前强制推进
6. **复用既有 Surface / Renderer 抽象** — [`2026-04-25-sdl2-window-backend-design.md §未来工作`](../../docs/specs/2026-04-25-sdl2-window-backend-design.md) 已留 `Surface::Present()` 抽象 + 双轨 CMake 模板可复用

**成功标准：**

1. **MVP-C 核心 P0 闭环** — C.1-C.10 全 ✅（含 G1+G2）
2. **嵌入式 Linux 60fps 复杂界面验证** — 车载 / 工业 HMI 真设备测试
3. **架构可扩展** — Vulkan / Metal / Direct3D 后端可在 G1 完成后顺势扩展
4. **资源加载策略明确** — C-G3 闭环（systemPatterns §待定架构决策清零）
5. **0 反复模式命中** — 沿用 [TASK-20260503-04 archive §6 评估 4/7 守门记录](../../memory-bank/archive/archive-TASK-20260503-04.md)

---

## 2. 探索的方案（3 方案 — 整体推进路径）

### 方案 A：大爆炸（一次性投入 40-80 h 完成 P0）

**描述：** 集中资源 1-2 个月一次性完成 C-G1 OpenGL ES + C-G2 DRM/KMS 双 P0 主线 / 完成后再启动 C-G3-G9 次线。

**优点：**
- 核心目标 #1+#2 一次性达成 / 战略里程碑明确
- 架构投资集中 / 减少上下文切换成本
- 完成后 MVP-C 立即可宣发（嵌入式硬件加速 + 真嵌入式 Linux 部署）

**缺点：**
- 单次资源投入巨大（40-80 h plan ×0.6 / standalone-AI 1-3 周 / human 数月）
- 阻塞期间无其他可见进展 / 项目"看似停滞"
- G1+G2 之间存在依赖（如 G2 DRM/KMS Surface 实现可能依赖 G1 GLES Renderer 接口）→ 不能完全并行
- 主线 build 阶段反复模式风险高（架构大幅改动 / 依赖未实证）

**复杂度：** 极高
**风险：** 高（资源投入失控 / 阻塞传染 / 反复模式风险）

---

### 方案 B：严格优先级串行（C-G1 → C-G2 → C-G3 → ... → C-G9）

**描述：** 按 spec §6.2 推荐立项顺序严格串行 9 个独立子任务 / 每个 reflection + archive 后再启动下一个。

**优点：**
- 每个子任务边界清晰 / Level 定级合理
- reflection 阶段可校准下一任务 estimating（[archive §6 校准范式](../../memory-bank/archive/archive-TASK-20260503-04.md)）
- 风险可控（任一任务超时不阻塞其他）

**缺点：**
- 总时间最长（10 任务 × 各自 overhead）
- C-G6 CSS 高级 5 项 + C-G7 图像扩展 3 项可拆并行而强行串行浪费机会
- 项目长期"按 list 推进"缺战略节奏感

**复杂度：** 中
**风险：** 低（每任务独立）

---

### 方案 C：分层推进（推荐 ✅ — P0 主线 + P1/P2/P3 次线并行）

**描述：** 按**优先级层 + 自然边界**分层并行：

| 层 | gap 集合 | 子任务数 | 时序 | 可并行性 |
|:-:|---|:-:|---|:-:|
| **第 1 层 P0 主线** | C-G1 OpenGL ES | 1（Level 4 多 Phase）| **strict 第一**（架构基础 / 阻塞 G2/G4）| 内部 Phase 串行 |
| **第 1 层 P0 主线** | C-G2 DRM/KMS | 1（Level 3-4）| 在 G1 Phase 1 架构占位完成后启动 | 与 G1 后续 Phase 并行 |
| **第 2 层 P1 次线** | C-G3 资源加载策略 | 1（Level 3 蓝图 + 实施）| 与 G1/G2 并行 | 完全独立 |
| **第 2 层 P1 次线** | C-G4 嵌入式 Linux 平台构建 | 1（Level 3）| 在 G2 完成后启动 | 依赖 G2 |
| **第 3 层 P2 长尾** | C-G5 DOM 节点动态创建删除 | 1（Level 3）| 任意时机 / 与 P0 并行 | 完全独立 |
| **第 3 层 P2 长尾** | C-G6 CSS 高级 5 项 | 5（独立 L2-3）| 任意时机 / 完全独立 | **5 项全可并行** |
| **第 4 层 P3 末梢** | C-G7 图像扩展 3 项 | 3（独立 L2）| 任意时机 | **3 项全可并行** |
| **第 4 层 P3 末梢** | C-G8 R9 EventManager HitTest | 1（L2-3）| 任意时机 | 独立 |
| **第 4 层 P3 末梢** | C-G9 性能优化收口 | 多任务集合 | 渐进式 | 多并行 |

**优点：**
- P0 主线优先 / 战略硬指标聚焦
- P1/P2/P3 并行 / 项目持续可见进展
- C-G6 / C-G7 拆并行最大化资源利用
- G1 内部分 Phase（如 Phase 1 架构占位 / Phase 2 基础 quad shader / Phase 3 glyph + image / Phase 4 dirty rect 集成）后允许 G2 提前启动 / 减少串行成本
- 与 [systemPatterns Level 4 多 Phase 范式](../../memory-bank/systemPatterns.md) 一致（DevTool 三件套 4 Phase / G1 复用此范式）

**缺点：**
- 协调成本（4 层 / 9 子任务集合 / 多并行需要清晰的优先级看板）
- 各层 reflection / archive 总数最多（~13-15 archives）

**复杂度：** 中（每层）/ 高（总协调）
**风险：** 中（需要明确的优先级看板 / 依赖图）

---

### 2.x 方案对比

| 维度 | 方案 A 大爆炸 | 方案 B 严格串行 | 方案 C 分层并行 |
|---|:-:|:-:|:-:|
| 核心目标达成时间 | **最快**（1-2 月）| 中（3-6 月）| 快（2-4 月）|
| 总投入 | 40-80 h | 60-120 h（含 overhead）| **40-80 h + ~5-10 h 协调** |
| 项目可见进展节奏 | ❌（停滞期）| ⚠️（按 list）| **✅（持续）** |
| 资源弹性 | ❌（必须集中）| ⚠️（必须串行）| **✅（可调节）** |
| 反复模式风险 | 高 | 低 | 中 |
| 架构投资集中度 | 高 | 低 | 中 |
| 与 systemPatterns 协同 | ⚠️ | ✅ | ✅ |
| **推荐度** | ❌ | ⭐ 备选 | **⭐⭐ 主推** |

---

## 3. 选定方案：C 分层并行（4 层 / ~13-15 子任务集合）

**推荐方案：C — 分层并行**

### 3.1 选择理由

1. **核心目标 #1+#2 优先聚焦** — C-G1 OpenGL ES 第一启动 / C-G2 DRM/KMS 在 G1 架构占位后启动 / 战略硬指标不偏离
2. **持续可见进展** — P2/P3 长尾 gap 与 P0 主线并行 / 项目不会"看似停滞"
3. **C-G6 / C-G7 充分并行** — 5+3 = 8 项独立小任务可分发给不同时间窗口 / 不阻塞主线
4. **资源弹性** — 主线慢 → 集中资源到 P0；主线快 → 释放资源做 P2/P3 尾部清理
5. **G1 内部多 Phase 范式** — 沿用 [DevTool 三件套 4 Phase 范式](../../memory-bank/archive/archive-TASK-20260430-04.md)（Phase 1 架构占位 + 接口预留 → Phase 2 基础 quad shader → Phase 3 glyph + image + DisplayList 翻译 → Phase 4 dirty rect 集成 + 性能优化）

### 3.2 推荐立项顺序（4 层时序）

```
时序 →

Layer 1 P0 主线:    G1 Phase 1 ──→ G1 Phase 2 ──→ G1 Phase 3 ──→ G1 Phase 4
                         ↓
                         └──→ G2 (可并行 G1 Phase 2-4)

Layer 2 P1 次线:    G3 资源加载 ───────────→ (任意时机 / 独立)
                                              ↓
                                         G4 嵌入式构建 (在 G2 后)

Layer 3 P2 长尾:    G5 DOM 节点动态 ──→ (任意时机 / 独立)
                    G6.a per-corner border-radius ──→ (任意时机 / 独立 / 5 并行)
                    G6.b @keyframes ──→ (...)
                    G6.c z-index stacking ──→ (...)
                    G6.d position: fixed 真支持 ──→ (...)
                    G6.e pointer-events: none 真支持 ──→ (...)

Layer 4 P3 末梢:    G7.a WebP ──→ (任意时机 / 独立 / 3 并行)
                    G7.b 双线性缩放 ──→ (...)
                    G7.c 异步图片加载 ──→ (...)
                    G8 R9 HitTest ──→ (任意时机 / 独立)
                    G9 性能优化收口 ──→ (渐进式 / 多任务集合)
```

### 3.3 风险缓解

| 风险 | mitigation |
|---|---|
| G1 Phase 1 架构占位失败（接口设计错误）| Phase 1 必走完整 spec + plan + creative + 用户批准 / build 前足够 brainstorm / 不允许跳过 |
| G2 DRM/KMS 平台特定 bug 难以本地复现 | 必须在真嵌入式 Linux 设备（树莓派 / 工业板）做 dogfood smoke test / 不允许仅 WSLg 测试 |
| 多并行子任务之间 commit 冲突（如同一 CMakeLists.txt）| 沿用 [TASK-20260502-02 archive §6 «共享文件改动隔离»](../../memory-bank/archive/archive-TASK-20260502-02.md) 范式 / 各子任务 reflection 阶段强制 audit |
| 总 archive 数量爆炸（~13-15）影响 productContext 可读性 | 每层 archive 完成后做 1 次"层级汇总"文档（如 `archive-mvp-c-layer1-summary.md`）/ 长期可读性维持 |

---

## 4. 算法/坐标约定一图（§d.1 audit）

**N/A**（本 creative 蓝图层级 / 无具体算法）—— 但 **G1 OpenGL ES 子任务的 creative 阶段必须做 §d.1 audit**：

- GL viewport 坐标系（左下原点 / Y 向上）vs DisplayList 坐标系（左上原点 / Y 向下）→ **2 个坐标系 / 必须画统一坐标约定单一图**
- shader uniform `u_resolution` / `u_offset` / `u_dpi` 多 uniform 协调

> **注：** 本 creative 在 §6.1.D 已**明示**子任务 creative 阶段必做 §d.1 audit / 不能省略。

---

## 5. 算法伪码累积语义 explicit method（§d.2 audit）

**N/A**（本 creative 蓝图层级 / 无算法伪码）—— 但 **G1 OpenGL ES 的 DisplayList → GL command 翻译涉及累积量**（`pending_quads` / `bound_textures` / `pending_state_changes`）→ **G1 子任务 creative 阶段必须做 §d.2 audit**：

- `pending_quads` = `Vec<Quad>` 累积 / 必须用 `pending_quads.PushBack(quad)` 不能用 `pending_quads = vec`
- `current_program` = GL program ID 覆盖语义 / 必须显式 `Replace(new_program)` 注释

> **注：** 本 creative 在 §6.1.D 已**明示**子任务 creative 阶段必做 §d.2 audit。

---

## 6. 实现指导（9 项 gap 详细补全方案）

### 6.1 C-G1：OpenGL ES / Vulkan 硬件渲染后端（核心 P0 / Level 4 多 Phase）

#### 6.1.A 架构占位

```
┌─────────────────────────────────────────────────────────────────┐
│  既有架构（MVP-A/B）                                              │
│                                                                 │
│  Application                                                    │
│    └── View                                                     │
│          └── DisplayList (软件 / drawcall list)                 │
│                └── SoftwareCanvas (CPU pixel manipulation)      │
│                      └── MemorySurface / SDL2Surface (CPU buffer)│
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│  扩展架构（MVP-C C-G1 后）                                       │
│                                                                 │
│  Application                                                    │
│    └── View                                                     │
│          └── DisplayList (drawcall list — 后端无关)             │
│                └── Renderer (抽象基类) ✨ NEW                    │
│                      ├── SoftwareRenderer (复用 SoftwareCanvas) │
│                      └── GLESRenderer ✨ NEW                     │
│                            └── GLContext (EGL / WGL / NSGL)    │
│                                  └── GLSurface ✨ NEW           │
│                                        └── (window 系统耦合)    │
└─────────────────────────────────────────────────────────────────┘
```

#### 6.1.B 4 Phase 划分

| Phase | 范围 | plan ×0.6 | 验收 |
|:-:|---|:-:|---|
| **Phase 1 架构占位** | `Renderer` 抽象基类 + `GLESRenderer` 空骨架 + `GLContext` EGL 桥 + `GLSurface` 抽象 + CMake `VX_RENDERER=gles\|software` 双轨 | ~5-10 h | A14 OFF binary 不膨胀 / SoftwareRenderer 维持 1284/1091 baseline / GLES OFF stub 可链接 |
| **Phase 2 基础 quad shader** | vertex + fragment shader / quad VBO / `glDrawArrays` 渲染 1 个矩形 | ~5-10 h | smoke test 渲染红色矩形 PNG 对比 |
| **Phase 3 glyph + image + DisplayList 翻译** | glyph atlas texture / image texture / DisplayList drawcall 全套翻译 / FreeType + HarfBuzz 集成 | ~10-20 h | hello.cc 内容在 GLES 下渲染 |
| **Phase 4 dirty rect 集成 + 性能优化** | scissor test / dirty rect 集成 / 60fps 验证（车载 HMI 复杂界面）| ~10-20 h | mvp-c-embedded smoke / 60fps 真设备验证 |

总估时：~30-60 h plan ×0.6（如 reflection 阶段实测 0.07-0.10× standalone-AI 加速因子可降至 ~5-15 h 实际 wall-clock）

#### 6.1.C 关键设计决策（待 G1 子任务 creative 阶段细化）

- shader 语言：GLSL ES 3.0（OpenGL ES 3.0+ / 嵌入式标准 / 兼容 ANGLE / WebGL2）
- shader 预编译 vs 运行时编译：**运行时编译**（开发期灵活 / 性能影响 ms 级 / 启动时一次性）
- glyph atlas 策略：动态扩展（参考 dear imgui / Skia）vs 预生成（参考 Veloxa MVP-A FreeType cache）
- DisplayList → GL command 翻译策略：1:1 映射 vs batch 优化
- color space：sRGB linear vs sRGB encoded（车载 HMI 推荐 sRGB encoded 与 SDL2/系统一致）
- DPR / HiDPI 处理：`u_dpi` uniform / 在 vertex shader 缩放

#### 6.1.D §d.1 / §d.2 audit 强制（G1 子任务 creative 阶段）

```
✅ §d.1 强制做 — GL 坐标系（左下原点 / Y 向上）vs DisplayList 坐标系
                  （左上原点 / Y 向下）+ shader uniform 坐标转换

✅ §d.2 强制做 — pending_quads / pending_textures / pending_state_changes
                  累积量必须用 explicit method (PushBack / Replace / Clear)
                  + 累积量语义对照表
```

---

### 6.2 C-G2：DRM/KMS 嵌入式 Linux 后端（核心 P0 / Level 3-4）

#### 6.2.A 架构占位（复用既有 Surface 抽象）

```
既有 Surface 抽象（已在 spec 2026-04-25-sdl2-window-backend §未来工作 中预留）：

  Surface (抽象)
    ├── MemorySurface ✅
    ├── SDL2Surface ✅
    └── DRMSurface ✨ NEW

  EventLoop (抽象)
    ├── HeadlessEventLoop ✅
    ├── SDL2EventLoop ✅
    └── DRMEventLoop ✨ NEW
        └── 使用 libinput 处理输入 / 直接 framebuffer 显示
```

#### 6.2.B 实施范围

| 范围 | plan ×0.6 | 验收 |
|---|:-:|---|
| `DRMSurface` + EGL on DRM 集成 | ~3-5 h | smoke 在 DRM 设备 framebuffer 显示矩形 |
| `DRMEventLoop` + libinput 输入桥 | ~3-5 h | 鼠标 / 触屏事件正确分发 |
| `Application::CreateDRMBackend()` factory | ~1-2 h | API 公开 |
| 嵌入式 Linux smoke test（label `mvp-c-embedded`）| ~2-4 h | 真设备 60fps 验证 |
| CMake `VX_PLATFORM_DRM=ON` + 工具链交叉编译 | ~1-2 h | yocto / buildroot 集成范例 |

总估时：~10-20 h plan ×0.6

#### 6.2.C 依赖

- libdrm / libgbm（DRM/KMS 用户空间库）
- libinput（输入处理）
- libudev（设备发现）
- C-G1 GLES Renderer 已就位（DRM Surface 渲染依赖 GLES context）

#### 6.2.D 风险

- 平台特定 bug（如 GBM allocator 行为差异 vs Mali / Adreno / Intel iGPU）
- 必须在真设备 dogfood / 不允许仅 QEMU 模拟测试

---

### 6.3 C-G3：资源加载策略（P1 / Level 3 蓝图 + 实施）

#### 6.3.A 三档资源策略

| 策略 | 描述 | 适用场景 |
|---|---|---|
| **打包** | 编译期内嵌资源（CMake `add_custom_command` + `bin2c`）| 嵌入式 / 资源不变 / 启动最快 |
| **文件系统** | 运行时从 sandbox 路径加载 | 桌面开发 / 资源频繁更新 / Hot Reload 配合 |
| **混合** | 编译期内嵌默认资源 + 运行时文件系统覆盖 | **推荐**（兼顾启动速度 + 开发灵活性）|

#### 6.3.B 推荐：混合策略

API 设计：

```cpp
// 优先从内嵌资源加载，失败时 fallback 到文件系统
auto data = vx_resource_load("logo.png");
//   1. 先查 internal binary blob registry (compile-time)
//   2. miss 后查 user-configured filesystem path (runtime)
//   3. 双 miss 返回 error
```

总估时：~5-10 h plan ×0.6

---

### 6.4 C-G4：嵌入式 Linux 平台构建支持（P1 / Level 3）

#### 6.4.A 范围

- 工具链交叉编译（`aarch64-linux-gnu-gcc` / `arm-linux-gnueabihf-gcc`）
- sysroot 配置（vcpkg / Conan 跨平台依赖）
- yocto layer 范例 / buildroot package 范例
- CI 矩阵扩展（GitHub Actions 加 ARM64 build job）

总估时：~5-10 h plan ×0.6（在 G2 完成后启动 / 依赖 G2 已 dogfood 实证）

---

### 6.5 C-G5：DomBindings 节点动态创建删除（P2 / Level 3）

#### 6.5.A 范围

- `document.createElement(tag)` API
- `element.removeChild(child)` / `element.appendChild(node)` API
- TextNode binding（`document.createTextNode`）
- 配套 mutation observer（可选 / 推荐留待长期）

#### 6.5.B 与 B-G3（innerHTML setter）的关系

innerHTML 是 declarative 替换 / createElement+appendChild 是 imperative 构造 / 两者**功能重叠但 API 互补**。在 dogfood 场景下 B-G3 已覆盖 80% 用例 / C-G5 是 W3C 完整性补充。

总估时：~3-5 h plan ×0.6

---

### 6.6 C-G6：CSS 高级特性 5 项（P2 / 5 个独立 L2-3 任务）

| 子项 | 描述 | Level | plan ×0.6 |
|:-:|---|:-:|:-:|
| **G6.a** | per-corner border-radius（`border-top-left-radius` 等 4 角独立）| L2 | ~1-2 h |
| **G6.b** | `@keyframes` Animations（vs 仅 transitions）| L3 | ~3-5 h |
| **G6.c** | 完整 z-index stacking context（vs 简化扁平化）| L3 | ~3-5 h |
| **G6.d** | `position: fixed` 真支持（vs 简化 absolute）| L2-3 | ~2-3 h |
| **G6.e** | `pointer-events: none` 真支持（与 R9 EventManager 集成）| L2 | ~1-2 h |

总估时：~10-17 h plan ×0.6（5 项可分发到 5 个并行时间窗口）

---

### 6.7 C-G7：图像扩展 3 项（P3 / 3 个独立 L2 任务）

| 子项 | 描述 | Level | plan ×0.6 |
|:-:|---|:-:|:-:|
| **G7.a** | WebP 解码（libwebp 集成）| L2 | ~2-3 h |
| **G7.b** | 双线性图片缩放（vs MVP 最近邻）| L2 | ~2-4 h |
| **G7.c** | 异步图片加载（thread pool + 状态机）| L2-3 | ~2-5 h |

总估时：~6-12 h plan ×0.6

---

### 6.8 C-G8：R9 EventManager HitTest 改造（P3 / Level 2-3）

复用 [TASK-20260502-02 P3 #2 R9 design](../../memory-bank/archive/archive-TASK-20260502-02.md)：

- HUD `pointer-events: none` 真支持（不依赖 `data-passthrough` HTML 属性占位）
- 与 G6.e 协同（CSS 解析 → EventManager 行为）

总估时：~1.5-2 h plan ×0.6

---

### 6.9 C-G9：性能优化收口（P3 / 多任务集合）

来源：

- TASK-20260502-02 P3 #1（#35 阶段 2 拆 LayoutEngine）
- TASK-30-03 R3+ #1（image_decoder 安全三件套）
- TASK-30-03 R3+ 其他 13 项 P1 候选

总估时：~10-30 h plan ×0.6（多任务集合 / 渐进式）

---

## 7. 推荐 example 范例（hello_mvp_c.cc / hello_drm.cc 占位 / 非本任务实施）

```cpp
// examples/hello_mvp_c.cc — MVP-C 真嵌入式部署 demo
// 验收：DRM/KMS + GLES 后端 / 60fps 复杂界面

#include <veloxa/veloxa_api.h>

int main() {
  // C-G2 DRM/KMS event loop（无 X11 / Wayland）
  auto* event_loop = vx_event_loop_create_drm();
  auto* view = vx_view_create();

  // C-G1 GLES Renderer surface
  auto* surface = vx_surface_create_drm_gles(/*plane=*/0);

  // 复杂车载 HMI 界面（参考真车载 Demo 设计）
  vx_view_load_html(view, R"(
    <div class="dashboard">
      <div class="speed">120 km/h</div>
      <div class="rpm">3500 rpm</div>
      <div class="map"><img src="map.webp" /></div>  <!-- C-G7.a WebP -->
      <div class="status pulse"></div>  <!-- C-G6.b @keyframes -->
    </div>
  )");

  vx_view_load_css(view, R"(
    .dashboard { display: flex; ... }
    .pulse { animation: pulse 1s infinite; }
    @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.3; } 100% { opacity: 1; } }
  )");

  vx_event_loop_run(event_loop);
  return 0;
}
```

---

## 8. 安全考量

### 8.1 与 MVP-B 范围一致的威胁面（沿用）

完全保留 MVP-A + MVP-B 已就位的 5+T 威胁面 mitigation。

### 8.2 MVP-C 引入的新威胁面

| Threat | 来源 | mitigation |
|---|---|---|
| **T9 GL command injection** | 如允许 user JS 直接 dispatch GL command 需限制 | DomBindings**不**暴露 GL primitives / GLES Renderer 仅接受 DisplayList 输入 / JS 层无 GL 直访 |
| **T10 DRM permission** | KMS 设备需 root / 容器化部署需 cap_sys_admin | 文档明示部署权限要求 / 不在 Veloxa 库内自动 escalate |
| **T11 shader DoS** | Hostile shader 可能 GPU 死循环 | shader 仅由 Veloxa 内部提供（不接受 user-defined shader）/ 第三方扩展时考虑 timeout |
| **T12 WebP 解析 RCE** | libwebp CVE 历史 | 沿用 [systemPatterns «image_decoder 安全三件套»](../../memory-bank/systemPatterns.md) 范式 / 长度上限 + try/catch 包装 + redacted error log |
| **T13 异步图片加载竞态** | C-G7.c thread 状态机竞态 | 设计阶段做 race detector audit + thread-sanitizer build job |

### 8.3 安全考量与 systemPatterns 协同

- T9-T13 全部需在各自子任务 plan 阶段做完整威胁建模（沿用 [`.cursor/rules/skills/security.mdc`](../../.cursor/rules/skills/security.mdc)）
- C-G1/G2 因为是核心架构 / 必须做完整威胁建模（**Security-related 标识**）
- 其他 gap 视各自暴露面决定是否 Security-related

---

## 9. 长期愿景对接（超 MVP-C）

MVP-C 闭环后可顺势启动：

| 子系统 | 触发条件 | 量级 |
|---|---|:-:|
| Vulkan 渲染后端 | 现代 GPU 优化需求（vs OpenGL ES）| Level 4 |
| DevTool Phase E · JS Debugger | 用户 / 第三方需求驱动 | Level 4 |
| DevTool Phase F · CDP 远程调试 | 第三方 IDE 集成需求 | Level 3-4 |
| DevTool Phase G · 完整 UI 编辑器 | 长期愿景 | Level 4+ |
| 完整 Web Animations API | 复杂动画需求驱动 | Level 4 |
| SVG 支持 | 矢量图标需求驱动 | Level 3-4 |
| 完整 ICU 国际化 / RTL | 多语言需求驱动 | Level 3 |

---

## 10. 与既有 systemPatterns 协同度自检

| # | systemPattern | 本 creative 对应 |
|:-:|---|---|
| 1 | Level 4 蓝图任务 V2=a 工作流变体 | ✅ 主路径 — G1 子任务即 Level 4 多 Phase |
| 2 | DevTool 4 Phase 范式 | ✅ §6.1.B G1 复用 4 Phase 范式 |
| 3 | DevTool 5 大可复用范式（双 UpdateManager / 双层 API / lazy-attach C ABI / canvas Translate / 资源嵌入）| ✅ G1 GLESRenderer + G2 DRMSurface 沿用 lazy-attach + canvas Translate + 资源嵌入 |
| 4 | Surface / EventLoop 抽象基类范式 | ✅ §6.2.A G2 直接复用预留接口 |
| 5 | Phase 0 投入越深 / build phase 越快定律 sept-evidence | ✅ 各子任务 plan 阶段沿用（特别 G1 架构占位 Phase 1）|
| 6 | commit body Source 溯源 + 实测数据 quad-evidence | ✅ 各子任务 commit 沿用 |
| 7 | A14 链接闭包零字节增长守门 | ✅ G1/G2 OFF 时 binary 不膨胀（VX_RENDERER=software / VX_PLATFORM_DRM=OFF）|
| 8 | reflection 沉淀回流模式 | ✅ 各子任务 reflection 阶段更新 archive §6 estimating 校准范围 |
| 9 | image_decoder 安全三件套 | ✅ §8.2 T12 WebP 沿用 |
| 10 | 共享文件改动隔离（CMakeLists.txt / vcpkg.json）| ✅ §3.3 风险缓解 / 多并行子任务必检 |
| 11 | systemPatterns «待定架构决策» 清零 | ✅ §6.3 C-G3 资源加载策略最终决策 |

**协同度自评：** 11/11（100% — 7 ✅ 直接命中 + 4 引用 / 0 新模式入库）

---

## 11. 验收标准（本 creative 文档级）

- [x] 设计挑战清晰（§1 问题陈述 + 6 项约束 + 5 项成功标准）
- [x] ≥2-3 个方案探索（§2 / 3 方案 A 大爆炸 / B 严格串行 / C 分层并行）
- [x] 方案对比表（§2.x / 8 维度对比）
- [x] 推荐方案 + 论证（§3 / C 分层并行 + 5 项理由 + 4 层时序图 + 4 项风险缓解）
- [x] §d.1 / §d.2 audit（§4 + §5 / 标 N/A 已 audit + 明示子任务必做）
- [x] 实现指导（§6 / 9 项 gap 详细 / G1 4 Phase + G2 实施范围 + G3-G9 各自范围）
- [x] hello_mvp_c.cc / hello_drm.cc example 占位（§7）
- [x] 安全考量（§8 / 沿用 MVP-B 5+T + 5 项新威胁面 T9-T13 + 全 mitigation）
- [x] 长期愿景对接（§9 / 7 项超 MVP 候选）
- [x] systemPatterns 协同度自检 ≥ 11 模式（§10 / 11/11 100%）

---

**文档结束。** 本 creative 由 [TASK-20260504-01 plan §4.4 子任务 4](../../docs/plans/2026-05-04-mvp-scope.md) 直接产出。下一步：commit 4 落盘 + 进入子任务 5 finalize（README + projectbrief + productContext + MB 全量同步）。
