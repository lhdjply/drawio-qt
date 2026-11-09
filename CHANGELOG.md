# 更新日志

本项目的重要变更都记在这里。格式参考
[Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循[语义化版本](https://semver.org/lang/zh-CN/)。
版本号的唯一来源是仓库根目录的 `VERSION` 文件（写法与 tag 一致，如 `v0.1.0`）。

## [v0.1.2] - 2026-11-09

### 新增

- 图层管理器：新建、删除、重命名、上下移动、显示/隐藏、锁定图层；
  每个图层下展示对象列表，画布与面板选中双向同步，对象支持层内上下调整。
- 连线控制点：选中连线后显示每段中点，可拖动整段平移；waypoint 与连线
  序列化修正后可在 draw.io 中正常打开。

### 修复

- 连线终点按 `entryX/entryY` 保持连接边，穿越形状的线段与箭头正确显示。
- 按文档顺序绘制，形状置于顶层后可以正确遮挡连线。
- 图层与所属对象按 draw.io 顺序写出，图层锁定/可见状态完整保留。
- 关闭最后一个文件标签页不再崩溃。

### 变更

- `.drawio` 统一保存为**未压缩、带缩进**的 `mxGraphModel`，方便阅读与 diff。
- `mxfile` 不再写入 `agent` / `version` 属性。

## [v0.1.1] - 2026-11-07

修复 Windows 下的编译问题，CI 全平台（Linux 四架构 + Windows x64/arm64）可正常出包。

### 修复

- 关闭 `CMAKE_INCLUDE_CURRENT_DIR`：它把源码根目录加进头文件搜索路径，
  Windows 文件系统大小写不敏感，Qt 头文件链中的 `#include <version>` 会命中
  根目录的 `VERSION` 文件（内容 `v0.1.0`）被当 C++ 编译，报
  "too many decimal points in number"。include 路径改由
  `target_include_directories` 显式给出。
- CI 的 Windows arm64 作业（MSYS2 CLANGARM64）只有 clang 没有 g++，
  打印工具链版本的步骤按矩阵编译器取名字，不再写死 `g++`。

## [v0.1.0] - 2026-11-07

首个公开发布版本：draw.io 桌面版（Electron）的原生 Qt6 / C++ 移植，
不使用 Electron、Node.js，也不嵌入 Qt WebEngine。

### 新增

- 完整的图表编辑器：mxGraph 兼容数据模型，打开与保存 `.drawio` 文件，
  文件格式与原版 draw.io 互通。
- 多文件标签页：同时打开多个文件，标签可关闭；多页面管理、撤销/重做。
- 编辑功能：选择、移动、缩放、旋转（45° 倍数磁吸 + 实时角度显示）、
  组合/取消组合、对齐排列、网格与参考线吸附。
- 连线：悬停吸附点、正交路由、可拖拽途经点，箭头样式覆盖 draw.io
  全部 marker 并改为图标选择。
- 文字编辑：双击/F2 编辑标签，Alt+Enter 换行。
- 形状库：按 draw.io 上游补充通用基本形状、箭头、云、高级布局、
  数据库/网络/流程图等分类；图片库 1900+ 个 SVG/PNG 图标零代码接入，
  分类与图标名称中文化，可勾选启停。
- 导出：PNG/SVG/PDF/HTML/XML，支持页面设置（A4/A5 等纸张预设、
  方向、网格）、打印与打印预览。
- 界面：深浅色主题跟随系统、缩放与状态栏、草稿自动恢复、最近文件。
- 命令行导出：无界面环境（服务器/CI）可直接调用 `drawio-qt -x` 导出。

### 修复

- 页面设置对话框按当前页面尺寸回选纸张预设，不再始终显示 A4。
- 旋转角度写入 style 的 `rotation=…`，与原版 draw.io 兼容；松开鼠标
  立即固定角度。

### 打包与发布

- Debian 打包（四架构 CI 发布）、Windows x64/arm64 便携包与 Inno Setup
  安装包、GitHub Actions 完整流水线。
- 内置检查更新：读取 Release 附带的更新清单，走 GitHub 转发镜像，
  发现新版本展示更新日志并跳转发布页。
