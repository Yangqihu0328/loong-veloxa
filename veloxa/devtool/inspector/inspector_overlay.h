#ifndef VELOXA_DEVTOOL_INSPECTOR_INSPECTOR_OVERLAY_H_
#define VELOXA_DEVTOOL_INSPECTOR_INSPECTOR_OVERLAY_H_

#include "veloxa/core/layout/layout_box.h"
#include "veloxa/core/render/paint_command.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/graphics/types.h"

namespace vx::devtool {

// Inspector hover highlight overlay 注入 (TASK-20260502-01 A.1.1, I3, A5).
//
// 与 spec §5.2 I3 (DisplayList overlay 注入点) 配合，在 DevTool Inspector
// hover 选取 LayoutBox 时，向 target Document 的 DisplayList 末尾追加 1 条
// PaintCommand::kOverlayHighlight 命令。Renderer 按 creative #1 决策 4 的
// 渲染顺序（target paint → dirty rect → layout box → hover）末位渲染该命令，
// 保证 hover 红框可见且不污染目标 DOM。
//
// === T5 mitigation 协议 ===
// 调用方约定：每帧首端必须先调用 render::ResetOverlayCommands(list) 清除
// 上一帧 overlay 命令（A.0.4 已落地），再选择性调用 InjectHoverHighlight()
// 追加新 hover 命令。本函数**仅追加**，不 reset，避免误清除目标 paint。
//
// === plan A.1.1 锁定签名（M2 修正 — 原 plan 假设 Document* 错误，因 Document
// 不持有 DisplayList） ===
class InspectorOverlay {
 public:
  // 向 list 末尾追加 1 条 kOverlayHighlight 命令，rect = hovered_box 的
  // border-box（含 padding/border，不含 margin —— 与 Chrome DevTools 选择
  // border-box 高亮规则一致）。
  //
  // null hovered_box → no-op（防御性，避免 hover_node 为空时崩溃）。
  // 颜色默认值：红 alpha=230 (~0.9)。stroke 默认 2.0px（与 creative #1
  // 决策 4 表「Inspector hover red 2px solid」一致）。
  static void InjectHoverHighlight(
      render::DisplayList& list, const layout::LayoutBox* hovered_box,
      gfx::Color color = gfx::Color::FromRGBA(255, 0, 0, 230),
      f32 stroke_width = 2.0f);

  // TASK-20260502-02 B.2.3 — Performance Overlay dirty rect 边框注入.
  //
  // 向 list 末尾追加 N 条 kOverlayHighlight 命令 (复用工厂
  // PaintCommand::OverlayDirtyRect — 黄绿色 1px stroke，区分 hover red 2px).
  // 每个非空 rect 产 1 命令；空 rect (IsEmpty()) 跳过避免 0×0 视觉伪影.
  //
  // T5 mitigation 协议复用 — 调用方约定每帧首端 ResetOverlayCommands(list)
  // 清旧 overlay，再调本函数追加本帧 dirty rect 边框.
  // 与 PerfOverlay::OnFrameStart 调 UpdateManager::ClearDirtyRects 配套
  // (B.0.2 + B.1.2)；本函数从 UpdateManager::dirty_rects() 读本帧累积.
  static void InjectDirtyRectHighlights(render::DisplayList& list,
                                         const Vector<gfx::Rect>& rects);
};

}  // namespace vx::devtool

#endif  // VELOXA_DEVTOOL_INSPECTOR_INSPECTOR_OVERLAY_H_
