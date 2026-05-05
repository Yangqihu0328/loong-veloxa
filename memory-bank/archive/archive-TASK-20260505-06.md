# 归档：TASK-20260505-06 G1.2 `GLESDisplay` 抽象 + `Sdl2EGLDisplay` 实施

**日期：** 2026-05-05
**任务 ID：** `TASK-20260505-06`
**复杂度级别：** Level 3 实施类（新平台抽象 + SDL2 子类实施 / 跨 G1+G2 桥接接口预留）
**状态：** ✅ 已完成
**任务定位：** **MVP-C 战略主线第二个实施任务** / GLES 蓝图实施第二步 / **G1.1 D1=A 推迟点正式落地**（首次引入 EGL/GLES dep）

---

## 1. 任务概述

### 1.1 目标

落地 [G1 OpenGL ES 硬件渲染后端蓝图](../../docs/plans/2026-05-05-gles-renderer-blueprint.md#32-子任务-g12--glesdisplay-抽象--sdl2egldisplay-实施) 子任务 G1.2：

- **`GLESDisplay` 纯虚抽象**（B8 G2 桥接接口 / G2 共享 / 11 virtual 方法）
- **`Sdl2EGLDisplay` SDL2 子类实施**（SDL_GL_SetAttribute ×6 + SDL_GL_CreateContext + glGetString/glGetStringi）
- **8 单测 + 反向探针**（驱动无关 / Mesa headless 兼容）
- **EGL/GLES dep 正式接入**（G1.1 D1=A 推迟点正式落地 / pkg_check_modules）

### 1.2 范围

- **创建：** `veloxa/platform/gles_display.h` + `veloxa/platform/sdl2/sdl2_egl_display.{h,cc}` + `tests/platform/sdl2_egl_display_test.cc`
- **修改：** `veloxa/platform/sdl2/CMakeLists.txt` + `tests/CMakeLists.txt`
- **不影响：** `veloxa/platform/CMakeLists.txt` 顶层（plan §3.2 偏差 #1 校正）
- **依赖引入：** `pkg_check_modules(EGL REQUIRED egl) + pkg_check_modules(GLESv2 REQUIRED glesv2)`（与 HARFBUZZ pattern 一致）

---

## 2. 技术方案

### 2.1 整体方案

**`GLESDisplay` 纯虚 header-only 抽象 + `Sdl2EGLDisplay` SDL2 实施 + ::testing::Environment headless fixture：**

- **抽象层**：`GLESDisplay` 11 个 virtual（Initialize / Shutdown / IsValid / MakeCurrent / DoneCurrent / SwapBuffers / IsContextLost / RestoreContext / HasExtension / gles_major/minor_version）+ G2 桥接预留（GpuFence 等）
- **实施层**：`Sdl2EGLDisplay : public GLESDisplay` 用 SDL2 GL APIs（SDL_GL_SetAttribute / SDL_GL_CreateContext / SDL_GL_MakeCurrent / SDL_GL_SwapWindow / SDL_GL_DeleteContext）+ glGetString/glGetStringi 解析版本与扩展
- **测试层**：8 TEST_F 覆盖 happy path + RestoreContext + Shutdown 幂等 + 反向探针（驱动无关 nullptr-window）+ ::testing::Environment 全局 SDL_Init + SDL_VIDEODRIVER=offscreen 双保险（test setup + ctest PROPERTIES ENVIRONMENT）

### 2.2 8 D 决策矩阵

| # | 决策项 | 选择 | 理由概要 |
|:-:|---|---|---|
| **D1** | testing fixture | **A SDL_VIDEODRIVER=offscreen** | Mesa swrast 路径成熟 / SDL2 2.0.16+ 原生支持 / CI 友好 |
| **D2** | ctx lost 覆盖 | **B 含 RestoreContext** | 嵌入式 G2 robustness 必需 / Shutdown + Initialize 链路 |
| **D3** | ext cache 策略 | **B eager std::unordered_set** | Initialize 一次 O(N) 构 / O(1) 查询 / 嵌入式 ~kB 可接受 |
| **D4** | 反向探针 | **C inline test SDL_GL_SetAttribute(MAJOR=99)** | 不污染源码 / REFACTOR 阶段实施细节调整为 nullptr-window（驱动无关 / Mesa silent fallback 经验）|
| **D5** | 偏差处理 | **A plan §0.4 详细校正** | 沿用 brainstorming P1.3 主动 push-back / 3 偏差全列 |
| **D6** | dep 声明位置 | **A sdl2/ 局部** | 与 HARFBUZZ pattern 一致 / 顶层 0 修改 / YAGNI |
| **D7** | commit 粒度 | **A 单 feat commit** | 6 文件强相关 / 0 collateral / git bisect 友好 |
| **D8** | P0 协议 | **A plan + MB 单 commit 自吃狗粮** | quint → sext-evidence 第 6 数据点 / 实施类 Level 3 首次实证 |

### 2.3 plan §3.2 偏差校正（brainstorming P1.3 主动 push-back triple-evidence）

VAN 阶段 Phase 0 audit 11/11 实证 + 3 偏差识别 → plan §0.4 详细校正 → build 阶段 100% 实施成功。

| # | 偏差 | plan §0.4 校正方向 | build 阶段实施 | 状态 |
|:-:|---|---|---|:-:|
| 1 | plan §3.2 改顶层 platform/CMakeLists.txt | sdl2/CMakeLists.txt（GLESDisplay.h header-only / 隐式头扫描）| ✅ 顶层 0 修改 / sdl2/ +13 -1 | ✅ 100% |
| 2 | plan §3.2 tests/platform/sdl2/ 子目录 | 扁平 tests/platform/（与 5 既有 _test.cc 一致）| ✅ tests/platform/sdl2_egl_display_test.cc | ✅ 100% |
| 3 | plan §3.2 headless fixture 未明示 | ::testing::Environment + SDL_VIDEODRIVER=offscreen | ✅ Sdl2EglEnvironment + SDL_setenv + ctest PROPERTIES ENVIRONMENT 双保险 | ✅ 100%+ |

---

## 3. 实现摘要

### 3.1 文件变更

| 操作 | 文件路径 | LOC | 说明 |
|---|---|:-:|---|
| 创建 | `veloxa/platform/gles_display.h` | 70 | GLESDisplay 11 virtual 抽象（B8 G2 桥接接口）|
| 创建 | `veloxa/platform/sdl2/sdl2_egl_display.h` | 65 | Sdl2EGLDisplay : public GLESDisplay |
| 创建 | `veloxa/platform/sdl2/sdl2_egl_display.cc` | 146 | SDL_GL_SetAttribute ×6 + CreateContext + glGetString 版本解析 + eager ext cache + RestoreContext |
| 创建 | `tests/platform/sdl2_egl_display_test.cc` | 189 | 8 TEST_F + ::testing::Environment + SDL_VIDEODRIVER=offscreen |
| 修改 | `veloxa/platform/sdl2/CMakeLists.txt` | +13 -1 | sdl2_egl_display.cc + EGL/GLESv2 pkg_check_modules + PRIVATE link |
| 修改 | `tests/CMakeLists.txt` | +12 | sdl2_egl_display_test 注册（gles config only / SDL_VIDEODRIVER=offscreen）|
| **总计** | **6 files** | **+495 -1** | **plan 估 ~520 行 / ×0.95 反向偏低** |

### 3.2 关键实现片段

**GLESDisplay 抽象层（gles_display.h）：**

```cpp
namespace vx::platform {
class GLESDisplay {
 public:
  virtual ~GLESDisplay() = default;
  virtual vx::Status Initialize() = 0;
  virtual void Shutdown() = 0;
  virtual bool IsValid() const = 0;
  virtual vx::Status MakeCurrent() = 0;
  virtual void DoneCurrent() = 0;
  virtual vx::Status SwapBuffers() = 0;
  virtual bool IsContextLost() const = 0;
  virtual vx::Status RestoreContext() = 0;
  virtual bool HasExtension(const char* name) const = 0;
  virtual vx::i32 gles_major_version() const = 0;
  virtual vx::i32 gles_minor_version() const = 0;
};
}
```

**Sdl2EGLDisplay 实施层片段（sdl2_egl_display.cc）：**

```cpp
vx::Status Sdl2EGLDisplay::Initialize() {
  if (window_ == nullptr) {
    return vx::Status(vx::StatusCode::kInvalidArgument,
                      "Sdl2EGLDisplay: window must not be null");
  }
  // SDL_GL_SetAttribute ×6（CONTEXT_PROFILE_MASK = ES + MAJOR=3 + MINOR=0
  // + RED/GREEN/BLUE_SIZE=8 + DEPTH=24 + DOUBLEBUFFER=1）
  gl_context_ = SDL_GL_CreateContext(window_);
  // glGetString(GL_VERSION) 解析 gles_major_/minor_
  // Eager-populate ext_cache_ via glGetStringi(GL_EXTENSIONS)
}

vx::Status Sdl2EGLDisplay::RestoreContext() {
  Shutdown();
  return Initialize();
}

bool Sdl2EGLDisplay::HasExtension(const char* name) const {
  if (name == nullptr) return false;
  return ext_cache_.find(name) != ext_cache_.end();  // O(1)
}
```

**T8 反向探针（REFACTOR 阶段调整为驱动无关路径）：**

```cpp
TEST_F(Sdl2EglDisplayTest, ReverseProbe_NullWindow_RejectsInitialize) {
  Sdl2EGLDisplay display(nullptr);
  vx::Status s = display.Initialize();
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), vx::StatusCode::kInvalidArgument);
  EXPECT_FALSE(display.IsValid());
}
```

### 3.3 关键决策

1. **D6=A sdl2/ 局部 dep（替代顶层 platform/）** — 与 HARFBUZZ pkg_check_modules pattern 一致 / 顶层 0 修改 / vx_platform_sdl2 PRIVATE link 限定 / software config 不退化
2. **D3=B eager extension cache（first-evidence 范式）** — Initialize 一次性 `glGetStringi` + `unordered_set::reserve(N) + emplace` / HasExtension O(1) 查询 / 内存常驻 ~2-10 KB / 嵌入式可接受
3. **D2=B 含 RestoreContext** — Shutdown + Initialize 链路 / GL_CONTEXT_LOST_KHR 触发后可重建 context / 嵌入式 robustness 必需
4. **D4=C inline test 反向探针 → REFACTOR 调整为 nullptr-window** — Mesa swrast silent fallback for MAJOR_VERSION=99（不严格 enforce 版本协商）/ 改用驱动无关 nullptr-window 路径 / 100% 可重现 / **D4=C 范式 + 驱动严格性分层 first-evidence**
5. **plan §3.2 3 处偏差 plan §0.4 校正** — VAN 阶段 brainstorming P1.3 主动 push-back（CMake 位置 / 测试路径 / headless fixture）/ build 阶段 100% 实施成功 / 节省 ~60-90 min 事故修复

### 3.4 安全决策

**本任务不涉及安全变更。**

| 维度 | 状态 | 备注 |
|---|:-:|---|
| 输入验证 | ✅ | Initialize() 检查 window 非 nullptr / HasExtension 检查 name 非 nullptr |
| 认证/授权 | N/A | 仅本地 GL context |
| 数据保护 | N/A | 无敏感数据 |
| 依赖审计 | ✅ | 新引入 EGL 1.5 + GLESv2 3.2（system / Mesa 24.x / Debian/Ubuntu 标准包 / 0 已知 CRITICAL/HIGH 漏洞）|
| 错误信息脱敏 | ✅ | Status 消息仅含 SDL_GetError 字符串（无敏感信息）|
| 敏感数据处理 | N/A | — |

---

## 4. 测试覆盖

### 4.1 单测矩阵（8 TEST_F / 全 PASS ~150ms）

| # | 测试名 | 覆盖路径 |
|:-:|---|---|
| T1 | `Initialize_CreatesValidContext` | Happy path / window 有效 / context 创建成功 / IsValid==true |
| T2 | `Initialize_PopulatesGLVersion` | gles_major_/minor_ 解析正确（≥ 3.0）|
| T3 | `Initialize_PopulatesExtensionCache` | ext_cache_ 非空 / HasExtension 已知扩展返回 true |
| T4 | `MakeCurrent_DoneCurrent_RoundTrip` | MakeCurrent / DoneCurrent 配对调用 / 0 错误 |
| T5 | `SwapBuffers_NoOp_OnHeadless` | SwapBuffers 在 offscreen 不报错（best-effort）|
| T6 | `Shutdown_Idempotent` | 二次 Shutdown 无 crash / IsValid==false |
| T7 | `RestoreContext_RebuildsContext` | Shutdown → RestoreContext → IsValid==true / context 重建 |
| **T8** | **`ReverseProbe_NullWindow_RejectsInitialize`** | **驱动无关反向探针 / nullptr 拒绝 → kInvalidArgument** |

### 4.2 ctest 三 build 矩阵全 PASS ✅

| 配置 | 总数 | sdl2_egl_display_test | 状态 |
|---|:-:|:-:|:-:|
| Matrix A (DEVTOOL=ON / VX_RENDERER=software default) | 1303 → **1303** | 不编译（仅 gles config）| ✅ 不退化 |
| Matrix B (DEVTOOL=OFF / VX_RENDERER=software) | 1110 → **1110** | 不编译（仅 gles config）| ✅ 不退化 |
| Matrix C (DEVTOOL=ON / VX_RENDERER=gles) | **1345** PASS | **+8 (Test #1191-1198)** | ✅ 全 PASS |

### 4.3 TDD 三阶完整 ✅

| 阶段 | 实测 | 关键证据 |
|---|:-:|---|
| RED | ~30s | `sdl2_egl_display.h: No such file or directory` 编译失败明确 |
| GREEN | ~3 min | 8/8 TEST_F PASS / 总时长 ~150ms / 0 retry / 0 flaky |
| REFACTOR | ~5 min | T8 反向探针调整（MAJOR=99 silent fallback 不可靠 → nullptr 驱动无关）|

---

## 5. 经验教训

### 5.1 关键经验（从 reflection 提取）

1. **brainstorming P1.3 + writing-plans P1.6 双 triple-evidence 续延** — TASK-04 first + TASK-05 dual + TASK-06 triple / 模式参数稳定（~3 偏差/Level 3 实施类首步任务 / 抑制效果 ~30-90 min/偏差）
2. **跨决策协同度 + 实施忠实度双 100% first → dual-evidence** — 8/8 D 决策 0 偏差实施 / 累计 streak 128 → 136 历史最高续刷 / 前置条件：VAN Phase 0 audit 完整 + plan §0.4 详细校正完整 + build 阶段允许「精神一致下的实施细节调整」
3. **P0 协议适用性矩阵 6 类全覆盖（quint → sext-evidence）** — V2=a 蓝图 + 工作流元 + 实施类 Level 2 + Level 1 + Level 4 多 Phase + **实施类 Level 3** ✅ / 适用性矩阵已穷举完毕
4. **D4=C 反向探针「驱动严格性分层」first-evidence** — 驱动无关层（nullptr / 非法 enum / 不变量违反）必选 / 驱动严格层（请求非法版本 / 非法 attribute）只在生产 GPU CI 可信 / Mesa headless 不可信
5. **D3=B eager extension cache 范式 first-evidence** — Initialize 一次 O(N) 构 + O(1) 查询 / 通用「初始化期一次性枚举 + 后续频繁查询」字符串集场景
6. **LOC buffer 双向 ±25% 反向校准（dual-evidence）** — TASK-05 ×1.4 偏高 + TASK-06 ×0.95 偏低 / 单向 ×1.3-1.5 → 双向 [0.85, 1.5] / 偏高根因（drift guard / REFACTOR 涌现 / 多表格）+ 偏低根因（注释精简 / 测试 GTEST_SKIP）
7. **plan ×0.6 dec-evidence 第 10 数据点 + 实施类 Level 3 子档新增** — 三子档 + 1 蓝图变体矩阵成熟（V2=a 蓝图 0.02-0.05× / 工作流元 0.11-0.19× / 实施类 Level 2 0.6-1.0× / 实施类 Level 3 0.30-0.55× build / 0.60-0.70× 总线）

### 5.2 反复模式抑制成果

**0/8 已知反复模式 reflect 阶段全程保持 ✅** — 累计 19 模式连续抑制 / 历史新高继续刷新（VAN + Plan + Build + Reflect 4 阶段全 0 命中）

| 模式 | 抑制方式 |
|---|---|
| 计划文件清单与实际变更不一致 | plan §0.4 校正后 100% 一致 |
| 前置依赖未验证 | VAN Phase 0 audit 11/11 实证 |
| 非默认路径遗漏验证 | RestoreContext + 反向探针 + Shutdown 幂等全覆盖 |
| 测试隔离问题 | ::testing::Environment + 每 TEST_F 独立 SDL_Window + SDL_GL_ResetAttributes |
| 提交粒度偏离计划 | D7=A + D8=A 严格执行 |
| TDD 严格度与场景不匹配 | RED → GREEN → REFACTOR 三阶完整 |

### 5.3 改进建议落地状态

- **P0 立即（0 项）**：本任务实施类 + 决策协同 100% / 0 紧急改进
- **P1 下次（7 项）**：✅ **全部 reflect 阶段直接落地**（systemPatterns 8 段更新 + writing-plans P1.5 段升级 quint → sext-evidence）
- **P2 长期（5 项 + 历史 5 项 = 10 项）**：✅ 已迁移到 [`activeContext.md` 待处理事项段](../activeContext.md)（累计 P1×1 + P2×10 = 11 项 ≥ 4 阈值 / **下次工作流元任务 triple-evidence 候选**）

---

## 6. 7 范式里程碑

| # | 里程碑 | 状态 |
|:-:|---|---|
| 1 | **跨决策协同度 100% 第 15 次连续命中**（streak 128 → 136 历史最高续刷）| ✅ systemPatterns 已沉淀 |
| 2 | **实施忠实度 G1.1 first → G1.2 dual-evidence**（8/8 D 决策 0 偏差实施）| ✅ systemPatterns 已沉淀（与 #1 合段）|
| 3 | **plan ×0.6 dec-evidence 第 10 数据点 + 实施类 Level 3 子档新增** | ✅ systemPatterns 已沉淀 |
| 4 | **brainstorming P1.3 主动 push-back triple-evidence** | ✅ systemPatterns 已沉淀 |
| 5 | **writing-plans P1.6 spec vs code audit triple-evidence** | ✅ systemPatterns 已沉淀 |
| 6 | **P0 协议 sext-evidence 第 6 数据点 + 适用性矩阵 6 类全覆盖** | ✅ systemPatterns + writing-plans 已沉淀 |
| 7 | **D3=B eager ext cache + D4=C inline test 反向探针 + 驱动严格性分层 双 first-evidence** | ✅ systemPatterns 已沉淀 |

---

## 7. 度量数据汇总

### 7.1 时间维度

| 阶段 | plan ×0.6 估时 | 实测 | 系数 |
|---|:-:|:-:|:-:|
| VAN | ~10-15 min | ~10-15 min | ~0.7-1.0× |
| Plan | ~25-40 min | ~25-35 min | ~0.7-1.0× |
| Build | ~50-90 min | ~25-35 min | **~0.30-0.55× 极速区** |
| Reflect | ~15-20 min | ~15-20 min | ~1.0× |
| Archive | ~10-15 min | ~10-15 min（预估命中）| ~1.0× |
| **总线** | **~110-180 min** | **~85-120 min** | **~0.60-0.70× 标准极速区** |

vs GLES 蓝图 plan §3.2 估时 ~4-6 h = 240-360 min → 实测总线 ~0.30-0.40× 极速区

### 7.2 LOC 维度

- plan 估 ~520 行 / 实际 +495 -1 ≈ 494 行 / **×0.95 反向偏低**
- LOC 偏低根因：注释精简（gles_display.h ×0.875）+ 测试 GTEST_SKIP 路径精简（test ×0.86）

### 7.3 commit 维度

5 commits 分支总计：

| commit | 类型 | 说明 |
|---|---|---|
| `545fa1f` | chore(workflow) | initialize VAN |
| `39d2981` | chore(plan) | land plan + memory bank（P0 sext-evidence 候选）|
| `4b095c4` | feat(platform) | add GLESDisplay abstract + Sdl2EGLDisplay impl（主交付 +495 行 / 6 文件）|
| `5bc70a0` | chore(build) | finalize TASK-20260505-06 memory bank state |
| `1a663fc` | docs(reflect) | add reflection for TASK-20260505-06（+710 / -15 / 6 文件）|

---

## 8. 参考文档

- **设计规格：** [`docs/specs/2026-05-05-gles-renderer-blueprint-design.md`](../../docs/specs/2026-05-05-gles-renderer-blueprint-design.md)（§3.3.2 GLESDisplay 抽象设计）
- **蓝图计划：** [`docs/plans/2026-05-05-gles-renderer-blueprint.md`](../../docs/plans/2026-05-05-gles-renderer-blueprint.md)（§3.2 子任务 G1.2）
- **本任务实施计划：** [`docs/plans/2026-05-05-gles-display-sdl2-egl.md`](../../docs/plans/2026-05-05-gles-display-sdl2-egl.md)（11 段全覆盖 / D1-D8 决策矩阵 + plan §0.4 详细校正 + 8 TEST_F 步骤）
- **回顾文档：** [`memory-bank/reflection/reflection-TASK-20260505-06.md`](../reflection/reflection-TASK-20260505-06.md)（10 段全覆盖 / 7 P1 沉淀 + 5 P2 改进 / 自评 4.7/5）
- **上游归档：** [`memory-bank/archive/archive-TASK-20260505-05.md`](archive-TASK-20260505-05.md)（G1.1 CMake VX_RENDERER flag）+ [`memory-bank/archive/archive-TASK-20260505-03.md`](archive-TASK-20260505-03.md)（GLES 蓝图）

---

## 9. 长期维护建议

### 9.1 G1.3+ 接入路径

- **G1.3 Sdl2GLWindowSurface 接入**：本任务 Sdl2EGLDisplay 期望接受预先创建的 SDL_Window；G1.3 的 Sdl2GLWindowSurface 应负责 SDL_CreateWindow with SDL_WINDOW_OPENGL flag + 实例化 Sdl2EGLDisplay。这是最自然的接入路径。
- **GpuFence 接口（B8 G2 预留）**：当前仅注释占位 / G2 蓝图阶段细化时再形式化为 virtual。本任务不引入避免 SDL2 子类需要 stub 空实现。

### 9.2 测试演进

- **未覆盖路径（P3 候选）**：
  - 真实 GL_CONTEXT_LOST_KHR 触发 → IsContextLost() == true 路径（需要 GPU mock 或 KHR_robustness extension 触发器 / Mesa swrast 不支持）
  - SwapBuffers 后 backbuffer 内容验证（需要 glReadPixels / G1.3 任务覆盖）
- **驱动严格性分层应用**：未来任何 GL/Vulkan/EGL 平台抽象测试反向探针默认从「驱动无关层」起手（nullptr / 非法 enum / 不变量违反）/ 驱动严格层作为 P3 优化

### 9.3 性能基线

- **Initialize 一次性 O(N) extension cache 构造**：N 通常 ~50-500 / glGetStringi 每次 ~ns-us / 总耗时 ~us-ms 级 / 嵌入式可接受
- **HasExtension O(1)**：std::unordered_set 查询 ~ns 级 / 不影响热路径
- **内存常驻 ~2-10 KB**（取决于 extension 数量 / Mesa swrast 实测 ~50-100 extensions）

### 9.4 G2 桥接预留

- `GLESDisplay` 抽象设计已为 G2 DRM/KMS 后端预留：所有 11 virtual 方法均与 SDL2 解耦 / G2 子类只需实现 EGL 原生绑定（eglGetDisplay / eglCreateContext / eglMakeCurrent / eglSwapBuffers）即可复用本抽象层
- B8 G2 桥接接口完整性已在 spec §3.3.2 + 本任务 GLESDisplay.h 注释段标注

---

**归档结束。** TASK-20260505-06 G1.2 GLESDisplay + Sdl2EGLDisplay 实施 ✅ 已完成 / 7 范式里程碑达成 / 5 commits / 6 文件 / +495 LOC / 8 单测 / 0 退化 / 0 lint / 0 反复模式命中 / Memory Bank 重置为空闲状态 / 待 G1.3 Sdl2GLWindowSurface 接入。
