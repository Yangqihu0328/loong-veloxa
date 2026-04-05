# DOM 树 + HTML 解析器设计规格

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-03
**状态：** 已批准

## 1. 目标

为 Veloxa 引擎构建 DOM 树数据结构和 HTML 解析器，支撑后续 CSS 引擎、Layout 引擎、事件系统。
面向车载 HMI 场景裁剪，不追求完整 HTML5 规范兼容。

## 2. 设计决策

### 2.1 DOM 节点类层次（方案 A：精简扁平）

```
Node (base)
├── Element  — tag + 属性 + 子节点
├── Text     — 文本内容
├── Comment  — 注释
└── Document — 文档根（特殊 Element）
```

- DOM 与 Layout 完全分离
- 节点不携带布局/渲染信息

### 2.2 Tag & Attribute 系统（方案 A：枚举 + InternedString）

- 已知标签用 `TagId` 枚举（~60 个），switch/case 可优化
- 未知/自定义标签通过 InternedString 兜底
- 每个 TagId 关联 `TagInfo`：TagType、ParseModel、ContentModel
- 属性存储：`SmallVector<Attribute, 4>`

### 2.3 HTML Tokenizer（方案 A：简化状态机）

- 参考 Sciter `markup::scanner`，单遍扫描
- 产出 Token 结构体（type + name + value + self_closing）
- 支持行号/列号追踪
- 处理基本字符实体（&amp; &lt; &gt; &quot;）
- 不处理完整 HTML5 spec 的所有边界 case

### 2.4 HTML Parser + DOM Builder

- Tokenizer 驱动，消费 Token 流构建 DOM 树
- 维护开放元素栈
- 处理隐式标签关闭（精简规则集）
- 处理 void 元素（不需要关闭标签）

### 2.5 内存管理（方案 A：Arena 分配）

- Document 拥有 ArenaAllocator
- 所有节点从 Document 的 Arena 分配
- Document 销毁时一次释放所有节点

## 3. 命名空间

- `vx::dom` — DOM 节点、TagId、TagInfo、Attribute
- `vx::html` — Tokenizer、Parser

## 4. 依赖

- `vx_foundation`：types, InternedString, StringView, String, Vector, SmallVector, HashMap, Status, ArenaAllocator

## 5. 非目标

- 完整 HTML5 规范兼容
- JavaScript DOM API（后续 script 模块）
- SVG 支持
- Shadow DOM
- 自定义元素（Custom Elements API）

## 6. 需要 /creative 阶段的组件

1. HTML Tokenizer 状态机 — 状态转移细节、字符实体处理
2. 隐式标签关闭规则 — 规则表设计
