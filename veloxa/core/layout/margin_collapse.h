#ifndef VELOXA_CORE_LAYOUT_MARGIN_COLLAPSE_H_
#define VELOXA_CORE_LAYOUT_MARGIN_COLLAPSE_H_

#include <algorithm>

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::layout {

// =============================================================================
// MarginChain — CSS 2.1 §8.3.1 vertical margin collapsing 累加器
//
// W3C 规则关键：「当 N 个相邻 vertical margin 接触（无 padding/border/content
// 间隔）时，合并值 = max(positives) + min(negatives)」。
//   - 正负各取一极值后相加（**不是**简单求 max）
//   - 例：margins {10, -5, 20} → max(10,20) + min(-5) = 20 + (-5) = 15
//   - 例：margins {-5, -10}    → 0 + min(-5,-10)     = 0  + (-10) = -10
//   - 例：margins {0}           → IsEmpty 等价
//
// 设计决策（creative-margin-collapsing.md D1.1/D1.2）：
//   - POD struct（header-only inline，arena 友好，零 vtable）
//   - LayoutBlock 内部以「滚动 chain」形式 in-line 累积
//   - has_clearance 字段为 P3 (TASK-26-02 float/clear 完整) 预留接口
// =============================================================================

struct MarginChain {
  f32 max_positive = 0.0f;
  f32 min_negative = 0.0f;  // 存储负数（不取绝对值）
  bool has_clearance = false;

  // 加入一个 margin 值；正负分流以满足 max(pos)+min(neg) 协议。
  void Add(f32 m) {
    // R3 优化（TASK-20260426-01）：m == 0 是 hot path（zero-margin tree），
    // 严格 `> 0 / < 0` 让 m=0 走 0 分支，编译器可省 max/min 调用与寄存器写。
    // 语义不变：max(p, 0) == p、min(n, 0) == n（0 始终为单位元）。
    if (m > 0.0f) {
      max_positive = std::max(max_positive, m);
    } else if (m < 0.0f) {
      min_negative = std::min(min_negative, m);
    }
  }

  // 当前合并值（W3C §8.3.1 collapsed margin）。
  f32 Collapsed() const { return max_positive + min_negative; }

  // 是否未累加任何非零 margin（含 has_clearance 标志位独立）。
  bool IsEmpty() const {
    return max_positive == 0.0f && min_negative == 0.0f;
  }

  // P3 占位：clearance 阻断 collapse 链。完整实施 = TASK-26-02 (float/clear)。
  // 当前 R3 仅设置标志位，不影响 Collapsed() 数值（Collapsed 语义保持纯加和）。
  void ApplyClearance(f32 /*amount*/) { has_clearance = true; }

#ifdef VX_DEBUG_LAYOUT
  void Trace(StringView name) const;
#else
  void Trace(StringView /*name*/) const {}
#endif
};

}  // namespace vx::layout

#endif  // VELOXA_CORE_LAYOUT_MARGIN_COLLAPSE_H_
