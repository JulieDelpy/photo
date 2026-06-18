# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 环境要求

- **MSYS2 UCRT64** + MinGW-w64 (gcc 15.2.0)
- **OpenCV 4.12** (core, imgproc, imgcodecs, objdetect, dnn, calib3d, face)
- **CMake 3.16+**，C++17
- 运行时需 MSYS2 DLL 路径：`export PATH="/c/msys64/ucrt64/bin:$PATH"`

## 构建命令

```bash
# 初始配置
cmake -B build -G "MinGW Makefiles"

# 增量编译（最常用）
cmake --build build -- -j$(nproc)

# 运行测试
cd build && ctest --output-on-failure

# 运行 CLI 质量检查
./build/photo_tool.exe --check <image.jpg>
./build/photo_tool.exe --batch <directory>
./build/photo_tool.exe --list-checkers

# 启动 web 服务器（端口 9876）
./build/photo_web_server.exe
```

构建前如遇 `photo_web_server.exe` 被占用：`taskkill -f -im photo_web_server.exe`

## 架构概览

```
┌─────────────────────────────────────────────────┐
│  src/web_server.cpp   │  src/cli/main.cpp       │  ← 入口
│  (cpp-httplib :9876)  │  (命令行)               │
└──────────┬───────────┴──────────┬───────────────┘
           │                      │
           ▼                      ▼
    FaceAnalyzer (YuNet ONNX + LBF 68点标定)
           │
           ▼
    PluginManager<IQualityChecker>  ← 40 个注册插件
           │
           ▼
    QualityReport → JSON / 终端输出
```

### 插件系统（核心架构）

每个 checker 是一个自注册插件，通过 `REGISTER_PLUGIN` 宏在 main() 之前自动注册：

```cpp
// src/quality/blur_checker.cpp
REGISTER_PLUGIN(IQualityChecker, BlurChecker, "blur_check")
```

- 接口：`include/photo/plugin/iquality_checker.hpp` — 纯虚函数 `check(image, face, standard)`
- 管理器：`include/photo/plugin/plugin_manager.hpp` — 线程安全单例
- 宏：`include/photo/plugin/plugin_macros.hpp` — 编译期静态注册
- 所有 checker 编译为 OBJECT library（`photo_quality`），link 到可执行文件以保留静态构造函数

### 两级判级体系（severity）

| Severity | 含义 | 对 overall_pass 影响 |
|----------|------|---------------------|
| `PASS` | 合格 | 不计数 |
| `WARNING` | 边缘建议（黄灯）| 不计入 failed_checks |
| `FAIL` | 硬阻断（红灯）| `overall_pass = (failed_checks == 0)` |

只有 15 个核心 checker 可以使用 `FAIL`：
- `face_detect_check` / `face_count_check` / `face_confidence_check`
- `eye_closure_check` / `mouth_open_check` / `head_pose_check`
- `head_margin_check` / `centering_check`
- `face_size_check` / `face_ratio_check`
- `face_skin_tone_check` / `face_occlusion_check`
- `background_color_check` / `overexposure_check`
- `head_tilt_check`

其余 26 个 checker 全部使用 `WARNING`（总数 41）。

### 人脸检测管线

1. `FaceAnalyzer` 调用 YuNet ONNX 模型 + LBF 68 点 landmark 模型（`models/` 目录）
2. 输出 `FaceInfo`：bbox, landmarks[68], yaw/pitch/roll, EAR (左/右), MAR
3. 每个 checker 接收同一个 `FaceInfo`，按需读取

### 智能预处理（仅 web_server）

`src/web_server.cpp` 在 `/api/v1/check` 端点的 decode 后被检测前，对大图 (>358x441) 做等比缩放+居中裁剪。小图不放大，避免画质劣化。

### 目录结构

- `include/photo/core/` — FaceInfo, IDPhotoStandard, Severity 定义
- `include/photo/plugin/` — 插件接口和注册宏
- `include/photo/result/` — CheckResult, QualityReport, JSON 序列化
- `src/quality/` — 40 个 checker 实现
- `src/beauty/` — 4 个美颜效果（磨皮/美白/大眼/瘦脸）
- `src/web_server.cpp` — cpp-httplib HTTP API + 静态文件服务
- `src/cli/main.cpp` — CLI 入口
- `web/` — 前端（index.html + js/app.js + css/style.css）
- `configs/` — 证件照标准 JSON 配置
- `models/` — YuNet ONNX + LBF YAML 模型文件
- `testset/` — 按缺陷分类的测试图片（pass/, head_tilt/, eyes_closed/, ...）

### 前端 JS

`web/js/app.js` 是单文件应用，关键函数：
- `makeMessageCN()` — 根据 checker 结构化数据生成中文消息
- `displayResults()` — 渲染质量报告表格
- `CHECKER_CN` / `CATEGORY_CN` — 中文映射表
- `HARD_BLOCK_CHECKS` — 硬阻断 checker 名单

### 添加新 checker 的步骤

1. `src/quality/xxx_checker.cpp` — 继承 `IQualityChecker`，实现 `check()`，文件末尾加 `REGISTER_PLUGIN`
2. `CMakeLists.txt` — 在 `photo_quality` OBJECT library 中添加 .cpp
3. `configs/id_card_cn.json` — 在 `checks` 数组中添加名称
4. `web/js/app.js` — 在 `CHECKER_CN` 中添加中文名，在 `makeMessageCN()` 的 switch 中添加中文消息模板

## 最近变更

### 2026-06-18：稳健性优化

**1. Checker 执行顺序改为按配置文件**
- `src/cli/main.cpp:76` / `src/web_server.cpp:38`
- 之前：跑 PluginManager 的全量注册 checker（字母序），忽略配置
- 现在：按 `configs/id_card_cn.json` 的 `checks` 数组顺序执行
- 如果配置中写了未注册的 checker 名，输出 warning 而非静默跳过

**2. 美颜 effect 前置校验**
- `src/cli/main.cpp:164` / `src/web_server.cpp:303`
- 之前：先加载 YuNet/LBF 模型、跑人脸检测，才发现 effect 名无效
- 现在：先校验 effect 名称，未知 effect 立即返回 400 / 可用列表，节省模型加载开销

**3. border_check 从 FAIL 降为 WARNING**
- `src/quality/border_checker.cpp:106`
- 边缘阴影判 FAIL 过于严格，改为 WARNING 符合"非核心 checker 不硬阻断"规则

**4. 前端硬阻断名单同步**
- `web/js/app.js:253`
- `HARD_BLOCK_CHECKS` 移除 `border_check`
- 失败摘要改用后端返回的 `failed_checks` 真实计数，消除"0 项核心未达标但不合格"的 UI 矛盾

**5. face_slimming 方向修复（v3.2.1）**
- `src/beauty/face_slimming.cpp:228-229`
- remap 采样坐标从 `+ offset` 改为 `- offset`，瘦脸效果现在正确向内收缩而非向外扩张

**6. head_pose 抬头检测增强（v1.3.0）**
- `src/quality/head_pose_checker.cpp:12`
- 新增 2D 抬头判定：`pm >= 2.35 && eye_rel_y <= 0.42 && emr >= 0.55`
- 命中则直接 pitch_ok=false（硬阻断），不依赖 pitch 角度
- 避免 `pass/12-1.jpg` 这类高 pm 但不像抬头的正常样例被误杀
- detail 字段扩充：追加 eye_y、chin_eye、chin_up 标志位

**7. mouth_open 低 MAR 露齿检测（v1.4.0）**
- `src/quality/mouth_open_checker.cpp:10`
- 新增低 MAR 区间（0.045 < MAR < 0.13）的牙齿检测：V>180, S<85 判为露牙 FAIL
- 新增牙齿像素计数：口腔 ROI（landmarks 48-67 外扩）逐像素统计 V>175, S<85
- detail 字段扩充：oral_v、oral_s、teeth_px、teeth_ratio、low_mar_teeth 标志位
- 测试集硬缺陷漏判从 3/18 降至 1/18

**8. eye_closure 非对称单侧闭合检测（v1.2.0）**
- `src/quality/eye_closure_checker.cpp:9`
- 新增非对称规则：一侧眼区纹理极低 (`min_var < 320`) + 另一侧仍有有效纹理 (`max_var > 800`) → FAIL
- 捕捉 LBF landmark 把眼裂算错时的明显闭眼/单侧闭合（如 5-3.jpg EAR=0.66 仍被正确拦截）
- 彩色眼镜/粗框眼镜不会被误杀（双眼都低纹理时 max_var 不超 800，规则不触发）
- detail 字段扩充：left_var、right_var、asymmetric_closed 标志位
- 测试集硬缺陷漏判：**0/18**，pass/ 硬失败：0/11

**9. head_pose yaw 可靠性门控（v1.3.1）**
- `src/quality/head_pose_checker.cpp:38`
- solvePnP yaw 不可靠时（`yaw>90°` 且 nose_off≤0.20），仅信任鼻尖偏移比，避免极端角度误判

**10. CheckMode 双模式系统（RAW/FINAL）**
- `include/photo/result/check_mode.hpp` — 新文件，核心逻辑
- FINAL（默认）：成品严格验收，所有 checker 正常判定
- RAW：原片预检，5 个可后期修复的 checker（face_size、centering、head_margin、background_color、face_skin_tone）从 FAIL 降为 WARNING
- CLI：`--check-mode raw|final`，API：`?check_mode=raw|final`
- 前端：模式下拉框，结果按严重度排序，摘要栏区分"成品严检"/"原图预检"

**11. 自动化测试恢复**
- `tests/test_plugins.cpp` 补充 `<fstream>` 和 `face_analyzer.hpp` 头文件
- `PHOTO_BUILD_TESTS=ON` 重新配置后 9/10 通过（1 个因工作目录跳过）
- 运行方式：`cmake -B build -DPHOTO_BUILD_TESTS=ON && cmake --build build && cd build && ctest --output-on-failure`

### 闭眼检测判定策略（验收口径）

`eye_closure_checker` v1.2.0 采用三层判定：

**规则 1 — 双信号一致（纹理 + EAR）**
| 纹理方差 (min < 400) | EAR (< 0.20) | 结果 |
|:-:|:-:|:--|
| ✓ | ✓ | **FAIL** |
| ✓ | ✗ | **WARNING** |
| ✗ | ✓ | **WARNING** |
| ✗ | ✗ | PASS |

**规则 2 — 非对称单侧闭合（像素级）**
| 一侧纹理 (< 320) | 对侧纹理 (> 800) | 结果 |
|:-:|:-:|:--|
| ✓ | ✓ | **FAIL**（捕捉 LBF landmark 误算 EAR 时的闭眼） |

两条规则任一命中即 FAIL。规则 2 不触发彩色/粗框眼镜（双眼都低纹理 → max_var < 800）。

**设计原则**：误杀小眼/眼镜/低分辨率/landmark 漂移的代价 > 漏掉边缘闭眼样例。

**当前指标**：硬缺陷漏判 0/18，pass/ 硬失败 0/11。
