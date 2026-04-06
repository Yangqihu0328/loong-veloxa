# 归档：C API 层

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-11
**复杂度级别：** Level 3
**状态：** ✅ 已完成

## 任务概述

构建稳定的 C ABI 对外接口层，使宿主应用可通过纯 C 函数嵌入 Veloxa 引擎。这是整个引擎栈的最外层封装，直接面向嵌入方（汽车 HMI 主机应用）。

## 技术方案

| 决策点 | 选定方案 | 理由 |
|--------|---------|------|
| 句柄模型 | 不透明指针（`VxView*`, `VxEventLoop*`, `VxSurface*`） | C API 标准，ABI 稳定 |
| 错误处理 | `VxResult` 枚举（VX_OK / VX_ERROR_NULL_PARAM / VX_ERROR_INVALID_STATE） | Vulkan/SDL 惯例 |
| 字符串传递 | `const char* + uint32_t len` | 兼容非终结字符串 |
| 资源层次 | EventLoop/Surface 独立创建，注入 VxViewConfig | 匹配 Application 的外部注入模式 |
| 桥接方式 | `reinterpret_cast` 不透明指针 ↔ C++ 类 | 零开销，类型安全 |

## 实现摘要

### API 表面

- **EventLoop**：`vx_event_loop_create_headless` / `destroy`
- **Surface**：`vx_surface_create_memory` / `destroy` / `save_ppm`
- **View**：`vx_view_create` / `destroy` / `load_html` / `load_css` / `inject_input` / `update` / `run` / `quit`
- **Info**：`vx_version`

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `veloxa/api/veloxa_api.h` | C99 公共头（96 行）— 不透明句柄 + 函数声明 + 类型定义 |
| 创建 | `veloxa/api/veloxa_api.cc` | C++ 桥接实现（138 行）— C 函数 → Application/EventLoop/Surface 转发 |
| 创建 | `veloxa/api/CMakeLists.txt` | vx_api 静态库（链接 vx_core + vx_graphics + vx_platform） |
| 创建 | `tests/api/api_test.cc` | 11 个单元测试 |
| 创建 | `tests/api/api_integration_test.cc` | 5 个集成测试（像素验证 + 交互验证） |
| 修改 | `CMakeLists.txt` | 添加 `add_subdirectory(veloxa/api)` |
| 修改 | `tests/CMakeLists.txt` | 添加 api_test / api_integration_test 目标 |
| 修改 | `veloxa/platform/headless/memory_surface.h` | 新增 `data() const` 只读像素访问器 |

### 关键决策

1. **每个 C 函数入口做 NULL 检查**：返回 VxResult 枚举错误码，不暴露内部异常
2. **MemorySurface::data() const 补充**：Application 构造时已 Lock Surface，测试无法再次 Lock，新增只读访问器解决测试像素读取需求
3. **无子代理**：纯桥接层复杂度适中，直接实现效率最优
4. **像素格式对齐**：集成测试使用 `PixelR/G/B` 辅助函数，按 Veloxa 标准 R[0:7] G[8:15] B[16:23] A[24:31] 提取

### 安全决策

- 所有 C API 函数检查 NULL 参数，返回 VX_ERROR_NULL_PARAM
- 仅返回枚举错误码，不暴露内部信息
- 无认证/数据保护需求（引擎层纯计算，无外部 I/O）

## 测试覆盖

- **单元测试**（11 个）：Version、CreateDestroy 生命周期、LoadHTML/CSS、InjectInput、RunQuit、NullParam 边界、SavePPM、Update 前无 HTML
- **集成测试**（5 个）：红色方块像素验证、Hover 变色、重加载 HTML 像素变化、Run/Quit 生命周期、多事件（Move+Down → :active 样式）
- **总测试**：763/763 通过，零回归

## 经验教训

1. **像素格式第二次出错**：R 在 bits[0:7]，不是 bits[16:23]。需标准化辅助函数
2. **测试基础设施需求应在计划阶段预见**：Surface Lock 排他性导致测试无法直接读像素
3. **aggregate struct 无构造函数**：Color 须用 `FromRGBA()` 而非 `Color(r,g,b,a)`
4. **计划精确度高**：5/5 新建文件完全吻合，仅 1 处计划外修改

## 参考文档

- 设计规格：`docs/specs/2026-04-05-c-api-design.md`
- 实现计划：`docs/plans/2026-04-05-c-api.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260405-11.md`
