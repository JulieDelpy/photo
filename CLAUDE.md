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

只有 12 个核心 checker 可以使用 `FAIL`：
- `face_detect_check` / `face_count_check` / `face_confidence_check`
- `eye_closure_check` / `mouth_open_check` / `head_pose_check`
- `head_margin_check` / `centering_check`
- `face_size_check` / `face_ratio_check`
- `background_color_check` / `overexposure_check`

其余 29 个 checker 全部使用 `WARNING`。

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
