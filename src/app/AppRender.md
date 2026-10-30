# AppRender 模块 (src\app\AppRender.h)

## 概述

业务侧对渲染设施的访问入口。视频帧需要上传到与 ImGui 同一 D3D11 设备的纹理，再由 `CTRL_TYPE_RENDER` 控件显示。设备与设备上下文来自框架宿主能力表（`HostApi`），业务层不直接依赖具体宿主实现。

### 函数

| 函数 | 说明 |
| --- | --- |
| `SetHost` | 登记宿主能力表；须在取设备之前调用，传 `NULL` 解除登记。 |
| `GetDevice` | 取宿主使用的 D3D11 设备；未登记或能力缺失时返回 `NULL`。 |
| `GetDeviceContext` | 取宿主使用的 D3D11 设备上下文；未登记或能力缺失时返回 `NULL`。 |

---

## SetHost 函数

### 语法

```cpp
VOID SetHost(CONST HostApi* pHost);
```

### 参数

| 参数 | 说明 |
| --- | --- |
| `pHost` | 宿主能力表；传 `NULL` 解除登记。 |

### 注解

由 `main` 在初始化阶段登记框架提供的宿主能力表；业务层只读取表中的函数指针，不持有其所有权。

---

## GetDevice 函数

### 语法

```cpp
ID3D11Device* GetDevice();
```

### 返回值

`ID3D11Device*`：`SetHost` 未登记，或宿主未提供 `GetNativeDevice` 时返回 `NULL`；否则返回宿主设备。

### 注解

调用方按可空处理；`VideoStream` 在设备为空时跳过纹理创建，解码、时长与重混仍可用。

---

## GetDeviceContext 函数

### 语法

```cpp
ID3D11DeviceContext* GetDeviceContext();
```

### 返回值

`ID3D11DeviceContext*`：`SetHost` 未登记，或宿主未提供 `GetNativeDeviceContext` 时返回 `NULL`；否则返回宿主设备上下文。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `AppRender.h`(`src\app\AppRender.h`) |
| 实现 | `AppRender.cpp` |
| 命名空间 | `AppRender` |
| 依赖 | `d3d11.h`、`HostApi`（`framework/render/Render.h`） |
| 平台 | Windows |
| 线程安全 | 是（只读入口，无状态） |
