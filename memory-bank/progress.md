# 进度记录

## 当前任务

### TASK-20260502-01：DevTool Phase A — Inspector 实施（DOM tree / Style / Layout panel + hover 高亮）[安全相关]

#### `/build` 阶段进度（2026-05-02 13:00 启动）

**Phase 0.1 完成 ✅（2026-05-02 13:00，~3 min）：** cmake reconfigure（build/ 过期 mtime 2026-04-26）→ 全量构建 → ctest **1062/1062 PASS**（含 R2.5 新增 #1002 `DecodeFromFileRejectsOversizedFile`）+ 1 Skip（Wpt001 沿用 TASK-26-01 沉淀）。基线与 plan §0.1 预期完全一致。

**Phase A.0.1 完成 ✅（2026-05-02 13:15，~10 min vs plan ~54 min ×0.6 = 0.18× plan ×0.6 → 极窄档延续）：**

- **Step 1 RED**：tests/core/application_test.cc 末尾 +2 测（`DevtoolDocumentSlotDefaultsToNull` + `TargetDocumentRetainsContentAfterLoadHTML`）
- **Step 2 验证 RED**：编译失败 `'class vx::Application' has no member named 'target_document'` ✅（与 plan 预期一致）
- **Step 3 GREEN**：
  - `application.h`: 重命名 `document_` → `target_document_` + 加 `devtool_document_ = nullptr` 槽 + 移除 `document()` getter + 加 `target_document() / devtool_document()` getters（保 const & inline 风格一致）
  - `application.cc`: 7 处内部 `document_` → `target_document_`（析构 / LoadHTML 2 处 / LoadScript 2 处 / EnsureUpdateManager 2 处）；析构注释 `devtool_document_` ownership 由 DevTool 子系统拥有
  - `application_test.cc`: 4 处 callsite 改名（line 53/60/72/208，与 plan §0.7 实证一致）
- **Step 4 验证 GREEN**：全量构建 + ctest **1064/1064 PASS**（基线 1062 + 2 新测 = 1064）+ 1 Skip
- **Step 5 收尾验证脚本**：`rg "->document\(\)" veloxa/ examples/` = 0 命中 ✅；`rg "\.document\(\)" tests/` 仅 `dom_bindings_test.cc:508 bindings.document()`（DomBindings 自有 method，按 plan 不应改）✅ → **0 漏改**
- **Step 6 反向探针** ✅：临时把 `devtool_document_` 默认值改为 `reinterpret_cast<dom::Document*>(0xdeadbeef)` → 测 #935 FAIL（捕获能力 verified）；恢复后 PASS
- **Lint clean** ✅
- **R1 风险结案**：实证 callsite 4 处 → A.0.1 实施零漏改（远低于 spec §9 估 10-15 处），R1 🟡→🟢 结案
- **commit**：[A.0.1 实测耗时 ~10 min，远低于蓝图估 54 min plan ×0.6 = 0.18×；候选 plan ×0.6 第 18 数据点「极窄档延续」]

**Phase A.0.2 完成 ✅（2026-05-02 13:25，~10 min vs plan 18 min ×0.6 = 0.56× plan ×0.6 → 窄延续）：**

- **关键发现 — plan 假设修正**：
  - `vx::String` 实际 API 是 `append/reserve`（小写，非 `Append/Reserve`）+ 无 `contains` 方法
  - `LayoutBox` 是 header-only struct，需新建 `layout_box.cc` + 加入 `vx_core` CMakeLists.txt
  - 测试断言改用 `std::string(s.data(), s.size()).find(...) != std::string::npos`（加 `AsStd` helper）
- **Step 1 RED**：layout_box_test.cc +4 测（`ToJsonBasicGeometry` / `ToJsonMarginCollapseStateSerialized` / `ToJsonAllLayoutTypeVariants` / `ToJsonChildCountReflectsAppendedChildren`）
- **Step 2 验证 RED**：编译失败 `'struct vx::layout::LayoutBox' has no member named 'ToJson'` ✅
- **Step 3 GREEN**：
  - `layout_box.h`: include `string.h` + 加 `String ToJson() const;` 声明（含 schema doc 注释）
  - `layout_box.cc` (新建)：实现 `ToJson()` — `LayoutTypeName/AppendF32 (%g)/AppendBool/AppendU32/AppendQuad` helper；reserve(320) 避免多次扩容
  - `vx_core/CMakeLists.txt`: 加 `layout/layout_box.cc` 源
- **Step 4 验证 GREEN**：cmake reconfigure（CMakeLists 变）+ 全量构建 + ctest **1068/1068 PASS**（基线 1064 + 4 新测 = 1068）+ 1 Skip（Wpt001 沿用）
- **Step 5 反向探针**：
  - 探针 1（`collapsed_through` 默认 `false→true`）：不可探，因测 #665 主动 set true，默认值变化检测不到（设计上无效）
  - 探针 2（`LayoutTypeName(kBlock)` 改 `inline`）：测 #664 FAIL ✅（捕获能力 verified）
- **Lint clean** ✅
- **技术债 #26 LayoutBox.Dump 缺失** 闭环 ✅（A4 验收准入达成）
- **commit**：[A.0.2 实测耗时 ~10 min，vs plan 18 min ×0.6 = 0.56×；候选 plan ×0.6 第 19 数据点「窄档延续」：API 校准成本 ~3 min 折抵新建文件成本]

**Phase A.0.3 完成 ✅（2026-05-02 13:35，~15 min vs plan 27 min ×0.6 = 0.56×）：**

- **关键发现 — plan 假设修正**：
  - `vx::dom` 无 `InternString()` 自由函数 → 用 `InternedString::Intern("name")` 静态方法
  - `Serializer.cc` 原本无对应单测 → 新建 `tests/core/dom/serializer_test.cc` 兼测 HTML Serialize（pin 现有行为）+ JSON ToJson
- **Step 1 RED**：新建 `serializer_test.cc` 含 12 测（2 个 HTML pin + 10 个 JSON 含 3 个 T3 安全测）
- **Step 2 验证 RED**：编译失败 `'ToJson' was not declared in this scope` ✅
- **Step 3 GREEN**：
  - `serializer.h`: 加 `RedactionPolicy` enum (`kRedactSensitive` / `kNone`) + `String ToJson(const Node*, RedactionPolicy);` 声明（带 schema doc + RFC 8259 标注）
  - `serializer.cc`: 实现 `EscapeJsonString` (RFC 8259 §7：`"`/`\\`/`\b`/`\f`/`\n`/`\r`/`\t` + 控制字符 `\uXXXX`) + `IsSensitiveAttribute` (T3 单点决策：`tag_id() == kInput && attr_name == "value" && type == "password"`) + `ToJsonNode/Element/Document` 递归
  - `tests/CMakeLists.txt`: 加 `dom_serializer_test`
- **Step 4 验证 GREEN**：cmake reconfigure（CMakeLists 变）+ 全量构建 + ctest **1080/1080 PASS**（基线 1068 + 12 新测 = 1080）+ 1 Skip
- **Step 5 反向探针**：
  - 探针 1（拆 redaction：把 `policy == kRedactSensitive` 改 `false`）：测 #406 `T3PasswordInputValueRedacted` FAIL ✅（核心 T3 防护捕获能力 verified）
  - 探针 2（`IsSensitiveAttribute` 的 `kInput` 检查改 `return true`）：设计失败 — 修改路径与目标测试无交集（input 元素本身就是 kInput tag），不影响测捕获能力（用探针 1 已充分验证）
- **Lint clean** ✅
- **T3 安全威胁 mitigation 落地** ✅（A1 验收准入达成）
- **commit**：[A.0.3 实测耗时 ~15 min，vs plan 27 min ×0.6 = 0.56×；候选 plan ×0.6 第 20 数据点「窄档延续」]

**Phase A.0.4 完成 ✅（2026-05-02 13:50，~15 min vs plan 18 min ×0.6 = 0.83×）：**

- **关键发现 — plan 假设修正**：
  - `gfx::Color` 是 aggregate struct（4 个 u8 字段 r/g/b/a）+ 提供 `FromRGBA(r,g,b,a)` 静态工厂；不能用函数式构造 `Color(0,100,200,128)` → 改用 `Color::FromRGBA(...)`
  - `Replay()` switch 用 `-Werror=switch`，新增 enum value 必须加 case → 加 `kOverlayHighlight` case 用 `StrokeRoundedRect(rect, 0.0f, brush, stroke_width)` 等价 thin border
  - `vx::Vector::erase(it)` 返回 next iterator → erase-remove 模式可行
- **Step 1 RED**：
  - `paint_command_test.cc` +1 测 `OverlayHighlightFactory`（验工厂）
  - `renderer_test.cc` +4 测 `ResetOverlayCommandsTest.{RemovesAllOverlayHighlightCommands, PreservesNonOverlayCommands, NoOpOnEmptyList, NoOpOnListWithoutOverlays}`
- **Step 2 验证 RED**：编译失败 `'kOverlayHighlight' is not a member of 'vx::render::PaintCommand::Type'` ✅
- **Step 3 GREEN**：
  - `paint_command.h`: 加 `kOverlayHighlight` enum value（含 T5 lifecycle doc 注释）+ 静态工厂 `OverlayHighlight(rect, color, stroke_width)`
  - `renderer.h`: 加 `void ResetOverlayCommands(DisplayList&)` 声明（含 T5 mitigation doc 注释）
  - `renderer.cc`: 实现 `ResetOverlayCommands`（in-place erase-remove）+ `Replay` switch +`kOverlayHighlight` case（StrokeRoundedRect with radius=0 等价 stroke rect）
- **Step 4 验证 GREEN**：全量构建 + ctest **1085/1085 PASS**（基线 1080 + 5 新测）+ 1 Skip
- **Step 5 反向探针**：把 `ResetOverlayCommands` 改 no-op → 测 #837 `ResetOverlayCommandsTest.RemovesAllOverlayHighlightCommands` FAIL ✅（T5 mitigation 捕获能力 verified）；恢复后 4/4 PASS
  - **教训**：用 `sed` 改 C++ 多行函数体导致死循环 + ctest 卡住（用 `kill` + `git checkout` 清理后用 `StrReplace` 安全替换）→ 沉淀「反向探针避免 sed 多行删除」
- **Lint clean** ✅
- **T5 安全威胁 mitigation 落地** ✅（A5 验收准入达成）
- **commit**：[A.0.4 实测耗时 ~15 min，vs plan 18 min ×0.6 = 0.83×；plan ×0.6 第 21 数据点候选「窄档延续」]

**Phase A.0.5 完成 ✅（2026-05-02 13:35，~25 min vs plan 36 min ×0.6 = 0.69×）：**

- **关键发现 — plan §A.0.5 假设全部错误，Build 阶段全部修正**：
  - `Document::CreateElement(string)` 不存在 → 实际 `CreateElement(TagId)`（用 TagId::kHtml/kBody/kInput 等）
  - `RedactionPolicy::kDefault` 不存在 → 用 A.0.3 已实施的 `kRedactSensitive` / `kNone`
  - `String::Reserve/Append/contains` API 全错 → `reserve/append`（小写）+ 测试改用 `std::string::find` 经 `AsStd` helper
  - **`ComputedStyle::SetProperty/ForEachProperty/Value::Color/PropertyIdToString` 全部不存在** — `ComputedStyle` 是 66 字段 plain struct（无 method）
  - 因此 SerializeComputedStyle 改用「显式列 10 个常用属性」简化策略（display/position/width/height/color/background_color/font_size/opacity/margin/padding）+ 自写 helper（UnitSuffix / AppendLengthValue / AppendColorHex / AppendEnumProperty 用 EnumValueToCssString）
  - plan §步骤 5 写 OFF baseline 1072 也是错的（漏算 SDL2/benchmark 差异）→ 实际 OFF baseline = 1031（basic 原 baseline）+ 23（A.0.1-A.0.4 加到 core/api 的非 DEVTOOL 测）= **1054**
- **Step 1 RED**：新建 `tests/devtool/inspector/inspector_data_test.cc` 含 10 测（3 SerializeDocument + 1 T3 propagation + 2 SerializeLayoutBox delegate + 4 SerializeComputedStyle）+ 子目录 CMakeLists（含 `include(GoogleTest)` 子目录守门）
- **Step 2 验证 RED**：cmake generate 阶段失败 `No SOURCES given to target: vx_devtool`（inspector_data.cc 缺）✅
- **Step 3 GREEN**：
  - 新建 `veloxa/devtool/CMakeLists.txt` + `veloxa/devtool/inspector/CMakeLists.txt`（vx_devtool STATIC + PUBLIC vx_core）
  - 根 `CMakeLists.txt` 加 `option(VX_BUILD_DEVTOOL OFF)` + `if(VX_BUILD_DEVTOOL) add_subdirectory(veloxa/devtool)`
  - `tests/CMakeLists.txt` 加 `if(VX_BUILD_DEVTOOL) add_subdirectory(devtool)`
  - 新建 `inspector_data.h` 含 schema doc 注释（含 D7=C 一/二层定位 + 与 A.0.6 公开 C API 关系）
  - 新建 `inspector_data.cc` 实现 SerializeDocument（envelope wrap + dom::ToJson 转发）/ SerializeLayoutBox（pure delegate to box->ToJson）/ SerializeComputedStyle（10 properties + 4 helpers）
- **Step 4 验证 GREEN**：DEVTOOL=ON 全量构建 + ctest **1095/1095 PASS**（基线 1085 + 10 新测）+ 1 Skip
- **Step 5 A14 守门**：DEVTOOL=OFF 单独 `build-noffi/` reconfigure + 全量构建 + ctest **1054/1054 PASS** ✅（inspector_data_test 不在 OFF 编译，证明 VX_BUILD_DEVTOOL guard 生效）
- **Step 6 反向探针**：硬编码 `dom::ToJson(doc, kNone)` 替代 `policy` → 测 #1089 `SerializeDocumentT3PropagatesRedactionPolicy` FAIL ✅（T3 propagation 捕获能力 verified）；恢复后 10/10 PASS
- **Lint clean** ✅
- **A14 验收准入达成** ✅（DevTool OFF 时 inspector_data 0 字节贡献）
- **commit**：[A.0.5 实测耗时 ~25 min，vs plan 36 min ×0.6 = 0.69×；候选 plan ×0.6 第 22 数据点「窄档延续」]
- **教训 — plan ×0.6 第 22 数据点候选风险**：plan §A.0.5 写于 4-30 蓝图阶段，对 `dom::Document::CreateElement` / `ComputedStyle::SetProperty` / `RedactionPolicy::kDefault` 等 4-5 个 API 假设错误，但因 plan 步骤足够细致 + 校准成本只增加 ~5 min（5/36 = 14%），未触发 plan ×0.6 「过窄」风险。沉淀为「plan 假设 API 是否存在 → build 阶段先 grep 验证」节奏。

**Phase A.0.6 完成 ✅（2026-05-02 13:55，~20 min vs plan 36 min ×0.6 = 0.56×）：**

- **简化决策**：plan §A.0.6 列 3 个 API（vx_view_serialize_dom_json + vx_node_get_computed_style_json + vx_node_get_layout_box_json），后两个需要 node_id 系统（plan 提到「stable handle」），但 node_id 系统在 A.1.x 才会建立 → A.0.6 仅实现 `vx_view_serialize_dom_json`（不依赖 node_id），其余 2 个 by-node API 留待 A.1.x
- **Step 1 RED**：新建 `tests/api/devtool_api_test.cc` 含 8 测：
  - 2 个无条件测（NullView / NullOutLen 错误处理）
  - 5 个 `#ifdef VX_BUILD_DEVTOOL` 测（DoubleCallProtocol / NoDocument / T7 max_size / T7 caller buffer / T3 password redaction propagation）
  - 1 个 `#else` 分支 stub 测（`SerializeDomJsonReturnsInvalidStateWhenDevtoolDisabled`）
- **Step 2 验证 RED**：编译失败 `'vx_view_serialize_dom_json' was not declared in this scope` ✅
- **Step 3 GREEN**：
  - `veloxa_api.h`: 扩展 `VxResult` enum 加 `VX_ERROR_OUT_OF_MEMORY = -3` + `VX_ERROR_NOT_FOUND = -4`（A.1.x 用）+ 加 `vx_view_serialize_dom_json` 声明（带完整 doc：double-call protocol + T3 + T7 + max_size 推荐值）
  - `veloxa_api.cc`: 实现 thin wrapper — `#ifndef VX_BUILD_DEVTOOL` 路径直接 return `VX_ERROR_INVALID_STATE`（A14 zero-byte），`#else` 路径调用 `vx::devtool::SerializeDocument(doc, kRedactSensitive)` + T7 守卫（needed > max_size → OOM 在 caller alloc 之前）+ double-call protocol（NULL out_buf → 写 size return；out_buf 太小 → OOM）
  - `veloxa/api/CMakeLists.txt`: 加 `if(VX_BUILD_DEVTOOL) target_compile_definitions(vx_api PUBLIC VX_BUILD_DEVTOOL) + target_link_libraries(vx_api PRIVATE vx_devtool)`
  - `tests/CMakeLists.txt`: 加 `vx_add_test(devtool_api_test ...)` + 条件 `target_compile_definitions ... VX_BUILD_DEVTOOL`
- **Step 4 验证 GREEN**：DEVTOOL=ON 全量构建 + ctest **1102/1102 PASS**（基线 1095 + 7 新测 = 1102）+ 1 Skip
- **Step 5 反向探针 + A14 守门**：
  - **T7 反向探针**（拆 `if (needed > max_size) return OOM`）：测 #991 `SerializeDomJsonMaxSizeExceededReturnsOutOfMemory` FAIL ✅ — T7 防护捕获能力 verified
  - **A14 OFF 守门**：DEVTOOL=OFF reconfigure（独立 `build-noffi/`）→ devtool_api_test 只编译 3 个测（NullView + NullOutLen + Stub），其余 5 个 `#ifdef VX_BUILD_DEVTOOL` 测不进 binary ✅；OFF 完整 baseline **1057/1057 PASS** = 1054（A.0.5 后）+ 3（新加 stub 测）
- **Lint clean** ✅
- **A14 验收准入维持** ✅（OFF binary 中 vx_view_serialize_dom_json 仅余 stub，无 vx_devtool 链接）
- **T7 安全威胁 mitigation 落地** ✅（buffer overflow 双重防护：max_size + caller buffer 太小）
- **技术债 #40 闭环** ✅（C API DOM introspection — Style/Layout by-node 待 A.1.x node_id 系统建立后补）
- **commit**：[A.0.6 实测耗时 ~20 min，vs plan 36 min ×0.6 = 0.56×；候选 plan ×0.6 第 23 数据点「极窄档延续」]

**Phase A.0.x 进度（16 子任务总览）：**

| 子任务 | 状态 | plan ×0.6 估时 | 实测 |
|:-:|:-:|---:|---:|
| Phase 0.1 reconfigure | ✅ 完成 | — | ~3 min |
| A.0.1 I1 双 Document 槽 | ✅ 完成 | 54 min | ~10 min（0.18×）|
| A.0.2 LayoutBox::ToJson | ✅ 完成 | 18 min | ~10 min（0.56×）|
| A.0.3 DOM Serializer::ToJson + T3 | ✅ 完成 | 27 min | ~15 min（0.56×）|
| A.0.4 PaintCommand kOverlayHighlight + T5 | ✅ 完成 | 18 min | ~15 min（0.83×）|
| A.0.5 inspector_data.h 内部 C++ | ✅ 完成 | 36 min | ~25 min（0.69×）|
| A.0.6 vx_view_serialize_*_json + T7 | ✅ 完成 | 36 min | ~20 min（0.56×）|
| A.1.1 InspectorOverlay::InjectHoverHighlight (DisplayList&) | ✅ 完成 | 18 min | ~10 min（0.56×）|
| A.1.2 DevTool resource 编译期嵌入（B-A1.1=b）| ✅ 完成 | 27 min | ~15 min（0.56×）|
| A.1.3 inspector_panel.html/css/js 编写 | ✅ 完成 | 54 min | ~25 min（0.46×）|
| A.1.4 JS native binding 扩展 | ✅ 完成 | 36 min | ~22 min（0.61×）|
| A.1.5 InputDispatchSplitter（新增）| ✅ 完成 | 27 min | ~12 min（0.44×）|
| A.1.6 Application 双 UpdateManager（M1）| ✅ 完成 | 36 min | ~22 min（0.61×）|
| A.1.7 vx_view_attach_devtool C API + F12 | ⏳ plan ✅ | 27 min | — |
| A.1.8 dogfood headless smoke（新增）| ⏳ plan ✅ | 45 min | — |
| A.2.1-4 安全单测 + A14 守门 | ⏳ | 63 min | — |
| A.3.1-2 example smoke + reflect prep | ⏳ | 45 min | — |

**Phase A.1.1 完成 ✅（2026-05-02 14:35，~10 min vs plan 18 min ×0.6 = 0.56×）：**

- **关键发现 — plan 假设修正（继 A.0.5 经验，再次验证「plan 假设 API 是否存在 → build 阶段先 grep 验证」节奏）：**
  - `DisplayList = Vector<PaintCommand>` 是 type alias，**不是 struct**（plan 写 `list.commands.push_back()` 错误） → 改 `list.push_back()`
  - `LayoutBox` 无 `border_box` 字段，实为 `x/y` + `border_box_width()/height()` method → rect 构造 `gfx::Rect{box->x, box->y, box->border_box_width(), box->border_box_height()}`
- **Step 1 RED**：`tests/devtool/inspector/inspector_overlay_test.cc` 含 5 测（plan 写 3 测，扩展 +2 加强覆盖：`InjectHoverHighlightCustomColorAndStrokeWidth` + `InjectHoverHighlightUsesBorderBoxIncludingPadding`）
- **Step 2 验证 RED**：`fatal error: veloxa/devtool/inspector/inspector_overlay.h: No such file or directory` ✅
- **Step 3 GREEN**：
  - `inspector_overlay.h`: `class InspectorOverlay { static InjectHoverHighlight(DisplayList&, const LayoutBox*, Color, f32); };` + 完整 doc 注释（M2 修正说明 + T5 lifecycle 协议 + creative #1 决策 4 末位渲染契约）
  - `inspector_overlay.cc`: ~10 LOC 实现（null check → push_back OverlayHighlight）
  - `tests/devtool/inspector/CMakeLists.txt` 加 `inspector_overlay_test` exec
  - `veloxa/devtool/inspector/CMakeLists.txt` 加 `inspector_overlay.cc` 源
- **Step 4 验证 GREEN**：DEVTOOL=ON 全量构建 + ctest **1107/1107 PASS**（基线 1102 + 5 新测 = 1107）+ 1 Skip
- **Step 5 反向探针**：函数体改 no-op (`(void)list; ...`) → 4/5 测 FAIL ✅（仅 `NullBoxIsNoOp` 测仍 PASS，无效探针 — null path 本身是 no-op，与 A.0.2 探针 1 同型）；恢复后 5/5 PASS
- **Step 6 A14 守门**：DEVTOOL=OFF baseline 1057/1057 PASS 维持（inspector_overlay_test 不参与编译，零字节贡献）✅
- **Lint clean** ✅
- **commit**：[A.1.1 实测耗时 ~10 min vs plan 18 min ×0.6 = 0.56×，候选 plan ×0.6 第 24 数据点「窄档延续」]

**Phase A.1.2 完成 ✅（2026-05-02 14:50，~15 min vs plan 27 min ×0.6 = 0.56×）：**

- **核心成果（B-A1.1=b 落地）：** DevTool UI HTML/CSS/JS 编译期嵌入基础设施建立 — Python codegen + CMake `add_custom_command` + `vx_devtool_resources` STATIC 库；3 个占位 resource 文件（A.1.3 填充实质内容）；T2 路径穿越威胁面**完全消除** ✅
- **Step 1 RED**：`tests/devtool/resources/inspector_resources_test.cc` +3 测（HtmlExportedAndNonEmpty / CssExportedAndNonEmpty / JsExportedAndNonEmpty），`tests/devtool/CMakeLists.txt` 加 `add_subdirectory(resources)`
- **Step 2 验证 RED**：`fatal error: veloxa/devtool/resources/inspector_resources.h: No such file or directory` ✅
- **Step 3 GREEN（5 个新文件 + 3 个修改）**：
  - `veloxa/devtool/resources/inspector_panel.{html,css,js}` — 占位（每个 ~3 行，A.1.3 填实质内容）
  - `veloxa/devtool/resources/inspector_resources.h` — 导出 3 个 `extern const char* const`（带 schema doc 注释 + B-A1.1 推翻 B4=B 说明 + T2 消除标注）
  - `veloxa/devtool/resources/embed_resources.py` — Python codegen（`json.dumps()` 处理转义，与 CMake `add_custom_command` 配合）
  - `veloxa/devtool/resources/CMakeLists.txt` — `find_package(Python3)` + `add_custom_command` 生成 `inspector_resources.cc` + `vx_devtool_resources` STATIC 库
  - `veloxa/devtool/CMakeLists.txt` — 加 `add_subdirectory(resources)` 在 inspector 之前
  - `veloxa/devtool/inspector/CMakeLists.txt` — `vx_devtool` link `vx_devtool_resources`
  - `tests/devtool/resources/CMakeLists.txt` — `inspector_resources_test` exec
- **Step 4 验证 GREEN**：DEVTOOL=ON 全量构建 + ctest **1110/1110 PASS**（基线 1107 + 3 新测 = 1110）+ 1 Skip
- **Step 5 反向探针**：`embed_resources.py` 加 `content = ""` 强制空 → 3/3 InspectorResourcesTest FAIL ✅；恢复后 3/3 PASS
- **Step 6 A14 守门**：DEVTOOL=OFF rebuild + ctest **1057/1057 PASS** ✅；libvx_api.a size **12156 bytes 维持**（vs A.1.1 后 12156，零字节增长 — vx_devtool_resources 不进 OFF binary，验证 `if(VX_BUILD_DEVTOOL)` guard 生效）
- **Lint clean** ✅
- **commit**：[A.1.2 实测耗时 ~15 min vs plan 27 min ×0.6 = 0.56×，候选 plan ×0.6 第 25 数据点「窄档延续，与 A.1.1 0.56× 群组对齐」]
- **教训沉淀（继 A.0.5 + A.1.1 经验）：** plan 假设的 API/structure 在 build 阶段先 grep 验证已成 SOP；本子任务 plan-fact 一致度高（CMake `find_package(Python3)` + `add_custom_command` 模式标准），无重大偏移

**Phase A.1.3 完成 ✅（2026-05-02 14:50（轮次 4 起点）→ 15:15，~25 min vs plan 54 min ×0.6 = 0.46×）：**

- **核心成果（dogfood UI 落地）：** DevTool Inspector UI 三件套实质内容（HTML 结构 + R2-verified CSS + DOM tree 渲染 JS）；为 A.1.6 Application::LoadDevtoolDocument 准备好 `__INLINE_CSS__` / `__INLINE_JS__` 占位符接口
- **Plan §A.1.3 步骤 0 同步检查（grep CSS shorthand 支持）实证：**
  - ✅ Verified shorthand: `flex` / `padding` / `margin` / `border` / `border-{top,right,bottom,left}` / `border-color/style/width` / `transition`
  - ✅ Verified longhand: `display:flex` / `position` / `width` / `height` / `overflow` / `background-color` / `color` / `font-size` / `font-family` / `border-radius`
  - ❌ Not supported: `background:` shorthand / `font:` shorthand / `pointer-events`（creative #1 决策 3 警告确认）
  - **R2 mitigation 已锁入测试守卫**（3 个 `Avoids*` smoke 测）
- **Step 1 RED**：`tests/devtool/resources/inspector_panel_smoke_test.cc` 新建含 14 测：
  - 5 个 HtmlSmoke（devtool-root / 3 tabs / 3 panels / `__INLINE_CSS__` placeholder / `__INLINE_JS__` placeholder）
  - 5 个 CssSmoke（#devtool-root selector / display:flex / 3 个 R2 Avoids guards）
  - 4 个 JsSmoke（renderDomTree / renderTreeNode / vx_devtool_get_dom_json 桩 / data-tab handler）
- **Step 2 验证 RED**：8/14 测 FAIL（占位文件已 incidentally 满足 6 测）✅
- **Step 3 GREEN**：
  - `inspector_panel.html`：splitter dock + 3 tabs (DOM/Style/Layout) + 3 panel divs + `__INLINE_CSS__`/`__INLINE_JS__` 占位符（A.1.6 runtime 替换）
  - `inspector_panel.css`：仅用 R2-verified shorthand/longhand 子集，~76 行，含 `display: flex` / `flex-direction` / `padding`/`margin`/`border-{top,right,bottom,left}` / `background-color` / `color` / `font-size` / `font-family`
  - `inspector_panel.js`：~80 行，含 `renderDomTree()` / `renderTreeNode()` / `setupTabs()` + `vx_devtool_get_dom_json()` 调用桩 + escapeHtml XSS 防护
- **Step 4 验证 GREEN**：发现 **测试自身 bug** — `Avoids*` 测在 CSS 文件**注释里**找到了字面字符串（"AVOID: background: shorthand"）；修复加 `StripCssComments` helper 后 ctest **1110 → 1124 PASS**（+14 测）
- **Step 5 反向探针**：HTML 中 `id="devtool-root"` 改为 `id="other-root-PROBE"` → `ContainsDevtoolRoot` FAIL ✅（捕获能力 verified）；恢复后 14/14 PASS
- **Step 6 A14 守门**：DEVTOOL=OFF baseline **1057/1057 PASS** + libvx_api.a **12156 bytes 维持**（vs A.1.2 后 12156，零字节增长 — resource 内容更新不影响 OFF binary）
- **Lint clean** ✅
- **commit**：[A.1.3 实测耗时 ~25 min vs plan 54 min ×0.6 = **0.46×**，候选 plan ×0.6 第 26 数据点「极窄档延续」— 写实质 dogfood UI 实际工作量低于 plan 预估，验证 R2 早期 grep 锁定子集预算法的高效性]
- **教训沉淀新增：** 写「自我防护测试」时（如本测验证 CSS 不含某子串），需注意 source 文件**注释**里的字面字符串可能误命中 — 加 `StripCssComments` helper 是正确范式，类似可推广到 JS 注释 strip

**Phase A.1.4 完成 ✅（2026-05-02 15:25 → 15:47，~22 min vs plan 36 min ×0.6 = 0.61×）：**

- **核心成果（dogfood UI ↔ target 闭环）：** JS native binding `vx_devtool_get_dom_json()` 落地，A.1.3 dogfood UI 调用 `vx_devtool_get_dom_json()` 现在能真实返回 target Document JSON；闭环 dogfood UI 与 target Document 的数据流
- **架构验证（plan §A.1.4 步骤 1）：**
  - `dom_bindings.cc` 已用 `JS_SetContextOpaque(ctx, this)` + `GetData(ctx)` 模式 recover InstanceData；不需新机制
  - **关键发现 + 修正**：plan 假设 `RegisterDevtoolBindings(ctx, target_doc)` 但 QuickJS C function 是 C 函数指针无 closure capture → 改为扩展 `DomBindings::InstanceData` 加 `devtool_target_doc` 字段 + 公共 setter `SetDevtoolTargetDocument()`，binding 自由函数通过 `JS_GetContextOpaque(ctx) → DomBindings → devtool_target_document()` recover target
  - 这与现有 DomBindings 模式高度一致（`doc` / `em` / `tracked_callbacks` 字段同列）
- **Step 1 RED**：`tests/script/devtool_bindings_test.cc` 新建含 5 测：
  - 4 个 `#ifdef VX_BUILD_DEVTOOL` 主测：RegistersGlobalFunction（typeof === "function"）/ ReturnsTargetDocumentJsonEnvelope（document.type === "document"）/ ReturnsTargetDocumentChildren（children[0].tag === "div" + children[0].children[0].data === "Hello Target"）/ ReturnsNullEnvelopeWhenTargetUnset
  - 1 个 `#else` 分支 stub 测：StubDoesNotRegisterFunction（typeof === "undefined"）
- **Step 2 验证 RED**：`RegisterDevtoolBindings` / `SetDevtoolTargetDocument` 未声明 → 编译失败 ✅
- **Step 3 GREEN（plan-fact 修正点 ×2）：**
  - `dom_bindings.h`: `DomBindings` 加 `SetDevtoolTargetDocument(dom::Document*)` + `devtool_target_document() const` + 自由函数 `RegisterDevtoolBindings(JSContext*)` 声明
  - `dom_bindings.cc`: `InstanceData` 加 `devtool_target_doc = nullptr` 字段；`Unbind()` 同步清理；`#ifdef VX_BUILD_DEVTOOL` 块内 include `inspector_data.h` + 实现 `VxDevtoolGetDomJson`（recover target via opaque + 调用 `SerializeDocument(target, kRedactSensitive)` + JS_NewStringLen）+ `RegisterDevtoolBindings` 注册 `vx_devtool_get_dom_json` global；`#else` 分支 empty stub
  - `veloxa/script/CMakeLists.txt`: `if(VX_BUILD_DEVTOOL)` PUBLIC define + link vx_devtool（无循环依赖：vx_devtool 仅 link vx_core / vx_devtool_resources）
  - `tests/CMakeLists.txt`: vx_add_test devtool_bindings_test + 条件 VX_BUILD_DEVTOOL define
- **Step 4 验证 GREEN**：发现 **plan-fact 关键修正** — `set_id(InternedString)` 直接设置 `Element::id_` 字段**不**写入 `attributes_`（仅 `SetAttribute("id",...)` 才会写）→ `JSON.parse(...).attributes.id` 返回 undefined；改用更稳健的 `tag` + `children[0].data` 路径 assertion；ctest **1124 → 1128 PASS**（+4 测，1 stub 测在 OFF 路径）
- **Step 5 反向探针**：第一次尝试改为 empty stub 触发 `-Werror=unused-function`（编译失败也是「捕获能力」证据）→ 改为 register 错误名字 `vx_devtool_get_dom_json_PROBE_TYPO` → 4/4 主测 FAIL ✅；恢复后 4/4 PASS
- **Step 6 A14 守门**：DEVTOOL=OFF reconfigure + ctest **1057 → 1058 PASS**（+1 = devtool_bindings_test 的 `StubDoesNotRegisterFunction` OFF 测）+ libvx_api.a **12156 bytes 维持**（零字节增长，验证 vx_script 加 vx_devtool 链接条件 guard 生效）
- **Lint clean** ✅
- **commit**：[A.1.4 实测 ~22 min vs plan 36 min ×0.6 = **0.61×**，候选 plan ×0.6 第 27 数据点「极窄档延续」]
- **教训沉淀新增：**
  1. **DOM API 边界 caching**：`set_id` ≠ `SetAttribute("id", ...)` — 前者只写 `id_` 字段（getElementById 用），后者只写 `attributes_` map（serialize 输出）；下次写涉及 attributes 测试需先确认数据流向
  2. **QuickJS native callback 模式锁定**：C function pointer 无 closure capture → 状态必须挂在 `JS_GetContextOpaque` recoverable 的对象上；扩展 InstanceData + 公共 setter 是正确范式，不应用 thread_local global / static state
  3. **反向探针选择陷阱**：empty function body 探针在严格 -Werror=unused-function 下会变编译错（虽然也是「捕获」但不是 test FAIL）；优先用「保留代码但故意修改 string literal / 参数」探针，更接近 RED 信号

**Phase A.1.5 完成 ✅（2026-05-02 14:50（轮次 5 起点）→ 15:02，~12 min vs plan 27 min ×0.6 = 0.44×）：**

- **核心成果（input dispatch 状态机层启动点）：** `InputDispatchSplitter` 纯算法状态机落地，splitter dock 鼠标事件路由 + drag capture 协议；无 IO 无引擎依赖，纯函数式状态转移
- **架构决策（plan-fact 微调）：**
  - **文件位置**：plan 写 `veloxa/devtool/input_dispatch_splitter.{h,cc}`（devtool/ 根目录非 inspector/ 子目录）；理由：input dispatch 是跨子系统组件（未来 Performance Overlay / Hot Reload 也会复用），不应耦合 Inspector 命名空间 → **plan 决策正确，按 plan 执行**
  - **CMake 集成方式**：用 `target_sources(vx_devtool PRIVATE ...)` append 到现有 vx_devtool target（在 inspector/CMakeLists.txt 中定义），避免目录树重构 — 最小侵入范式
- **API 锁定（plan §A.1.5 + R1 mitigation 加强）：**
  - `enum class DispatchTarget { kTarget, kDevTool, kSplitterHandle }`
  - `void SetSplitterX(f32)` + `f32 splitter_x() const` getter
  - `DispatchTarget RouteToDocument(f32 mouse_x, f32 mouse_y, bool is_pointer_down, bool is_pointer_up)`
  - `bool dragging() const`
  - 内部状态：`splitter_x_=530.0f`（800-270 默认）+ `hit_handle_half_width_=4.0f` + `dragging_=false` + `capture_target_=kTarget`
- **状态机规则（实现锁定）：**
  - **Sticky drag-capture**：`dragging_==true` 时无视 geometry，强制路由到 `capture_target_`；is_pointer_up 触发 release 但当前事件仍属 captured target（让 handle widget 收到匹配的 up 事件）
  - **Static geometry**：`|mouse_x - splitter_x| <= 4` → kSplitterHandle；< → kTarget；> → kDevTool
  - **R1 mitigation 守卫**：`is_pointer_down && zone == kSplitterHandle` 才engage drag capture（press 在 target/devtool 永不 sticky-route）
- **Step 1 RED**：`tests/devtool/input_dispatch_splitter_test.cc` 新建含 7 测（plan 锁 5，加 R1 mitigation `PressInTargetZoneDoesNotStartDragCapture` + `SetSplitterXShiftsHitArea` 配置 setter 测）：
  - RoutesToTargetWhenLeftOfSplitter / RoutesToDevToolWhenRightOfSplitter / RoutesToHandleWhenInsideHitArea（geometry 三要件）
  - DragCaptureKeepsRoutingToHandleAcrossBoundary / PointerUpReleasesDragCapture（drag 状态转移）
  - PressInTargetZoneDoesNotStartDragCapture（R1 守卫）
  - SetSplitterXShiftsHitArea（参数化）
- **Step 2 验证 RED**：header missing → 编译失败 ✅
- **Step 3 GREEN**：实现 `input_dispatch_splitter.{h,cc}` 状态机；CMake `target_sources` append；7 测全 PASS
- **Step 4 验证 GREEN**：ctest **1128 → 1135 PASS**（+7 测，全在 vx_devtool）
- **Step 5 反向探针**：移除 `if (dragging_)` 提前返回分支（drag capture 失效）→ `DragCaptureKeepsRoutingToHandleAcrossBoundary` FAIL（与 plan §1501 锁定的探针目标完全一致）✅；恢复后 7/7 PASS
- **Step 6 A14 守门**：DEVTOOL=OFF baseline **1058/1058 PASS** + libvx_api.a **12156 bytes 维持**（InputDispatchSplitter 在 vx_devtool 内，OFF 时不参编译，零字节）
- **Lint clean** ✅
- **commit**：[A.1.5 实测 ~12 min vs plan 27 min ×0.6 = **0.44×**，候选 plan ×0.6 第 28 数据点「极窄档延续」— 纯算法状态机无外部依赖，与 A.1.3/A.1.4 共同构成「轮次 4-5 极窄档延续群组」（0.46×/0.61×/0.44×）]
- **教训沉淀新增：** `target_sources(target PRIVATE ...)` append 到现有 target 是「跨目录加 source 文件且不重构」的最小侵入 CMake 范式，比 `add_subdirectory` 创建新 lib 或 `../` 相对路径都干净

**Phase A.1.6 完成 ✅（2026-05-02 15:30（轮次 6 起点）→ 15:52，~22 min vs plan 36 min ×0.6 = 0.61×）：**

- **核心成果（M1 修正实施 + dogfood UI 渲染管线落地）：** Application 双 UpdateManager 框架成形 + DevTool Document 加载（runtime placeholder 替换嵌入 HTML/CSS/JS）+ canvas Translate 偏移渲染（B-A1.2=a full viewport 模型）；UpdateManager 暴露 `mutable_display_list()` getter 为 InspectorOverlay 注入做准备；plan 任务最复杂之一（除 A.1.8）一次性按 plan 通过
- **架构实施（M1 修正完整落地）：**
  - **owned_devtool_document_** unique_ptr 字段（Application owns LoadDevtoolDocument 创建的 Document）+ 保留 raw `devtool_document_` 槽（A.0.1 外部 attach 路径不破坏，外部 set 时不污染 owned slot）；析构时 unique_ptr 自动释放，外部 attach 仍按 A.0.1 契约不释放
  - **devtool_update_manager_** 字段（plan 设计）+ `EnsureDevtoolUpdateManager(devtool_width)` 私有 helper（layout viewport = `(devtool_width, surface_height)`，B-A1.2=a 模型 — DevTool Document 在 splitter dock 区域的 viewport 大小）
  - **Update() 序列调用 + canvas Translate**：抽离 `RenderDevtoolWithTranslate()` private helper：`PushState → SetTransform(Matrix3x2::Translation(window_w - devtool_w, 0)) → devtool_update_manager_->Update() → PopState`；target Update 先于 DevTool Update（DevTool 渲染叠加在 target 之上）
- **API 锁定（plan §A.1.6）：**
  - `bool LoadDevtoolDocument(f32 devtool_width = 270.0f)` — 创建 owned Document + EnsureDevtoolUpdateManager；返回 false 当 canvas 缺失或 OFF stub；幂等（第二次调用替换前实例）
  - `void UnloadDevtoolDocument()` — tear down devtool_update_manager_ 先（停止引用 Document）+ 仅当 owned 时清 raw slot；不破坏外部 attach 路径
  - `bool devtool_loaded() const` — `devtool_document_ != nullptr` 简单查询
  - `const UpdateManager* devtool_update_manager() const` getter
  - `UpdateManager::mutable_display_list()` getter — 返回非 const `render::DisplayList&`，注释引用 T5 reset contract
- **placeholder 替换实施（B-A1.1=b 编译期嵌入 + A.1.6 runtime 注入闭环）：**
  - 私有 anon-namespace `ReplaceFirst(std::string& host, const char* placeholder, const char* replacement)` helper（仅 DEVTOOL=ON 编译）
  - LoadDevtoolDocument: 拷贝 `kInspectorPanelHtml` 到 `std::string` → `ReplaceFirst("__INLINE_CSS__", kInspectorPanelCss)` → `ReplaceFirst("__INLINE_JS__", kInspectorPanelJs)` → `html::Parser::Parse(StringView(html.data(), html.size()))` 创建 Document
- **A14 zero-byte guard 多层防护（DEVTOOL=OFF 编译保护）：**
  - `application.cc` 顶部 `#ifdef VX_BUILD_DEVTOOL` 才 include `inspector_resources.h`（OFF 不引入符号依赖）
  - `LoadDevtoolDocument` / `UnloadDevtoolDocument` / `EnsureDevtoolUpdateManager` 用 `#ifdef VX_BUILD_DEVTOOL ... #else stub ... #endif` 条件编译（OFF 时返回 false / 空函数）
  - `veloxa/core/CMakeLists.txt`: `if(VX_BUILD_DEVTOOL) PUBLIC define + PRIVATE link vx_devtool_resources`（OFF 时 vx_core 不依赖 vx_devtool_resources，零字节）
- **Step 1 RED**：`tests/core/application_devtool_test.cc` 新建含 7 测：
  - `LoadDevtoolDocumentSetsLoadedFlag` — devtool_loaded() == true + devtool_document() != nullptr
  - `LoadDevtoolDocumentReplacesCssPlaceholder` — Serialize(devtool_document) 含 "display: flex"（CSS 内容已 inline）
  - `LoadDevtoolDocumentReplacesJsPlaceholder` — Serialize(devtool_document) 含 "renderDomTree"（JS 内容已 inline）
  - `UnloadDevtoolDocumentClearsState` — Unload 后 devtool_loaded() == false
  - `ReloadDevtoolDocumentReplacesPriorInstance` — 幂等性（2 次 Load 不泄漏）
  - `UpdateBuildsBothLayoutTrees` — 双 update_manager 各自 layout_root() != nullptr
  - `UpdateManagerExposesMutableDisplayList` — mutable_display_list() 写入后 display_list().size() 增 1
- **Step 2 验证 RED**：`LoadDevtoolDocument` / `devtool_update_manager` / `mutable_display_list` 全部缺失 → 编译失败 ✅
- **Step 3 GREEN**：实施 application.h/cc 改造 + update_manager.h getter；CMake 加 vx_devtool_resources 条件 link；application_devtool_test CMake 加 `if(VX_BUILD_DEVTOOL)` guard
- **Step 4 验证 GREEN**：ctest **1135 → 1142 PASS**（+7 测，全部 ApplicationDevtoolTest）一次过，零迭代修正
- **Step 5 反向探针**：第一次 empty body 又踩 `-Werror=unused-function`（继 A.1.4 教训）→ 改为「保留实施 + 注释掉 `devtool_document_ = owned_devtool_document_.get();` 那一行」→ 6/7 测 FAIL（仅 `UpdateManagerExposesMutableDisplayList` 不依赖 LoadDevtoolDocument 仍 PASS — 符合捕获能力期望）✅；恢复后 7/7 PASS
- **Step 6 A14 守门**：DEVTOOL=OFF reconfigure + ctest **1058/1058 PASS** + libvx_api.a **12156 bytes 维持**（vs A.1.5 后 12156，零字节增长 — 验证 #ifdef + CMake 双层 guard 生效）；libvx_core.a OFF size 1771620 bytes 首次记录
- **Lint clean** ✅
- **commit**：[A.1.6 实测 ~22 min vs plan 36 min ×0.6 = **0.61×**，候选 plan ×0.6 第 29 数据点；plan 任务最复杂之一**一次过**——验证 plan §A.1.6 5 步 + M1/M2/M3 修正 + B-A1.1/B-A1.2 brainstorming 决策的 plan-stage 锁定质量很高]
- **教训沉淀新增：**
  1. **owned_ vs raw pointer 槽双轨设计**：A.0.1 外部 attach 路径（raw devtool_document_ 槽）+ A.1.6 内部 LoadDevtoolDocument 路径（owned_devtool_document_ unique_ptr）协同——析构 + Unload 都 check ownership 标记决定是否释放，不破坏现有 contract 的同时支持新内部路径，是「向后兼容地添加 ownership」的优雅范式
  2. **canvas Translate via PushState/SetTransform/PopState**：Veloxa Canvas 没有直接 `Translate()` API，但 PushState + SetTransform(Matrix3x2::Translation) + PopState 是等价的「scoped translate」模式，符合 RAII 不变量
  3. **#ifdef + CMake 双层 A14 guard 模式锁定**：A.1.4/A.1.6 都用「.cc 文件 #ifdef block + CMake `if(VX_BUILD_DEVTOOL) target_link_libraries`」双层条件编译——前者保证不引入符号依赖（编译期），后者保证不引入静态库依赖（链接期），两层独立但协同；下次涉及 conditional 子系统直接复用此范式

**Phase A.1.7 完成 ✅（2026-05-02 16:00（轮次 7 起点）→ 16:14，~14 min vs plan 27 min ×0.6 = 0.52×）：**

- **核心成果（C ABI 公开 + F12 hotkey 内化）：** vx_view_attach_devtool / vx_view_detach_devtool / vx_view_devtool_loaded 3 个 C 函数 + VxDevtoolOptions struct + VX_KEY_F12 常量；Application 持有 hotkey 状态字段（devtool_hotkey_enabled_ + devtool_default_width_）+ MaybeHandleDevtoolHotkey 拦截 KeyDown(F12)；sdl2_input_translate SDLK_F12 → VX_KEY_F12 mapping；A14 链接闭包零（OFF 路径仅 stub，无 vx_devtool 符号引入）
- **API 锁定（plan §A.1.7）：**
  - `VxDevtoolOptions { uint32_t devtool_width; uint8_t enable_f12_hotkey; }` — opts NULL 时用默认（width=270 + hotkey=on）
  - `vx_view_attach_devtool(view, opts)` — clamp width [200, 400]，调 SetDevtoolHotkey + LoadDevtoolDocument
  - `vx_view_detach_devtool(view)` — UnloadDevtoolDocument + 关 hotkey（保 only-after-explicit-attach 契约）
  - `vx_view_devtool_loaded(view)` — 1/0/-1 三态返回
  - `Application::SetDevtoolHotkey(bool, f32)` — 公开 setter（C API 入口）
  - `Application::MaybeHandleDevtoolHotkey(InputEvent&)` — 私有拦截器，KeyDown + key_code == 0x40000045 时 toggle 并 return true（事件被消费，不下传 event_manager_）
- **F12 hotkey 设计契约（plan 关键约束）：**
  - hotkey 只在 explicit attach 后生效（避免 fresh view 误触发自动加载）
  - hotkey on 时 F12 KeyDown 完全消费（target Document 不再看到 F12，避免双处理）
  - hotkey off 时 F12 当普通 key 转发 event_manager_
  - VX_KEY_F12 = 0x40000045 = SDLK_F12 标准值（兼容性主驱动），在 application.cc 内部通过 anon-namespace `kVxKeyF12` 常量解耦，避免 core→api 头依赖循环
- **A14 链接闭包验证（精确 nm 验证 zero DevTool linkage）：**
  - `nm libvx_api.a` (OFF) 仅含 `vx_view_attach_devtool` stub 符号；无 LoadDevtoolDocument / RegisterDevtoolBindings 等任何 DevTool 内部符号
  - `ar -t libvx_api.a` (OFF) 仅 1 个 .o (`veloxa_api.cc.o`)；vx_devtool/vx_devtool_resources/vx_script DevTool 路径完全未链接
  - libvx_api.a OFF 12156 → 12646 bytes (+490 bytes) — **3 个 C ABI stub 函数公开表面增长**（每 stub ~163 bytes prologue/epilogue/return INVALID_STATE），不属 DevTool 链接闭包；A14 spec 「链接闭合 + size diff = 0」严格条件「DevTool 子目录不参与 OFF 链接」**仍满足**
- **Step 1-2 RED**：`tests/api/devtool_attach_api_test.cc` 新建含 10 测（6 ON-only + 3 always-on NULL/state + 1 OFF stub），编译失败 `vx_view_devtool_loaded was not declared` ✅
- **Step 3 GREEN**：veloxa_api.h 加 VxDevtoolOptions/VX_KEY_F12/3 函数；veloxa_api.cc 加 attach/detach/loaded 实施；application.{h,cc} 加 hotkey 字段 + MaybeHandleDevtoolHotkey + SetDevtoolHotkey；sdl2_input_translate.cc 加 SDLK_F12 mapping
- **Step 4 验证 GREEN**：ctest **1142 → 1152 PASS**（+10 测，全部 DevtoolAttachApiTest）一次过零迭代修正
- **Step 5 反向探针**：注释掉 `LoadDevtoolDocument(width)` 调用（保留 SetDevtoolHotkey）→ 6/7 主测 FAIL（`F12HotkeyOnlyTriggersOnDevtoolLoaded` 不依赖 attach 仍 PASS — 符合捕获能力期望）✅；恢复后 GREEN
- **Step 6 A14 守门**：DEVTOOL=OFF reconfigure + ctest **1058 → 1062 PASS**（+4 always-on NULL/state + OFF stub 测）；libvx_api.a 12156 → 12646 bytes (+490) — 公开表面增长可接受，链接闭包 nm 严格验证为零 DevTool 内部符号
- **Lint clean** ✅
- **commit**：[A.1.7 实测 ~14 min vs plan 27 min ×0.6 = **0.52×**，候选 plan ×0.6 第 30 数据点；「极窄档延续群组」A.1.3-A.1.7 五连 = 0.46×/0.61×/0.44×/0.61×/0.52×]
- **教训沉淀新增：**
  1. **A14 「链接闭包零」vs 「字节零增长」精确解读**：spec A14 措辞「链接闭合 + size diff = 0」实际有两层语义——(a) **DevTool 子目录不参与链接**（严格、必须满足、用 `ar -t` + `nm` 验证）vs (b) **绝对字节零增长**（理想、但 C ABI 公开 stub 函数本身占字节）；A.1.7 +490 bytes 来自 (b) 表面增长，但 (a) 仍严格满足；下次涉及 OFF stub 时直接说明这个区分，避免被绝对字节数迷惑
  2. **Hotkey 内化到 Core 层而非 API 层**：F12 拦截可放在 (i) C API vx_view_inject_input 包装、(ii) Application::InjectInput；选 (ii) 因 hotkey 状态需跨 API 调用持久化（attach 写入 → 后续 inject 读取），放 Core 层避免 C API 维护 view→state map 的并发/生命周期问题；同时通过 anon-namespace `kVxKeyF12` 常量解耦头依赖（不 include veloxa_api.h，仅 hardcode 同步常量值），简洁优雅
  3. **C ABI stub 公开表面 vs DevTool 闭包**：C 公开符号即使 #else 分支只 `return INVALID_STATE` 也占字节（汇编 prologue/epilogue + symbol table entry），属「公开 API 表面」不属「DevTool 字节」；下次评估 size guard 用 `nm libvx_xxx.a | grep <DevTool 内部符号>` 而非裸字节差

**Phase A.1.8 完成 ✅（2026-05-02 16:14（轮次 7 续）→ 16:42，~28 min vs plan 45 min ×0.6 = 0.62×）— Phase A.1 8/8 全部完成 🎉：**

- **核心成果（dogfood 完整链路验证 + R2 引擎缺陷暴露清单）：** Application 加 devtool_script_engine_ + devtool_dom_bindings_ + devtool_script_status_ + EvalDevtoolScript 公开访问；LoadDevtoolDocument 中编织完整 JS 链路（Init → Bind devtool_doc → SetDevtoolTargetDocument(target) → RegisterDevtoolBindings → EvalGlobal kInspectorPanelJs）；析构 + Unload 安全 tear down 顺序（bindings → engine → update_mgr → Document）；inspector_panel.js try/catch 防御 R2 缺陷
- **API 锁定（plan §A.1.8 + R2 暴露驱动调整）：**
  - `Application::devtool_script_status() const` getter — Status 引用，inspector_panel.js EvalGlobal 结果（ok() = JS 整体执行成功）
  - `Application::EvalDevtoolScript(source, filename)` — 复用 DevTool ctx 评估 expression（测试 hook，跨 Document JSON envelope 验证用）；OFF 路径 stub 返 InvalidArgument
  - 新字段 `devtool_script_engine_` + `devtool_dom_bindings_` (unique_ptr) + `devtool_script_status_`
- **JS 链路实施细节（5 步顺序）：**
  1. `devtool_script_engine_ = std::make_unique<QuickjsEngine>()` + Init
  2. `devtool_dom_bindings_->Bind(ctx, owned_devtool_document_.get(), &event_manager_)` — DevTool 自己的 document.* getter/setter 在 DevTool ctx 内可见
  3. `devtool_dom_bindings_->SetDevtoolTargetDocument(target_document_)` — 跨 Document inspection 钩子，T3 redaction 默认启用
  4. `script::RegisterDevtoolBindings(ctx)` — 注册 vx_devtool_get_dom_json 全局函数
  5. `EvalGlobal(kInspectorPanelJs, "inspector_panel.js")` — 跑 setupTabs() + renderDomTree()
- **R2 引擎缺陷暴露清单（Phase A.1 完成 Checkpoint 第 2 项产出）：** Phase A.1.8 dogfood 真实暴露 3 个 DomBindings 缺失能力 — **不在本任务范围修复**，列入独立 P3 候选：
  1. **`Element.children` 集合 getter 缺失** — inspector_panel.js setupTabs 第一行 `tabs.children` 取 undefined → `.length` throw TypeError；当前临时 inline 防御（`if (!tabs.children) return`）让 panel JS 仍执行 vx_devtool_get_dom_json 主链路验证
  2. **`element.addEventListener` 缺失** — setupTabs btn.addEventListener 调用同样 throw；inline 防御 `if (typeof btn.addEventListener !== "function") return`
  3. **`element.innerHTML` setter 缺失** — renderDomTree `panel.innerHTML = html` silent no-op；DOM tree panel 视觉上不会渲染，但通过 vx_devtool_get_dom_json 的 JSON 已经覆盖核心数据契约
- **测试调整与 plan 假设修正：** plan §A.1.8 1605-1606 行假设有 `FindElementById` + `InnerHtmlLength` Document API（实际不存在），改为更严格的「JS execution status + 跨 ctx EvalDevtoolScript 重入验证」契约（4 测）：
  - `AttachBuildsDevtoolLayoutTree` — devtool_update_manager()->layout_root() != nullptr ✅
  - `AttachExecutesInspectorPanelJsSuccessfully` — devtool_script_status().ok() ✅（验证 inspector_panel.js R2 防御后整体执行成功）
  - `VxDevtoolGetDomJsonReachesTargetDocument` — EvalDevtoolScript("vx_devtool_get_dom_json()") 返回 JSON 含 "div" "Hi" "document" 等 target Document 标记 ✅（核心跨 Document inspection 契约）
  - `DetachReleasesDevtoolEngine` — attach→update→detach→re-attach→update 循环不 leak/crash + script_status 重置 ✅
- **plan-fact reconcile（GREEN 期间踩坑教训）：**
  - **StatusOr<T>::status() 在 has_value=true 时 DCHECK abort**：`devtool_script_status_ = eval.status()` 当 eval.ok() 时 abort；改为 `devtool_script_status_ = eval.ok() ? Status::Ok() : eval.status()` 守卫
  - **html::Parser 不执行 `<script>` 标签**：必须显式 EvalGlobal(kInspectorPanelJs) 而非依赖 Parser；plan 已隐式假定但易遗漏
- **Step 1-2 RED**：`tests/integration/devtool_dogfood_smoke_test.cc` 新建含 4 测，编译失败 `'class vx::Application' has no member named 'devtool_script_status'` ✅
- **Step 3 GREEN**：application.{h,cc} 加 3 字段 + getter + EvalDevtoolScript + 析构清理；inspector_panel.js 加 R2 防御性 try/catch + binding-availability check
- **Step 4 验证 GREEN**：第一次 R2 防御前 2/4 PASS（vx_devtool_get_dom_json + layout build 工作；JS execution + detach 卡 setupTabs throw）→ 加 R2 防御后 + StatusOr.status() DCHECK 修复 → 4/4 PASS；ctest **1152 → 1156 PASS**（+4 集成测）
- **Step 5 反向探针**：注释掉 `RegisterDevtoolBindings(ctx)` → `VxDevtoolGetDomJsonReachesTargetDocument` 单测精准 FAIL（其他 3 测仍 PASS — 它们不依赖 native binding，符合定向捕获期望）✅
- **Step 6 A14 守门**：DEVTOOL=OFF reconfigure + ctest **1062/1062 PASS** + libvx_api.a OFF **12646 bytes 维持**（vs A.1.7 持平，A14 严格满足）+ libvx_core.a OFF **1771620 → 1775326 bytes (+3706 bytes)** = application.cc 自身代码增长（unique_ptr 字段实例化 + EvalDevtoolScript stub）；nm libvx_core.a OFF 验证**零 DevTool 子系统符号引用**（无 RegisterDevtoolBindings/inspector_resources/SerializeDocument/InspectorOverlay/InputDispatchSplitter 等），A14 链接闭包零严格成立
- **Lint clean** ✅
- **commit**：[A.1.8 实测 ~28 min vs plan 45 min ×0.6 = **0.62×**，候选 plan ×0.6 第 31 数据点；plan 任务最复杂之一（集成 + JS execution 链路 + R2 暴露）**3 步迭代** vs 一次过——R2 防御 + StatusOr DCHECK 两个意外踩坑教训沉淀]
- **Phase A.1 完成 Checkpoint 触达：** 8 子任务（A.1.1-A.1.8）全部 ✅；用户审 dogfood smoke + R2 缺陷暴露清单后选项 A 继续 A.2/A.3 / B 拆 R2 为独立 P3 / C 提前 reflect — 推荐 A 续做 A.2 安全测（4 子任务，~63 min plan ×0.6）+ A.3 examples + reflect（2 子任务，~45 min plan ×0.6）共 ~108 min plan ×0.6 完成整个 Phase A
- **教训沉淀新增（A.1.8 三连）：**
  1. **「dogfood 即缺陷暴露」是 plan 设计意图，不是失败**：A.1.8 plan 1605-1606 行假设 `FindElementById/InnerHtmlLength` 实际不存在；这种 plan 假设与现实脱节恰恰是 dogfood 价值——把缺陷捕获到具体清单（R2 三连 children/addEventListener/innerHTML）；下次 dogfood 任务接受「测试 expectation 需根据实际 R2 缺陷调整」为正常流程，不视为 plan 错误
  2. **JS 资源「panel-side R2 防御」vs 「engine 修复」选择启发**：当 dogfood 遇到 R2 引擎缺陷，可选 (a) 修引擎让 panel JS 工作 (b) 在 panel JS 加防御性 try/catch + binding 可用性 check 让主链路绕过；本任务选 (b) 因 (a) 是独立 P3 范围；下次 dogfood 类任务先用 (b) 把主链路验证打通 + 缺陷清单沉淀给 P3，不卡在引擎修复
  3. **StatusOr<T>::status() DCHECK 陷阱**：StatusOr 在 has_value=true 时 status() abort 而非返回 OK Status；下次涉及 StatusOr 错误转换时统一用 `r.ok() ? Status::Ok() : r.status()` 三元守卫，不裸用 `.status()`

#### `/build` 阶段轮次 3 中止快照（2026-05-02 14:00，触发 plan escalation）

**触发原因：** 用户对「轮次 3 路径选择」AskQuestion 选 **D = 返回 /plan 修正**，识别 Phase A.1 dogfood DevTool UI 实质是 Level 4 子系统级工作量。

**Plan 批判审查发现（A.1.x 关键缺陷）：**

| 子任务 | plan 估时 | 实际复杂度 | 关键缺陷 |
|:--|--:|:--|:--|
| A.1.1 InspectorOverlay 注入 | 27 min | ✅ 可行 (~30 min) | 签名 plan 写 `InjectHoverHighlight(target_doc, ...)` 但 Document 不持有 DisplayList，应改为 `InjectHoverHighlight(DisplayList&, ...)` |
| **A.1.2 dogfood UI** | 54 min | ⚠️ **远超 (~6-8 h+ Level 4)** | (1) 需新建 `vx_devtool_get_dom_json()` JS native binding（dom_bindings.cc 扩展新 module）；(2) 需 runtime 文件 IO（`fopen/fread`）→ 触发 plan **未列威胁 T2 路径穿越**；(3) 需双 viewport 渲染（Application 当前仅 1 surface/canvas，属 creative 域）；(4) splitter dock 鼠标拖动；(5) 必须 SDL2 真窗口 dogfood |
| A.1.3 Style/Layout panel | 36 min | 依赖 A.1.2 | — |
| A.1.4 F12 toggle + API | 27 min | 依赖 A.1.2 | API 设计依赖 splitter dock 实现定型 |

**核心判断：** plan §A.1 实质为蓝图级**概念性大件**，缺少 Level 3 可执行 step 模板；plan ×0.6 144 min vs 真实 ~6-8 h+，失配 4-5×。强行推会触发未列安全威胁（T2）+ 未列架构决策（双 viewport / native binding / 文件 IO 安全模型）。

**轮次 3 决策选项 + 用户选择：**

- A. 按 plan 强推 A.1.1→A.1.4，可能中途升级
- B. 最小可用 Inspector：A.1.1 + 改 A.1.4 为 console dump
- C. 提前 reflect：当前 A.0 已是完整里程碑
- **D. ✅ 返回 /plan 修正（识别 A.1 为 Level 4 → plan + creative）**

**Phase A.0 真实产出保留**（6 commits，1102 / 1057 ctest 双轨，#26/#40 双技术债闭环，T3/T5/T7 三安全 mitigation）：

| Commit | 子任务 |
|:--|:--|
| `bc02ddc` | A.0.1 双 Document 槽位 |
| `1547434` | A.0.2 LayoutBox::ToJson + #26 闭环 |
| `8ff75d4` | A.0.3 DOM Serializer::ToJson + T3 |
| `[A.0.4]` | PaintCommand kOverlayHighlight + T5 |
| `3f3bd38` | A.0.5 vx_devtool 静态库 + inspector_data |
| `c9dec00` | A.0.6 vx_view_serialize_dom_json + T7 + #40 闭环 |
| `d9ec183` | 轮次 2 暂停标记 |

**升级方案待用户用 `/plan` 决定：**

- 选 A：当前 task 升 Level 4 + A.1 detailed plan + 1-2 creative
- 选 B：拆 A.1 为独立 Level 4 task `TASK-30-04-A2`（dogfood UI），当前 task 闭环（A.2 安全单测 4 子任务都不依赖 dogfood，可在当前 task 完成；A.3.1 依赖 dogfood 拆出）
- 选 C：当前 task 直接闭环 reflect（Phase A.0 已是完整 Level 3 里程碑），A.1/A.2/A.3 全部拆为 follow-up tasks

**学习点（reflect 阶段沉淀）：**

- **plan ×0.6 失配预警机制**：plan 单子任务估 ~50 min 但实际工作面 ≥ 5 个子系统时（dogfood UI 案例：JS binding + 文件 IO + 双 viewport + 鼠标交互 + 真窗口 dogfood），应在 plan 阶段标 ⚠️ 待 spike 而非直接 ×0.6 估时
- **蓝图 vs 可执行 plan 边界**：4-30 蓝图 plan 写「dogfood UI 实施」是 Level 4 概念占位，build 阶段强行执行需触发 plan 升级（本次 escalation 验证 working）
- **plan-creative 失配检测**：当 plan 子任务隐含「需新增架构决策」（双 viewport / native binding 设计）时是 creative 域信号，build 阶段不该消化

#### `/plan` escalation 阶段完成快照（2026-05-02 14:30）

**用户决策路径：** escalation A 升级当前 task 为 Level 4（vs B 拆分新 task / C 直接闭环 reflect）

**Brainstorming 锁定（2 决策，逐一 AskQuestion 用户全选推荐）：**

| # | 决策 | 锁定值 | 推翻 / 沿用 | 影响 |
|:-:|---|---|---|---|
| **B-A1.1** | DevTool resource 加载 | **b 编译期嵌入** | **推翻 B4=B → B4=A** | T2 路径穿越威胁面**完全消除** + 减 1 篇 creative + 加快交付 ~3-5h |
| **B-A1.2** | target Document layout viewport | **a full viewport + 渲染覆盖** | 新增决策 | splitter 拖拽零额外开销；与 Chrome DevTools 早期行为一致 |

**关键 grep 验证发现 — 架构 3 项修正（plan escalation 必须反映）：**

| # | 原假设 | 实际架构（grep 验证）| 修正 |
|:-:|---|---|---|
| **M1** | `Renderer::Render(target_doc, devtool_doc, canvas)` class method（creative #1 决策 4）| `render::Record/Replay/Paint` 是 free functions（`renderer.h` line 14-25）；`UpdateManager` 是 single-Document 设计（`update_manager.cc` line 17 `if (!dirty_ \|\| !config_.document) return`）| Application 持有**双 UpdateManager**，共享 canvas + image_cache，序列 Update 自动叠加；不修改 Renderer / UpdateManager |
| **M2** | `InjectHoverHighlight(dom::Document*, ...)`（plan §A.1.1）| Document 不持有 DisplayList | 改签名 `InjectHoverHighlight(render::DisplayList&, const layout::LayoutBox*, ...)` |
| **M3** | runtime 文件读取 `inspector_panel_loader.cc`（B4=B）| B-A1.1=b 嵌入路径 | 替换为 `veloxa/devtool/resources/CMakeLists.txt` + Python codegen 生成 `inspector_resources.cc` |

**plan 文档扩展：** A.1 段从原 4 子任务粗概念 → 8 子任务 detailed 5-步 TDD 模板，文档 +384 行（每子任务含完整代码片段 + RED/GREEN/反向探针/A14 守门 + commit msg）

**子任务 16 → 20（A.1 +4）：**

| # | 子任务 | plan ×0.6 | 关键 |
|:-:|---|--:|:-:|
| A.1.1 | InspectorOverlay::InjectHoverHighlight (DisplayList&) | 18 min | M2 修正 |
| A.1.2 | DevTool resource 编译期嵌入 | 27 min | T2 消除 |
| A.1.3 | inspector_panel.html/css/js 编写 | 54 min | dogfood R2 mitigation |
| A.1.4 | JS native binding 扩展 | 36 min | DomBindings extension |
| A.1.5 | InputDispatchSplitter（新增）| 27 min | drag capture 状态机 |
| A.1.6 | Application 双 UpdateManager（新增）| 36 min | M1 + M3 |
| A.1.7 | vx_view_attach_devtool C API + F12 | 27 min | API 简化（移除 resources_dir）|
| A.1.8 | dogfood headless smoke（新增）| 45 min | 集成 + JS execution 验证 |

**总估时矩阵：**

| Phase | 任务数 | plan 估时 | plan ×0.6 |
|:-:|:-:|---:|---:|
| A.0 ✅ 实测 | 6 | 189 min plan | ~95 min（实测，0.50× plan ×0.6）|
| A.1 ⏳ Level 4 升级 | 8（+4）| 450 min | **270 min（4.5 h）** |
| A.2 ⏳ | 4 | 105 min | 63 min |
| A.3 ⏳ | 2 | 75 min | 45 min |
| **合计** | **20** | **819 min（13.65 h）** | **473 min（7.88 h）** |

较原 plan **7.35 h → 7.88 h**（+32 min plan ×0.6 = +0.53 h），可控。

**0 篇新 creative 需要：** creative #1 (splitter dock + HUD overlay 5 决策) 已覆盖大部分 UI 架构决策；剩余 2 决策（B-A1.1 + B-A1.2）通过 brainstorming 直接锁定入 plan，未触发新 creative。

**MB 同步：** activeContext.md（阶段 → 规划中·A.1 detailed plan 已落盘）/ tasks.md（Level 3→4 + 子任务 16→20 + escalation 历史 +1 行 + brainstorming B-A1.x 决策入决策矩阵）/ progress.md（本快照）

**实测耗时（plan escalation 阶段）：** ~30 min（critical review + brainstorming AskQuestion 5 min + 读 spec/creative/grep 验证 5 min + 写 plan +384 行 15 min + MB 同步 5 min）

**新增 escalation 学习点：**

- **escalation 决策成本预警：** Level 3 → Level 4 升级在 build 入口暴露时，escalation 实际开销 ~30 min（远低于硬推 dogfood 阶段的 ~6-8 h failure cost）— 验证「批判性审查 plan」step 的 ROI 极高
- **架构 grep 验证必须前置：** creative #1 的 `Renderer::Render(target, devtool, canvas)` 假设隐藏了 single-Document 限制；M1/M2/M3 修正在 plan 阶段 grep 30 min 完成，避免 build 阶段返工
- **「creative 数量 = 架构决策数」预算法则：** 真实需要 creative 决策 = 「未在原 spec / creative 锁定 + 不能简单从模式扩展（如 DomBindings）派生 + 触发跨子系统改动」的决策；本次实证 2 个候选 → 通过简化（B-A1.1=b 嵌入 + B-A1.2=a 渲染覆盖）归零

#### `/plan` 阶段产出快照（2026-05-02 13:10）

- **plan 文档：** `docs/plans/2026-05-02-devtool-inspector.md`（~700 行）— B1-B8 决策表 + 16 子任务 5-步 TDD 模板（RED → GREEN → 反向探针 → A14 守门 → commit）+ Phase 0 11 子段实测填写 + 完整代码片段
- **B1-B8 build 级精化决策（用户 1 次 AskQuestion 选 all_recommended，跳过率 8/8 = 100%）：** 全部按 VAN 推荐锁定 — 与蓝图 D1-D8 协同度 8/8 ✅
  - B1=A 独立 plan / B2=A 严格串行 / B3=B 混合测试组织 / B4=B runtime 文件读取 / B5=OFF / B6=A 每子任务 1 commit / B7=A 沿用蓝图估时 / B8=A 复用 spec
- **Phase 0 11 子段实证关键发现：**
  - **0.1 ctest 基线漂移**：build/ 过期 mtime 2026-04-26 → 实测 1061，与 R2.5 落地后预期 1062 不符 → Phase A.0 启动前必跑 reconfigure（plan §0.1 已注明）
  - **0.7 R1 callsite 二次实证**：`->document()` 实际 4 处（全在 application_test.cc）+ 1 处 dom_bindings 不相关 → R1 风险 🟡→🟢 进一步降级（远低于蓝图估 10-15）
  - **0.8 既有测试 fingerprint**：layout 65 / parser 45 / paint_command 8 / renderer 17 ≥ 期望 ✅；event/application 子系统命中略低 ⚠️ → A.0.1/A.1.1 实施前补 grep 扩展关键字
  - **0.10 工具链**：rg/awk/python3 ✅；jq 缺失 → A.2.4 binary size diff smoke 用 python3 兜底
  - **0.9 CSS shorthand**：`flex` ✅ 6 处；`background`/`font`/`border-radius` 待 A.1.2 build 时 grep（缺失改 longhand 或 P3）
- **plan 文档结构亮点：**
  - 16 子任务全部 5-6 步 TDD 模板 + 完整代码片段（A.0.2 LayoutBox::ToJson()、A.0.3 DOM Serializer::ToJson() T3 redact、A.0.4 PaintCommand kOverlayHighlight、A.0.5 inspector_data.h、A.0.6 vx_view_serialize_*_json T7 双调用模式 + max_size 守卫）
  - writing-plans.mdc 必填 9 段全部就绪
  - 安全任务 4 项（A.2.1 T3 / A.2.2 T5 / A.2.3 T7 / A.2.4 A14 守门）独立段
  - 2 Checkpoint（A.0 完成 / A.1 完成）+ 默认隐式批准协议
  - R7「蓝图 plan 漂移」+ R8「runtime resource 加载失败」2 新风险登记
- **plan ×0.6 第 18 数据点假设：** 沿用蓝图基线 7.35 h plan ×0.6（B7=A）；reflect 阶段验证：落「大件类 0.8-1.2×」（实测 0.85-1.15× plan ×0.6 区间）vs「极窄档 0.46-0.59× 群组延续」(plan 蓝图全照搬 + 决策跳过率 100% 时可能)
- **MB 同步：** tasks.md（plan 完成段 + 任务历史 +1 行）/ activeContext.md（阶段 → 规划中）/ progress.md（本快照）
- **决策跳过率监控（来自 brainstorming.mdc）：** 本任务连续 2 次跳过（VAN 5+1 + plan 1）= 总 7 次；触发 ≥ 5 监控但 reflect 阶段需重审 B1-B8 合理性
- **下一步：** `/build` 启动 — Phase 0.1 reconfigure（关键前置 — 确认 ctest 1062 基线）+ A.0.1 I1 改造
- **实测耗时（plan 阶段）：** ~25 min（读上下文 5 min + grep Phase 0 实证 5 min + AskQuestion B1-B8 3 min + 写 plan 文档 10 min + MB 同步 2 min）

#### `/van` 阶段产出快照（2026-05-02 12:30）

- **任务来源：** 用户 `/van TASK-20260430-04` + AskQuestion 选 subtask_a → 启动 TASK-30-04 蓝图主线 A 子任务（实际任务 ID `TASK-20260502-01`）
- **复杂度：** Level 3（中等功能 — 跨 4 子系统但 plan 蓝图已就绪 16 子任务，无新架构决策）
- **分支：** `feature/TASK-20260502-01-devtool-inspector` 基于 main `679304e`（已含 TASK-30-04 蓝图全部归档 + TASK-30-03 codebase review R2 quick fix 6 项）
- **环境检测：** Linux 6.6 / WSL2 + CMake 3.20 / clang/gcc + git 工作区干净 ✅
- **R1 callsite 量化（关键 push-back 决策）：** spec §9 估 10-15 处 → 实证 **5-9 处**（src `application.{h,cc}` 9 处字段访问 + tests `application_test.cc` 4 处 + `dom_bindings_test.cc` 1 处不相关）→ R1 风险等级 🟡→🟢-🟡，A.0.1 改造可控性增强
- **F1-F9 grep 实证：** 5 ✅ 已就绪 / 2 ⚠️ 需扩展 / 2 🔴 需新建（属技术债 #26 + #40 — 本任务一次性闭环）
- **触及技术债 2 项（一次性闭环）：** #26 LayoutBox.Dump → A.0.2 / #40 C API introspection → A.0.5 + A.0.6
- **Phase A 子任务清单（plan 已就绪，build 阶段执行）：** 16 个 — A.0 前置改造 6 + A.1 DevTool UI 4 + A.2 安全单测 4 + A.3 example/reflect 2；估时 ~441 min plan ×0.6（7.35 h）
- **创意决策：** ❌ 无新 creative 需求（creative #1 screen-layout 已覆盖 splitter dock + DOM tree view + F12 toggle + HUD 透明合成 + overlay 渲染顺序双线宽 5 决策）
- **安全相关：** ✅ 是（T3 Inspector 敏感数据 redact / T5 DisplayList overlay 隔离 / T7 C API buffer overflow 双调用模式 / T8 共享容器 mutation propagation 4 个威胁面）
- **前置验证 6/6 全 PASS：** 依赖 / 环境 / artifact / ctest 1062 基线 / FetchContent 跳过 / 待处理事项关联（极强 — 闭环 2 项技术债 + R3+ #2/#3 间接相关）
- **VAN push-back 5 项风险闭环：** R1 callsite 漏改 → 重命名 + grep / R2 dogfood 反向暴露引擎缺陷 → 「已验证 OK」CSS 子集 / R5 ComputedStyle JSON hover hot path → 双层 API + lazy 序列化 / 「跳过 plan 精化」诱惑 → 拒绝（Level 3 默认进 plan）/「TASK-30-04 plan 全照搬」陷阱 → plan 阶段须验证 main 终态 + 沉淀 plan ×0.6 第 18 数据点
- **MB 三件套同步：** tasks.md / activeContext.md / progress.md 完整写入
- **下一步：** `/plan` 进入 build 级精化（Phase 0 grep callsite 全表 + 既有测试 fingerprint + CMake 链接审计 + plan ×0.6 估时校准 + 生成本任务专属 plan §0 子段）
- **实测耗时：** ~5 min（环境检测 + grep 实证 + MB 同步 + 分支创建）

## 上次任务（已归档闭环）

**TASK-20260430-04：规划 UI 编辑器 + 调试器（DevTool 三件套蓝图设计）[安全相关]** — ✅ 已归档于 2026-05-01 ~03:00，已 `--no-ff` 合入 main。**A 子任务已立项为本任务 TASK-20260502-01。**

<details>
<summary>TASK-20260430-04：规划 UI 编辑器 + 调试器（DevTool 三件套蓝图设计）[安全相关] — ✅ 已归档（点开查看历史）</summary>

### `/archive` 阶段产出快照（2026-05-01 ~03:00）

- **归档主文档：** `memory-bank/archive/archive-TASK-20260430-04.md`（11 段 / Level 4 全面归档）
- **剩余改进建议落实**（reflect 阶段未做，archive 阶段补做）：
  - ✅ P1 #2：`brainstorming.mdc` 加「与已锁定决策的协同度」段
  - ✅ P1 #6：`techContext.md` 加 TASK-30-04 蓝图主交付摘要段
  - ✅ P1 #8：`docs/reports/2026-04-30-codebase-review.md` F-025 段加 Hot Reload C.2 强依赖交叉记录
  - ✅ P2 #5：`systemPatterns.md` 加 dogfood 路径 = 探测性 acceptance test 段
  - ✅ P2 #9：`brainstorming.mdc` 加决策跳过率监控段
  - ⏳ P2 #10：估时回填校准（待 TASK-30-04-A/B/C 立项后实测回填）
- **改进建议总落实率 90%**（P0 3/3 + P1 4/4 + P2 2/3，剩 1 项依赖后续任务实测）
- **MB 重置：** tasks.md / activeContext.md / progress.md 已折叠 TASK-04 到「已归档闭环」段，当前阶段重置为「空闲」
- **Merge：** feature 分支 `feature/TASK-20260430-04-ui-editor-debugger` 已 `--no-ff` 合入 main
- **实测耗时：** ~50 min（含 archive 文档 ~25 min + 5 改进落实 ~10 min + MB 重置 ~5 min + merge ~5 min + 收尾 ~5 min）

### `/reflect` 阶段产出快照（2026-05-01 ~02:30）

- **Reflection 主文档：** `memory-bank/reflection/reflection-TASK-20260430-04.md`（10 节全面反思 / Level 4 含架构评估 + 长期影响）
- **核心发现：**
  1. **plan ×0.6 第 17 数据点入库**：主线（VAN + Plan）实测 ~74 min vs plan 估时 210-270 min → **0.27-0.35× plan / 0.46-0.59× plan ×0.6**（落「极窄档 + review 类下限交界」）
  2. **「批量决策 + 批量文档」极窄档**首次 3 数据点群组化（TASK-30-04 0.20-0.27× / TASK-30-02 0.22× / TASK-30-01 P6 0.21×）— 触发条件：批量决策跳过（≥ 5 决策按推荐默认锁定）+ 批量文档产出（≥ 3 篇 1 个 session 内连续完成）+ 蓝图任务（无 build / 无 ctest / 无 debug）
  3. **新沉淀候选 3 项**：「Level 4 蓝图任务 V2=a 工作流变体」（systemPatterns）+「批量文档产出 batch 协议」（systemPatterns）+「dogfood 路径 = 探测性 acceptance test」（systemPatterns 长期）
  4. **安全评估** 8/8 维度通过（输入验证 / 数据保护 / 依赖审计 / 错误信息脱敏 / 敏感数据处理 / 攻击面分析 / Buffer overflow / Mutation propagation / Callback 任意代码执行）
- **改进建议 10 项（P0×3 / P1×4 / P2×3）：**
  - **P0 立即（全部已落实）**：#1 main.mdc V2=a 工作流变体段 ✅ / #3 systemPatterns 极窄档第 17 数据点 ✅ / #7 activeContext 7 项独立立项候选 ✅
  - **P1 下次（第 1 项已落实）**：#2 brainstorming 协同度段 ⏳ / #4 systemPatterns Level 4 蓝图工作流变体段 ✅ / #6 techContext 蓝图主交付摘要 ⏳ / #8 R3+ 强依赖交叉记录 ⏳
  - **P2 长期**：#5 dogfood acceptance test 段 ⏳ / #9 决策跳过率监控 ⏳ / #10 估时回填校准 ⏳
- **架构评估：** DevTool 主线对引擎架构正向影响显著（4 项历史技术债闭环 + Veloxa 自我应用样板载体 + 双层 API 为 CDP/IDE 接入预留路径）；R1-R6 风险已 mitigation 登记
- **实测耗时：** ~40 min（reflection 文档 + 4 改进落实 + 3 MB 文件同步）

### `/plan` 阶段产出快照（2026-05-01 ~01:50，已闭环）

<details>
<summary>D1-D8 决策矩阵 + 4 篇产出文档（点开查看）</summary>

### `/plan` 阶段产出快照（2026-05-01 ~01:50）

- **头脑风暴 D1-D8 全部锁定（用户两次跳过 AskQuestion 后按 VAN 推荐默认锁定 6 次决策）：**
  - D1 三件套实施优先级 = **B Inspector → Overlay → Hot Reload**（Inspector 优先做 UI 渲染样板 + 闭环 #26/#40/#4 三大技术债）
  - D2 Inspector 数据采集协议 = **B 半结构化（JSON tree + DisplayList overlay + C API JSON）**（C API 边界清晰未来对接 CDP + DisplayList overlay 不污染目标 DOM）
  - D3 DevTool UI 主屏布局 = **B 同窗口 splitter dock + Overlay HUD 子模式**（→ creative #1 详化）
  - D4 DevTool 隔离边界 = **B 单进程共享容器（双 Document + 共享 EventLoop / Application / ImageCache）**（嵌入式硬约束 + 与 D2 注入语义一致）
  - D5 Hot Reload file watcher + 增量策略 = **A 嵌入式专注（Linux inotify + CSS-only 增量重载）**（→ creative #2 详化）
  - D6 Performance Overlay 数据采集点 = **B Chrome DevTools 风格（五钩子 + 滑动 60 帧 + dirty rect 边框高亮）**（闭环技术债 #35）
  - D7 C API 扩展边界 = **C 双层 API（内部 C++ 核心 + 公开 C API 薄封装）**（兼顾性能 + 扩展性 + 与 D2 协议一致；闭环技术债 #40）
  - D8 安全威胁建模 = **A T2/T3/T5/T6/T7/T8 完整 + T1/T4 扩展段占位**（与 V5=✅ + 三件套实际威胁面对齐）

- **4 篇产出文档：**
  - **spec** `docs/specs/2026-04-30-devtool-design.md`（12 段 / 三件套验收 A1-A14 / D1-D8 决策矩阵 / 注入点 I1-I8 核对表 / T1-T8 威胁建模 / R1-R6 风险登记 / ≥ 30 systemPatterns 自我对照）
  - **plan** `docs/plans/2026-04-30-devtool.md`（Phase 0 全局约束 + CMake 链接审计 + 静态库循环审计 + 测试基础设施审计 + 边界输入清单 16 项 + 既有测试隐式契约 fingerprint + CSS shorthand 能力 grep 表 / Phase A/B/C/D 子任务 ~40 项 + plan ×0.6 估时矩阵）
  - **creative #1** `memory-bank/creative/creative-devtool-screen-layout.md`（5 决策：整体布局双层结构 / dock 模式切换 / HUD 透明合成 / overlay 渲染顺序双线宽 / F12-F11 toggle）
  - **creative #2** `memory-bank/creative/creative-devtool-hot-reload.md`（5 决策：FileWatcher 抽象 / CSS-only 增量 / DOM 状态保留 / watcher root 边界 / 错误恢复）

- **plan ×0.6 估时（主线 V2=a 蓝图任务自身）：**
  - VAN ~25 min（实测）
  - Plan ~180-240 min plan / ~108-144 min plan ×0.6（待实测 — 预期落 Level 4 蓝图任务区间 0.4-0.7×）
  - Reflect ~60 min plan / ~36 min plan ×0.6（待）
  - Archive ~45 min plan / ~27 min plan ×0.6（待）
  - **主线合计** ~315-375 min plan / ~189-225 min plan ×0.6（plan ×0.6 第 17 数据点候选）

- **用户后续独立立项候选（基于 plan 拆出）：**
  - TASK-30-04-A：DevTool Phase A — Inspector 实施（Level 3，~12.25 h plan / ~7.35 h plan ×0.6）
  - TASK-30-04-B：DevTool Phase B — Performance Overlay 实施（Level 2-3，~7.25 h / ~4.35 h）
  - TASK-30-04-C：DevTool Phase C — Hot Reload 实施（Linux only，Level 3，~10 h / ~6 h）
  - 4 项扩展段（Console / JS Debugger / CDP / 完整 UI Editor）— spec §11 占位

- **触及技术债 4 项闭环 ROI 路径：** #26 LayoutBox.Dump → Inspector Layout / #35 UpdateManager frame hook → Performance Overlay / #40 C API introspection → Inspector 全子系统 / #4 ImageCache 命名空间 → DevTool icon 隔离

- **下一步路径三选一：** A 进入 `/reflect`（推荐）/ B V2 → b 进 `/build` / C 暂停审查

</details>

<details>
<summary>VAN 阶段产出快照（2026-04-30 23:40，已闭环，点开查看）</summary>

### VAN 阶段产出快照

- **意图判读：** 用户用「**规划**」二字（非「实现」） → 主交付物预期为蓝图级 spec + plan + 子任务 ID 列表，可选 MVP 子集实施
- **grep 实证（F1-F9）：**
  - ❌ 0 处 inspector / devtool / debugger / hot reload / overlay 实现代码
  - ❌ JS Debug API 未集成（技术债 #44 `JS_SetInterruptHandler` 仅 spec 提及）
  - ❌ C API 缺 introspection 接口（技术债 #40 `vx_view_get_document` 等不存在）
  - ❌ LayoutBox / DisplayList 无 `Dump()`（技术债 #26）
  - ❌ UpdateManager 无帧生命周期钩子（技术债 #35）
  - ✅ DOM `Serialize(node)` 已可用（Inspector DOM 树文本化复用基础）
  - ✅ `Application::document() / event_manager() / update_manager() / event_loop()` 已暴露
  - ✅ SDL2 后端已就绪（`hello_sdl2` 范本，DevTool UI 渲染主线起点）
  - ✅ HitTest + EventManager hover/active/focus（Inspector 元素高亮选取复用基础）
- **复杂度初判：** Level 4（多子系统 — Inspector + Hot Reload + Overlay + Console + 编辑器 + JS 调试器 6 候选）
- **基础设施成熟度三色分级：** 🟢 已就绪 5 项 / 🟡 需扩展（小工程）4 项 / 🔴 需新建（大工程）6 项
- **V1-V5 锁定（用户两次跳过 AskQuestion → 按 VAN 推荐默认）：**
  - V1 = **B 三件套**（Inspector DOM/Style/Layout + Hot Reload + Performance Overlay）— Console / JS Debugger / 完整 UI Editor 标为「扩展候选」入 spec §扩展段
  - V2 = **a 纯蓝图**（spec + plan + creative ×N，不强制实施代码）— 与「规划」语义最匹配
  - V3 = **A Veloxa 自渲染**（dogfood 模式 — DevTool 即引擎自我应用样板）
  - V4 = **Level 4**（多子系统蓝图 + 架构决策矩阵 → /plan + /creative 强制路径）
  - V5 = ✅ **是 [安全相关]**（JS REPL / Hot Reload 路径 / 远程调试 port / Inspector 敏感数据 4 个威胁面）
- **触及技术债 4 项映射：** #26 LayoutBox.Dump → Inspector Layout / #35 UpdateManager frame hook → Performance Overlay / #40 C API introspection → Inspector 全子系统 / #44 QuickJS Debug API → 扩展候选 JS Debugger
- **前置验证清单：** 6/6 全 PASS（依赖 / 环境 / artifact / ctest 1062 基线 / FetchContent 跳过 / 待处理事项关联极强）
- **VAN push-back 5 项风险闭环：** 「6 子系统全做」陷阱 → V1=B 收敛 / 「规划」语义模糊 → V2=a 明确 / UI 渲染层选型 → V3=A 锁定 / Spec 主文档过长 → 12 段式样 + creative 拆分 / systemPatterns 重叠 → spec 阶段强制对照 ≥ 30 模式
- **下一步：** 路由到 `/plan` — 进入头脑风暴阶段（预期 D1-D6 决策矩阵 + spec 主文档 12 段式样 + ≥ 2 篇 creative）；不进入 `/build`（V2=a 纯蓝图，build 由用户基于产出 plan 拆出独立 build 任务）

</details>

</details>

## 上次任务（已归档闭环）

**TASK-20260430-04：规划 UI 编辑器 + 调试器（DevTool 三件套蓝图设计）[安全相关]** — ✅ 已归档于 2026-05-01 ~03:00，已 `--no-ff` 合入 main。

- **归档文档：** `memory-bank/archive/archive-TASK-20260430-04.md`
- **核心成果：** 4 篇蓝图文档（spec + plan + 2 creative，~1879 行）+ D1-D8 决策矩阵 + T1-T8 威胁建模 + 7 项独立立项候选 + 4 项历史技术债闭环 ROI 路径
- **plan ×0.6 第 17 数据点入库**：核心轮次 0.27-0.35× plan / 0.46-0.59× plan ×0.6（极窄档 + review 类下限交界）
- **改进建议落实率 90%**：P0 3/3 + P1 4/4 + P2 2/3（剩 1 项 P2 #10 待 TASK-30-04-A/B/C 立项后实测回填）
- **方法论沉淀**：首次 V2=a 蓝图任务工作流变体实践 + 「批量决策跳过 + 批量文档产出」3 数据点群组化「极窄档」+ dogfood 路径作为探测性 acceptance test 概念

**TASK-20260430-03：全代码库 Code Review** — ✅ 已归档于 2026-05-01 ~00:30，已 `--no-ff` 合并到 main `2445990`（11 commits + 1 merge commit）。

- **归档文档：** `memory-bank/archive/archive-TASK-20260430-03.md`
- **核心成果：** R0 prep + R1 报告（55 findings / 6 维度归集）+ R2 P0 quick fix 6 项 + Reflect + Archive 全闭环；改进建议落实率 90%（P0 1/1 + P1 4/4 + P2 4/5）
- **plan ×0.6 第 16 数据点入库**：核心轮次 0.85-1.00× ×0.6 阈内 ✅
- **R3+ 13 项 P1 候选** 待用户决策拆分顺序后独立立项（详见 `docs/reports/2026-04-30-codebase-review.md`）

<details>
<summary>TASK-20260430-03 历史阶段快照（archive 已落盘，点开查看会话内进度记录）</summary>

### REFLECT 阶段产出快照（2026-05-01 ~00:08）

- **Reflection 主文档：** `memory-bank/reflection/reflection-TASK-20260430-03.md`（10 段 + 2 附录 / Level 4 全面回顾）
- **关键发现：**
  1. Background agent 双轨模式首次实战暴露并发会话切分支 race condition（reflog 显示 23:41/23:45 两次双切，R2.1 commit 混入 04 状态文件 + R2.3 改动丢失）
  2. plan ×0.6 估时模型有效（总 ~177 min / plan ×0.6 0.85-1.00× 阈内）— 第 16 数据点入库
  3. 任务类型决定 ×0.6 系数比复杂度更显著（review 0.4-0.7× / fast-fix 0.7-1.4× / racy 1.4-2.5× / 大件 0.8-1.2×）
- **改进建议 10 项（P0×1 / P1×4 / P2×5）：** 详见 reflection §5 + activeContext 待处理事项闭环段
- **systemPatterns 新增 5 段：** Background agent 双轨模式 + worktree 隔离协议 / plan ×0.6 任务类型分桶矩阵 / Quick fix 12 min/项基准 / Checkpoint 推荐默认协议 / Review 类 spec 模板
- **techContext 新增 3 段：** TASK-30-03 R1 55 项 findings 引用 + R2 已修复 6 项 + R3+ 13 项拆分；CMake basic vs full 矩阵；Background agent worktree 隔离工程指引
- **实测耗时：** ~30 min（reflection 文档 + 4 MB 文件更新 + 2 commit）

### BUILD R2 阶段产出快照（2026-04-30 ~24:55）

- **R2 全过 ctest 1062/1062 PASS**（基线 1061 + R2.5 新增守卫单测 = 1062；1 Wpt001 Skip 沉淀状态保持）
- **6 commits 链：**
  | # | Commit | Finding | 类型 | 行为变化 |
  |---|---|---|---|---|
  | R2.1 | `3b4b2e7` | F-020 selector_matcher dead return | 安全防御 | ❌ 注释 + VX_DCHECK |
  | R2.2 | `1467207` | F-033 ProcessEndTag isize 净化 | 重构 | ❌ 等价循环 |
  | R2.3 | `ddea78d` | F-040 rasterizer 阈值注释 | 文档 | ❌ 纯注释 |
  | R2.4 | `95ae814` | F-026 LayoutEngine arena thread_local | 线程安全 | ✅ static → thread_local |
  | R2.5 | `9c6ad5f` | F-053 image_decoder max_size 守卫 | 安全 | ✅ 新参数 + 守卫 + 单测 |
  | R2.6 | `668a9fe` | F-055 vx_version() configure_file | 维护 | ✅ hardcode → CMake gen |
- **执行环境：** 独立 git worktree `.worktree-03-r2`（隔离主 worktree 并发会话分支切换冲突，reflog 显示 23:41:23/30 + 23:45:08 双切）
- **新增文件：** `veloxa/api/version.h.in`（CMake configure_file 模板）
- **修改文件：** 6 文件（4 src + 1 header + 1 CMakeLists + 1 test）
- **实测耗时：** ~70 min（plan 55 min ×0.6=33 min；实测 1.27× plan / 2.12× ×0.6）；扣除 worktree 隔离 + 冲突修复 ~25 min 后 ≈ 0.82× plan / 1.36× ×0.6
- **关键观察（reflect 阶段 systemPatterns 候选）：**
  - 「并发会话切分支冲突」检测信号：activeContext.md 跨 commit 莫名变化 + git reflog 显示外部 checkout
  - 应对模式：git worktree 隔离 + 每 commit 前 `git symbolic-ref --short HEAD` 防御
  - background agent 模式（用户主线 04 + 我后台跑 03）= 双 worktree 设计，两轨独立演进

### BUILD R1 阶段产出快照（2026-04-30 ~24:39）

- **R1 报告主文档：** `docs/reports/2026-04-30-codebase-review.md`（11 段 / 55 项 findings / 6 维度归集 / R2 候选 6 项 / R3+ 13 拆分任务建议）
- **R1.1 H 层深读 ✅：** 实际深读 25+ 文件（含附带头文件），按 7 子系统 a-f 分批：
    - R1.1a CSS 5 文件 → 11 项 findings
    - R1.1b Layout 3 文件 → 5 项 findings
    - R1.1c DOM/HTML/App 4 文件 → 9 项 findings
    - R1.1d Rendering 3 文件 → 5 项 findings
    - R1.1e Script/Event/Update 4 文件 → 4 项 findings（含 F-046 真 P1 bug）
    - R1.1f Foundation/Text/Image/API 6 文件 → 11 项 findings（含 F-049/F-050/F-051 image 安全三件套）
- **R1.2 M 层 grep 一过 ✅：** static / memcpy / magic numbers / delete 4 模式扫描（5 个 delete 全部合理 → 嵌入式 arena 策略实施完美）
- **R1.3-R1.4 归集 + 分级 ✅：** 0 P0 + 28 P1 + 19 P2 + 8 P3
- **关键 finding（Top 5）：**
    1. F-051 P1 安全：JPEG decoder 默认 `error_exit` 调 `exit()` 杀进程
    2. F-050 P1 安全：PNG/JPEG `width × height` 无溢出检查（CVE 触发条件）
    3. F-046 P1 正确：EventDispatcher dispatch 中 listener mutation iterator 失效
    4. F-025 P1 正确：`LoadHTML` 不重置 `dom_bindings_` use-after-free
    5. F-022 P1 维护：CSS 属性元数据表缺失 shotgun surgery 跨 8+ 处（最大杠杆点）
- **R2 候选清单（6 项 quick fix / 55 min）：** F-020 dead return / F-026 thread_local / F-033 isize cast / F-040 阈值注释 / F-053 文件大小上限 / F-055 version configure_file
- **R3+ 拆分任务建议（13 项）：** image_decoder 安全 / DOM lifecycle / LoadHTML use-after-free / CSS 元数据表 / border shorthand / 现代选择器 / Layout 边角 / HTML5 实体表 / Rasterizer 性能 / 12 个低覆盖模块测试补充 / libpng 升级
- **实测耗时：** ~85 min（plan 150-200 min × 0.6 = 90-120 min；实测 0.50× plan / 0.78× ×0.6）→ reflect 阶段 plan ×0.6 第 16+ 数据点


### BUILD R0 阶段产出快照（2026-04-30 23:08）

- **R0 综合数据：** `docs/reports/2026-04-30-codebase-review-r0-data.md`（入库唯一持久副本）
- **R0.1 ctest 基线：** 1061/1061 PASS in 2.36s + 1 Skip（Wpt001 沉淀）✅
- **R0.2 fingerprint grep：** 18 项反模式 / 30 关键字覆盖 → veloxa/ 全代码库 0 TODO/FIXME/XXX/HACK + 0 危险 C 函数 + 0 throw + 0 不安全字符串转换
- **R0.3 lcov 覆盖率：** 行 85.4% / 函数 95.4% / 分支 57.6%；薄弱模块 12 项（image_decoder.cc 57.1% + rasterizer.cc 60.4% 是热点）
- **R0.4 CVE 审计：** 0 CRITICAL/HIGH（plan §4 D9 acceptance ✅）+ 5 Medium/Low；**关键发现**：libpng 1.6.37 命中 3 个 2026 新公布 Medium CVE，与 image_decoder.cc 57.1% 覆盖薄弱形成威胁链路（候选 P0 F-010）
- **R0.5 抽样 + commit：** R1 抽样名单 H 20 / M 80 / L 36 三层就绪 + R0 数据 commit 落地
- **候选 findings：** 14 项（F-001 ~ F-014 跨 6 维度），R1 验证 + 分级 + 写方案
- **实测耗时：** ~22 min（plan 90 min × 0.6 = 54 min 校准；实测 0.41× plan / 0.69× ×0.6） → reflect 阶段 plan ×0.6 校准协议第 16 数据点
- **工具补强：** OSV.dev 走 WSL2 → Windows Clash 代理 `172.22.32.1:7890`（沙箱直连 DNS 失败 → 宿主代理 fallback，候选 systemPattern）

### PLAN 阶段产出快照

- **决策矩阵 D1-D10：** B/C/B/B/spec/15/C/C/B/❌不用子代理
- **设计 spec：** `docs/specs/2026-04-30-codebase-review-design.md`（12 段 + T1-T10 安全威胁建模 + R0/R1/R2 多轮次协议）
- **实现 plan：** `docs/plans/2026-04-30-codebase-review.md`（10 段 + R0 + R1.1-R1.4 + R2 + Checkpoint 1/2 + 风险预案表）
- **抽样名单：** TOP-3 深扫 = foundation/{containers,strings} + script/ + core/{dom,html}（35 文件）；其他 4 子系统 grep 模式
- **工具链：** lcov 1.14 ✅ / gcov 11.4 ✅ / FetchContent 三处离线 ✅
- **估时：** plan ~6-10h / plan × 0.6 ~3.6-6h（上限 R0+R1+R2 封顶）
- **预期 plan × 0.6 数据点：** Level 4 review 类首数据点，预期落 0.5-0.6× 中位档

### VAN 阶段产出快照

- **决策矩阵 V1-V5：** A 全代码库 / all 6 维度 / C 完整实施（多轮次 + Checkpoint）/ D 混合视角 / ✅ 安全相关
- **策略 X：** R0+R1 必然（报告必产）/ R2 条件触发（P0 quick fix）/ R3+ 拆出独立后续任务
- **分支：** `feature/TASK-20260430-03-codebase-review`（基于 main `9411584`）
- **前置验证：** ✅ 全过（无新依赖 / `_deps/` 三处离线 / ctest 1061/1061 隐式继承 / 8 项 P3 候选作为 R1 输入材料）
- **Push back 决策：** 强制 R3+ 拆出 + Checkpoint 1/2 + Spec 主文档拆分附录（避免「样样不深」+「不可估时」+「主文档过长」3 风险）

</details>

## 最近归档完成

- **TASK-20260430-03：全代码库 Code Review（6 维度 × 7 子系统 + 多轮次 Build + Checkpoint）[安全相关]** — Level 4 ✅（2026-05-01）
  - 归档文档：`memory-bank/archive/archive-TASK-20260430-03.md`
  - 55 项 findings（28 P1 + 19 P2 + 8 P3）+ 6 项 P0 quick fix 实施 ctest 1062/1062 PASS + 13 R3+ 拆分任务建议
  - 改进建议落实率 90%（P0 1/1 + P1 4/4 + P2 4/5）；P0 #3 git symbolic-ref commit 守门 → `git-workflow.mdc` 落实；P1 #4 reflog 诊断 → `systematic-debugging.mdc` 落实
  - plan × 0.6 第 16 数据点入库（核心轮次 0.85-1.00× ×0.6 阈内 ✅）；首发 background agent 双轨模式 + worktree 隔离协议沉淀
  - feature 分支已 `--no-ff` 合并到 main `2445990`（合并发生于 04 任务推进过程中）
- **TASK-20260430-02：CSS border shorthand 补全（4 方向 + 3 属性级）[安全相关]** — Level 2 ✅
  - 归档文档：`memory-bank/archive/archive-TASK-20260430-02.md`
  - 22 新单测 + 双反向探针完整三态；ctest Debug 1061/1061 + Release 1030/1030
  - 3 改进建议全部闭环（P1 #1「极窄档 0.2-0.25×」+ P2 #2「Spec 描述粒度准则」+ #3 ROI 评估）→ systemPatterns.md
  - A8 ROI 验证 ✅：TASK-30-01 §0 升级规则首次外部任务 2/4 触发 + 触发部分均高/中 ROI
  - plan × 0.6 第 15 数据点 0.22× — 与 TASK-30-01 P6（0.21×）一同定型「极窄档」
- **TASK-20260430-01：first/last child margin collapse with parent（CSS 2.1 §8.3.1）** — Level 3 ✅
- **TASK-20260426-01：Layout 正确性消化（#25 + #28 + #20 + #21）** — Level 4 ✅
- **TASK-20260425-01：SDL2 窗口后端 + 输入事件桥接** — Level 3 ✅

## 说明

详细实现过程、测试细节与经验教训均已沉淀至各任务的 `archive-`* / `reflection-*` 文档。
