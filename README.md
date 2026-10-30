# VideoCut（Kanat 派生版）

VideoCut 是一个 Windows 原生的视频拼接与剪切辅助编辑器：FFmpeg 负责解码、裁剪与重混，Kanat 负责声明式界面、布局几何与控件事件。

本仓库以 Kanat 框架为基派生，把旧 VideoCut 的业务功能迁移到 Kanat 之上；旧 `VideoCut/` 目录只作参考源，不参与构建，也不作为修改基础。迁移范围、控件与事件映射、以及需要框架配合的阻塞项见 [docs/MIGRATION.md](docs/MIGRATION.md)。

## 当前状态

- 业务层已迁移：媒体扫描与分组、视频流解码与倍速、片段起止、重混导出、进度反馈。
- 框架层按确认补齐：渲染设备访问（G1）、时间片列表（G2）、可编辑时间条（G3）、双手柄范围滑块（G4）、窗口图标（G7）。
- 双击最大化与拖边调整窗口暂缓（G5 / G6），当前只记录日志或保持固定尺寸。
- 构建时由 vcpkg manifest 自动安装 FFmpeg；语法检查已通过，端到端运行验证随构建进行。
- 第 8 次提交（`980065c`）含用户人工修改的标题栏 / 关闭钮绘制（改用屏幕坐标、1 像素内缩、只圆上角）；该版本是后续开发基线，不得还原，详见 [docs/MIGRATION.md](docs/MIGRATION.md) 的「2026-10-25 人工修改（第六轮）」。

## 特性

- 文件管理：按文件名前缀分组，按末尾数字排序加载；
- 片段剪辑：起止时间精确到毫秒，支持添加多个剪辑段；
- 倍速预览：播放速度 1/4x 至 32x 可调；
- 硬件加速：支持 `h264_qsv`（需 Intel 核显及驱动）；
- 声明式布局：界面由 `VideoCut.json` 描述；编辑布局是基座窗口，准备布局是它的内嵌(flex)浮层，按需显隐；
- 内存管理：框架会话分配器 + 应用侧线性分配池。

## 目录结构

```text
VideoCut-Kanat/
├─ src/
│  ├─ main.cpp                  进程入口（调度初始化 / 业务初始化 / 主循环）
│  ├─ Identity.h                项目身份常量
│  ├─ app/                      业务层
│  │  ├─ AppState.*             业务状态、控件缓冲、每帧推进
│  │  ├─ AppEvents.*            事件绑定表与处理函数
│  │  ├─ AppBuffer.*            应用侧线性分配池
│  │  ├─ AppRender.*            渲染设备访问入口（当前受框架缺口阻塞）
│  │  ├─ VideoStream.*          视频流解码与重混
│  │  ├─ VideoEncoder.*         重编码与复用
│  │  ├─ StreamPipe.hpp         倍速采样管道
│  │  ├─ MediaOrganizer.*       文件分组
│  │  └─ MediaFileUtils.*       文件系统与文件夹选择
│  └─ framework/                Kanat 框架内核（未改动）
├─ docs/                        框架文档与迁移报告
├─ third_party/                 ImGui 1.92.8 / RapidJSON 1.1.0
├─ VideoCut.json                布局文档（构建后复制到 exe 同目录）
└─ VideoCut.vcxproj             构建工程
```

## 构建

环境要求：

| 项 | 要求 |
| --- | --- |
| 系统 | Windows 10 / 11 x64 |
| 编译器 | Visual Studio 2026（或 2022 17.10+），安装“使用 C++ 的桌面开发”工作负载 |
| 语言标准 | C++17 |
| 依赖 | Dear ImGui 1.92.8（随仓库）、RapidJSON 1.1.0（随仓库）、FFmpeg（vcpkg 静态库） |

FFmpeg 依赖：

```powershell
C:\OtherApp\vcpkg\vcpkg.exe install ffmpeg[x264,qsv]:x64-windows-static
```

构建命令：

```powershell
msbuild VideoCut.sln /p:Configuration=Release /p:Platform=x64 /m
```

产物在 `out\Release\VideoCut.exe`，构建后自动把 `VideoCut.json` 复制到产物目录。

## 运行

工作目录必须是可执行文件所在目录：

```powershell
cd out\Release
.\VideoCut.exe
```

运行参数走环境变量：

| 环境变量 | 作用 |
| --- | --- |
| `VIDEOCUT_DOC` | 覆盖布局文档文件名（缺省 `VideoCut.json`） |
| `VIDEOCUT_AUTOCLOSE_MS` | 启动后 N 毫秒自动退出，便于无人值守验证 |
| `VIDEOCUT_OPEN_DIR` | 启动后直接打开该目录，便于无人值守验证 |
| `VIDEOCUT_START_LAYOUT` | 缺省停留编辑界面；设为 `prepare` 时启动即显示准备浮层（无人值守验证用） |

操作顺序：选择视频文件夹 → 打开文件夹 → 选择视频分组 → 设置起止毫秒 → 添加片段 → 播放预览 / 设置倍速 → 执行重混。

## 已知限制

- 仅支持 Windows 与 MSVC；渲染后端为 ImGui + D3D11。
- 导入视频需保持相同的分辨率与帧率。
- 音频输出固定为 AAC 48kHz 立体声。
- QSV 硬件加速需要 Intel 核显与驱动。
- 双击最大化与拖边调整窗口暂缓，见 [docs/MIGRATION.md](docs/MIGRATION.md) 的 G5 / G6。

## 许可

本仓库业务代码沿用旧 VideoCut 的 MIT License；框架层为 Kanat 的 MIT License。

FFmpeg 的静态链接可能使最终二进制受 LGPL/GPL 约束，分发前请阅读 [FFmpeg 许可证说明](https://ffmpeg.org/legal.html)。

## 致谢

- Kanat（暗渠框架）— 声明式界面与布局内核
- FFmpeg — 多媒体处理
- Dear ImGui — 即时模式 GUI
- RapidJSON — JSON 解析
