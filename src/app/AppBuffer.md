# AppBuffer 模块 (src\app\AppBuffer.h)

## 概述

应用侧线性分配池。框架的小对象池只服务不超过 4096 字节的块；媒体路径、文件节点表这类大块由本模块按页申请、按 16 字节对齐切分。池内对象不单独释放，池析构时一次性归还。

本模块属 **应用层**，不依赖框架的分配器，也不向框架注册。

### 函数

| 函数 | 说明 |
| --- | --- |
| `AppBuffer` | 按请求字节数建立线性分配池。 |
| `~AppBuffer` | 归还整块池内存。 |
| `Allocate` | 从池中切分一段 16 字节对齐的内存。 |

---

## AppBuffer 构造函数

### 语法

```cpp
AppBuffer(size_t qwSize);
```

### 参数

`[in] qwSize`

请求字节数。为 `0` 时不申请内存，池保持空态；不足一页按一页申请。

### 注解

底层使用 `VirtualAlloc`（`MEM_COMMIT | MEM_RESERVE`，`PAGE_READWRITE`）。申请失败时池为空，后续 `Allocate` 恒返回空。

---

## Allocate 函数

### 语法

```cpp
BufferInfo Allocate(size_t qwSize);
```

### 参数

`[in] qwSize`

请求字节数。

### 返回值

`BufferInfo`：`buffer` 非空表示切分成功，`size` 为请求字节数；池为空或余量不足时 `buffer` 为 `NULL`、`size` 为 `0`。

### 注解

实际占用按 16 字节对齐；对齐后的占用超过余量即失败，不进行扩容。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `AppBuffer.h`(`src\app\AppBuffer.h`) |
| 实现 | `AppBuffer.cpp` |
| 命名空间 | 全局作用域 |
| 依赖 | `framework/base/mem/MemoryManager.h`(`BufferInfo`) · `kernel32.dll` |
| 平台 | Windows |
| 线程安全 | 否（单线程使用；调用方自行保证） |
