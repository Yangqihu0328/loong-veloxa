# 活跃上下文

## 当前阶段
构建中

## 当前任务
- **ID**：TASK-20260405-04
- **描述**：构建 CSS 引擎
- **复杂度**：Level 4
- **代码规范**：Google C++ Style Guide

## 设计决策（已确认）
1. **属性子集**：方案 A — 核心最小集 ~45 属性
2. **值类型系统**：A+C 混合 — 解析用 CssValue(8B)，计算用 ComputedStyle 直存
3. **CSS Tokenizer**：方案 A — 主分支子扫描器 + 注释分离 + 空白保留
4. **选择器系统**：从右到左匹配
5. **层叠/继承**：标准 CSS3 specificity
6. **DOM 集成**：Element 缓存 id/classes
7. **颜色解析**：排序数组(18项) + 二分查找

## 构建进度
- Phase 1：进行中
