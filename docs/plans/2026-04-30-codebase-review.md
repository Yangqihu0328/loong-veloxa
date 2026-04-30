# 全代码库 Code Review 实现计划

- **任务 ID：** TASK-20260430-03
- **设计 spec：** `docs/specs/2026-04-30-codebase-review-design.md`
- **复杂度：** Level 4（多子系统 + 多轮次 + Checkpoint）
- **总估时：** plan ~6-10h / plan × 0.6 ~3.6-6h（上限 R0+R1+R2）
- **多轮次：** R0（必）+ R1（必）+ R2（条件）+ R3+（拆出）

---

## §0 总览：Phase 表 + 轮次切分（systemPatterns「Phase 表附加轮次切分建议」实践）

| Phase | 内容 | plan (min) | × 0.6 (min) | commits | 轮次切分建议 |
|:-:|---|---:|---:|:-:|---|
| **R0** prep | grep fingerprint + lcov + CVE + 抽样名单 | 90 | 54 | 1 | R0 单轮（独立产出物：fingerprint 数据 + 覆盖率 + CVE 状态）|
| **R1.1** 深扫 TOP-3 | foundation/{containers,strings} + script + core/{dom,html} 全文件深读 | 120 | 72 | 0 | 与 R1.2 同轮（连续上下文，避免代码记忆衰减）|
| **R1.2** grep 4 子系统 | foundation/{base,memory,log} + 最近活跃集 + platform+api + tests | 60 | 36 | 0 | 与 R1.1 同轮 |
| **R1.3** 6 维度交叉分析 | 对 R1.1+R1.2 输出按维度交叉归类 + ROI 评估 | 60 | 36 | 0 | 与 R1.1/R1.2 同轮 |
| **R1.4** 报告撰写 | 主报告 + 各章节 + 元规则附录 + CVE 长期追踪表 | 90 | 54 | 1 | 与 R1.3 同轮 |
| **Checkpoint 1** | 用户审 + 决定 R2 范围 | — | — | — | 阻塞点 |
| **R2** P0 quick fix | 5-15 个 P0 修复（条件触发）| 60-180 | 36-108 | 5-15 | R2 单轮（按 P0 数量 1 commit/项）|
| **Checkpoint 2** | 用户决定 R3+ 拆分 | — | — | — | 阻塞点 |
| **合计** | — | **480-600** | **288-360** | **7-17** | — |

---

## §1 R0 — 准备阶段（90 min plan / 54 min × 0.6）

### R0.1 ctest 基线核验（5 min）

```bash
# 任务起步必做：核验 1061/1061 PASS（继承 TASK-30-02 终态）
cd /home/qihooz/code/loong-veloxa
cmake --build build -j 2>&1 | tail -3
ctest --test-dir build -j 2>&1 | tail -10
# 期望：1061/1061 PASS in ~2-3s
```

**验收**：1061/1061 PASS（如失败 → STOP，先排查继承基线问题）

---

### R0.2 6 维度 fingerprint grep 完整产出（30 min）

每维度 ≥ 5 关键字 grep 全代码库（`veloxa/` + `tests/`），结果写入 `/tmp/review_fingerprint.md` 供后续阶段引用。

```bash
mkdir -p /tmp/codebase_review
cd /home/qihooz/code/loong-veloxa

# 性能维度（perf）
{
  echo "=== PERF: O(N^2) / linear-scan / push_back-in-loop ==="
  rg "for.*for.*push_back\|find\(.*for\|Vector\<.*\>\.push_back.*for\b" --type cpp veloxa/ tests/
  echo "=== PERF: TODO/FIXME perf ==="
  rg "(TODO|FIXME|XXX).*perf" --type cpp veloxa/ tests/ -i
  echo "=== PERF: memcpy/memmove 大常量 ==="
  rg "memcpy.*[0-9]{3,}\|memmove.*[0-9]{3,}" --type cpp veloxa/
  echo "=== PERF: 链式 String ==="
  rg "String\(.*\)\s*\+\s*String\|Append.*Append.*Append" --type cpp veloxa/
  echo "=== PERF: 重复 hash / Find ==="
  rg "Find\(.*\).*Find\(\|hash\(.*\).*hash\(" --type cpp veloxa/
} > /tmp/codebase_review/perf.txt

# 正确性维度（correct）
{
  echo "=== CORRECT: TODO/FIXME bug/correctness ==="
  rg "(TODO|FIXME|XXX|HACK).*\b(bug|correctness|wrong|broken)\b" --type cpp veloxa/ tests/ -i
  echo "=== CORRECT: UB / 未定义行为 ==="
  rg "\b(UB|undefined.behavior|uninitialized|narrowing)\b" --type cpp veloxa/ -i
  echo "=== CORRECT: signed/unsigned 比较 ==="
  rg "static_cast\<\b(int|i32|i64)\>.*size\(\)\|\bsize_t\b.*<\s*0\b" --type cpp veloxa/
  echo "=== CORRECT: 边界值缺失 ==="
  rg "if\s*\(\s*\!\s*ptr\s*\)\s*return\b" --type cpp veloxa/ | wc -l
  echo "=== CORRECT: 隐式假设 ==="
  rg "// assume|// expect|// must" --type cpp veloxa/ -i
} > /tmp/codebase_review/correct.txt

# 可维护性维度（maint）
{
  echo "=== MAINT: magic numbers ==="
  rg "\b(if|while|for|return|=)\s+[\(\s]*[0-9]{3,}\b" --type cpp veloxa/ | head -50
  echo "=== MAINT: hardcoded paths/IPs ==="
  rg "\"/\w+/\w+\|192\.168\|172\.\d+\|127\.0\.0\.1" --type cpp veloxa/
  echo "=== MAINT: 长函数（粗发现，>= 100 行）==="
  for f in $(find veloxa -name "*.cc"); do
    awk '/^[a-zA-Z].*\([^;]*\{$/{start=NR;name=$0} /^\}$/&&start{if(NR-start>=100)print FILENAME":"start": "(NR-start)" lines: "name; start=0}' "$f" 2>/dev/null
  done | head -30
  echo "=== MAINT: 复制粘贴模式（连续 3+ 相同语句）==="
  rg -U "^(\s+)[\w\.\-\>]+\(\)?[;,]?\n\1[\w\.\-\>]+\(\)?[;,]?\n\1[\w\.\-\>]+\(\)?[;,]?$" --type cpp veloxa/ -m 5
  echo "=== MAINT: deprecated 未标 ==="
  rg "// deprecated\|// legacy\|// obsolete\|// old" --type cpp veloxa/ -i
} > /tmp/codebase_review/maint.txt

# 安全维度（security）
{
  echo "=== SECURITY: 危险 C 函数 ==="
  rg "\b(gets|strcpy|strcat|sprintf|vsprintf|scanf|sscanf)\s*\(" --type cpp veloxa/
  echo "=== SECURITY: system / exec ==="
  rg "\b(system|exec[lvp]?|popen)\s*\(" --type cpp veloxa/
  echo "=== SECURITY: 外部输入 ==="
  rg "\bgetenv\(|\bargv\[|stdin\b" --type cpp veloxa/
  echo "=== SECURITY: 整数溢出风险 ==="
  rg "\*\s*[a-z_][a-z0-9_]*\s*\+\|size\(\)\s*\*\s*sizeof" --type cpp veloxa/ | head -20
  echo "=== SECURITY: 硬编码 token / password / proxy ==="
  rg "\b(token|password|secret|api_key|proxy)\s*=\s*\"" --type cpp veloxa/ -i
} > /tmp/codebase_review/security.txt

# 测试维度（test）
{
  echo "=== TEST: DISABLED_ / SKIP ==="
  rg "(DISABLED_|GTEST_SKIP|SKIP\b)" --type cpp tests/
  echo "=== TEST: TODO test ==="
  rg "(TODO|FIXME|XXX).*test" --type cpp tests/ -i
  echo "=== TEST: 未测试公共函数（粗：source 中有 public 函数定义但 tests 中无 EXPECT/ASSERT 引用）==="
  echo "(详细分析在 R0.4 lcov 阶段)"
  echo "=== TEST: 测试中的 magic value ==="
  rg "EXPECT_(EQ|NE|LT|GT)\([^,]+,\s*[0-9]{3,}" --type cpp tests/ | head -20
  echo "=== TEST: 测试缺少边界条件（空 / nullptr / max）==="
  rg "TEST\b.*\(.*Empty\|.*Null\|.*Zero\|.*Max" --type cpp tests/ | wc -l
} > /tmp/codebase_review/test.txt

# 一致性维度（consistency）
{
  echo "=== CONSISTENCY: 风格混杂（snake_case vs camelCase 公共 API）==="
  rg "^(static\s+)?\w+::\w+::\w+\s+\w+::[A-Z]\w+\(" --type cpp veloxa/ | head -20
  echo "=== CONSISTENCY: 头文件包含风格混杂（< > vs \" \"） ==="
  rg "^#include\s+<veloxa/" --type cpp veloxa/ | head -10
  echo "=== CONSISTENCY: namespace 风格 ==="
  rg "^namespace\s+\w+\s*\{$" --type cpp veloxa/ | sort -u | head -20
  echo "=== CONSISTENCY: 错误处理风格混杂 ==="
  rg "VX_DCHECK\|VX_CHECK\|assert\(\|abort\(\|throw\b" --type cpp veloxa/ | wc -l
  rg "(VX_DCHECK|VX_CHECK|assert\(|abort\(|throw\b)" --type cpp veloxa/ | awk -F: '{split($1,a,"/");print a[2]}' | sort | uniq -c | sort -rn | head -10
  echo "=== CONSISTENCY: T_t / T_ 后缀 vs Get / Set 前缀 ==="
  rg "->\w+_\(\)\|\.\w+_\(\)" --type cpp veloxa/ | head -10
} > /tmp/codebase_review/consistency.txt

# 汇总
ls -la /tmp/codebase_review/
echo "---"
wc -l /tmp/codebase_review/*.txt
```

**验收 A1**：6 维度文件均产出 + 每维度 ≥ 5 关键字 grep 命中（即使部分 0 命中也算覆盖）

---

### R0.3 lcov 覆盖率构建（25 min — 含构建时间）

```bash
# 独立 build-coverage/ 目录避免污染既有 build/
cmake -B build-coverage \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="--coverage -O0 -g" \
  -DCMAKE_C_FLAGS="--coverage -O0 -g" \
  -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
  -DVX_BUILD_TESTS=ON 2>&1 | tail -5

cmake --build build-coverage -j 2>&1 | tail -5

# 重置覆盖率计数器
lcov --directory build-coverage --zerocounters 2>&1 | tail -3

# 跑全部测试（继承 1061 测试）
ctest --test-dir build-coverage -j 2>&1 | tail -10

# 收集覆盖率
lcov --capture --directory build-coverage \
     --output-file /tmp/codebase_review/coverage.info \
     --rc lcov_branch_coverage=1 2>&1 | tail -5

# 排除第三方代码 + tests 自身 + system 头
lcov --remove /tmp/codebase_review/coverage.info \
     '/usr/*' '*/build-coverage/*' '*/tests/*' '*/_deps/*' \
     --output-file /tmp/codebase_review/coverage_filtered.info \
     --rc lcov_branch_coverage=1 2>&1 | tail -5

# 生成 HTML 报告（可选 — 用于深读）
genhtml /tmp/codebase_review/coverage_filtered.info \
        --output-directory /tmp/codebase_review/coverage_html \
        --branch-coverage 2>&1 | tail -10

# 提取每子系统 line/branch/function 三维度数字
lcov --list /tmp/codebase_review/coverage_filtered.info > /tmp/codebase_review/coverage_summary.txt
```

**验收 A4**：
- `coverage_filtered.info` 生成成功
- `coverage_summary.txt` 含每文件 / 每子系统的 line / branch / function 覆盖率
- 总体覆盖率 ≥ 70%（如低于此 → 重大盲区，R1 报告需重点处理）
- HTML 报告可访问 `/tmp/codebase_review/coverage_html/index.html`

---

### R0.4 第三方依赖 CVE 审计（25 min — 需 full_network）

7 依赖 + 各自版本：
- quickjs-ng v0.14.0 (FetchContent)
- google/benchmark v1.9.1 (FetchContent)
- FreeType (system pkg)
- HarfBuzz (system pkg)
- libpng (system pkg)
- libjpeg-turbo (system pkg)
- SDL2 v2.0.20 (system pkg)

```bash
# 通过 GitHub Security Advisories 网页 API 查（需 full_network）
{
  echo "=== quickjs-ng v0.14.0 ==="
  curl -s "https://api.github.com/repos/quickjs-ng/quickjs/security-advisories" 2>&1 | head -50
  echo ""
  echo "=== google/benchmark v1.9.1 ==="
  curl -s "https://api.github.com/repos/google/benchmark/security-advisories" 2>&1 | head -50
  echo ""
  echo "=== FreeType (system pkg) ==="
  apt show libfreetype6 2>&1 | grep -i version | head -2
  echo "https://www.freetype.org/announcements.html"  # 手工记录
  echo ""
  echo "=== HarfBuzz (system pkg) ==="
  apt show libharfbuzz0b 2>&1 | grep -i version | head -2
  echo ""
  echo "=== libpng (system pkg) ==="
  apt show libpng16-16 2>&1 | grep -i version | head -2
  echo ""
  echo "=== libjpeg-turbo (system pkg) ==="
  apt show libjpeg-turbo8 2>&1 | grep -i version | head -2
  echo ""
  echo "=== SDL2 v2.0.20 ==="
  curl -s "https://api.github.com/repos/libsdl-org/SDL/security-advisories" 2>&1 | head -50
} > /tmp/codebase_review/cve_audit.txt
```

**Fallback（如 full_network 不可用）**：手工查询 https://github.com/<repo>/security/advisories 网页并截图，结果记入 `/tmp/codebase_review/cve_audit_manual.txt`。

**验收 A5**：7 依赖各列出当前版本 + GitHub Security Advisories 数量 + 严重性分布（critical / high / moderate / low）

---

### R0.5 抽样名单 + R0 commit（5 min）

确认 §5 抽样名单不变（除非 R0.2 fingerprint 显示某 4 grep 子系统命中数 >> TOP-3，则有权调整）：

```
TOP-3 深扫：
  1. veloxa/foundation/containers/ + veloxa/foundation/strings/（13 文件）
  2. veloxa/script/（8 文件）
  3. veloxa/core/dom/ + veloxa/core/html/（14 文件）

4 grep：
  1. veloxa/foundation/{base,memory,log}/
  2. veloxa/core/{css,layout,render,event,image}/ + veloxa/text/ + veloxa/graphics/
  3. veloxa/platform/ + veloxa/api/
  4. tests/
```

R0 收尾 commit：
```bash
git add docs/specs/2026-04-30-codebase-review-design.md \
        docs/plans/2026-04-30-codebase-review.md \
        memory-bank/tasks.md memory-bank/activeContext.md memory-bank/progress.md
git commit -m "docs(plan): TASK-20260430-03 codebase review spec + plan"
```

**验收**：spec + plan 入仓；R0 数据全部在 `/tmp/codebase_review/` 待 R1 引用

---

## §2 R1 — 全代码库 review 报告（300 min plan / 180 min × 0.6）

### R1.1 深扫 TOP-3 子系统（120 min）

#### R1.1.a foundation/containers + foundation/strings（45 min）

文件清单（13 个）：
- `containers/vector.h` / `small_vector.h` / `intrusive_list.h` / `hash_map.h` / `optional.h` / `status_or.h` / `function.h`
- `strings/string.h` / `string_view.h` / `interned_string.h` / `interned_string.cc` / `utf.h` / `utf.cc`

每文件 review 维度清单（6 维交叉）：

| 文件 | perf | correct | maint | security | test | consistency |
|---|:-:|:-:|:-:|:-:|:-:|:-:|
| `vector.h` | ⚠️ 动态扩容策略 | ⚠️ size_type / iterator 一致性 | ⚠️ 与 std::vector 偏差文档 | — | ⚠️ 边界值测试 | ⚠️ namespace 风格 |
| `hash_map.h` | ⚠️ djb2 vs identity hash 边界 | ⚠️ rehash 时迭代器失效 | ⚠️ Find / Insert 名称 | — | ⚠️ 反复 grow 测试 | — |
| `string.h` | ⚠️ SSO 阈值 / Append 拷贝 | ⚠️ -Werror=array-bounds 历史 | ⚠️ memcpy noinline 候选 | ⚠️ utf8 边界 | ⚠️ 中文/emoji 边界 | — |
| ... 其余 10 文件类似 | | | | | | |

**输出**：每文件产生 0-3 项不足项条目（追加到 `/tmp/codebase_review/findings_top3.md`）

#### R1.1.b script/（45 min）

文件清单（8 个）：
- `quickjs_engine.h` / `quickjs_engine.cc` / `dom_bindings.h` / `dom_bindings.cc` / `js_value_helpers.h` / `js_class_registry.h` / 其他

重点 review 维度（**T7 安全重点**）：
- JSContext opaque / pimpl 模式正确性（TASK-18-01 已 review，本次复检）
- `JS_NewClassID` 进程级状态泄漏风险
- JS callback 寿命管理（lambda-vs-JSValue 同寿命模式）
- `removeEventListener` token 一致性（TASK-19-01 已加，复检）
- 内存限制 32MiB / 源码 256KiB 上限的边界绕过

#### R1.1.c core/dom + core/html（30 min）

文件清单（14 个）：
- `dom/document.h` / `dom/element.h` / `dom/node.h` / `dom/text.h` / `dom/comment.h` / `dom/tag.{h,cc}`
- `html/parser.h` / `html/parser.cc` / `html/tokenizer.h` / `html/tokenizer.cc` / `html/inline_style_blacklist.{h,cc}`

重点：
- DOM 析构顺序（TASK-19-01 反向析构观察者，复检）
- HTML parser 边界（TASK-26-01 R2 三件套护栏，**不动**仅复检）
- TagInfo 静态表完整性 / 隐式关闭规则一致性
- 双向链表操作的 lifetime 假设

**验收 R1.1**：3 子系统全文件深读完成；累计不足项条目 `/tmp/codebase_review/findings_top3.md` 至少 15-30 条

---

### R1.2 4 grep 子系统快速过（60 min）

#### R1.2.a foundation/{base,memory,log}/（10 min）
- ArenaAllocator / PoolAllocator 边界（TASK-24-01 已大幅 review，仅复检）
- log macros 编译时裁剪一致性

#### R1.2.b core/{css,layout,render,event,image}/ + text/ + graphics/（25 min）
- 这是「最近活跃」集，30+ archive 任务深度沉淀
- 聚焦：与 systemPatterns 模式偏差 / TODO 残留 / 一致性

#### R1.2.c platform/ + api/（10 min）
- SDL2 backend（TASK-25-01 刚加，复检）
- Headless backend
- C API 不透明指针 ABI 边界

#### R1.2.d tests/（15 min）
- DISABLED_ / GTEST_SKIP 详查每条 rationale 是否充分
- 测试本身的 magic value
- 大测试文件分解候选
- WPT fixture 适配（TASK-26-01 R3/R4）

**输出**：`/tmp/codebase_review/findings_grep.md`，预期 5-15 条不足项

**验收 R1.2**：4 grep 子系统 fingerprint 完整查阅；条目分类归档

---

### R1.3 6 维度交叉分析 + ROI 评估（60 min）

#### R1.3.a 维度归类与去重（20 min）
- 把 R1.1+R1.2 的 20-50 条不足项按 6 维度独立成表
- 跨维度重叠的合并为 1 条（如「`Vector::push_back` 在 hot loop」既是 perf 又是 maint → 取 perf）

#### R1.3.b ROI 矩阵评估（20 min）

ROI 矩阵：
```
                  低成本（≤ 5 行）    中成本（5-50 行）    高成本（50+ 行）
高影响（hot path / 多调用方 / 安全） |  P0 高 ROI ✅       |  P1 高 ROI ⭐      |  P2 战略 |
中影响（单子系统 / 偶尔触发）        |  P0 中 ROI         |  P1 中 ROI         |  P2 待定 |
低影响（trivial / 文档 / 注释）      |  P0 低 ROI（可选）  |  P1 低 ROI（不推） |  P2 拒绝 |
```

每条不足项分配：(成本, 影响) → (P0/P1/P2, ROI 等级)

#### R1.3.c 与 systemPatterns 对照（20 min）
- 每条不足项标注：
  - **已规则化**：systemPatterns 已有规则覆盖此类问题（如违反「跨 LayoutType 独立 box-model」）
  - **部分规则化**：规则提到但场景未覆盖（如「N-cap 复用」规则但本次发现新场景）
  - **未规则化**：完全新发现（→ 进入「需新增规则」候选）

**验收 R1.3**：每条不足项有完整属性 (维度, 优先级, ROI, 规则状态)

---

### R1.4 报告撰写 + R1 commit（90 min）

#### R1.4.a 主报告骨架（15 min）

按 spec §7 章节框架创建：
```
docs/specs/2026-04-30-codebase-review-report.md
  §0 执行摘要
  §1 P0 不足清单
  §2 P1 不足清单
  §3 P2 不足清单
  §4 与 systemPatterns 对照
  §5 测试覆盖率盲区
  §6 第三方依赖 CVE 长期审计追踪表
  §7 元规则潜在问题
  §8 ROI 矩阵 + Checkpoint 1 决策建议
  附录 A/B/C
```

#### R1.4.b 各章节填充（60 min）

**§0 执行摘要**（8 min）：
- 总览数字
- ROI 矩阵图（文本表）
- 最重要发现 TOP 5

**§1-3 不足清单**（30 min）：
- 按 R1.3 输出排版（D3=B 结构化：问题 + 方向 + 工时 + 代码量）
- 表格列：`# | 文件:行 | 维度 | 描述 | 方案方向 | 工时 | 代码量 | ROI | 规则状态`

**§4 systemPatterns 对照**（5 min）：
- 已规则化项汇总
- 「需新增规则」候选 → 列入 §7

**§5 lcov 覆盖率盲区**（10 min）：
- 子系统覆盖率分布表
- 未覆盖热点 TOP 10
- 建议补测候选

**§6 CVE 长期追踪**（5 min）：
- 7 依赖当前状态表
- 升级候选 + 周期 audit 节奏

**§7 元规则候选**（5 min）：
- 矛盾点 / 过时段（D7=C：仅记录不修改）

**§8 ROI 决策建议**（2 min）：
- 推荐 R2 启动 P0 子集（5-10 个最高 ROI）
- 推荐拆出 R3+ 任务的 P1 排序

#### R1.4.c 附录（15 min）：
- 附录 A：抽样深度详情（哪些子系统全扫 / 抽样方式）
- 附录 B：grep fingerprint 完整结果链接（`/tmp/codebase_review/*.txt`）
- 附录 C：lcov HTML 报告链接

#### R1.4.d R1 commit：
```bash
git add docs/specs/2026-04-30-codebase-review-report.md
git commit -m "docs(review): TASK-20260430-03 R1 — full codebase review report"
```

**验收 R1.4 / A6-A12**：
- 主报告落盘
- 包含 ≥ N 项不足（预期 20-50 项）
- 每项含完整 D3=B 结构化属性
- §4 / §5 / §6 / §7 / §8 各章节齐全

---

## §3 Checkpoint 1 — 用户审报告（用户阻塞点）

**触发条件**：R1.4 commit 后

**用户决策项**：
1. R2 启动 P0 数量（0 / 5 / 10 / 15 / 自定义子集）
2. R2 跳过的 P0 是否拆为 R3+ 后续任务（拆 / 不拆）
3. P1 拆分顺序（按 ROI 自动 / 用户重排）
4. P2 是否进入下季度 backlog

**AskQuestion 形态**：
- 列表前 5-10 个 ROI 最高的 P0 + P1
- 让用户对每项做「R2 / 拆 R3+ / 跳过」三选

如果用户决定 R2 启动 ≥ 1 个 P0 → 进入 §4；否则跳到 §5（用户决定全部拆出后续任务，本任务直接 reflect）。

---

## §4 R2 — P0 quick fix（条件触发，60-180 min plan / 36-108 min × 0.6）

### R2.X — 单 P0 修复模板

每 P0 修复独立 commit，遵循以下模板：

```bash
# 1. 准备
git status  # 必须 clean

# 2. RED 反向探针（§9.3 强制，除非 trivial 改动如笔误）
# 临时破坏被测路径 → 跑测试确认 FAIL → 撤销
# 仅对会加测试的 P0 强制；笔误等可豁免

# 3. 实施修复（≤ 5 行 / ≤ 15 min）
# 修改源代码 + 加单测（如适用）

# 4. 验证
ctest --test-dir build -j 2>&1 | tail -5  # 1061 → 1061+N PASS
# 如涉及 Release 路径
cmake --build build-bench -j 2>&1 | tail -3  # 0 -Werror

# 5. commit
git add <files>
git commit -m "fix(<subsystem>): TASK-20260430-03 R2 — <one-line description>"
```

### R2 上限管理

- 总上限 15 个 P0（spec §6）
- 每 P0 严格控制 ≤ 15 min
- 如某 P0 实施超 20 min → STOP，记入「应升级为 P1，拆 R3+」
- R2 累计超过 90 min 强制中止，剩余 P0 拆 R3+

**验收 A13-A16**：
- ctest 1061+N PASS（N = R2 修复数）
- Release `-O3 -Werror` 0 err/warn
- `bench_*` baseline 不退化超 +5%（继承基线 — R2 trivial 改动不应触发任何 hot path 性能改动）
- 每 P0 1 commit

---

## §5 Checkpoint 2 — 用户决定 R3+ 拆分

**触发条件**：R2 完成 OR 用户决定跳过 R2

**用户决策项**：
1. 拆出哪些 P1/P2 为独立后续任务
2. 每个后续任务的 sticky ID 命名（建议 `TASK-20260430-04` / `-05` / ... 接续）
3. 后续任务的 plan 阶段（自动 `/van` 一个个起 / 集中 backlog 待立项）

**AskQuestion 形态**：
- 列出 R1 报告 §3 P2 + §2 未启动的 P1
- 让用户对每项决策「立即拆 TASK-* / 进 backlog / 拒绝」

---

## §6 风险预案表（writing-plans.mdc §风险预案要求）

| 风险场景 | 触发条件 | 应对（已写明两路结果）|
|---|---|---|
| R0.4 CVE 审计 full_network 失败 | curl GitHub API 返回错误 | 切换 fallback：手工查 GitHub Security Advisories 网页 + 记入 `cve_audit_manual.txt`；接受 30 min 损耗，不回滚 |
| R0.3 lcov 覆盖率构建失败 | gcov / lcov 版本兼容性问题 | gcov 11.4 + lcov 1.14 已实证可用；如真失败 → 切换 fallback `gcovr`（pip install）；最坏 D9 降级到 grep 模式 |
| R1 不足项数量超 50 | R1.3 归类后超 50 条 | 接受结果；ROI 矩阵自动筛 TOP-30 进主报告；其余进附录补遗 |
| R1 不足项数量不足 10 | R1.3 归类后 < 10 条 | 接受结果（说明代码库已较成熟）；R1.3 重点放在「与 systemPatterns 对照」+「测试盲区」+「CVE」三大块；reflection 阶段记录「review 净发现少 → 30+ archive 沉淀已饱和」 |
| Checkpoint 1 用户决定全部拆 R3+（R2 跳过）| 用户审 P0 后认为没有真正高 ROI 项 | 接受；本任务跳到 reflect；不强行制造 R2 工作 |
| R2 某 P0 实施超 20 min | 单 P0 超时 | 立即 STOP；revert 该 commit；标记为「应升级 P1，拆 R3+」；继续下一 P0 |
| Phase 切换时 ctest 退化 | 任何 phase 后 ctest < 1061 | 立即 git diff + bisect 定位回归 commit；如本任务引入 → 修复或 revert |

---

## §7 验收清单（一页速查）

```
R0 完成 标志：
  ☐ A1: 6 维度 fingerprint 落盘 /tmp/codebase_review/*.txt
  ☐ A2: 抽样名单确认（TOP-3 深扫 + 4 grep）
  ☐ A3: ctest 1061/1061 PASS
  ☐ A4: lcov coverage_filtered.info 生成 + summary 数字
  ☐ A5: CVE 审计 7 依赖状态表

R1 完成 标志：
  ☐ A6: 主报告 docs/specs/2026-04-30-codebase-review-report.md 落盘
  ☐ A7: ≥ N 项不足含完整属性
  ☐ A8: ROI 矩阵
  ☐ A9: systemPatterns 对照
  ☐ A10: 元规则附录
  ☐ A11: CVE 长期追踪
  ☐ A12: 覆盖率盲区报告

Checkpoint 1（用户）：决定 R2 启动 0-15 个 P0

R2 完成 标志（条件）：
  ☐ A13-A16: ctest / Release / bench / commit 全过

Checkpoint 2（用户）：决定 R3+ 拆分

进入 /reflect 阶段
```

---

## §8 估时校准协议（systemPatterns「bench 类任务估时校准」泛化应用）

| 阶段 | plan (min) | × 0.6 (min) | 关键不确定性 |
|:-:|---:|---:|---|
| R0 | 90 | 54 | lcov build 30 min 是固定项；CVE network 是变量 |
| R1.1 (深扫) | 120 | 72 | 不足项数量决定深读速度，不可线性 |
| R1.2 (grep) | 60 | 36 | 4 grep 子系统稳定 |
| R1.3 (交叉) | 60 | 36 | 归类速度受 R1.1 数据质量影响 |
| R1.4 (撰写) | 90 | 54 | 模板化产出 |
| R2 | 60-180 | 36-108 | 取决于 P0 数量 |
| **合计上限** | **480-600** | **288-360** | — |

**估时档位预测**：Level 4 多轮次 review 类 plan × 0.5-0.6（中位档）— 与 TASK-26-01 0.44× 对照属合理上区间（review 类相对实施类略慢，因思考密度高）。

---

## §9 与既有任务的衔接

| 已归档任务 | 衔接方式 |
|---|---|
| TASK-26-01 | R1 复检既有护栏（不动）+ 验证 systemPatterns 沉淀的多模式 |
| TASK-30-01 | R1 复检 layout API 决策模式（不动）|
| TASK-30-02 | R1 复检 CSS shorthand 模式（不动）|
| TASK-19-* 系列 | R1 复检 -Werror 修复 / 流程规则 / Benchmark 集成 |
| TASK-24-* 系列 | R1 复检 DrawText 优化 / Layout knee 修复 |
| TASK-18-01 / TASK-19-01 | R1 重点深扫（D1 TOP-3 含 script/）|

---

## §10 收尾 → /reflect

R2 完成（或跳过）+ Checkpoint 2 完成 → 准备进入 `/reflect` 阶段：
- 13 段全维度 reflection 文档
- plan × 0.6 第 16 数据点（Level 4 review 类首数据点）
- 累计 R3+ 拆出后续任务 ID 入 `activeContext.md` 待立项
- 改进建议候选（P0/P1/P2 / 元规则潜在问题）

---

> **「Spec 实施模式描述粒度准则」自我应用**：本 plan 详细到「具体 phase 命令脚本」级别（与 spec 的「契约 + 约束」相辅相成），但不绑定具体不足项数量 / 名称（因为 R1 实证才知）。BUILD 阶段实证发现可微调 plan 细节（如调整 grep 关键字 / 增减 phase 时间），spec 主线保持不变。
