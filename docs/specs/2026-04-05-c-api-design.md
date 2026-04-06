# 设计规格：C API 层

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-11

## 概述

构建稳定的 C ABI 对外接口，使宿主应用通过纯 C 函数嵌入 Veloxa 引擎。

## 设计决策

| 决策点 | 选定方案 | 理由 |
|--------|---------|------|
| 句柄模型 | 不透明指针（`VxView*`） | C API 标准，类型安全 |
| 错误处理 | `VxResult` 枚举 | Vulkan/SDL 惯例 |
| 字符串 | `const char* + uint32_t len` | 兼容非终结字符串 |
| 资源层次 | EventLoop/Surface 独立创建，注入 View | 匹配 Application 的外部注入模式 |

## API 表面

```c
/* === Types === */
typedef struct VxView VxView;
typedef struct VxEventLoop VxEventLoop;
typedef struct VxSurface VxSurface;

typedef enum {
    VX_OK = 0,
    VX_ERROR_NULL_PARAM = -1,
    VX_ERROR_INVALID_STATE = -2,
} VxResult;

typedef enum {
    VX_EVENT_POINTER_MOVE = 0,
    VX_EVENT_POINTER_DOWN,
    VX_EVENT_POINTER_UP,
    VX_EVENT_KEY_DOWN,
    VX_EVENT_KEY_UP,
} VxEventType;

typedef struct {
    VxEventType type;
    float x, y;
    uint8_t button;
    uint32_t key_code;
    uint32_t modifiers;
} VxInputEvent;

typedef struct {
    VxEventLoop* event_loop;
    VxSurface* surface;
    uint32_t target_fps;
    uint32_t background_color;  /* RRGGBBAA */
} VxViewConfig;

/* === Event Loop === */
VxEventLoop* vx_event_loop_create_headless(void);
void         vx_event_loop_destroy(VxEventLoop* loop);

/* === Surface === */
VxSurface* vx_surface_create_memory(uint32_t w, uint32_t h);
void       vx_surface_destroy(VxSurface* surface);
VxResult   vx_surface_save_ppm(const VxSurface* surface, const char* path);

/* === View === */
VxView*  vx_view_create(const VxViewConfig* config);
void     vx_view_destroy(VxView* view);
VxResult vx_view_load_html(VxView* view, const char* html, uint32_t len);
VxResult vx_view_load_css(VxView* view, const char* css, uint32_t len);
VxResult vx_view_inject_input(VxView* view, const VxInputEvent* event);
VxResult vx_view_update(VxView* view);
VxResult vx_view_run(VxView* view);
VxResult vx_view_quit(VxView* view);

/* === Info === */
const char* vx_version(void);
```
