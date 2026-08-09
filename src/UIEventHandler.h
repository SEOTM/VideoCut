#pragma once
#include "FuncPtr.h"
// ================================
// 类名: UIEventHandler
// 描述: UI事件处理类，静态类
// ================================
class UIEventHandler
{
private:
    UIEventHandler() = delete;
    ~UIEventHandler() = delete;
    UIEventHandler(const UIEventHandler&) = delete;
    UIEventHandler& operator=(const UIEventHandler&) = delete;

public:
    // 函数: BindEventHandler
    // 描述: 绑定事件处理函数
    // 输入:
    //    LPCSTR eventname - 事件名称
    // 返回值:
    //    EventInfo - 返回事件处理信息
    static EventInfo BindEventHandler(LPCSTR eventname);
};

