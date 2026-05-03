# TASK-20260503-05 — QuickJS Interrupt Handler + SetEvalInterruptBudget API（技术债 #44 组件 1 Phase 2 闭环）

| 字段 | 值 |
|---|---|
| **任务 ID** | `TASK-20260503-05` |
| **创建日期** | 2026-05-03 |
| **复杂度级别** | **Level 2**（多文件修改 / 需求清晰 / creative §组件 1 方案 C Phase 2 已预决策 / 无新设计决策 / 无新组件） |
| **安全相关** | ✅ 是（T1 mitigation 基础设施 — JS eval CPU DoS 缓解 / 不可信脚本执行预算保护） |
| **分支** | `feature/TASK-20260503-05-quickjs-interrupt-handler`（基于 main `72f011e` clean / VAN commit `67c8c81`） |
| **依赖文档** | `memory-bank/creative/creative-quickjs-host.md` §组件 1 方案 C Phase 2（已批准 2026-04-13） |
| **触及威胁** | T1 基础设施（JS eval CPU DoS mitigation — 解锁 TASK-30-04-D Console JS REPL 完整 T1 mitigation） |
| **触及技术债** | #44 QuickJS Interrupt Handler（本任务闭环组件 1 Phase 2 / 组件 3 `JSMallocFunctions` 留后续 TASK / `JS_SetMemoryLimit` 已落地无需本任务） |

---

## §0 — Phase 0 工具链与子系统关闭守门（极简 1 子段）

### §0.1 工具链快照（VAN 阶段已固化 / plan 阶段二次验证）

| 工具 | 版本 | 状态 |
|---|---|:-:|
| cmake | 4.2.3 | ✅ |
| gcc | 15.2.0 (Ubuntu 15.2.0-16ubuntu1) | ✅ |
| ninja | 1.13.2 | ✅ |
| ctest | 与 cmake 4.2.3 同捆 | ✅ |
| ld (binutils) | 2.46（hotfix `--start-group/--end-group` 已生效，TASK-20260503-01 沉淀） | ✅ |

### §0.2 ctest baseline 二次验证

```
DEVTOOL=ON: 1247/1247 PASS (6.42s)
```

→ 与 main `72f011e` 状态一致 ✅；本任务 E3 完成后预期 **1247 + 5 新测 = 1252**

### §0.3 关键 grep 实证（Phase 0 关键发现 — 反复模式 #1 第 10 次抑制）

#### §0.3.1 `JS_SetInterruptHandler` 签名实证（quickjs.h:1147-1149）

```c
/* return != 0 if the JS code needs to be interrupted */
typedef int JSInterruptHandler(JSRuntime *rt, void *opaque);
JS_EXTERN void JS_SetInterruptHandler(JSRuntime *rt, JSInterruptHandler *cb, void *opaque);
```

→ 签名固定 / 无失败返回 / opaque 用于传递 `QuickjsEngine*` 实例

#### §0.3.2 `JS_INTERRUPT_COUNTER_INIT` 实证（quickjs.c:474-476 + 8174）

```c
/* must be large enough to have a negligible runtime cost and small
   enough to call the interrupt callback often. */
#define JS_INTERRUPT_COUNTER_INIT 10000
// JS_HandleInterrupt 在 ctx->interrupt_counter 归零时触发 handler
```

→ **关键发现**：QuickJS 内部每 ~10000 字节码触发一次 handler；我们的 `interrupt_budget_` 是「**handler 调用次数预算**」（不是字节码预算）→ creative §组件 1 §选定方案「**10⁷ 级检查点**」字面值会导致 10⁷ × 10⁴ = 10¹¹ 字节码 ≈ 100-1000 秒死循环才中止 ❌ 与原意「死循环被中止」相悖 → **D8b 决策修正：默认 budget 重校准 = 10000**（详见 §3.D8b）

#### §0.3.3 `JS_ThrowInterrupted` 错误信息实证（quickjs.c:8165-8169）

```c
static void JS_ThrowInterrupted(JSContext *ctx) {
    JS_ThrowInternalError(ctx, "interrupted");
    JS_SetUncatchableError(ctx, ctx->rt->current_exception);
}
```

→ 错误消息**固定字符串「interrupted」+ uncatchable**（QuickJS 上游稳定）→ 可作为 EvalGlobal 错误消息识别 anchor

#### §0.3.4 `Status.h` StatusCode audit（D2 决策触发）

```cpp
// veloxa/foundation/base/status.h:12-19
enum class StatusCode : u8 {
  kOk = 0, kInvalidArgument, kOutOfMemory, kNotFound, kAlreadyExists, kInternal,
};
```

→ **无 `kAborted`/`kCancelled`/`kDeadlineExceeded`**；u8 空间充裕（0-255 / 当前仅 6 项）→ **D2 细化为 D2.B.1**：本任务在 `Status.h` 新增 `kAborted = 6`（E1 文件范围扩大 +1 行 / backwards-compatible / lower-risk）

### §0.4 配置矩阵假设（验收 §6 自我吃狗粮 — 来自 writing-plans.mdc «ctest 数量预期 config 矩阵» TASK-20260503-02 沉淀）

| Config | Baseline | 本任务后预期 | 备注 |
|---|:-:|:-:|---|
| **DEVTOOL=ON**（`build/`，VX_BUILD_BENCHMARKS=ON 隐含）| 1247 | **1252（+5）** | 5 新测全部进 quickjs_engine_test |
| **DEVTOOL=OFF**（min config）| 1082 | 1082（不变）| QuickjsEngine 在 vx_script，DEVTOOL=OFF 仍含 |
| **SDL2=ON**（`build-sdl2/`）| 1265 | 1270（+5）| 同 DEVTOOL=ON 路径 |

**验收门**：CP1 自审 DEVTOOL=ON 1252 PASS；CP2 自审 三 config 全不退化（实测 DEVTOOL=ON + DEVTOOL=OFF 双 verify，SDL2=ON 可选）

### §0.5 smoke 工具可用性

| 工具 | 状态 | 备用 |
|---|:-:|---|
| cmake/gcc/ninja/ctest | ✅ | — |
| `rg` | ❌ MISS | Grep 工具兜底（本会话已实证可用） |
| `python3` | ✅ | — |

---

## §1 — 任务总览

### §1.1 文件清单（3 源码 + 2 MB 文件）

| # | 文件 | 修改类型 | 估行数 | [影响前序测试]? |
|:-:|---|---|:-:|:-:|
| 1 | `veloxa/foundation/base/status.h` | **新增 1 enum** `kAborted` | +1 | ❌ |
| 2 | `veloxa/script/quickjs_engine.h` | 新增 API + 私有字段 | +12-15 | ❌ |
| 3 | `veloxa/script/quickjs_engine.cc` | 新增 InterruptCallback 静态函数 + Init 注册 + EvalGlobal 重置 + 错误识别 | +40-50 | ❌（既有 4 测继续 PASS） |
| 4 | `tests/script/quickjs_engine_test.cc` | 新增 5 测（D5 A+C 组合策略） | +80-100 | ❌（既有 4 测不动） |
| 5 | `memory-bank/techContext.md` | 更新 #44 条文（JS_SetMemoryLimit 已落地 + interrupt handler 本任务闭环 + JSMallocFunctions 仍记技术债） | +5-8 | — |

**[共享文件]：** `Status.h` 是 foundation 层共享头（被 56+ 文件 include），但本次仅追加 enum 不修改既有值 → ABI/API backwards-compatible。

### §1.2 子任务清单（5 子任务 + 2 CP）

| # | ID | 标题 | 文件 | 测试模式 | plan ×0.6 |
|:-:|---|---|---|---|:-:|
| 1 | E1 | API + StatusCode 扩展声明（包含 D2.B.1 audit 决策落实）| `Status.h` + `quickjs_engine.h` | [覆盖补充] | ~10 min |
| 2 | E2 | Init 注册 InterruptHandler + EvalGlobal 重置预算 + 静态 InterruptCallback + 错误识别 | `quickjs_engine.cc` | [覆盖补充] | ~30-40 min |
| 3 | E3 | 5 新测（D5 A+C 组合 + 4 维度覆盖）| `quickjs_engine_test.cc` | [覆盖补充] | ~30-40 min |
| — | 🛑 **CP1 自审** | E3 完成 → DEVTOOL=ON 1252 通过 / DEVTOOL=OFF 1082 不退化 | — | — | — |
| 4 | E4 | techContext #44 条文更新 | `techContext.md` | [文档调整模式] | ~5 min |
| 5 | E5 | finalize（MB 三件套同步 + 分支合并 ff）| `tasks.md` + `activeContext.md` + `progress.md` | — | ~10 min |
| — | 🛑 **CP2 自审** | E5 完成 → 5 commits Source 溯源完整 / 反复模式 0/7 自检 | — | — | — |
| **合计** | — | — | — | — | **~85-105 min（plan ×0.6）** |

**实测预期**：~25-45 min（落「极窄档延续高效区 0.30-0.45×」候选 / 含真实 C++ 代码 + 5 单测 RED-GREEN cycle / 比 02 文档极速 0.21× 略高 / 比 03 含 ctest 等待 0.42× 略低）

---

## §2 — 设计决策汇总（brainstorm 4 + B1-B8 = 12 决策表）

### §2.A — Brainstorm 决策（4 项 / D1+D2+D5+D8b — 用户已批准 2026-05-03 23:0X）

| # | 维度 | 决策 | 理由 |
|:-:|---|---|---|
| **D1** | `WasInterrupted()` Phase 2 vs Phase 3 边界 | **B：Phase 2 实现（私有 bool flag + getter）** | activeContext 已列入 Phase 2 范围；单测语义清晰；TASK-04 复用；最小扩展（+5 行）。creative 字面意图主要是「Phase 3 才与 Application 集成」而非「Phase 2 不能有该 API」 |
| **D2** | interrupt 错误码 / 消息设计 | **B.1：本任务新增 `StatusCode::kAborted = 6` enum 值 + EvalGlobal 识别「interrupted」字符串 + 改写消息为「script aborted (interrupt budget exhausted)」** | Phase 0 §0.3.4 audit 触发：Status.h u8 空间充裕（0-255 / 当前仅 6 项）；新增 enum backwards-compatible；StatusCode 直接区分 / 调用方只查 code / 单测干净 |
| **D5** | budget=0 死循环测避免 ctest hang 策略 | **A+C 组合**：A（budget=0 时短脚本 PASS / 反向「budget=0 关闭」）+ C（小 budget=10 准死循环 / 反向「budget=N 中止」）| 0 hang 风险 + 双向覆盖。std::thread 引入复杂度过高（与「车载嵌入式优先」原则相悖） |
| **D8b** | creative「10⁷ 级检查点」单位语义重定义 | **B：默认 `kDefaultInterruptBudgetCheckpoints = 10000`（handler 调用次数）** | Phase 0 §0.3.2 grep 实证 JS_INTERRUPT_COUNTER_INIT=10000；budget=10000 → 10⁸ 字节码 ≈ 100-500ms 死循环可中止（合理）；creative 文档字面 10⁷ 会导致 10¹¹ 字节码 ≈ 100-1000s 才中止 ❌ 与原意相悖 |

### §2.B — Plan 阶段 B1-B8 决策（8 项 / all_recommended 锁定）

| # | 维度 | 锁定 | 理由 |
|:-:|---|---|---|
| **B1** | 子任务执行顺序 | E1 → E2 → E3 (CP1) → E4 → E5 (CP2) | 标准 Level 2 串行；E1+E2+E3 是「同 quickjs_engine 模块 batch」高效区 |
| **B2** | 测试模式 | E1/E2/E3 [覆盖补充] / E4 [文档调整] / E5 — | E1+E2+E3 共同实现新 API + 新单测；E4 纯文档；E5 finalize |
| **B3** | 默认 budget 值常量 | `static constexpr usize kDefaultInterruptBudgetCheckpoints = 10000`（公开 / 暴露给单测） | D8b 决策；公开常量便于单测引用（避免 magic number） |
| **B4** | InterruptCallback 实现 | 静态 free function（cc anon namespace）+ opaque ptr → `QuickjsEngine*` + `std::atomic<int64_t>` 计数器 | creative §组件 1 §实现指导：「仅 atomic 递减 + return != 0 / 不调 EventLoop / 不分配复杂对象」 |
| **B5** | budget=0 语义 | 显式短路：budget=0 时 handler 内 `return 0` 不递减 / 不设 was_interrupted_。**正向语义状态优于阈值**（沿用 TASK-20260502-02 B.1.2 教训） | 「显式语义状态优于数值阈值」已入 techContext 安全测设计原则段。统一注册 handler 路径（D4 选项 a）减少代码分支 |
| **B6** | EvalGlobal 重置点 | 入口处一次性：`interrupt_counter_.store(budget, std::memory_order_relaxed); was_interrupted_ = false;` | 简单 / 无锁 / 单线程契约下 relaxed 足够 |
| **B7** | Phase 0 + Checkpoint | Phase 0 极简 1 子段（已落 §0）+ CP1（E3 后 DEVTOOL=ON 1252）+ CP2（E5 后 commits 溯源完整 + 反复模式自检） | Level 2 + 范围明确 → 极简；CP1 是 RED-GREEN 验收点，CP2 是工作流闭环点 |
| **B8** | commit 粒度 + 估时 | 5 commits 1/子任务 + Source 溯源前缀「`Source: TASK-20260503-05 creative-quickjs-host.md §组件 1 方案 C Phase 2`」+ 1 reflect + 1 archive = 7 commits 总 / **plan ×0.6 ~85-105 min → 实测预期 ~25-45 min** | git-workflow.mdc «Multi-subtask commit 拆分» 范式；commit body Source 溯源 11 commits 累计 → triple-evidence 升级 |

---

## §3 — 子任务详细 TDD 模板

### §3.E1 — API + StatusCode 扩展声明

**RED**: 不适用（声明性变更 / 无新可执行单测；E2 实现期间编译 RED 是 E1 的隐性 RED）

**GREEN（实现）**：

#### `veloxa/foundation/base/status.h` 第 12-19 行修改

```cpp
enum class StatusCode : u8 {
  kOk = 0,
  kInvalidArgument,
  kOutOfMemory,
  kNotFound,
  kAlreadyExists,
  kInternal,
  kAborted,  // [TASK-20260503-05] Operation aborted by interrupt/cancellation.
};
```

#### `veloxa/script/quickjs_engine.h` 改写

```cpp
#ifndef VELOXA_SCRIPT_QUICKJS_ENGINE_H_
#define VELOXA_SCRIPT_QUICKJS_ENGINE_H_

#include <atomic>
#include <string>

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

struct JSContext;
struct JSRuntime;

namespace vx {
namespace script {

// Minimal QuickJS embed: global eval only. No js_std_add_helpers (see
// memory-bank/creative/creative-quickjs-host.md).
//
// Eval interrupt budget: see creative-quickjs-host.md §组件 1 方案 C Phase 2.
// Default disabled (budget=0). Callers opt-in via SetEvalInterruptBudget(N>0).
// Each EvalGlobal resets the counter; if exhausted, eval aborts with kAborted.
class QuickjsEngine {
 public:
  // Default budget recommendation when caller opts in (10000 handler
  // invocations ≈ 10^8 bytecode ≈ 100-500ms infinite loop budget on modern
  // CPUs). See QuickJS internal JS_INTERRUPT_COUNTER_INIT=10000.
  static constexpr usize kDefaultInterruptBudgetCheckpoints = 10000;

  QuickjsEngine();
  ~QuickjsEngine();

  QuickjsEngine(const QuickjsEngine&) = delete;
  QuickjsEngine& operator=(const QuickjsEngine&) = delete;

  Status Init();
  void Shutdown();

  StatusOr<std::string> EvalGlobal(StringView source, StringView filename);

  // Set the per-EvalGlobal interrupt budget in handler-invocation count.
  // 0 disables the budget (default). Must be called after Init() to take
  // effect on subsequent EvalGlobal calls.
  void SetEvalInterruptBudget(usize max_checkpoints);

  // True iff the most recent EvalGlobal was aborted by the interrupt handler.
  // Reset to false at the start of every EvalGlobal call.
  bool WasInterrupted() const { return was_interrupted_; }

  JSContext* context() const { return ctx_; }

 private:
  static int InterruptCallback(JSRuntime* rt, void* opaque);

  JSRuntime* rt_ = nullptr;
  JSContext* ctx_ = nullptr;
  bool inited_ = false;

  usize interrupt_budget_ = 0;            // 0 = disabled.
  std::atomic<int64_t> interrupt_counter_{0};
  bool was_interrupted_ = false;
};

}  // namespace script
}  // namespace vx

#endif  // VELOXA_SCRIPT_QUICKJS_ENGINE_H_
```

**GREEN 验证**: `cmake --build build` 成功（仅头变更，编译通过）

**Commit**:

```
feat(script): add QuickJS interrupt budget API + kAborted status (E1)

Source: TASK-20260503-05 creative-quickjs-host.md §组件 1 方案 C Phase 2.

Add SetEvalInterruptBudget/WasInterrupted public API and kAborted
StatusCode in preparation for E2 (handler registration) and E3 (5 new
tests). API only; no behavior change yet (default budget=0 = disabled).
```

---

### §3.E2 — Init 注册 InterruptHandler + EvalGlobal 重置预算 + 静态 InterruptCallback + 错误识别

**RED**: 不适用（实现性变更 / E3 写测前 E2 实现先入）

**GREEN（实现）**：

#### `veloxa/script/quickjs_engine.cc` 改写

```cpp
#include "veloxa/script/quickjs_engine.h"

extern "C" {
#include "quickjs.h"
}

#include <string>

namespace vx {
namespace script {

namespace {

constexpr usize kMaxSourceBytes = 256 * 1024;
constexpr u64 kRuntimeMemoryLimitBytes = 32ULL * 1024 * 1024;

constexpr const char* kInterruptedQuickjsMessage = "interrupted";
constexpr const char* kAbortedHumanMessage =
    "script aborted (interrupt budget exhausted)";

std::string JsValueToUtf8(JSContext* ctx, JSValue val) {
  JSValue str = JS_ToString(ctx, val);
  if (JS_IsException(str)) {
    JS_FreeValue(ctx, str);
    return "stringify failed";
  }
  const char* utf8 = JS_ToCString(ctx, str);
  std::string out = utf8 ? utf8 : "";
  if (utf8) {
    JS_FreeCString(ctx, utf8);
  }
  JS_FreeValue(ctx, str);
  return out;
}

}  // namespace

QuickjsEngine::QuickjsEngine() = default;

QuickjsEngine::~QuickjsEngine() { Shutdown(); }

Status QuickjsEngine::Init() {
  if (inited_) {
    return Status::Ok();
  }
  rt_ = JS_NewRuntime();
  if (!rt_) {
    return Status(StatusCode::kOutOfMemory, "JS_NewRuntime failed");
  }
  JS_SetMemoryLimit(rt_, static_cast<size_t>(kRuntimeMemoryLimitBytes));
  // Always register the interrupt handler. Behavior switches by budget:
  //   budget == 0 -> handler returns 0 (no abort) cheaply.
  //   budget >  0 -> handler decrements counter; aborts when <= 0.
  // (creative §组件 1 §实现指导: callback only does atomic decrement + return.)
  JS_SetInterruptHandler(rt_, &QuickjsEngine::InterruptCallback, this);
  ctx_ = JS_NewContext(rt_);
  if (!ctx_) {
    JS_FreeRuntime(rt_);
    rt_ = nullptr;
    return Status(StatusCode::kOutOfMemory, "JS_NewContext failed");
  }
  inited_ = true;
  return Status::Ok();
}

void QuickjsEngine::Shutdown() {
  if (ctx_) {
    JS_FreeContext(ctx_);
    ctx_ = nullptr;
  }
  if (rt_) {
    JS_FreeRuntime(rt_);
    rt_ = nullptr;
  }
  inited_ = false;
}

void QuickjsEngine::SetEvalInterruptBudget(usize max_checkpoints) {
  interrupt_budget_ = max_checkpoints;
}

// Static callback registered via JS_SetInterruptHandler. opaque == this.
// Single-threaded contract per JSRuntime; std::atomic is defensive
// (creative §组件 1 §实现指导).
int QuickjsEngine::InterruptCallback(JSRuntime* /*rt*/, void* opaque) {
  auto* self = static_cast<QuickjsEngine*>(opaque);
  if (self == nullptr || self->interrupt_budget_ == 0) {
    return 0;  // Disabled: never abort.
  }
  // Decrement; abort when budget exhausted.
  if (self->interrupt_counter_.fetch_sub(1, std::memory_order_relaxed) <= 1) {
    self->was_interrupted_ = true;
    return 1;
  }
  return 0;
}

StatusOr<std::string> QuickjsEngine::EvalGlobal(StringView source,
                                                StringView filename) {
  if (!inited_) {
    return Status(StatusCode::kInvalidArgument, "QuickjsEngine not initialized");
  }
  if (source.size() > kMaxSourceBytes) {
    return Status(StatusCode::kInvalidArgument,
                  "script source exceeds max size");
  }
  // Reset interrupt state at every EvalGlobal entry.
  interrupt_counter_.store(static_cast<int64_t>(interrupt_budget_),
                           std::memory_order_relaxed);
  was_interrupted_ = false;

  const std::string fn(filename.data(), filename.size());
  JSValue val =
      JS_Eval(ctx_, source.data(), static_cast<size_t>(source.size()),
              fn.c_str(), JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(val)) {
    JS_FreeValue(ctx_, val);
    JSValue ex = JS_GetException(ctx_);
    std::string err = JsValueToUtf8(ctx_, ex);
    JS_FreeValue(ctx_, ex);
    // Identify QuickJS interrupt: "InternalError: interrupted" (uncatchable).
    if (was_interrupted_ ||
        err.find(kInterruptedQuickjsMessage) != std::string::npos) {
      was_interrupted_ = true;
      return Status(StatusCode::kAborted, kAbortedHumanMessage);
    }
    return Status(StatusCode::kInternal, err);
  }
  std::string out = JsValueToUtf8(ctx_, val);
  JS_FreeValue(ctx_, val);
  return out;
}

}  // namespace script
}  // namespace vx
```

**GREEN 验证**: `cmake --build build` 成功；`ctest -R QuickjsEngine` 既有 4 测继续 PASS（无回归）

**Commit**:

```
feat(script): register InterruptHandler + reset budget per EvalGlobal (E2)

Source: TASK-20260503-05 creative-quickjs-host.md §组件 1 方案 C Phase 2.

- Always register InterruptCallback in Init (budget=0 short-circuits cheaply).
- EvalGlobal entry: reset atomic counter to budget; clear was_interrupted_.
- InterruptCallback: atomic decrement; abort + set was_interrupted_=true on
  exhaustion. No EventLoop coupling, no allocation (per creative).
- EvalGlobal recognizes QuickJS "interrupted" message + sets kAborted +
  human-readable "script aborted (interrupt budget exhausted)".

Existing 4 QuickjsEngine tests: PASS (no regression).
```

---

### §3.E3 — 5 新测（D5 A+C 组合 + 4 维度覆盖）

**RED**: 写测前先确保编译通过 + 失败合理（如初始 InterruptCallback 未连线则测 1/4 失败）

**GREEN（实现）**：

#### `tests/script/quickjs_engine_test.cc` 末尾追加

```cpp
// === [TASK-20260503-05] Interrupt Handler tests (creative §组件 1 方案 C Phase 2) ===

// (1) D5.C 反向探针: budget=N small → quasi-infinite loop aborts within budget.
// Avoids ctest hang by setting tiny budget (100 handler invocations).
TEST(QuickjsEngineInterruptTest, AbortsLongLoopWhenBudgetExhausted) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  engine.SetEvalInterruptBudget(100);  // ~10^6 bytecode ceiling.

  // Long-running loop that would not naturally terminate within budget.
  // Body intentionally trivial (no allocation) to keep bytecode/iter low.
  auto r = engine.EvalGlobal(
      "var i = 0; while (true) { i = i + 1; if (i < 0) break; }",
      "<long-loop>");

  ASSERT_FALSE(r.ok());
  EXPECT_EQ(r.status().code(), StatusCode::kAborted);
  EXPECT_NE(r.status().message().find("aborted"), std::string::npos);
  EXPECT_TRUE(engine.WasInterrupted());
}

// (2) D5.A 正向: budget=0 (default disabled) → short legit script PASSes.
// Does NOT test infinite loop with budget=0 (would hang ctest by design).
TEST(QuickjsEngineInterruptTest, BudgetZeroDisablesInterrupt) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  // budget defaults to 0; no SetEvalInterruptBudget call.
  auto r = engine.EvalGlobal("1 + 2 + 3", "<short>");
  ASSERT_TRUE(r.ok()) << (!r.ok() ? r.status().message() : "");
  EXPECT_EQ(r.value(), "6");
  EXPECT_FALSE(engine.WasInterrupted());
}

// (3) D5.A: budget=default (10000) does NOT spuriously abort legit scripts.
TEST(QuickjsEngineInterruptTest, DefaultBudgetDoesNotAbortLegitScripts) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  engine.SetEvalInterruptBudget(
      QuickjsEngine::kDefaultInterruptBudgetCheckpoints);

  // Sum 1..1000 = 500500: bounded loop well within ~10^8 bytecode budget.
  auto r = engine.EvalGlobal(
      "var s = 0; for (var i = 1; i <= 1000; i++) s = s + i; s",
      "<bounded>");

  ASSERT_TRUE(r.ok()) << (!r.ok() ? r.status().message() : "");
  EXPECT_EQ(r.value(), "500500");
  EXPECT_FALSE(engine.WasInterrupted());
}

// (4) Repeated EvalGlobal automatically resets the budget counter.
// First call exhausts budget; second call (with same engine) succeeds.
TEST(QuickjsEngineInterruptTest, RepeatedEvalResetsBudget) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  engine.SetEvalInterruptBudget(100);

  // First call: long loop aborts.
  auto r1 = engine.EvalGlobal(
      "var i = 0; while (true) { i = i + 1; if (i < 0) break; }",
      "<long-loop-1>");
  ASSERT_FALSE(r1.ok());
  EXPECT_EQ(r1.status().code(), StatusCode::kAborted);
  EXPECT_TRUE(engine.WasInterrupted());

  // Second call: short script succeeds (budget reset on entry).
  auto r2 = engine.EvalGlobal("40 + 2", "<short>");
  ASSERT_TRUE(r2.ok()) << (!r2.ok() ? r2.status().message() : "");
  EXPECT_EQ(r2.value(), "42");
  EXPECT_FALSE(engine.WasInterrupted());  // Reset on entry.
}

// (5) WasInterrupted() semantics: tracks last EvalGlobal result.
TEST(QuickjsEngineInterruptTest, WasInterruptedTracksLastEval) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());

  // Initially false.
  EXPECT_FALSE(engine.WasInterrupted());

  // budget=0 short eval: false.
  auto r1 = engine.EvalGlobal("1", "<a>");
  ASSERT_TRUE(r1.ok());
  EXPECT_FALSE(engine.WasInterrupted());

  // budget=100 long loop: true.
  engine.SetEvalInterruptBudget(100);
  auto r2 = engine.EvalGlobal(
      "var i = 0; while (true) { i = i + 1; if (i < 0) break; }",
      "<b>");
  ASSERT_FALSE(r2.ok());
  EXPECT_TRUE(engine.WasInterrupted());

  // Subsequent short eval (budget still 100, but bounded): false (reset).
  auto r3 = engine.EvalGlobal("2", "<c>");
  ASSERT_TRUE(r3.ok());
  EXPECT_FALSE(engine.WasInterrupted());
}
```

**GREEN 验证**:

```
cmake --build build && cd build && ctest -R QuickjsEngine --output-on-failure
```

**预期**: 既有 4 测 + 新 5 测 = 9 测全 PASS

**🛑 CP1 自审清单**（E3 完成后执行）:

- [ ] DEVTOOL=ON ctest baseline 1247 → **1252 PASS**（5 新测全部通过）
- [ ] DEVTOOL=OFF ctest baseline 1082 PASS（不退化 — quickjs_engine 在 vx_script，OFF 仍含）
- [ ] 既有 4 QuickjsEngine 测全部 PASS（无回归）
- [ ] 5 新测全部 PASS / 单测耗时 < 1s 总（小 budget 准死循环 ≤ 100 handler invocations × 10000 bytecode = 10⁶ bytecode ≈ <10ms）
- [ ] D2.B.1 验证: `r.status().code() == kAborted` 在测 1+4+5 中 PASS（kAborted enum 已生效）
- [ ] D5.A+C 验证: 测 1+4 短 budget 准死循环（无 hang）+ 测 2+3+5 短/合法脚本 PASS（双向覆盖）
- [ ] D8b 验证: 默认 budget=10000 在测 3 中合法脚本 PASS（无误杀） + 测 1 budget=100 在 ms 级中止（无 100s hang）

**Commit**:

```
test(script): add 5 interrupt handler tests (D1+D5 A+C coverage) (E3)

Source: TASK-20260503-05 creative-quickjs-host.md §组件 1 方案 C Phase 2.

5 new tests covering 4 dimensions (D5 A+C combo strategy):
- (1) AbortsLongLoopWhenBudgetExhausted: D5.C reverse probe (small budget=100).
- (2) BudgetZeroDisablesInterrupt: D5.A forward probe (default behavior).
- (3) DefaultBudgetDoesNotAbortLegitScripts: D8b validation (10000 budget OK).
- (4) RepeatedEvalResetsBudget: per-EvalGlobal reset semantics.
- (5) WasInterruptedTracksLastEval: D1.B status flag semantics.

DEVTOOL=ON ctest: 1247 -> 1252 PASS (no regression).
```

---

### §3.E4 — `techContext.md` #44 条文更新

**[文档调整模式]**

修改 `memory-bank/techContext.md` 第 620 行附近：

```markdown
44. ~~`vx_script`：`JS_SetInterruptHandler` / 执行预算未实现（见 `creative-quickjs-host.md` Phase 2）~~ ✅ **已实现（TASK-20260503-05）**：`QuickjsEngine::SetEvalInterruptBudget(usize max_checkpoints)` + `JS_SetInterruptHandler` 注册（Init 始终注册 / budget=0 短路 / budget>0 atomic 计数 + return != 0 中止）+ `WasInterrupted()` getter + `StatusCode::kAborted` 新增；默认值 `kDefaultInterruptBudgetCheckpoints = 10000`（handler 调用次数 = bytecode/10000，与 QuickJS `JS_INTERRUPT_COUNTER_INIT = 10000` 对齐 / 实证「死循环 100-500ms 内中止」）；5 新测覆盖（kAborted + budget=0 关闭 + 默认不误杀 + 重置 + WasInterrupted 语义）。`JS_SetMemoryLimit` 32 MiB 已落地（creative 组件 3 方案 B 一期闭环）。**`JSMallocFunctions` 与 Foundation 分配器未对齐仍记技术债**（creative 组件 3 方案 C / 留独立 TASK 后续）
```

**Commit**:

```
docs(memory-bank): close #44 component 1 Phase 2 + retain JSMallocFunctions debt (E4)

Source: TASK-20260503-05 creative-quickjs-host.md §组件 1 方案 C Phase 2.

#44 条文：
- JS_SetInterruptHandler / SetEvalInterruptBudget / WasInterrupted / kAborted 已落地
- JS_SetMemoryLimit 32 MiB 注明已落地（一期闭环）
- JSMallocFunctions 仍记技术债（组件 3 方案 C / 独立 TASK 后续）
```

---

### §3.E5 — finalize（MB 三件套 + 分支合并 ff）

**[Memory Bank 同步]**:

- `memory-bank/tasks.md`: 当前任务段标 ✅ 已完成 / 写实测耗时 / 反复模式自检 0/7 / 迁移到「任务历史」段
- `memory-bank/activeContext.md`: 阶段「构建中 → 回顾中」 / 5 子任务全 ✅ / 新 P3 候选迁移（如有）
- `memory-bank/progress.md`: 任务里程碑追加 5 commits 时间戳 + ctest 终局 + 实测耗时矩阵

**Commit**:

```
chore(build): finalize TASK-20260503-05 memory bank state (E5)

Source: TASK-20260503-05 creative-quickjs-host.md §组件 1 方案 C Phase 2.

5/5 子任务完成 (E1 + E2 + E3 + E4 + finalize) + CP1 + CP2 自审通过.
DEVTOOL=ON 1247 → 1252 PASS (+5).
反复模式: 0/7 (连续第 5 次零反复目标达成 if validated).
plan ×0.6 实测 ~XX min vs plan ×0.6 ~85-105 min = 0.XX× ratio.
```

**🛑 CP2 自审清单**（E5 完成后执行）:

- [ ] 5 commits 全部含 Source 溯源前缀「`Source: TASK-20260503-05 creative-quickjs-host.md §组件 1 方案 C Phase 2`」
- [ ] 5 commits 1 commit/子任务（无合并 / 无遗漏）
- [ ] DEVTOOL=ON ctest 1252 PASS / DEVTOOL=OFF 1082 PASS（双 config）
- [ ] MB 三件套同步完成（tasks/activeContext/progress 阶段全部「构建中」/「规划中」标识 → 适当推进）
- [ ] 反复模式 0/7 自检：D8b push-back 在 plan 阶段 surface（避免「现有实现 runtime 行为假设未实证」反复）+ D2 Status.h audit 在 Phase 0 surface（避免「API 表面假设未实证」反复）
- [ ] 新 P3 候选识别（如：JS_SetInterruptHandler 失败处理 / Phase 3 hook 接口 / kCancelled 进一步拆分等）→ 迁移到 `activeContext.md` 待处理事项段
- [ ] feature 分支 fast-forward merge 到 main（待用户 archive 阶段决策）

---

## §4 — Checkpoint 自审 SOP（CP1 + CP2 详细见 §3.E3 + §3.E5）

| CP | 触发点 | 重点 |
|:-:|---|---|
| **CP1** | E3 完成后 | RED-GREEN cycle 验收 — 5 新测全 PASS / 既有 4 测 不退化 / 双 config baseline 不退化 / D2/D5/D8b 全部实证 |
| **CP2** | E5 完成后 | 工作流闭环 — 5 commits 完整性 / Source 溯源 / 反复模式 0/7 自检 / MB 三件套同步 |

---

## §5 — 验收要点

| # | 检查项 | 验证方式 |
|:-:|---|---|
| A1 | DEVTOOL=ON ctest 1252 PASS（baseline 1247 + 5 新测） | `cd build && ctest -j8` |
| A2 | DEVTOOL=OFF ctest 1082 PASS（不退化）| 配置 OFF + ctest |
| A3 | 既有 4 QuickjsEngine 测无回归 | `ctest -R QuickjsEngine` 9 测全 PASS |
| A4 | `Status.h` 仅追加 `kAborted` enum（既有 6 项 enum 值不变）| git diff `Status.h` |
| A5 | `quickjs_engine.h` 公开 API 扩展 `SetEvalInterruptBudget` + `WasInterrupted` + `kDefaultInterruptBudgetCheckpoints` | git diff `quickjs_engine.h` |
| A6 | `quickjs_engine.cc` `InterruptCallback` 静态函数 / atomic counter / 仅 `return != 0` 中止 / 无 EventLoop 调用 / 无复杂对象分配 | code review §3.E2 实现 |
| A7 | EvalGlobal interrupt 错误返 `kAborted` + 消息「script aborted (interrupt budget exhausted)」+ `WasInterrupted() == true` | 测 1/4/5 |
| A8 | budget=0 关闭 / budget=10000（默认）不误杀 / budget=100（小）准死循环中止 | 测 1/2/3/4/5 矩阵 |
| A9 | techContext.md #44 条文已更新 ✅ + 保留 `JSMallocFunctions` 技术债 | 文件 grep |
| A10 | 5 commits 全 Source 溯源 + 1 commit/子任务 | `git log --format="%h %s%n%b" main..HEAD` |

---

## §6 — 风险登记

| # | 风险 | 影响 | 缓解 |
|:-:|---|:-:|---|
| **R1** | InterruptCallback 始终注册 → handler 在 budget=0 时也每 10000 字节码触发一次 → 微小 overhead | 低 | 实测 D5.A 测 2 PASS；overhead 仅 1 atomic load + 比较 + return 0；与不注册 handler 相比每秒 ~10⁵ 次回调（10⁹ bytecode/s ÷ 10000）× 几 ns = µs 级 / 可忽略 |
| **R2** | atomic counter 在 D8b 默认 budget=10000 下，准死循环测可能在不同 CPU 上耗时差异 | 低 | 测 1/4 用小 budget=100 → 即使最慢 CPU 也 <100ms；测 3 用合法 bounded loop（i=1..1000）字节码上限明确 |
| **R3** | `JS_SetUncatchableError` 意味着 JS 内 try/catch 无法捕获 interrupt → 设计意图 | 低 | 设计意图正确（中断必须不可被脚本绕过）；如未来需要可 catch 再开 P3 |
| **R4** | `kAborted` 新增 enum 值，依赖 Status.h 的下游 56+ 文件理论上需要扩展 switch case | 低 | StatusCode 仅在 Status::ok() 检查 / message() 路径用 / 既有代码无 switch on StatusCode 全枚举 → grep 验证（plan 阶段已抽样）+ build 阶段全配置编译验证 |
| **R5** | 反复模式 #1 第 10 次潜在命中 — 如 Phase 0 漏一个 runtime 行为假设 | 中 | Phase 0 §0.3 三 grep 实证全做（签名 + 内部 counter + 错误信息）；D8b 主动 push-back；D2 audit 提前；CP2 自检明示 |

---

## §7 — 9 systemPatterns 协同度自我对照

| # | systemPattern | 适用度 | 本任务体现 |
|:-:|---|:-:|---|
| 1 | Phase 0 投入定律 quint-evidence | ✅ | Phase 0 §0.3 三 grep 实证 + ctest baseline + Status.h audit = ~5 min Phase 0 / 预计 build 节省 ~10 min（D2/D5/D8b 提前发现避免 build 阶段返工）→ 第 6 数据点候选 |
| 2 | 反复模式渐进式抑制 | ✅ | 反复模式 #1 第 10 次抑制：Phase 0 grep §0.3.2 发现 creative 文档「10⁷ 检查点」单位歧义 → 主动 push-back D8b 重校准 = 10000 |
| 3 | Multi-subtask commit 拆分 | ✅ | 5 commits 1/子任务 + Source 溯源前缀（git-workflow.mdc 范式）/ commit body Source 溯源 11→16 commits 累计 → triple-evidence 升级候选 |
| 4 | 显式语义状态优于阈值 | ✅ | B5 budget=0 显式短路（handler 直接 return 0）+ D1 was_interrupted_ bool 显式语义 vs 阈值检查 |
| 5 | 双层 API 范式（C ABI + 内部 C++）| ⊘ | N/A — 本任务不涉及 C ABI（QuickjsEngine 是 vx::script 内部 C++ 类）|
| 6 | A14 链接闭包黑名单 | ⊘ | N/A — vx_script 是 DEVTOOL=OFF 必备（不属于 vx_devtool） |
| 7 | lazy-attach C ABI 容错 | ⊘ | N/A — 不涉及 C ABI 跨子系统 attach |
| 8 | dogfood 路径 | ⊘ | N/A — 本任务覆盖单测 + 未来 TASK-30-04-D Console 才会 dogfood |
| 9 | 范式 0/7 反复模式连续抑制 | 🎯 | **目标：连续第 5 次零反复**（02 0/7 → 03 1/7 → 本任务回到 0/7 抵消 03 回升）/ Phase 0 §0.3 三实证 + D8b push-back + D2 audit 三层抑制 |

---

## §8 — 反复模式预防清单

| # | 反复模式 | 历史命中 | 本任务抑制策略 |
|:-:|---|:-:|---|
| 1 | 前置依赖/环境/API 能力未验证 | 9 次（最后 03 task） | Phase 0 §0.3 三 grep 实证（签名 + counter + 错误信息）+ D2 Status.h audit + D8b creative 单位重校准 = 三层抑制 |
| 2 | 测试覆盖不全 | 4 次 | 5 新测覆盖 4 维度（中止 / 关闭 / 不误杀 / 重置 / 状态查询）+ D5 A+C 双向覆盖 |
| 3 | 共享文件冲突 | 6 次 | E1 Status.h 仅追加 enum（无修改既有）+ build 阶段全 config verify |
| 4 | commit 粒度过粗 | 8 次 | B8 锁定 5 commits 1/子任务 + Source 溯源前缀 |
| 5 | 安全边界遗漏 | 5 次 | D2 kAborted 显式语义 + 安全测设计原则段沿用（B5 显式语义状态优于阈值）|
| 6 | MB 同步遗漏 | 7 次 | E5 finalize 三件套同步 + CP2 自审检查 |
| 7 | 反复模式自检遗漏 | 3 次 | CP2 自审清单明确含「反复模式 0/7 自检」+ §8 本表 |

---

## §9 — plan ×0.6 数据点假设入库

**蓝图估时**：~85-105 min plan ×0.6（5 子任务 × ~17-21 min/子任务）

**实测预期**：~25-45 min（落「极窄档延续高效区 0.30-0.45×」候选 / 因含真实 C++ 代码 + 5 单测 RED-GREEN cycle / 比 02 文档极速 0.21× 略高 / 比 03 含 ctest 等待 0.42× 略低）

**新数据点假设入库**：plan ×0.6 第 67-72 数据点群组（5 子任务 + 1 finalize 群组），假设比值 0.30-0.45×

---

## §10 — 后续连接（不在本任务范围）

| # | 后续 | 触发条件 |
|:-:|---|---|
| 1 | 恢复 TASK-20260503-04 DevTool Phase D Console JS REPL | 本任务归档完成后（用户显式 `/van TASK-30-04-D` 或 `/van 恢复 TASK-20260503-04`）/ 已锁定决策 V1=B 完整 Console Panel + V3=A 严格按 spec |
| 2 | creative §组件 3 方案 C `JSMallocFunctions → vx::MallocAllocator` | 独立 TASK / 长期；techContext #44 仍记技术债 |
| 3 | creative §组件 1 Phase 3 宿主 `Application` 集成 | 与 TASK-30-04-E JS Debugger backend 耦合时再做 / 留 P2 候选 |
| 4 | `JS_SetInterruptHandler` 失败处理（API 本身无失败但宿主可能传非法 ptr）| 当前 InterruptCallback 已 nullptr 守门 / 留 P3 候选「opaque ptr lifetime audit」 |

---

**plan 完成。下一步：用户 `/build` 启动 5 子任务串行执行。**
