# 活跃上下文

## 当前阶段
初始化

## 当前任务
- **ID**：TASK-20260405-04
- **描述**：构建 CSS 引擎
- **复杂度**：Level 4
- **代码规范**：Google C++ Style Guide

## 焦点
- CSS Tokenizer（字节流 → CSS Token 流）
- CSS Parser（Token 流 → 规则集/声明列表）
- 选择器系统（选择器解析 + DOM 匹配）
- 属性系统（CSS 属性子集 + 值类型）
- 层叠与继承（specificity 计算 + 属性继承 + 计算值）

## Sciter 参考要点
- css_istream：三种 token 模式（selector/body/value）
- style_parser：规则解析 + @media/@import
- STYLE_ATTS 枚举：CSS 属性 ID 符号化
- char_style：可继承属性结构体
- style_bag：文档级样式规则集

## 待处理事项
- **P1**：补充 Benchmark（网络恢复后，来源 TASK-20260405-01）
- **P1**：子代理 prompt 模板增加「跨模块数据格式」段（来源 TASK-20260405-02）— 已验证有效
- **P1**：集成测试优先验证数据格式一致性（来源 TASK-20260405-02）
- **P1**：Document 集成 ArenaAllocator（来源 TASK-20260405-03）
