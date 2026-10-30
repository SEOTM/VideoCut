# 第三方依赖

依赖以头文件与静态库形式直接随仓库提供，由 `Kanat.vcxproj` 引用；不需要 NuGet、CMake 或网络。

## 目录

```text
third_party/
├─ imgui/                       Dear ImGui 1.92.8 头文件
│  ├─ imgui.h
│  ├─ imgui_internal.h
│  ├─ imconfig.h
│  ├─ imgui_impl_win32.h
│  ├─ imgui_impl_dx11.h
│  ├─ imgui_stdlib.h
│  └─ imstb_*.h
├─ rapidjson/                   RapidJSON 1.1.0 头文件（含 internal/ 与 error/）
├─ lib/
│  ├─ debug/imguid.lib          Debug 静态库（x64-windows-static，/MTd）
│  └─ release/imgui.lib         Release 静态库（x64-windows-static，/MT）
├─ THIRD-PARTY-NOTICES.txt      第三方许可声明
└─ README.md                    本文件
```

| 组件 | 版本 | 形式 | 用途 |
| :--- | :--- | :--- | :--- |
| Dear ImGui | 1.92.8 | 预编译静态库 + 头文件 | 控件绘制、Win32 / DX11 后端 |
| RapidJSON | 1.1.0 | 头文件 | 布局文档 JSON 解析 |

工程中的头文件搜索路径为 `src`、`third_party/imgui`、`third_party`；Debug 链接 `lib/debug/imguid.lib`，Release 链接 `lib/release/imgui.lib`。

## 许可

Dear ImGui 与 RapidJSON 均以 MIT License 分发。

```text
MIT License

Copyright (c) 2014-2025 Omar Cornut
Copyright (C) 2015 THL A29 Limited, a Tencent company, and Milo Yip.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 更新依赖

1. 替换 `third_party/imgui/` 与 `third_party/rapidjson/` 下的头文件；
2. 替换 `third_party/lib/debug/imguid.lib` 与 `third_party/lib/release/imgui.lib`，保持目录名不变；
3. 删除 `out/` 后重新生成解决方案；
4. 更新本文件的版本表、`THIRD-PARTY-NOTICES.txt` 与许可声明。
