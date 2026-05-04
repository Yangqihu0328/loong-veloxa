# Plan: DevTool Phase D — Console JS REPL + console.log 桥接（V1=B 扩展段）

**任务 ID：** `TASK-20260503-04`（搁置任务恢复 / 沿用原 ID）
**日期：** 2026-05-04（VAN 恢复 + plan 落盘）
**复杂度：** Level 3（V3=A 锁定 — 不 escalate / 12/12 决策锁定）
**安全相关：** ✅ 是（T1 任意 eval mitigation — Console 引入是 T1 威胁面落地实施）
**分支：** `feature/TASK-20260503-04-devtool-console-repl`（基于 main `509fec3` ✅ 已创建）
**状态：** Plan 落盘完成 → 待 `/creative` 启动 4 维度详细设计
**触及威胁：** T1 任意 eval（首次完整暴露） / T6 buffer 上限保护（console_log_buffer 双重上限）
**触及技术债：** 闭环 spec §11.1 Console 占位 / 复用 TASK-05 闭环的 `SetEvalInterruptBudget` API + `WasInterrupted()` / 解锁 TASK-30-04-E JS Debugger backend

---

## 1. 概要

实施 spec §11.1 占位的 Console 子系统，在 DevTool 三件套主线收官后引入第 4 件套：JS REPL（用户输入 JS 表达式 → eval → 结果回显）+ `console.log/error/warn` 桥接（Console 内 JS 执行的 console 输出写到 panel output 区域）。

**已锁定决策（12/12 通过 1 次 AskQuestion + all_recommended 锁定）：**

### Brainstorm 决策（C1-C4 详细设计 → `/creative` 阶段落盘到 `creative-devtool-console.md`）

| # | 维度 | 选定 | 一句话理由 |
|:-:|---|---|---|
| C1 | DevTool 主屏第 4 件套布局 | **A 第 4 tab 与既有 3 tab 平级**（DOM/Style/Layout/Console） | 复用现有 tab 切换协议 + splitter dock-to-bottom 模式天然变 Chrome 风格底部 console |
| C2 | isolated JSRuntime 实施 | **B 新建 `console_script_engine_`**（第 3 个 QuickjsEngine 实例） | 已实证 Application 持有 `devtool_script_engine_`（spec §11.1 隔离已部分满足）；Console 再隔离一层避免污染 DevTool UI scope |
| C3 | capability allowlist | **A 严格隔离 binding**（仅 console.log/error/warn + 既有 query-only DevTool 函数 + **不**调 `DomBindings::Bind`） | RegisterDevtoolBindings 已是 read-only query-only，复用零 mutator；Console binding 不调 Bind = 不暴露 ElementSetAttribute 等 mutator |
| C4 | console.log 桥接 | **A drain 模式**（push to `console_log_buffer_` + `vx_devtool_get_console_log_drain` query） | 与既有 `vx_devtool_get_dom_json` query-style 范式 100% 一致 / 单线程同步 / 无新 callback API |

### Plan 阶段决策（B1-B8 → 本文件覆盖）

| # | 维度 | 选定 |
|:-:|---|---|
| B1 | 子任务划分 | **A 5 子任务串行**（D.1 → D.2 → D.3 [CP1] → D.4 → D.5 [CP2]） |
| B2 | 测试模式 | **A 推荐组合**（D.1/D.2/D.4 [覆盖补充] / D.3 [文档/资源调整] / D.5 [覆盖补充] + [集成测]） |
| B3 | console_log_buffer 上限 | **A 双上限**（max_count = 1000 + 单条 max_len = 4 KiB / drop oldest + truncated flag） |
| B4 | console_script_engine_ lifecycle | **A 与 LoadDevtoolDocument 同步**（Load 时 devtool_engine 后创建 console_engine 前；Unload 时反序释放 + buffer 同步 clear） |
| B5 | drain JSON envelope 格式 | **A 推荐**（`{"messages":[{"level":..,"text":..,"ts":..}],"truncated":<bool>,"dropped_count":<u32>}` — 与 hot_reload_status envelope 范式一致） |
| B6 | 公开 C API | **A 推荐**（`vx_view_eval_console` + `vx_view_console_log_drain` — 与 `vx_view_serialize_dom_json` 范式一致 + lazy-attach 容错） |
| B7 | Checkpoint | **A 推荐**（CP1 D.3 后视觉验证 dogfood / CP2 D.5 后 OFF + A14 + 反复模式 0/7 自检） |
| B8 | commit + 估时 | **A 推荐**（5 commits 1/子任务 + Source 溯源 + 1 reflect + 1 archive = 7 commits / plan ×0.6 ~180-240 min → 实测 ~70-110 min 落极窄档延续高效区） |

---

## 2. Phase 0 实证（已完成 — VAN + plan 阶段）

| § | 维度 | 实证结果 |
|:-:|---|---|
| 0.1 | ctest baseline DEVTOOL=ON | **1252/1252 PASS 6.39s** ✅（与 TASK-05 闭环一致） |
| 0.2 | isolated runtime 可行性 | ✅ QuickjsEngine self-contained（`rt_`/`ctx_` 独立）/ Application 已有 `devtool_script_engine_`（`application.cc:322`） |
| 0.3 | 资源嵌入范式 | ✅ `inspector_resources.h` 暴露 3 个 `extern const char* const k{...}Panel{Html,Css,Js}` / CMake codegen / A14 OFF 不链接 |
| 0.4 | DomBindings + RegisterDevtoolBindings | ✅ 既有 binding mutator-rich（针对 DevTool UI 自身 Document），但 `RegisterDevtoolBindings` 注册的 3 个 DevTool 函数（`vx_devtool_get_dom_json` / `vx_view_get_perf_stats` / `vx_devtool_get_hot_reload_status`）全 query-only 零 mutator |
| 0.5 | console.log 桥接路径 | ✅ 当前无 push-style callback API；query-style drain 模式与既有范式一致 |
| 0.6 | 配置矩阵假设（含 add_test guard 检查 — 闭环 TASK-05 P1 #1） | DEVTOOL=ON 1252→**1252+5**（5 新测落 D.5 估算）/ DEVTOOL=OFF 1087→**1087+0**（如新测全在 `tests/devtool/console/` 子目录 + `if(VX_BUILD_DEVTOOL)` guard） / SDL2=ON 1265→1265+1（hello_devtool console smoke +1） |
| 0.7 | 反复模式 #1 第 10 次抑制 | ✅ §0.2+§0.4 实证「devtool_script_engine_ 已存在 + RegisterDevtoolBindings 已 query-only」避免假设错误（避免 plan-fact reconcile 「既有实现 runtime 行为假设未实证」第 11 次） |

**Phase 0 ROI 投入定律 sept-evidence**（如本任务 ~5 min Phase 0 投入 → 节省 ~30+ min build phase 返工）— TASK-05 sext-evidence 升级为 **sept-evidence**（A 5.3× / B 5.2× / C 7.6× / 02 6.7× / 03 8.0× / 05 16× / **04 预期 6-10×**）。

---

## 3. 子任务详细（5 子任务串行 + CP1+CP2）

### D.1 — `ConsoleEngine` 类 + Application lifecycle 集成

**测试模式：** [覆盖补充]
**估时：** plan ×0.6 ~30-40 min / 实测预期 ~12-18 min
**文件：**
- 🆕 `veloxa/devtool/console/console_engine.h` (~50 行 / 新建)
- 🆕 `veloxa/devtool/console/console_engine.cc` (~80 行 / 新建)
- 🆕 `veloxa/devtool/console/CMakeLists.txt` (~20 行 / 新建)
- 🟡 `veloxa/devtool/CMakeLists.txt` [共享文件] (+1 行 / `add_subdirectory(console)`)
- 🟡 `veloxa/core/application.h` (+~15 行 / `console_script_engine_` + `console_log_buffer_` 字段 + getter)
- 🟡 `veloxa/core/application.cc` (+~30 行 / `LoadDevtoolDocument` + `UnloadDevtoolDocument` 同步 lifecycle / 默认 `SetEvalInterruptBudget(10000)`)
- 🟡 `tests/script/CMakeLists.txt` [共享文件] (+~3 行 / 新增 `console_engine_test`，`if(VX_BUILD_DEVTOOL)` guard 闭环 TASK-05 P1 #1)
- 🆕 `tests/devtool/console/console_engine_test.cc` (~80 行 / 4-5 单测：Init/Shutdown/EvalGlobal/SetEvalInterruptBudget 复用 + WasInterrupted 死循环)

**5-step TDD：**
1. **RED**：写 `ConsoleEngine` 4 单测（构造/Init/EvalGlobal/SetEvalInterruptBudget 复用） → 编译失败（class 不存在）
2. **GREEN 骨架**：建 `console_engine.{h,cc}` + CMakeLists.txt → ctest 4 PASS
3. **集成**：Application 持 `console_script_engine_` 成员 + `LoadDevtoolDocument` 创建（`devtool_script_engine_` 后） + `UnloadDevtoolDocument` 反序释放
4. **GREEN 默认安全**：默认 `SetEvalInterruptBudget(10000)` 入 ConsoleEngine::Init（不可绕过 / D8b 实证默认值 → T1 mitigation 维度 1+2+3 同步达成）
5. **commit**：`feat(devtool/console): add ConsoleEngine + Application lifecycle integration (D.1)` Source: 本 plan §3.D.1

**代码骨架（详细设计 → /creative C2.B 段）：**

```cpp
// veloxa/devtool/console/console_engine.h
namespace vx::devtool::console {

// Isolated QuickJS runtime for DevTool Console JS REPL.
// Wraps script::QuickjsEngine with default-safe T1 mitigation:
//   - SetEvalInterruptBudget(10000) auto-applied at Init() (cannot bypass).
//   - No DomBindings::Bind (no DOM mutator surface).
//   - No js_std_add_helpers (inherits creative-quickjs-host §组件 2 方案 B).
class ConsoleEngine {
 public:
  ConsoleEngine();
  ~ConsoleEngine();

  ConsoleEngine(const ConsoleEngine&) = delete;
  ConsoleEngine& operator=(const ConsoleEngine&) = delete;

  Status Init();
  void Shutdown();

  StatusOr<std::string> EvalGlobal(StringView source, StringView filename);
  bool WasInterrupted() const;

  JSContext* context() const;

 private:
  std::unique_ptr<script::QuickjsEngine> engine_;
};

}  // namespace vx::devtool::console
```

```cpp
// veloxa/devtool/console/console_engine.cc
#include "veloxa/devtool/console/console_engine.h"
#include "veloxa/script/quickjs_engine.h"

namespace vx::devtool::console {

ConsoleEngine::ConsoleEngine()
    : engine_(std::make_unique<script::QuickjsEngine>()) {}

ConsoleEngine::~ConsoleEngine() = default;

Status ConsoleEngine::Init() {
  auto s = engine_->Init();
  if (!s.ok()) return s;
  // T1 mitigation: default-safe budget cannot be bypassed by Console
  // user input (caller must supply a different ConsoleEngine instance).
  engine_->SetEvalInterruptBudget(
      script::QuickjsEngine::kDefaultInterruptBudgetCheckpoints);
  return Status::Ok();
}

void ConsoleEngine::Shutdown() { engine_->Shutdown(); }

StatusOr<std::string> ConsoleEngine::EvalGlobal(StringView source,
                                                StringView filename) {
  return engine_->EvalGlobal(source, filename);
}

bool ConsoleEngine::WasInterrupted() const { return engine_->WasInterrupted(); }
JSContext* ConsoleEngine::context() const { return engine_->context(); }

}  // namespace vx::devtool::console
```

---

### D.2 — `RegisterConsoleBindings` + console.log 桥接 + drain API

**测试模式：** [覆盖补充]
**估时：** plan ×0.6 ~40-50 min / 实测预期 ~15-22 min
**文件：**
- 🆕 `veloxa/devtool/console/console_bindings.h` (~30 行 / 新建)
- 🆕 `veloxa/devtool/console/console_bindings.cc` (~150 行 / 新建 / RegisterConsoleBindings + ConsoleLogPush + ConsoleLogDrainEnvelope)
- 🟡 `veloxa/devtool/console/CMakeLists.txt` (+~3 行 / 加入 console_bindings.cc 编译)
- 🟡 `veloxa/core/application.h` (+~5 行 / `ConsoleLogBuffer` struct / getter)
- 🟡 `veloxa/core/application.cc` (+~10 行 / D.1 LoadDevtoolDocument 后调 RegisterConsoleBindings + 绑定 console_log_buffer_)
- 🆕 `tests/devtool/console/console_bindings_test.cc` (~100 行 / 5-6 单测：console.log push / multiple log levels / buffer overflow drop oldest / single message truncate / drain envelope JSON shape / drain clears buffer)

**5-step TDD：**
1. **RED**：写 6 单测覆盖 console.log push / level 区分 / 上限 drop oldest / 单条 truncate / drain envelope JSON / drain 后 clear → 编译失败
2. **GREEN 骨架**：实现 RegisterConsoleBindings (3 函数 console.log/error/warn) + ConsoleLogPushImpl (含 B3.A 双上限) + ConsoleLogDrainImpl (B5.A envelope JSON) → ctest 6 PASS
3. **集成**：Application::LoadDevtoolDocument 在 console_engine 创建后调 RegisterConsoleBindings + 关联 console_log_buffer_
4. **覆盖补充**：T1 mitigation 维度 4（buffer 上限）+ 维度 5（单条 truncate） 双向反向探针测
5. **commit**：`feat(devtool/console): add RegisterConsoleBindings + console.log bridge + drain API (D.2)` Source: 本 plan §3.D.2

**关键代码片段：**

```cpp
// veloxa/devtool/console/console_bindings.h
namespace vx::devtool::console {

// One log entry. ts is unix-epoch milliseconds (host time, not eval time).
struct ConsoleLogEntry {
  enum class Level : u8 { kLog, kError, kWarn };
  Level level = Level::kLog;
  std::string text;
  u64 ts_ms = 0;
};

// Buffer with double cap (B3.A): max 1000 entries + max 4096 bytes per
// text. When over count cap: drop oldest + increment dropped_count_.
// When over per-text cap: truncate + append "... [truncated]".
class ConsoleLogBuffer {
 public:
  static constexpr usize kMaxCount = 1000;
  static constexpr usize kMaxTextBytes = 4096;

  void Push(ConsoleLogEntry::Level lvl, std::string text);
  // Drains all entries, returns JSON envelope (B5.A format), clears buf.
  std::string DrainAsJson();
  void Clear();

  usize size() const { return entries_.size(); }
  u32 dropped_count() const { return dropped_count_; }

 private:
  std::deque<ConsoleLogEntry> entries_;
  u32 dropped_count_ = 0;
  bool truncated_ = false;
};

// Registers 4 globals on `ctx`:
//   - console (object)
//   - console.log(arg) / console.error(arg) / console.warn(arg)
//   - vx_devtool_get_console_log_drain() (returns JSON envelope)
// Pre-condition: caller has stored `buffer` pointer somewhere reachable
// via JS_GetContextOpaque(ctx)+offset (we use a side-channel struct).
void RegisterConsoleBindings(JSContext* ctx, ConsoleLogBuffer* buffer);

}  // namespace vx::devtool::console
```

**JSON envelope（B5.A）：**

```json
{
  "messages": [
    {"level": "log", "text": "hello", "ts": 1714809600000},
    {"level": "error", "text": "boom", "ts": 1714809600123}
  ],
  "truncated": false,
  "dropped_count": 0
}
```

---

### D.3 — Console panel 资源（HTML/CSS/JS）+ inspector_panel.html 第 4 tab 集成

**测试模式：** [文档/资源调整模式]
**估时：** plan ×0.6 ~40-50 min / 实测预期 ~15-22 min
**文件：**
- 🆕 `veloxa/devtool/resources/console_panel.html` (~30 行 / 新建 / `<div id="console-output">` + `<input id="console-input">` + label "JS:")
- 🆕 `veloxa/devtool/resources/console_panel.css` (~40 行 / 新建 / output 滚动 div + input 单行 + level 颜色区分)
- 🆕 `veloxa/devtool/resources/console_panel.js` (~80 行 / 新建 / Enter eval + 每帧 drain console_log + append output)
- 🟡 `veloxa/devtool/resources/inspector_resources.h` (+3 行 / `kConsolePanelHtml/Css/Js` extern declare)
- 🟡 `veloxa/devtool/resources/CMakeLists.txt` [共享文件] (+~10 行 / Python codegen 加入 console_panel.{html,css,js})
- 🟡 `veloxa/devtool/resources/inspector_panel.html` (+~6 行 / 第 4 tab button "Console" + 第 4 panel `<div id="console-panel">` 占位 + `__INLINE_CONSOLE_HTML__` 占位符)
- 🟡 `veloxa/devtool/resources/inspector_panel.css` (+~5 行 / .active 风格延续；console-panel 容器样式)
- 🟡 `veloxa/devtool/resources/inspector_panel.js` (+~5 行 / 第 4 tab 切换识别延续既有 toggle 逻辑 + console.js 内嵌引导)
- 🟡 `veloxa/core/application.cc` (+~3 行 / 4 处 ReplaceFirst：__INLINE_CONSOLE_HTML__ + __INLINE_CONSOLE_CSS__ + __INLINE_CONSOLE_JS__ 拼接)
- 🟡 `tests/CMakeLists.txt` [共享文件] (+~3 行 / 集成测 hello_devtool_console_smoke 占位 — 实际单测在 D.5)

**5-step TDD（资源类不走传统 RED→GREEN，按 [文档调整模式]）：**
1. **设计**：HTML 结构 + CSS 选择器 + JS REPL 行为骨架（参考 Chrome DevTools console 极简版）
2. **资源就位**：3 资源文件落盘 + inspector_resources.h 加 3 extern + CMakeLists Python codegen 加入
3. **集成**：inspector_panel.html 加第 4 tab + Application::LoadDevtoolDocument 加 3 ReplaceFirst
4. **build verify**：cmake build → DEVTOOL=ON 1252 不退化 + 4 tab 视觉验证（手动 hello_devtool 切 Console tab）
5. **commit**：`feat(devtool/console): add Console panel HTML/CSS/JS + tab integration (D.3)` Source: 本 plan §3.D.3

**console_panel.html 结构：**

```html
<div id="console-output"></div>
<div id="console-input-row">
  <span id="console-prompt">JS&gt;</span>
  <input id="console-input" type="text" autocomplete="off"/>
</div>
```

**console_panel.js 行为（简版伪码 — 详细 → /creative C1.A 段）：**

```js
// 1. Enter eval
document.getElementById("console-input").addEventListener("keydown", (e) => {
  if (e.key === "Enter") {
    var src = e.target.value;
    e.target.value = "";
    appendOutputLine("input", "JS> " + src);
    // Eval through host C API (D.4). For dogfood, use vx_view_eval_console
    // exposed as a JS native binding registered alongside RegisterConsoleBindings.
    if (typeof vx_console_eval === "function") {
      var result = vx_console_eval(src);
      appendOutputLine("result", "= " + result);
    }
  }
});

// 2. Drain console.log buffer every frame (or on a timer)
function drainConsoleLog() {
  if (typeof vx_devtool_get_console_log_drain !== "function") return;
  var raw = vx_devtool_get_console_log_drain();
  var env = JSON.parse(raw);
  for (var i = 0; i < env.messages.length; i++) {
    var m = env.messages[i];
    appendOutputLine(m.level, m.text);
  }
  if (env.truncated || env.dropped_count > 0) {
    appendOutputLine("warn", "[truncated/dropped " + env.dropped_count + "]");
  }
}
setInterval(drainConsoleLog, 200);  // 5 fps polling
```

**🛑 CP1 自审（D.3 后）：**

- DEVTOOL=ON 1252→1252（D.3 无新单测，仅资源 + 集成）✅ 不退化
- inspector_panel.html 4 tab 切换视觉验证（hello_devtool 启动 + F12 attach + click "Console" tab → 看到 #console-output + #console-input）
- 3 commits Source 溯源前缀完整（D.1 + D.2 + D.3）
- console_engine + console_bindings DEVTOOL=ON ctest 全 PASS

---

### D.4 — 公开 C API：`vx_view_eval_console` + `vx_view_console_log_drain` + JS native binding

**测试模式：** [覆盖补充]
**估时：** plan ×0.6 ~30-40 min / 实测预期 ~12-18 min
**文件：**
- 🟡 `veloxa/api/veloxa_api.h` (+~25 行 / 2 公开 C API 声明 + 1 ABI struct vx_console_eval_status + 文档注释)
- 🟡 `veloxa/api/veloxa_api.cc` (+~50 行 / 2 公开 C API 实现：lazy-attach 容错 + 复用 ConsoleEngine + ConsoleLogBuffer)
- 🟡 `veloxa/devtool/console/console_bindings.cc` (+~30 行 / RegisterConsoleBindings 加 vx_console_eval JS 函数 — 让 console_panel.js 可调)
- 🟡 `tests/api/CMakeLists.txt` [共享文件] (+~3 行 / 新增 console_api_test，`if(VX_BUILD_DEVTOOL)` guard)
- 🆕 `tests/api/console_api_test.cc` (~80 行 / 4-5 单测：DEVTOOL=ON ConsoleEngine eval 成功 + 死循环 budget 超时返回 INVALID_STATE + DEVTOOL=OFF 返 INVALID_STATE + lazy-attach 未 attach 返 INVALID_STATE + drain envelope round-trip)

**5-step TDD：**
1. **RED**：写 5 API 单测 → 编译失败
2. **GREEN 骨架**：实现 2 公开 C API + lazy-attach 容错 + JS native binding (vx_console_eval) → ctest 5 PASS
3. **DEVTOOL=OFF stub**：`#ifdef VX_BUILD_DEVTOOL` guard 双路径，OFF 返 INVALID_STATE
4. **覆盖补充**：T1 mitigation 维度 1-5 完整性测（默认安全 + opt-in / 不可绕过 / 单线程 / 状态可查 / 回调约束）
5. **commit**：`feat(api): add vx_view_eval_console + vx_view_console_log_drain (D.4)` Source: 本 plan §3.D.4

**关键代码片段：**

```cpp
// veloxa/api/veloxa_api.h
typedef enum vx_console_eval_status {
  VX_CONSOLE_EVAL_OK = 0,
  VX_CONSOLE_EVAL_INTERRUPTED = 1,    // T1: budget exhausted → kAborted
  VX_CONSOLE_EVAL_SYNTAX_ERROR = 2,   // QuickJS syntax/parse error
  VX_CONSOLE_EVAL_RUNTIME_ERROR = 3,  // QuickJS runtime exception
  VX_CONSOLE_EVAL_NOT_ATTACHED = 4,   // DevTool not attached / DEVTOOL=OFF
  VX_CONSOLE_EVAL_BUFFER_TOO_SMALL = 5,
} vx_console_eval_status;

// Eval `source` in DevTool Console isolated runtime.
// Writes result toString() into out_buf (truncated to out_buf_size-1).
// Returns vx_console_eval_status.
//
// T1 mitigation: budget = ConsoleEngine::kDefaultInterruptBudgetCheckpoints
// (10000); cannot be bypassed by source. INTERRUPTED return means budget
// exhausted (e.g. infinite loop).
//
// DEVTOOL=OFF or DevTool not attached: returns NOT_ATTACHED with empty out_buf.
// out_status (nullable): receives the eval-time WasInterrupted() snapshot.
i32 vx_view_eval_console(vx_view_handle view, const char* source,
                         usize source_len, char* out_buf, usize out_buf_size,
                         vx_console_eval_status* out_status);

// Drain ConsoleLogBuffer; writes JSON envelope into out_json_buf.
// Returns the number of bytes written (excluding NUL); -1 if buffer
// too small (out_json_buf untouched). DEVTOOL=OFF: writes empty {} envelope.
isize vx_view_console_log_drain(vx_view_handle view, char* out_json_buf,
                                usize out_buf_size);
```

---

### D.5 — 单测扩充 + dogfood smoke + A14 黑名单 + finalize

**测试模式：** [覆盖补充] + [集成测]
**估时：** plan ×0.6 ~40-60 min / 实测预期 ~16-25 min
**文件：**
- 🟡 `tests/devtool/console/console_engine_test.cc` (+~50 行 / 补充 T1 mitigation 5 维度完整性测：默认安全 / opt-in / 不可绕过 / 单线程 atomic / 状态可查 / 回调约束)
- 🟡 `tests/devtool/console/console_bindings_test.cc` (+~30 行 / 补充：drain envelope JSON parse round-trip + level 区分 + ts_ms 单调递增 / mock host time)
- 🆕 `tests/integration/devtool_console_dogfood_smoke_test.cc` (~120 行 / 端到端：Application::LoadDevtoolDocument → click Console tab → input "1+1" + Enter → drain console.log 含 "= 2" + WasInterrupted 验证 false / 死循环 source → INTERRUPTED 返回 + WasInterrupted true)
- 🟡 `tests/integration/CMakeLists.txt` [共享文件] (+~3 行 / 新增 devtool_console_dogfood_smoke_test，`if(VX_BUILD_DEVTOOL)` guard)
- 🟡 `tests/smoke/devtool_a14_link_closure.cmake` (+~3 行 / A14 黑名单加 ConsoleEngine + ConsoleLogBuffer + RegisterConsoleBindings 3 项 + Phase D 区分注释)
- 🟡 `examples/hello_devtool.cc` (+~10 行 / dogfood smoke autoquit 后调 vx_view_eval_console("1+2") + 验证 = 3 / 文件头 docstring 加第 4 件套范式 — 补 TASK-03 P2 装裱协议)
- 🟡 `tests/CMakeLists.txt` [共享文件] (+~3 行 / 新增 hello_devtool_console_smoke autoquit 600ms / [覆盖补充])
- 🟡 `memory-bank/techContext.md` (+~10 行 / TASK-04 Phase D 实施摘要 + spec §11.1 闭环 + T1 mitigation 5 维度完整性记录)
- 🟡 `memory-bank/tasks.md` + `activeContext.md` + `progress.md` (finalize 阶段同步)

**5-step TDD（D.5 含 finalize）：**
1. **RED**：补充 T1 mitigation 5 维度测 + dogfood smoke 端到端测 → 编译失败 / 部分测验证 D.1-D.4 是否漏点
2. **GREEN**：补全 ConsoleEngine T1 mitigation 维度测（5 维度全 PASS）
3. **dogfood smoke**：hello_devtool 加 console eval 调用 + ctest hello_devtool_console_smoke 集成测
4. **A14**：CMake smoke template 加 ConsoleEngine + ConsoleLogBuffer + RegisterConsoleBindings 黑名单 → DEVTOOL=OFF nm 验证 0 符号泄漏
5. **finalize**：MB 三件套同步 + techContext.md TASK-04 摘要 + commit `chore(build): finalize TASK-20260503-04 console phase D (D.5)` Source: 本 plan §3.D.5

**🛑 CP2 自审（D.5 后 — 终局）：**

- DEVTOOL=ON 1252→**1257**（5 新测：D.1 4 + D.2 6 + D.4 5 + D.5 6 = 21 总；D.3 0 - 但部分测从 D.1-D.2 上移到 D.5 / 估算交付 21 测，实际可能 ~20-23）→ **plan §0.6 假设 +5 偏差** plan-fact reconcile #1 候选（如实际差超 ±20% 触发反复模式 #1 子维度「ctest 数量预期偏差」P3）
- DEVTOOL=OFF 1087→**1087** 不变（A14 黑名单完整 + 所有新测在 if(VX_BUILD_DEVTOOL) guard 下 — 闭环 TASK-05 P1 #1 audit 子条）
- SDL2=ON 1265→**1266+** （+ hello_devtool_console_smoke）
- 5 commits Source 溯源完整（D.1 + D.2 + D.3 + D.4 + D.5 finalize）
- 反复模式 0/7 自检（连续第 6 次零反复目标）
- T1 mitigation 5 维度完整性 ✅
- A14 nm 验证 0 console 符号泄漏 ✅
- techContext.md 摘要同步 ✅
- 1 plan-fact reconcile（如有）✅ 已记录

---

## 4. 验收要点（A1-A12）

| # | 要点 | 验证 |
|:-:|---|---|
| A1 | DEVTOOL=ON ctest 1252 → 1252+N PASS | CP2 自审 |
| A2 | DEVTOOL=OFF ctest 1087 不变 | CP2 自审（含 add_test guard 实证） |
| A3 | SDL2=ON ctest 1265+1 PASS（含 hello_devtool_console_smoke） | CP2 自审 |
| A4 | A14 黑名单 +3 项（ConsoleEngine + ConsoleLogBuffer + RegisterConsoleBindings） | CP2 自审 nm 验证 |
| A5 | T1 mitigation 5 维度完整性 | D.5 单测覆盖 5 维度 |
| A6 | console_engine isolated 不能 manipulate target Document | D.4 反向探针测：尝试 ElementSetAttribute 等 → undefined |
| A7 | console.log push to buffer + drain JSON envelope round-trip | D.2 + D.5 单测 |
| A8 | 双上限 mitigation（max_count 1000 + max_text 4 KiB） | D.2 双上限单测 |
| A9 | 死循环死循环被中止（SetEvalInterruptBudget 默认 10000） | D.5 5 维度测 |
| A10 | 公开 C API 2 函数 lazy-attach 容错 | D.4 单测 4 路径（ON+attached / ON+!attached / OFF / 错误源） |
| A11 | 5 commits 全 Source 溯源前缀 + 实测数据 | git log verify |
| A12 | 反复模式 0/7 命中（连续第 6 次零反复） | CP2 自检表 |

---

## 5. 完整代码片段索引（详细见 D.1-D.5 节）

| 文件 | 行数估算 | 子任务 | 关键内容 |
|---|---:|:-:|---|
| `veloxa/devtool/console/console_engine.h` | ~50 | D.1 | ConsoleEngine class（wraps QuickjsEngine + 默认 budget） |
| `veloxa/devtool/console/console_engine.cc` | ~80 | D.1 | Init/Shutdown/EvalGlobal/SetEvalInterruptBudget/WasInterrupted 转发 |
| `veloxa/devtool/console/console_bindings.h` | ~30 | D.2 | ConsoleLogEntry/Buffer/RegisterConsoleBindings |
| `veloxa/devtool/console/console_bindings.cc` | ~150 | D.2 | console.log/error/warn JS 函数 + ConsoleLogBuffer Push/Drain + JSON envelope |
| `veloxa/devtool/console/CMakeLists.txt` | ~25 | D.1+D.2 | vx_devtool_console 静态库 |
| `veloxa/devtool/resources/console_panel.html` | ~30 | D.3 | output 滚动 div + input 单行 |
| `veloxa/devtool/resources/console_panel.css` | ~40 | D.3 | 滚动 div + level 颜色 + REPL 单行 input |
| `veloxa/devtool/resources/console_panel.js` | ~80 | D.3 | Enter eval + drain console_log + append output + tab 切换 |
| `veloxa/api/veloxa_api.h/cc` | +~75 | D.4 | vx_view_eval_console + vx_view_console_log_drain |
| `tests/devtool/console/*.cc` | ~360 | D.1+D.2+D.5 | 4 单测文件 / ~20 测 |
| `tests/integration/devtool_console_dogfood_smoke_test.cc` | ~120 | D.5 | dogfood 端到端 |
| **总变更** | **+800-900 行** | — | 4 文件 OFF guard 完整 / 0 既有签名变更 |

---

## 6. R 风险登记（5 项）

| # | 风险 | 概率 | 影响 | mitigation |
|:-:|---|:-:|:-:|---|
| R1 | console.js Enter 监听器与既有 inspector_panel.js 全局 keydown 冲突 | 中 | 中 | console.js Enter 在 input element 监听（不是 window 全局） + check active tab == "console" |
| R2 | console_log_buffer 上限 mitigation 单条 4 KiB UTF-8 边界（截断不能在多字节字符中间） | 中 | 中 | utf8 边界检测函数（找最近 ≤ 4 KiB 的合法 UTF-8 边界 / 测覆盖中文/emoji 多字节） |
| R3 | console_engine 与 devtool_engine 同帧析构次序错（dangling JSContext callback） | 低 | 高 | B4.A lifecycle 协议：UnloadDevtoolDocument 反序释放 console_engine 在前 + 单测覆盖 Load/Unload 5 次循环（无 leak） |
| R4 | A14 OFF 路径 unique_ptr<ConsoleEngine> 在 application.h 含不完整类型 → OFF link 失败（与 TASK-03 Phase C HotReloadManager 同源） | 中 | 中 | application.h 加 #ifdef VX_BUILD_DEVTOOL guard 包围 unique_ptr 字段 + getter 双向条件编译（沿用 Phase C 范式） |
| R5 | hello_devtool_console_smoke 多帧 dirty_ 阻塞（沿用 TASK-03 P1 教训） | 中 | 中 | 不依赖多帧 — smoke 仅测 console eval 即时返回（autoquit 600ms + 1 eval 即足够） |

---

## 7. T1 mitigation 5 维度自检表

| # | 维度 | 实施位置 | 测覆盖 |
|:-:|---|---|---|
| 1 | 默认安全（默认 budget = 10000） | ConsoleEngine::Init 强制注入 | D.1 单测 + D.5 维度 1 测 |
| 2 | opt-in（caller 显式调 SetEvalInterruptBudget(0) 才关闭） | QuickjsEngine 既有 API（继承） | D.5 维度 2 测 |
| 3 | 不可绕过（user JS source 无法 reset budget） | budget 在 ConsoleEngine 类持有，user 不可访问 | D.5 维度 3 反向探针测 |
| 4 | 状态可查（WasInterrupted 暴露） | ConsoleEngine::WasInterrupted forward | D.4 + D.5 维度 4 测 |
| 5 | 回调约束（InterruptCallback 单线程 atomic） | QuickjsEngine 既有实现（继承） | D.5 维度 5 测（非新工作） |

---

## 8. Checkpoint 详细

### 🛑 CP1 — D.3 完成后

| 检查项 | 期望 |
|---|---|
| DEVTOOL=ON ctest | 1252→1252+N（D.1 4 + D.2 6 = 10 新测）PASS |
| 视觉验证 | hello_devtool 启动 + F12 attach → 见 4 tab 切换（DOM/Style/Layout/Console）/ click "Console" → 见 #console-output + #console-input |
| commits | 3 commits Source 溯源完整（D.1 + D.2 + D.3） |
| lint | 0 错误 |

### 🛑 CP2 — D.5 完成后（终局）

| 检查项 | 期望 |
|---|---|
| DEVTOOL=ON ctest | 1252→1252+N PASS（N 估 21 / 实际可能 18-23 / 偏差超 ±20% 触发反复模式 #1 P3） |
| DEVTOOL=OFF ctest | 1087 不变（add_test guard 完整 — 闭环 TASK-05 P1 #1）|
| SDL2=ON ctest | 1265→1266 PASS（含 hello_devtool_console_smoke） |
| A14 nm | 0 console 符号在 OFF binary 中 |
| 5 commits Source 溯源 | 100% 含「Source: TASK-20260503-04 plan §X」前缀 |
| 反复模式 0/7 自检 | ✅ 连续第 6 次零反复目标 |
| T1 mitigation 5 维度 | ✅ 单测覆盖 100% |
| techContext.md 同步 | ✅ TASK-04 Phase D 摘要段 |
| plan-fact reconcile | ≤ 1 项（如有 ctest 数偏差 等） |

---

## 9. 9 systemPatterns 协同度自我对照

| # | systemPattern | 本任务对照 | 评级 |
|:-:|---|---|:-:|
| 1 | 双 UpdateManager / 双层 API 范式（Phase A 沉淀） | DomBindings 复用 vs 新建 ConsoleBindings 隔离 = 双层 API 子模式应用 | ✅ |
| 2 | #ifdef + CMake conditional 范式（Phase A 沉淀） | console/ 子目录全程 #ifdef + R4 unique_ptr 子模式（Phase C 沉淀） | ✅ |
| 3 | lazy-attach C ABI 容错模式（Phase B 沉淀 + Phase C 扩展） | vx_view_eval_console + vx_view_console_log_drain 双 API lazy-attach | ✅ |
| 4 | 资源嵌入范式（Phase A 沉淀） | inspector_resources.h 加 3 extern + console_panel.{html,css,js} | ✅ |
| 5 | dogfood 路径范式（Phase A 沉淀） | hello_devtool_console_smoke + dogfood console eval | ✅ |
| 6 | Phase 0 投入定律（A→05 sext-evidence） | 本任务极简 1 子段 5 min 投入 → 预期节省 ~30+ min | 🆕 sept-evidence 升级候选 |
| 7 | A14 守门（Phase A 沉淀 + 维护演进透明度） | CMake smoke template 加 3 项 + Phase D 区分注释 | ✅（自动化沿用） |
| 8 | isolated JSRuntime 隔离模式 | C2.B 第 3 个 QuickjsEngine 实例 + console_log_buffer 隔离 buffer | 🆕 新沉淀候选 |
| 9 | capability allowlist 范式（query-only 默认） | C3.A 严格隔离 binding（不调 DomBindings::Bind） | 🆕 新沉淀候选 |

**协同度：5 ✅ 复用 + 1 升级 sept-evidence + 2 🆕 新沉淀 + 1 自动化沿用 = 跨范式协同度 100%**

---

## 10. 反复模式预防清单（连续第 6 次零反复目标）

| # | 反复模式 | 历史 | 本任务预防 | 状态 |
|:-:|---|:-:|---|:-:|
| 1 | 前置依赖/环境/API 能力未验证 | 10 次最高 | Phase 0 §0.2+§0.4 实证 devtool_script_engine_ + RegisterDevtoolBindings 已 query-only / §0.6 含 add_test guard 子条 audit（闭环 TASK-05 P1 #1） | 🛡️ 三层抑制 |
| 2 | 测试覆盖不全 | 多次 | 5 子任务 21 测分布 + T1 5 维度全覆盖 + dogfood smoke + A14 反向探针 | 🛡️ |
| 3 | 共享文件冲突 | 多次 | 7 个 [共享文件] 标注（CMakeLists.txt ×4 + inspector_resources.h + inspector_panel.html + tests/CMakeLists.txt）+ B1.A 严格串行 | 🛡️ |
| 4 | commit 粒度不一致 | 数次 | 5 commits 1/子任务 + Source 溯源（B8.A） | 🛡️ |
| 5 | 安全边界遗漏 | 多次 | T1 mitigation 5 维度自检表 + 双上限 buffer + isolated runtime 反向探针 | 🛡️ |
| 6 | MB 同步漏项 | 多次 | D.5 finalize 同步 tasks/activeContext/progress + techContext | 🛡️ |
| 7 | 反复模式自检疏漏 | 偶次 | CP2 必含反复模式 0/7 自检表 | 🛡️ |

---

## 11. 需要 creative 阶段的组件

按 spec V4=✅ creative 必要 + brainstorm C1-C4 4 维度待 creative 阶段详细设计。

**待产出文档：** `memory-bank/creative/creative-devtool-console.md`（预期 ~400-500 行）

| # | 维度 | brainstorm 锁定方向 | creative 阶段详细设计内容 |
|:-:|---|---|---|
| C1 | DevTool 主屏第 4 件套布局 | A 第 4 tab 平级 | tab 切换协议精化 / inspector_panel.html 集成方式 / dock-to-bottom 模式下的 console 视觉范式（Chrome DevTools 参考） / CSS 子集 grep 验证 input element 支持情况 / Enter keydown 与既有 keydown 冲突解决 |
| C2 | isolated JSRuntime 实施 | B 新建 console_script_engine_ | 与既有 devtool_script_engine_ 协同设计 / lifecycle 时序图 / 创建/释放顺序 / DEVTOOL=OFF 路径 #ifdef 处理 / 与 creative-quickjs-host §组件 1 方案 C Phase 2 + 组件 2 方案 B + 组件 3 方案 B 兼容性自查 |
| C3 | capability allowlist 实施 | A 严格隔离 binding | RegisterConsoleBindings 全量 API 表（whitelist） / vs RegisterDevtoolBindings 对比表 / 不调用 DomBindings::Bind 的安全证明 / 反向探针单测设计（尝试 ElementSetAttribute → undefined） / spec §11.1 「禁 file/network/Hot Reload trigger」实施细节（这些 API 本就不暴露） |
| C4 | console.log 桥接 | A drain 模式 | drain JSON envelope 完整规范 / B3.A 双上限 mitigation 算法详细 / utf8 边界截断算法 / drain 频率 vs 实时性 trade-off / level 颜色 mapping / ts_ms 时间源选择（host time vs eval time） |

---

## 12. plan ×0.6 数据点假设

**蓝图 plan 估时**：~3-4 h plan ×0.6 = 180-240 min（VAN 校准 spec 的 ~3-5 h 下调 ~10-20% 因 TASK-05 减负 + 5 大范式 100% 复用）

**子任务粒度估时**：

| # | 子任务 | plan ×0.6 | 实测预期 | 比值假设 |
|:-:|---|:-:|:-:|:-:|
| D.1 | ConsoleEngine + lifecycle | 30-40 min | 12-18 min | 0.40-0.45× |
| D.2 | RegisterConsoleBindings + drain API | 40-50 min | 15-22 min | 0.38-0.44× |
| D.3 | console_panel 资源 + tab 集成 | 40-50 min | 15-22 min | 0.38-0.44× |
| D.4 | 公开 C API + JS native binding | 30-40 min | 12-18 min | 0.40-0.45× |
| D.5 | 单测扩充 + dogfood + A14 + finalize | 40-60 min | 16-25 min | 0.40-0.42× |
| **合计** | — | **180-240 min** | **70-105 min** | **0.39-0.44×** |

**预期数据点群组**：plan ×0.6 第 73-77 数据点（5 子任务 + 1 finalize），落「极窄档延续高效区 0.30-0.45×」候选续延 Phase B 0.40× / Phase C 0.31× 同档（不会触发新效率区候选 — 因为本任务有完整 ~800 行新代码 + JS REPL UI 调试 + dogfood smoke，不像 TASK-05 ~200 行极简）。

**反复模式 0/7 命中目标**：连续第 6 次零反复（Phase A → B → C → 02 → 05 ↔ 03 部分命中 → 本任务）

---

## 13. 推荐工作流

`/van` ✅ → `/plan` ✅ **本文件** → **`/creative`（4 维度详细设计 → creative-devtool-console.md）**→ `/build`（5 子任务串行 + CP1 + CP2）→ `/reflect` → `/archive`

**下一步：** 用户调用 `/creative` 启动创意阶段（4 维度详细设计文档 → `creative-devtool-console.md`）

---

## 14. 与既有 spec / creative 的交叉引用

- **spec `docs/specs/2026-04-30-devtool-design.md` §11.1**：本任务闭环 Console 占位 / 「依赖 D2=B JSON 协议 + D7=C 双层 API」 + 「威胁面 T1：1) eval 在 isolated JSRuntime；2) capability allowlist」全部对应实施
- **creative `creative-devtool-screen-layout.md` 决策 1**：splitter dock + HUD overlay 双层结构 — 本任务 C1.A 第 4 tab 复用 splitter dock 区域，HUD 不变
- **creative `creative-devtool-screen-layout.md` 决策 2**：splitter dock-to-right / dock-to-bottom 双模式 — Console tab 在 dock-to-bottom 时天然变 Chrome 风格底部 console
- **creative `creative-quickjs-host.md` §组件 1 方案 C Phase 2**：TASK-05 已闭环 SetEvalInterruptBudget API — 本任务 ConsoleEngine 复用
- **creative `creative-quickjs-host.md` §组件 2 方案 B**：不调 js_std_add_helpers — 本任务 ConsoleEngine 严格遵守
- **creative `creative-quickjs-host.md` §组件 3 方案 B**：JS_SetMemoryLimit ~32 MiB — 本任务 ConsoleEngine 继承（不显式覆盖）
- **TASK-20260502-01 archive 5 大范式**：双层 API + #ifdef + CMake / 资源嵌入 / canvas Translate / dogfood — 本任务全部复用且加深
- **TASK-20260502-02 archive lazy-attach C ABI 容错模式**：vx_view_eval_console + vx_view_console_log_drain 复用此模式
- **TASK-20260503-01 archive #ifdef + CMake conditional 范式 unique_ptr 子模式**：R4 mitigation 直接复用

---

**文档结束**。审查者请重点关注：

1. §3 D.1-D.5 子任务划分粒度是否合理（B1.A 5 子任务串行）
2. §6 R4 风险（#ifdef + unique_ptr 子模式）是否充分覆盖（沿用 Phase C 教训）
3. §7 T1 mitigation 5 维度自检表是否完整（默认安全 / opt-in / 不可绕过 / 状态可查 / 回调约束）
4. §11 待产出 creative 文档 4 维度划分是否与 brainstorm 锁定方向一致
