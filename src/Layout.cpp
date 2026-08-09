#pragma once
#include <new>
#include "Layout.h"


// ================================
// 类名: Layout
// 描述: 布局类
// ================================

// ========================
// 静态成员变量初始化
// ========================
SecureBuffer* Layout::secbuf = NULL;
Layout* Layout::NULLOBJ = NULL;
CtrlInfo Layout::NULLINFO = {};

// 函数: 构造函数
// 描述: 初始化成员变量
Layout::Layout(size_t num) :
	pCtrlInfos(nullptr),
	qwNumCtrl(0)
{
	if (secbuf)
	{
		// 初始化控件信息数组
		qwNumCtrl = num;
		// 分配控件信息数组
		BufferInfo bufInfo = secbuf->Allocate(sizeof(CtrlInfo) * num);
		pCtrlInfos = static_cast<CtrlInfo*>(bufInfo.buffer);
		// 初始化控件信息数组
		for (size_t i = 0; i < num; ++i)
		{
			new (&pCtrlInfos[i]) CtrlInfo(); // 使用定位 new 调用构造函数
		}
		// 注意: 不需要手动释放 pCtrlInfos，因为它是通过 secbuf 分配的
	}


}
// 函数: NULL 对象构造函数
// 描述: NULL
Layout::Layout()
{
	pCtrlInfos = NULL;
	qwNumCtrl = 0;
}
// 函数: SetSecBuffer
// 描述: 设置安全缓冲区对象
VOID Layout::SetSecBuffer(SecureBuffer* buffer)
{
	secbuf = buffer;
}
// 函数: CreateLayout
// 描述: 静态工厂函数，创建布局对象
Layout* Layout::CreateLayout(size_t num)
{
	if (secbuf)
	{
		// 使用 SecureBuffer 分配内存
		BufferInfo bufInfo = secbuf->Allocate(sizeof(Layout));
		if (bufInfo.buffer == nullptr || bufInfo.size < sizeof(Layout))
		{
			return nullptr; // 分配失败
		}
		// 使用定位 new 创建 Layout 对象
		Layout* layout = new (bufInfo.buffer) Layout(num);
		return layout;
	}
}
// 函数: GetNullObject
// 描述: 获取空引用
Layout& Layout::GetNullObject()
{
	static Layout nullobj;
	NULLOBJ = &nullobj;
	return nullobj;
}
// 函数: DestroyLayout
// 功能: 静态销毁函数，用于释放对象
VOID Layout::DestroyLayout(Layout* layout)
{
	if (layout)
	{
		layout->~Layout();
	}
	return;
}

// 函数: 析构函数
// 描述: 释放布局对象
Layout::~Layout()
{
	// 调用控件信息数组中每个控件的析构函数
	for (size_t i = 0; i < qwNumCtrl; ++i)
	{
		pCtrlInfos[i].~CtrlInfo();
	}
	// 注意: 不需要手动释放 pCtrlInfos，因为它是通过 secbuf 分配的
}

// 函数: SetCtrlInfo
// 描述: 设置指定索引的控件信息
BOOL Layout::SetCtrlInfo(size_t index, const CtrlInfo& info)
{
	BOOL ret = FALSE;
	if (index < qwNumCtrl)
	{
		pCtrlInfos[index] = info;
		ret = TRUE;
	}
	return ret;
}

// 函数: GetCtrlInfo
// 描述: 获取指定索引的控件信息
CtrlInfo* Layout::GetCtrlInfo(size_t index)
{
	CtrlInfo* ret = nullptr;
	if (index < qwNumCtrl)
	{
		ret = &pCtrlInfos[index];
	}
	return ret;
}

// 函数: GetNumCtrl
// 描述: 获取控件数量
size_t Layout::GetTotalNum() const
{
	return qwNumCtrl;
}
// 函数: GetCtrlInfoRef
// 描述: 获取指定索引的控件信息引用
CtrlInfo& Layout::GetCtrlInfoRef(size_t index)
{
	if (index < qwNumCtrl)
	{
		return pCtrlInfos[index];
	}
	else
	{
		return NULLINFO;
	}
}
// 函数: GetWindow
// 描述: 获取窗体控件
CtrlInfo* Layout::GetWindow()
{
	CtrlInfo* ret = nullptr;
	if (qwNumCtrl > 0 && pCtrlInfos[0].type == CTRL_TYPE_WND)
	{
		ret = &pCtrlInfos[0];
	}
	return ret;
}
// 函数: GetTitle
// 描述: 获取窗体控件
CtrlInfo* Layout::GetTitle()
{
	CtrlInfo* ret = nullptr;
	if (qwNumCtrl > 1 && pCtrlInfos[1].type == CTRL_TYPE_TITLE)
	{
		ret = &pCtrlInfos[1];
	}
	return ret;
}
// 函数: GetContainer
// 函数: 获取客户区容器
CtrlInfo* Layout::GetContainer()
{
	CtrlInfo* ret = nullptr;
	if (qwNumCtrl > 1 && pCtrlInfos[1].type == CTRL_TYPE_CNTR)
	{
		ret = &pCtrlInfos[1];
	}
	if (qwNumCtrl > 2 && pCtrlInfos[2].type == CTRL_TYPE_CNTR)
	{
		ret = &pCtrlInfos[2];
	}
	return ret;
}
// 函数: GetShellNum
// 描述: 获取非客户区控件数量
size_t Layout::GetShellNum()
{
	size_t ret = 0;
	if (this->GetWindow())
	{
		ret++;
	}
	if (this->GetTitle())
	{
		ret++;
	}
	if (this->GetContainer())
	{
		ret++;
	}
	return ret;
}
// 函数: IsNull
// 描述: 是否为无效引用
BOOL Layout::IsInvalid()
{
	BOOL ret = FALSE;
	if (this == NULLOBJ)
	{
		ret = TRUE;
	}
	else if (!(this->GetWindow() && this->GetContainer()))
	{
		ret = TRUE;
	}
	else if (this->GetWindow()->size == vec2{ 0,0 })
	{
		ret = TRUE;
	}
	return ret;
}
// 函数: GetTypeEnum
// 描述: 获取控件类型枚举
CtrlType CtrlInfo::GetTypeEnum(LPCSTR type)
{
	// 我们找到控件类型枚举如下：
	// 枚举：控件类型
	// 描述：定义各个控件的类型
	//enum CtrlType
	//{
	//	CTRL_TYPE_INVALID = 0,  // 无效类型
	//	CTRL_TYPE_WND = 1,		// 窗体类型
	//	CTRL_TYPE_TITLE,		// 标题栏类型
	//	CTRL_TYPE_CNTR,			// 容器类型
	//	CTRL_TYPE_CHILD,		// 子容器
	//	CTRL_TYPE_TEXT,			// 文本标签
	//	CTRL_TYPE_INPUT_SINGLE, // 单行输入框
	//	CTRL_TYPE_INPUT_MULTI,  // 多行输入框
	//	CTRL_TYPE_BUTTON,       // 按钮
	//	CTRL_TYPE_LIST,			// 列表
	//	CTRL_TYPE_TIMEBAR,		// 时间控件
	//	CTRL_TYPE_RENDER,		// 呈现器
	//	CTRL_TYPE_SLIDER,		// 滑动条
	//	CTRL_TYPE_COMBO,		// 组合框
	//	CTRL_TYPE_PROG,			// 进度显示
	//	CTRL_TYPE_INPUT_REAL,	// 实数输入框
	//};
	CtrlType ret = CTRL_TYPE_INVALID;
	if (type)
	{
		if (strcmp(type, "CTRL_TYPE_WND") == 0)
		{
			ret = CTRL_TYPE_WND;
		}
		else if (strcmp(type, "CTRL_TYPE_TITLE") == 0)
		{
			ret = CTRL_TYPE_TITLE;
		}
		else if (strcmp(type, "CTRL_TYPE_CNTR") == 0)
		{
			ret = CTRL_TYPE_CNTR;
		}
		else if (strcmp(type, "CTRL_TYPE_CHILD") == 0)
		{
			ret = CTRL_TYPE_CHILD;
		}
		else if (strcmp(type, "CTRL_TYPE_TEXT") == 0)
		{
			ret = CTRL_TYPE_TEXT;
		}
		else if (strcmp(type, "CTRL_TYPE_INPUT_SINGLE") == 0)
		{
			ret = CTRL_TYPE_INPUT_SINGLE;
		}
		else if (strcmp(type, "CTRL_TYPE_INPUT_MULTI") == 0)
		{
			ret = CTRL_TYPE_INPUT_MULTI;
		}
		else if (strcmp(type, "CTRL_TYPE_BUTTON") == 0)
		{
			ret = CTRL_TYPE_BUTTON;
		}
		else if (strcmp(type, "CTRL_TYPE_LIST") == 0)
		{
			ret = CTRL_TYPE_LIST;
		}
		else if (strcmp(type, "CTRL_TYPE_TIMEBAR") == 0)
		{
			ret = CTRL_TYPE_TIMEBAR;
		}
		else if (strcmp(type, "CTRL_TYPE_RENDER") == 0)
		{
			ret = CTRL_TYPE_RENDER;
		}
		else if (strcmp(type, "CTRL_TYPE_SLIDER") == 0)
		{
			ret = CTRL_TYPE_SLIDER;
		}
		else if (strcmp(type, "CTRL_TYPE_COMBO") == 0)
		{
			ret = CTRL_TYPE_COMBO;
		}
		else if (strcmp(type, "CTRL_TYPE_PROG") == 0)
		{
			ret = CTRL_TYPE_PROG;
		}
		else if (strcmp(type, "CTRL_TYPE_INPUT_REAL") == 0)
		{
			ret = CTRL_TYPE_INPUT_REAL;
		}
	}
	return ret;
}
