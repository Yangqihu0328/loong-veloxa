# W3C web-platform-tests Fixtures

来源：https://github.com/web-platform-tests/wpt
许可证：BSD-3-Clause（详见 wpt 仓库 LICENSE）
拉取时间：2026-04-26
拉取分支：master
拉取 commit（如 API 可达）：unknown
代理地址：见 `memory-bank/techContext.md` §FetchContent 与代理（开发环境注意）

## 文件清单（6 testcase + 5 ref，11 文件）

### css/CSS2/margin-padding-clear/
- `margin-collapse-001.xht` + `-ref` — CSS 2.1 §8.3.1 case 1 (sibling adjoining margin)
- `margin-collapse-002.xht` + `-ref` — CSS 2.1 §8.3.1 case 2 (parent + first child)
- `margin-collapse-003.xht` + `-ref` — CSS 2.1 §8.3.1 case 3 (collapse-through)
- `margin-collapse-005.xht` — 复杂 collapse 组合（多层嵌套，无 ref）

### css/CSS2/linebox/
- `inline-formatting-context-001.xht` + `-ref` — IFC 基本断言
- `vertical-align-baseline-001.xht` + `-ref` — vertical-align: baseline 几何

## 实证微调记录（plan §0.P0.2 → R0 实施差异）

- plan 原选 `margin-collapse-091` 不存在（HTTP 404，wpt master 上 088-095 全 404）
- 替代为 `margin-collapse-005`（HTTP 200，1742 bytes，覆盖复杂 collapse 形态）
- vertical-align baseline fixture 在 `linebox/` 子目录下（不在独立 `vertical-align/`）

## 拉取脚本（已成功执行）

见 `docs/plans/2026-04-26-layout-correctness.md` §0.P0.2

## 集成测试入口（R3-R4 实施）

`tests/integration/wpt_layout_test.cc`（待 R3 创建）— 用 `vx::html::Parser` 加载 fixture，断言 layout box 几何（坐标 / 尺寸 ± 1 px 容差）。XHTML 头预处理：去 `<?xml ... ?>` + DOCTYPE 替换为 HTML5（plan §3 R3.5 + §4 R4.8）。
