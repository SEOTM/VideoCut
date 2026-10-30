# AppRender 模块 (src\app\AppRender.h)

## 概述

业务侧对渲染设施的访问入口。视频帧需要上传到与 ImGui 同一 D3D11 设备的纹理，再由 `CTRL_TYPE_RENDER` 控件显示。

当前 Kanat 框架未向业务层暴露宿主设备与设备上下文，本模块的取设备入口按阻塞口径返回空指针；视频解码、时长与重混不依赖纹理，仍可用。框架缺口与建议见 `docs\MIGRATION.md` 的 G1。

### 函数

| 函数 | 说明 |
| --- | --- |
| `GetDevice` | 取渲染宿主的 D3D11 设备（当前恒返回空）。 |
| `GetDeviceContext` | 取渲染宿主的 D3D11 设备上下文（当前恒返回空）。 |

---

## GetDevice 函数

### 语法

```cpp
ID3D11Device* GetDevice();
```

### 返回值

`ID3D11Device*`：当前框架未提供访问入口，恒返回 `NULL`。

### 注解

框架补齐能力表入口后，本函数应改为返回宿主设备；调用方已按可空处理（`VideoStream` 在设备为空时跳过纹理创建）。

---

## GetDeviceContext 函数

### 语法

```cpp
ID3D11DeviceContext* GetDeviceContext();
```

### 返回值

`ID3D11DeviceContext*`：当前恒返回 `NULL`。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `AppRender.h`(`src\app\AppRender.h`) |
| 实现 | `AppRender.cpp` |
| 命名空间 | `AppRender` |
| 依赖 | `d3d11.h` |
| 平台 | Windows |
| 线程安全 | 是（只读入口，无状态） |
