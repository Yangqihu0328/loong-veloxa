# QuickJS 脚本引擎集成 实现计划

**目标：** 通过 quickjs-ng 固定版本引入 QuickJS，新增 `vx_script` 模块与 `QuickjsEngine` 封装，提供可测试的全局求值 API 与基础安全边界（源码长度上限），**不**接入 `Application` / C API。

**架构：** 根 `CMakeLists.txt` 增加 `veloxa/script` 子目录；子目录内 `FetchContent` 拉取 **quickjs-ng v0.14.0**，构建上游目标 `qjs` 与 `qjs-libc`；`vx_script` 为静态库，链接 `qjs` `qjs-libc`，对外暴露 C++17 RAII 封装，错误统一为 `vx::Status` / `vx::StatusOr<std::string>`。

**技术栈：** CMake 3.20+、C++17、quickjs-ng（C11）、GTest。

**复杂度级别：** Level 4

**关联设计：** `docs/specs/2026-04-13-quickjs-integration-design.md`

**创意决策（`/creative`）：** 实现须对齐 `memory-bank/creative/creative-quickjs-host.md`。**首合入额外要求：** 在 `QuickjsEngine::Init` 成功创建 `JSRuntime` 后调用 `JS_SetMemoryLimit(rt, 32 * 1024 * 1024)`（或等价常量）；**不得**调用 `js_std_add_helpers` / `js_std_init_handlers`；**不**注册 `JS_SetInterruptHandler`（留后续 Phase 2 API）。

---

## 文件结构（新建 / 修改）

| 路径 | 职责 |
|------|------|
| `CMakeLists.txt` | `[共享文件]` 增加 `add_subdirectory(veloxa/script)` |
| `veloxa/script/CMakeLists.txt` | `[共享文件]` FetchContent、选项、`add_library(vx_script ...)`、`target_link_libraries` |
| `veloxa/script/quickjs_engine.h` | `vx::script::QuickjsEngine` 声明 |
| `veloxa/script/quickjs_engine.cc` | QuickJS 生命周期与 `EvalGlobal` |
| `tests/script/quickjs_engine_test.cc` | 功能与安全测试 |
| `tests/CMakeLists.txt` | `[共享文件]` 注册 `quickjs_engine_test`，链接 `vx_script` |

**不修改（本任务）：** `veloxa/core/*`、`veloxa/api/*`、`examples/*` — 避免 `[影响前序测试]` 的无关链接变化。

---

## 技术验证

- **依赖：** 首次配置需访问 `https://github.com/quickjs-ng/quickjs.git`（仅 FetchContent 阶段）；离线环境需预先填充 CMake `FETCHCONTENT_FULLY_DISCONNECTED` 缓存或改用子模块镜像（记录在 `progress.md`）。
- **编译器：** quickjs-ng 使用 C11；GCC/Clang 与当前 `-Werror` **仅作用于 Veloxa 目标**；不对 `qjs` 强制 `QJS_BUILD_WERROR`。
- **验证命令：** `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j && ctest --test-dir build --output-on-failure`

---

## 任务列表

### 任务 1：脚本模块 CMake 骨架 [TDD]

**文件：**
- 创建：`veloxa/script/CMakeLists.txt`（初版可不拉取依赖，仅 `add_library(vx_script STATIC quickjs_engine.cc)` 占位）
- 创建：`veloxa/script/quickjs_engine.h`、`veloxa/script/quickjs_engine.cc`（空实现或 `return Status(StatusCode::kInternal, "stub");`）
- 修改：`CMakeLists.txt` `[共享文件]`
- 测试：先跳过，任务 2 再写

**步骤：**

- [ ] **步骤 1：** 在 `veloxa/script/CMakeLists.txt` 写入最小骨架（无 FetchContent），导出 `vx_script`。

```cmake
# veloxa/script/CMakeLists.txt（任务 1 骨架；任务 2 再插入 FetchContent）
add_library(vx_script STATIC
  quickjs_engine.cc
)
target_include_directories(vx_script PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vx_script PUBLIC vx_foundation)
```

- [ ] **步骤 2：** 根目录 `CMakeLists.txt` 在 `add_subdirectory(veloxa/core)` **之后**插入：

```cmake
add_subdirectory(veloxa/script)
```

- [ ] **步骤 3：** 提交  
  `git add veloxa/script CMakeLists.txt && git commit -m "chore(script): add vx_script CMake skeleton"`

---

### 任务 2：引入 quickjs-ng（FetchContent） [TDD]

**文件：**
- 修改：`veloxa/script/CMakeLists.txt` `[共享文件]`

**步骤：**

- [ ] **步骤 1：** 将 `veloxa/script/CMakeLists.txt` 替换为（含固定版本与关闭安装）：

```cmake
include(FetchContent)

set(QJS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  quickjsng
  GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
  GIT_TAG v0.14.0
)
FetchContent_MakeAvailable(quickjsng)

add_library(vx_script STATIC
  quickjs_engine.cc
)
target_include_directories(vx_script PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vx_script PUBLIC vx_foundation)
target_link_libraries(vx_script PRIVATE qjs qjs-libc)
```

- [ ] **步骤 2：** 运行 `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`  
  **预期：** 配置成功（需网络）；若失败，检查代理 / `GIT_TAG` 是否存在。

- [ ] **步骤 3：** `cmake --build build --target vx_script`  
  **预期：** 编译通过。

- [ ] **步骤 4：** 提交  
  `git commit -am "feat(script): fetch quickjs-ng v0.14.0 and link vx_script"`

---

### 任务 3：`QuickjsEngine` 正常求值 [TDD]

**文件：**
- 修改：`veloxa/script/quickjs_engine.h`
- 修改：`veloxa/script/quickjs_engine.cc`
- 创建：`tests/script/quickjs_engine_test.cc`
- 修改：`tests/CMakeLists.txt` `[共享文件]`

**步骤：**

- [ ] **步骤 1：编写失败测试** — `tests/script/quickjs_engine_test.cc`：

```cpp
#include "veloxa/script/quickjs_engine.h"

#include <gtest/gtest.h>

namespace vx::script {
namespace {

TEST(QuickjsEngineTest, EvalArithmetic) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  auto r = engine.EvalGlobal("1 + 2", "<test>");
  ASSERT_TRUE(r.ok()) << (!r.ok() ? r.status().message() : "");
  EXPECT_EQ(r.value(), "3");
}

}  // namespace
}  // namespace vx::script
```

在 `tests/CMakeLists.txt` 末尾增加：

```cmake
vx_add_test(quickjs_engine_test script/quickjs_engine_test.cc)
target_link_libraries(quickjs_engine_test PRIVATE vx_script)
```

- [ ] **步骤 2：** `cmake --build build --target quickjs_engine_test && ./build/tests/quickjs_engine_test`  
  **预期：** FAIL（`Init` 未实现或 `EvalGlobal` 未接 QuickJS）。

- [ ] **步骤 3：最少实现** — `veloxa/script/quickjs_engine.h`：

```cpp
#ifndef VELOXA_SCRIPT_QUICKJS_ENGINE_H_
#define VELOXA_SCRIPT_QUICKJS_ENGINE_H_

#include <string>

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/strings/string_view.h"

struct JSContext;
struct JSRuntime;

namespace vx {
namespace script {

class QuickjsEngine {
 public:
  QuickjsEngine();
  ~QuickjsEngine();

  QuickjsEngine(const QuickjsEngine&) = delete;
  QuickjsEngine& operator=(const QuickjsEngine&) = delete;

  Status Init();
  void Shutdown();

  StatusOr<std::string> EvalGlobal(StringView source, StringView filename);

 private:
  JSRuntime* rt_ = nullptr;
  JSContext* ctx_ = nullptr;
  bool inited_ = false;
};

}  // namespace script
}  // namespace vx

#endif  // VELOXA_SCRIPT_QUICKJS_ENGINE_H_
```

**修正：** `StatusOr` 与 `StringView` 同在 `veloxa/foundation` — 删除重复 `#include status.h`，保留：

```cpp
#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/strings/string_view.h"
```

`veloxa/script/quickjs_engine.cc`：

```cpp
#include "veloxa/script/quickjs_engine.h"

extern "C" {
#include "quickjs.h"
}

#include <cstring>
#include <string>

namespace vx {
namespace script {

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

StatusOr<std::string> QuickjsEngine::EvalGlobal(StringView source,
                                                StringView filename) {
  if (!inited_) {
    return Status(StatusCode::kInvalidArgument, "QuickjsEngine not initialized");
  }
  JSValue val = JS_Eval(ctx_, source.data(),
                        static_cast<size_t>(source.size()),
                        std::string(filename.data(), filename.size()).c_str(),
                        JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(val)) {
    JSValue ex = JS_GetException(ctx_);
    const char* msg = JS_ToCString(ctx_, ex);
    std::string err = msg ? msg : "unknown exception";
    if (msg) {
      JS_FreeCString(ctx_, msg);
    }
    JS_FreeValue(ctx_, ex);
    JS_FreeValue(ctx_, val);
    return Status(StatusCode::kInternal, err);
  }
  const char* out = JS_ToCString(ctx_, val);
  if (!out) {
    JS_FreeValue(ctx_, val);
    return Status(StatusCode::kInternal, "JS_ToCString failed");
  }
  std::string result(out);
  JS_FreeCString(ctx_, out);
  JS_FreeValue(ctx_, val);
  return result;
}

}  // namespace script
}  // namespace vx
```

- [ ] **步骤 4：** 重新配置并运行测试  
  **预期：** PASS。

- [ ] **步骤 5：** 提交  
  `git commit -am "feat(script): QuickjsEngine Init and EvalGlobal"`

---

### 任务 4：异常与语法错误路径 [TDD]

**文件：**
- 修改：`tests/script/quickjs_engine_test.cc`

**步骤：**

- [ ] **步骤 1：编写失败测试**（实现若已正确则可能直接 PASS；若未处理异常则 RED）：

```cpp
TEST(QuickjsEngineTest, EvalSyntaxError) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  auto r = engine.EvalGlobal("+++", "<test>");
  EXPECT_FALSE(r.ok());
  EXPECT_FALSE(r.status().message().empty());
}

TEST(QuickjsEngineTest, EvalThrownError) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  auto r = engine.EvalGlobal("throw new Error('x')", "<test>");
  EXPECT_FALSE(r.ok());
  EXPECT_NE(r.status().message().find("x"), std::string::npos);
}
```

- [ ] **步骤 2：** 运行 `ctest --test-dir build -R QuickjsEngineTest --output-on-failure`  
  **预期：** PASS；若 `EvalGlobal` 未 `JS_FreeValue` 异常分支，用 ASan 或 LeakSanitizer 后续增强（本任务可选）。

- [ ] **步骤 3：** 提交

---

### 任务 5：安全 — 源码长度上限 [TDD]

**文件：**
- 修改：`veloxa/script/quickjs_engine.h`（可选：暴露 `SetMaxSourceBytes` 用于测试）
- 修改：`veloxa/script/quickjs_engine.cc`
- 修改：`tests/script/quickjs_engine_test.cc`

**步骤：**

- [ ] **步骤 1：编写安全测试**

```cpp
#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/strings/string_view.h"

using vx::StatusCode;
using vx::StringView;

TEST(QuickjsEngineSecurityTest, RejectsOversizedSource) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  std::string big(300 * 1024, 'a');
  big += "\n1";
  auto r = engine.EvalGlobal(
      StringView(big.data(), static_cast<usize>(big.size())), "<big>");
  ASSERT_FALSE(r.ok());
  EXPECT_EQ(r.status().code(), StatusCode::kInvalidArgument);
}
```

（默认上限 **256 * 1024**，与测试 300KiB 一致为「应拒绝」。）

- [ ] **步骤 2：** 运行测试 **预期：** FAIL。

- [ ] **步骤 3：实现** — 在 `EvalGlobal` 开头：

```cpp
static constexpr size_t kDefaultMaxSourceBytes = 256 * 1024;

StatusOr<std::string> QuickjsEngine::EvalGlobal(StringView source,
                                                StringView filename) {
  if (!inited_) {
    return Status(StatusCode::kInvalidArgument, "QuickjsEngine not initialized");
  }
  if (source.size() > kDefaultMaxSourceBytes) {
    return Status(StatusCode::kInvalidArgument, "script source exceeds max size");
  }
  // ... existing JS_Eval ...
}
```

- [ ] **步骤 4：** 测试 PASS。

- [ ] **步骤 5：** 依赖审计（记录）：查阅 quickjs-ng `v0.14.0` Release Note / 已知 CVE；无自动化 `npm audit` 适用项，在 `reflection` 中手写结论。

- [ ] **步骤 6：** 提交  
  `git commit -am "feat(script): cap eval source size for DoS mitigation"`

---

### 任务 6：全量回归与文档收尾

- [ ] **步骤 1：** `ctest --test-dir build --output-on-failure`  
  **预期：** 全部 PASS，`vx_core` 测试无新增链接问题。

- [ ] **步骤 2：** 若设计规格有用户修订，同步更新 `docs/specs/2026-04-13-quickjs-integration-design.md` 版本表。

- [ ] **步骤 3：** 提交 Memory Bank 更新（与 `/reflect` / `/archive` 规范一致的可单独提交或并入最终实现提交）。

---

## 需要 `/creative` 的组件（Level 4）

以下内容在 **`/creative`** 中产出决策文档（`memory-bank/creative/creative-quickjs-host.md` 建议命名）：

1. **执行预算默认值：** `JS_SetInterruptHandler` / 指令计数 / 与主线程 `EventLoop` 的协作方式。  
2. **`qjs-libc` 与 `js_std_add_helpers` 是否默认启用** 及对车载信号/计时的影响。  
3. **长期内存策略：** QuickJS 默认 `malloc` vs 未来对接 `vx::Arena` / 自定义 `JS_SetRuntimeOpaque` 方案草图。

本实现计划 **不包含** DOM 绑定、C API 导出、`<script>` 标签加载。

---

## 执行交接

计划完成并保存到 `docs/plans/2026-04-13-quickjs-integration.md`。

**准备执行？** 设计规格批准后：先 **`/creative`**（处理上表三项），再 **`/build`** 按任务 1→6 顺序实施。
