# Loong Veloxa — 项目简介

## 项目名称
Loong Veloxa（龙·疾风）

## 项目定位
面向**车载 HMI（人机交互界面）与嵌入式面板**的高性能开源 UI 渲染引擎。

## 项目愿景
构建一个轻量、高效、可嵌入的 HTML/CSS 渲染引擎，专为资源受限的嵌入式环境（车载系统、工业面板、IoT 设备）优化，提供类 Web 的开发体验与原生级的渲染性能。

## 核心目标
1. **轻量化**：二进制体积小，内存占用低，适合嵌入式 Linux / RTOS
2. **高性能渲染**：硬件加速（OpenGL ES / Vulkan），60fps 流畅动画
3. **Web 兼容子集**：支持车载 HMI 常用的 HTML/CSS/JS 子集（非完整浏览器）
4. **可嵌入性**：清晰的 C API，易于集成到现有车载/嵌入式框架
5. **开源开放**：MIT/Apache 2.0 双许可

## 参考分析
基于对 Sciter 5.0.2.16 源码架构的深度分析（源码路径 `/mnt/d/workspace/stable-5.0.2.16/`），借鉴其分层设计理念，但针对嵌入式场景做全面重新设计。

## 目标平台
- 嵌入式 Linux（主要）
- Linux Desktop（开发/测试）
- 车载 RTOS（远期）
- Windows（开发/测试，次要）

## 目标用户
- 车载 HMI 开发者
- 嵌入式 UI 开发者
- IoT 设备面板开发者

## MVP 范围参考（V1=D 三档分级）

本项目采用**三档分级 MVP-A/B/C**，每档对应一组验收标准 + 完成度 + 已知 gap + 估时 plan ×0.6：

| MVP 档 | 完成度 | 代表性场景 | 与核心目标对应 |
|:-:|:-:|---|---|
| **MVP-A 最小可运行 demo** | ✅ **100%** | 嵌入式 boot 屏 / 简单状态显示 | 核心目标 #4 + #5 |
| **MVP-B 桌面 dogfood 完整** | ⚠️ **~90%** | HMI 原型开发 / 第三方桌面 evaluation | + 核心目标 #3 |
| **MVP-C 真嵌入式部署** | 🟡 **~70%** | 车载中控 / 仪表盘 / 工业 HMI / IoT 面板 / 医疗设备 | + 核心目标 #1 + #2（嵌入式 Linux + 硬件加速 60fps）|

**详细 MVP 定义、验收清单、9 项 gap 路径、推荐立项顺序：** [`docs/specs/2026-05-04-mvp-scope.md`](../docs/specs/2026-05-04-mvp-scope.md)
**各档详细设计：** [`memory-bank/creative/creative-mvp-A.md`](creative/creative-mvp-A.md) + [`creative-mvp-B.md`](creative/creative-mvp-B.md) + [`creative-mvp-C.md`](creative/creative-mvp-C.md)
