// CSS 2.1 §10.8 line box 模型 — TASK-20260426-01 R4 #21。
//
// LineBox 抽象封装了一个 inline formatting context 的「一行」状态：每行有显式
// 的 baseline_y / max_ascent / max_descent / line-height，以及该行所含 inline
// item 的列表。本文件仅定义 POD struct（无算法），算法在 layout_engine.cc
// `LayoutInline` 中以 D2.B 严格 2-pass vertical-align 实现。
//
// 设计依据：memory-bank/creative/creative-line-box-model.md D2.A（A.1 显式
// LineBox + Vector<LineBox>）+ D2.B（严格 2-pass）。
//
// 内存压力：32K inline 节点 × ~360 行 ≈ 11.5 MB（ArenaAllocator 允许范围内）。

#ifndef VELOXA_CORE_LAYOUT_LINE_BOX_H_
#define VELOXA_CORE_LAYOUT_LINE_BOX_H_

#include "veloxa/core/layout/layout_box.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/containers/vector.h"

namespace vx::layout {

// 单个 line-box 内的 inline 单元（text run / inline-block / replaced）。
//
//   box              — 指向该单元的 LayoutBox（layout 期已分配）
//   ascent / descent — 该单元贡献给 line-box 的字体度量（CSS 2.1 §10.8.1）
//                     · text run：来自 TextMetrics.ascent / descent
//                     · inline-block：border_box_height 当 ascent，0 当 descent
//                       （creative D2.D：atomic 处理；CSS 2.1 §10.8 strut 简化）
//                     · replaced：image height 当 ascent，0 当 descent
//   baseline_offset  — 该单元相对 line-box baseline 的垂直偏移（正值上移；
//                     由 vertical-align 决定，2-pass 算法填充）
struct LineBoxItem {
  LayoutBox* box = nullptr;
  f32 ascent = 0.0f;
  f32 descent = 0.0f;
  f32 baseline_offset = 0.0f;
};

// 一行 inline-formatting-context 的完整状态。
//
//   start_x / end_x  — 行的水平范围（content-box 坐标系；end_x 为 cursor 位置）
//   top              — 行顶部 y 坐标（在 IFC content-box 中相对偏移）
//   baseline_y       — 行 baseline 在 IFC content-box 中的 y 坐标
//                     （resolve 阶段计算：top + max_ascent + half_leading_top）
//   max_ascent       — 行内 item.ascent + item.baseline_offset 的最大值
//   max_descent      — 行内 item.descent - item.baseline_offset 的最大值
//   line_height      — 实际行高（max(parent.line_height, max_ascent + max_descent)）
//   items            — 行内 inline 单元；2-pass 算法访问对象
//
// CSS 2.1 §10.8.1 半-leading：
//   leading = line_height - (max_ascent + max_descent)
//   half_leading_top    = leading / 2
//   half_leading_bottom = leading - half_leading_top    (奇数像素归底部)
struct LineBox {
  f32 start_x = 0.0f;
  f32 end_x = 0.0f;
  f32 top = 0.0f;
  f32 baseline_y = 0.0f;
  f32 max_ascent = 0.0f;
  f32 max_descent = 0.0f;
  f32 line_height = 0.0f;
  Vector<LineBoxItem> items;

  bool empty() const { return items.empty(); }

  // 行高（含半-leading 撑高）。CSS 2.1 §10.8.1：当 line-height 比 metrics 大时，
  // 行高被外部 line-height 控制；反之被 metrics 撑开。语义上恒为 line_height。
  f32 height() const {
    const f32 metrics_height = max_ascent + max_descent;
    return line_height > metrics_height ? line_height : metrics_height;
  }
};

}  // namespace vx::layout

#endif  // VELOXA_CORE_LAYOUT_LINE_BOX_H_
