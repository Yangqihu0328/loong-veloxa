# 活跃上下文

## 当前阶段
初始化

## 当前任务
- **ID**：TASK-20260405-03
- **描述**：构建 DOM 树 + HTML 解析器
- **复杂度**：Level 4
- **代码规范**：Google C++ Style Guide

## 焦点
- DOM 节点类层次（Node → Element / Text / Document）
- HTML Tokenizer（字节流 → Token 流）
- HTML Parser + DOM Builder（Token 流 → DOM 树）
- 车载 HMI 场景裁剪（去除桌面特有 DOM 功能）

## Sciter 参考要点
- node 基类：NODE_TYPE、parent 指针、遍历、序列化
- element：tag symbol、子节点 array、属性 map
- tag 命名空间：符号表 + TAG_TYPE/PMODEL/CMODEL 定义
- scanner：状态机 tokenizer
- dom_builder：token → DOM，处理隐式关闭

## 待处理事项
- **P1**：补充 Benchmark（网络恢复后，来源 TASK-20260405-01）
- **P1**：子代理 prompt 模板增加「跨模块数据格式」段（来源 TASK-20260405-02）
- **P1**：集成测试优先验证数据格式一致性（来源 TASK-20260405-02）
