# VideoCut

---

一个基于 FFmpeg + Kanat 框架 + D3D11 的 Windows 原生视频剪辑工具，支持按文件名前缀批量分组、毫秒级片段选取、倍速预览与 Intel QSV 硬件加速编码，适合快速预处理批量视频素材。

## 界面主题

应用提供亮色与暗色两套主题，启动时根据系统主题自动选择。界面分两层：编辑界面是基座窗口，准备界面作为内嵌浮层按需显隐。

## 特性

- **文件管理**：自动按文件名前缀分组，按末尾数字排序加载；
- **片段剪辑**：时间条与双手柄范围滑块精确选取起止点，也可直接输入毫秒，支持添加/删除多个剪辑段；
- **倍速预览**：播放速度 1/4× ~ 32× 可调，实时解码显示；
- **硬件加速**：优先使用 `h264_qsv`（需 Intel 核显及驱动支持），不可用时回退 `libx264`；
- **声明式布局**：界面由 `VideoCut.json` 描述，布局几何与控件规格分离，自动适配窗口尺寸；
- **内存管理**：框架会话分配器 + 应用侧线性分配池。

## 功能概览

1. **准备**：选择视频文件夹，自动扫描常见视频格式（`.mpg`、`.mpeg`、`.mp4`、`.avi`、`.mkv`、`.wmv`、`.ts`）
2. **编辑**：
   - 下拉框切换视频分组
   - 时间条 + 双手柄滑块精确定位起止点，或在输入框直接填毫秒
   - 添加剪辑片段到列表，可删除或重新选中
   - 实时预览（播放/暂停）
   - 设置导出倍速（1/4× ~ 32×）
3. **导出重混**：自动合并重叠片段，输出 `<源目录>\<分组名>[remix].mp4`

## 目录结构

```text
VideoCut/
├─ src/
│  ├─ main.cpp                  进程入口（调度初始化 / 业务初始化 / 主循环）
│  ├─ Identity.h                项目身份与环境变量常量
│  ├─ app/                      业务层
│  │  ├─ AppState.*             业务状态、控件缓冲、每帧推进
│  │  ├─ AppEvents.*            事件绑定表与处理函数
│  │  ├─ AppBuffer.*            应用侧线性分配池
│  │  ├─ AppRender.*            渲染设备访问入口
│  │  ├─ VideoStream.*          视频流解码与重混
│  │  ├─ VideoEncoder.*         重编码与复用
│  │  ├─ StreamPipe.hpp         倍速采样管道
│  │  ├─ MediaOrganizer.*       文件分组
│  │  └─ MediaFileUtils.*       文件系统与文件夹选择
│  └─ framework/                界面、布局与渲染内核
├─ docs/                        架构、构建、布局格式等文档
├─ third_party/                 Dear ImGui 1.92.8 / RapidJSON 1.1.0
├─ tools/json2bin.py            布局文档转换工具
├─ VideoCut.json                布局文档（构建后复制到 exe 同目录）
├─ VideoCut.sln                 解决方案
├─ VideoCut.vcxproj             构建工程
└─ vcpkg.json                   依赖清单
```

注：由于 FFmpeg 许可限制，本仓库不提供任何形式的可执行文件、二进制产物。

## 依赖库

| 库 | 用途 | 获取方式 |
|---|---|---|
| [FFmpeg](https://ffmpeg.org/) | 视频解码/编码/复用 | vcpkg manifest 自动安装 `ffmpeg[nvcodec,qsv,x264]:x64-windows-static` |
| [Dear ImGui 1.92.8](https://github.com/ocornut/imgui) | 即时模式 GUI | 随仓库 `third_party/imgui` |
| [RapidJSON 1.1.0](https://rapidjson.org/) | JSON 解析 | 随仓库 `third_party/rapidjson` |
| Windows SDK | D3D11、Media Foundation、COM | Visual Studio 自带 |

### 关于 FFmpeg 的重要说明

静态链接 FFmpeg 或使用 libx264 编码器会触发 **LGPL/GPL 许可证的传染性条款**，要求**公开应用程序的源代码**并**允许用户重新链接/替换 FFmpeg 组件**。

本仓库已公开完整源代码以满足上述要求。如你计划分发二进制文件，请：

1. 确保遵守 FFmpeg 的许可证要求（提供完整源码、构建脚本、允许用户重新链接）；
2. 或改用 FFmpeg 的 DLL 动态链接方式，或放弃 libx264 编码器（LGPL 允许动态链接而不传染）。

**本项目不提供预编译的二进制文件**，需要用户自行构建。

## 构建指南

### 环境要求

| 项 | 要求 |
|---|---|
| 系统 | Windows 10 / 11 x64 |
| 编译器 | Visual Studio 2026（或 2022 17.10+），安装“使用 C++ 的桌面开发”工作负载 |
| 语言标准 | C++17 |
| 运行时库 | `/MT`（Release）/ `/MTd`（Debug），静态链接 |
| 依赖管理 | vcpkg manifest（工程已开启 `VcpkgEnableManifest`） |

### 使用 vcpkg + Visual Studio

1. 安装 [vcpkg](https://vcpkg.com/)，并设置环境变量 `VCPKG_ROOT`；
2. 在项目根目录执行 `vcpkg integrate install`，或在 Visual Studio 中启用 vcpkg 集成；
3. 构建时由 `vcpkg.json` 自动安装 `ffmpeg[nvcodec,qsv,x264]:x64-windows-static`；
4. 生成解决方案并构建：

```powershell
msbuild VideoCut.sln /p:Configuration=Release /p:Platform=x64 /m
```

产物在 `out\Release\VideoCut.exe`，构建后自动把 `VideoCut.json` 复制到产物目录。

### 手动构建（不使用 vcpkg）

1. **FFmpeg**：自行编译静态库（参考 [FFmpeg 官方文档](https://trac.ffmpeg.org/wiki/CompilationGuide)），启用 `--enable-static --disable-shared` 与 `--enable-libx264 --enable-qsv`；
2. **Dear ImGui**：使用仓库内 `third_party/imgui`，无需额外获取；
3. **RapidJSON**：使用仓库内 `third_party/rapidjson`，无需额外获取。

## 使用说明

1. 启动程序，点击「浏览文件夹」选择视频目录；
2. 点击「打开文件夹」扫描视频，进入编辑器界面；
3. 在下拉框中选择视频分组（按文件名前缀分组）；
4. 使用时间条或双手柄滑块调整剪辑起止点，点击「添加片段」；
5. 在片段列表中可删除不需要的条目；
6. 设置播放倍速（1/4× ~ 32×），点击「播放预览」查看效果；
7. 点击「执行重混」导出合并视频（输出到源目录，文件名为 `<分组名>[remix].mp4`）。

> **导出耗时**：取决于视频长度和编码设置，Intel QSV 可显著加速；如使用 NVIDIA，可在 `VideoEncoder` 中接入 `h264_nvenc`。

## 运行参数

程序不解析命令行参数，运行参数通过环境变量传入：

| 环境变量 | 作用 |
| --- | --- |
| `VIDEOCUT_DOC` | 覆盖布局文档文件名（缺省 `VideoCut.json`） |
| `VIDEOCUT_AUTOCLOSE_MS` | 启动后 N 毫秒自动退出，便于无人值守验证 |
| `VIDEOCUT_OPEN_DIR` | 启动后直接打开该目录，便于无人值守验证 |
| `VIDEOCUT_START_LAYOUT` | 缺省停留编辑界面；设为 `prepare` 时启动即显示准备浮层 |

工作目录必须是可执行文件所在目录：

```powershell
cd out\Release
.\VideoCut.exe
```

## 已知限制

- 仅支持 Windows 平台（依赖 D3D11、Win32 API）与 MSVC；
- 所有导入视频需保持**相同的分辨率与帧率**（否则加载失败）；
- 音频输出格式固定为 AAC 48kHz 立体声；
- QSV 硬件加速需要 Intel 核显及正确安装的驱动程序；
- 窗口双击最大化与拖边缩放尚未启用。

## 许可证

[MIT License](LICENSE) — 自由使用、修改、分发。

**但请注意**：MIT 许可证仅适用于本仓库的源代码。FFmpeg 的静态链接可能使最终二进制文件受到 LGPL/GPL 约束，请在使用和分发前仔细阅读 [FFmpeg 许可证说明](https://ffmpeg.org/legal.html)。

## 致谢

- [Kanat](https://github.com/SEOTM/Kanat) — 声明式界面、布局几何与控件事件框架
- [FFmpeg](https://ffmpeg.org/) — 多媒体处理核心
- [Dear ImGui](https://github.com/ocornut/imgui) — 简洁高效的 GUI 库
- [RapidJSON](https://rapidjson.org/) — 快速 JSON 解析
