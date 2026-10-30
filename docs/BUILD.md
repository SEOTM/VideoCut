# 构建与运行

## 1. 仓库内容

```text
VideoCut/
├─ src/
│  ├─ main.cpp                  进程入口
│  ├─ Identity.h                项目身份常量
│  ├─ app/                      业务层（VideoCut 功能）
│  └─ framework/                Kanat 框架内核
├─ docs/                        架构、构建、布局格式等文档
├─ third_party/                 ImGui 1.92.8 / RapidJSON 1.1.0
├─ VideoCut.json                布局文档
├─ VideoCut.vcxproj             构建工程
└─ vcpkg.json                   依赖清单
```

## 2. 环境要求

| 项 | 要求 |
| --- | --- |
| 系统 | Windows 10 / 11 x64 |
| Visual Studio | 2022（17.10 或更高）或 2026；安装“使用 C++ 的桌面开发”工作负载 |
| 平台 | x64 |
| 语言标准 | C++17 |
| 第三方依赖 | Dear ImGui 1.92.8（Win32 + DX11 后端）、RapidJSON 1.1.0、FFmpeg（含 `avcodec` / `avformat` / `avutil` / `swscale` / `swresample`） |

ImGui 与 RapidJSON 以头文件与静态库形式随 `third_party/` 提供；FFmpeg 由 vcpkg 安装。

## 3. 依赖自动安装

工程使用 vcpkg manifest 模式，`vcpkg.json` 声明 `ffmpeg[nvcodec,qsv,x264]`，`VideoCut.vcxproj` 导入 vcpkg 的 MSBuild 集成。构建时自动执行安装，无需手动运行 `vcpkg install`：

- 安装根：`$(ProjectDir)vcpkg_installed\x64-windows-static`；
- 三元组：`x64-windows-static`（静态 CRT，与工程 `/MT` 一致）；
- registry 基线固定在 `vcpkg-configuration.json`；
- 首次构建会下载并编译 FFmpeg，耗时较长；后续构建复用已安装结果。

vcpkg 根目录取环境变量 `VCPKG_ROOT`，未设置时缺省 `C:\OtherApp\vcpkg`。

### 离线构建（无外网 / 已有 FFmpeg 安装）

manifest 自动安装需要访问 GitHub registry；本机无外网时会停在 `Fetching registry information`。
此时可关闭 manifest 安装，并把一份已装好的 `x64-windows-static` 安装树作为安装根：

```powershell
msbuild VideoCut.sln /p:Configuration=Debug /p:Platform=x64 `
  /p:VcpkgEnableManifest=false /p:VcpkgManifestInstall=false `
  /p:VcpkgInstalledDir=<安装根>
```

`<安装根>` 之下应有以三元组命名的目录（如 `x64-windows-static\include`、`x64-windows-static\lib`）。
该做法只改构建参数、不改工程配置；默认构建路径仍是 manifest 自动安装。构建时 MSBuild 会提示
“vcpkg manifest 已禁用但发现清单文件”，属预期提示。

## 4. 构建

```powershell
msbuild VideoCut.sln /p:Configuration=Release /p:Platform=x64 /m
```

- 产物：`out\Release\VideoCut.exe`
- 构建后自动复制 `VideoCut.json` 到产物目录
- 运行库：Release `/MT`、Debug `/MTd`，与 `third_party` 中的 ImGui 静态库一致
- 编译选项：`/utf-8 /Zc:char8_t-`、`UNICODE`、`_AMD64_`

Debug 构建：

```powershell
msbuild VideoCut.sln /p:Configuration=Debug /p:Platform=x64 /m
```

## 5. 运行

工作目录必须是可执行文件所在目录：

```powershell
cd out\Release
.\VideoCut.exe
```

加载链：`VideoCut.bin` 优先，其次 `VideoCut.json`。

| 环境变量 | 作用 |
| --- | --- |
| `VIDEOCUT_DOC` | 覆盖布局文档文件名（缺省 `VideoCut.json`） |
| `VIDEOCUT_AUTOCLOSE_MS` | 启动后 N 毫秒自动退出；不设则常驻 |
| `VIDEOCUT_OPEN_DIR` | 启动后直接打开该目录（UTF-8），用于无人值守验证 |
| `VIDEOCUT_START_LAYOUT` | 缺省停留编辑界面；设为 `prepare` 时启动即显示准备浮层，用于界面验证 |

## 6. 布局文档

`VideoCut.json` 包含两份布局（槽序即声明顺序）：

- `Editor`（槽 0，基座）：视频分组、预览面、起止时间条（可编辑时码）、预览滑块、片段列表、播放、倍速、进度、重混、重设；承载唯一的应用窗口。
- `Prepare`（槽 1，内嵌(flex)成员）：源路径输入、浏览、打开；在 `Editor` 窗口内叠加显示，按可见性显隐，没有独立窗口。

内嵌关系由业务侧声明（`AppState::Init` 调 `Dispatch::SetLayoutNested`），布局文档本身不携带该关系；首枚布局决定窗口几何与建面字号。

字段与控件类型见 [LAYOUT-FORMAT.md](LAYOUT-FORMAT.md)；业务事件名与控件缓冲的对应关系见 [AppEvents.md](../src/app/AppEvents.md)。

## 7. 排障

| 现象 | 原因与处置 |
| --- | --- |
| 找不到 `libavcodec/avcodec.h` | FFmpeg 未安装，或 `VCPKG_ROOT` 未设置 |
| 链接报 `LNK2019`（`av_*`） | 未链接 FFmpeg 静态库，或运行库（`/MT`）不一致 |
| 启动弹窗“应用初始化失败” | 工作目录不对，或 `VideoCut.json` 不在 exe 同目录 |
| 修改 JSON 后界面没变 | 同目录存在旧的 `VideoCut.bin`，加载链优先 bin |
| 双击标题栏无最大化 | 运行期布局重算尚未实现，双击只记日志 |
| 窗口不能拖边调整 | 宿主窗口为无边框窗口，没有调整边框与 hit test |
