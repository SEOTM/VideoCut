#include "LayoutSolver.h"
#include <Windows.h>
// 使用 RapidJSON 库解析 JSON 数据
#include "rapidjson/document.h"

// ========================
// 区域: 辅助函数声明区
// ========================

// 函数: SolverFactor
// 描述: 解析 JSON 数据中的系数信息，并存储到 LayoutSolver::SizeFactor 结构体中
BOOL SolverFactor(LayoutSolver::SizeFactor& dest, rapidjson::Value& src);
// 函数: CopyBuffer
// 描述: 复制字符串的缓冲区（便于 doc 释放后能够被持久保存）
LPCSTR CopyBuffer(LPCSTR text, SecureBuffer* secbuf);

// ================================
// 库名: LayoutSolver
// 描述: 布局求解器，用于根据布局信息计算控件的实际位置和尺寸，并提供布局调整和渲染的功能
// ================================

// 函数: LayoutSolver
// 描述: 构造函数，初始化成员变量
LayoutSolver::LayoutSolver(SecureBuffer* secbuf)
{
	this->secbuf = secbuf;
	this->dwLayouts = 0;
	this->layouts = nullptr;
}

// 函数: ~LayoutSolver
// 描述: 析构函数，释放控件规格信息数组的内存
LayoutSolver::~LayoutSolver()
{
	secbuf = nullptr;				// 不负责释放 secbuf 的内存，由外部管理
	this->layouts = nullptr;			// 不负责释放 layouts 的内存，由外部管理
}
// 函数: LoadLayouts
// 描述: 从文件中加载布局信息
BOOL LayoutSolver::LoadLayouts(LPCSTR filename)
{
	// 读取文件后，通过指定内存解析的方式进行加载布局
	// 通过内存映射的方式打开文件

	BOOL bRet = FALSE;
	SIZE_T size = 0;
	HANDLE hMap = NULL;
	LPVOID data = NULL;
	// 创建文件 / 打开文件
	HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD err = GetLastError();
	if (hFile == INVALID_HANDLE_VALUE)
	{
		goto END;
	}
	// 判断判断文件是否为空
	size = GetFileSize(hFile, NULL);
	if (size == 0)
	{
		goto END;
	}
	// 创建文件映射
	hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, size, NULL);
	if (hMap == NULL)
	{
		goto END;
	}
	// 映射文件到内存
	data = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, size);
	if (data == NULL)
	{
		goto CLOSEMAP;
	}
	// 加载布局信息
	bRet = LoadLayouts(data, size);
	// 标记
UNMAP:
	if (data != NULL)
	{
		UnmapViewOfFile(data);
	}
CLOSEMAP:
	if (hMap != NULL)
	{
		CloseHandle(hMap);
	}
CLOSEFILE:
	CloseHandle(hFile);
END:
	return bRet;
}
// 函数: LoadLayouts
// 描述: 从指定内存区域加载布局信息
BOOL LayoutSolver::LoadLayouts(LPVOID data, DWORD size)
{
	// 使用 RapidJSON 库解析 JSON 数据	
	BOOL bRet = FALSE;
	// 解析 JSON 数据
	rapidjson::Document doc;
	// 读取数据
	doc.Parse((char*)data);
	if (doc.HasParseError())
	{
		goto END;
	}

	// 开始解析
	if (doc.HasMember("layouts") && doc["layouts"].IsArray())
	{
		// 获取布局数量
		this->dwLayouts = doc["layouts"].Size();
		// 申请布局数组内存
		DWORD dwSize = sizeof(LayoutInfo**) * this->dwLayouts;
		BufferInfo bufinfo = secbuf->Allocate(dwSize);
		if (!bufinfo.buffer)
		{
			goto END;
		}
		layouts = (LayoutInfo**)bufinfo.buffer;
		// 统计控件数量
		for (int i = 0; i < this->dwLayouts; i++)
		{
			// 临时变量
			rapidjson::Value& layout = doc["layouts"][i];


			// 先读取控件数量
			DWORD dwControls = layout["ctrls"].Size();
			// 计算总体大小
			DWORD dwSize = sizeof(ControlSpec) * dwControls + sizeof(LayoutInfo);
			// 申请内存
			BufferInfo bufinfo = secbuf->Allocate(dwSize);
			if (!bufinfo.buffer)
			{
				goto END;
			}
			// 填充数据
			layouts[i] = (LayoutInfo*)bufinfo.buffer;
			layouts[i]->dwControls = dwControls;


			// 为便于你的理解，我想你提供相关结构定义
				// 结构: CoeffInfo
				// 描述: 存储系数信息的结构体 (描述控件使用各参数的系数)
				//struct CoeffInfo
				//{
				//	AutoSize::SizeInfoFLOAT coeff;	// 控件使用各参数的系数 (相对于参考尺寸的比例)
				//	int offset;						// 固定偏移量 (用于微调控件位置)
				//};

				// 结构: ControlSpec
				// 描述: 存储控件规格信息的结构体
				//struct ControlSpec
				//{
				//	int id;				// 控件ID
				//	LPCSTR type;		// 控件类型
				//	LPCSTR label;		// 控件标签
				//	LPCSTR text;			// 控件文本内容
				//	LPCSTR event;		// 控件事件
				//	CoeffInfo x;		// 控件X轴位置系数信息
				//	CoeffInfo y;		// 控件Y轴位置系数信息
				//	CoeffInfo width;	// 控件宽度系数信息
				//	CoeffInfo height;	// 控件高度系数信息
				//};

				// 结构: LayoutInfo
				// 描述: 存储布局信息的结构体
				//struct LayoutInfo
				//{
				//	LPCSTR name;			// 布局名称
				//	LPCSTR label;			// 布局标签
				//	BOOL IsMaxiable;		// 是否最大化
				//	BOOL IsResizeable;		// 是否可调整大小
				//	CoeffInfo width;		// 布局宽度系数信息
				//	CoeffInfo height;		// 布局高度系数信息
				//	DWORD dwControls;		// 控件数量
				//	ControlSpec ctrls[];		// 控件规格信息数组
				//};


				// 读取并填充结构
			layouts[i]->name = layout["name"].GetString();
			layouts[i]->label = layout["label"].GetString();
			layouts[i]->IsMaxiable = layout["IsMaxiable"].GetBool();
			layouts[i]->IsResizeable = layout["IsResizeable"].GetBool();
			// 读取尺寸
			SolverFactor(layouts[i]->width, layout["width"]);
			SolverFactor(layouts[i]->height, layout["height"]);
			// 读取控件

			// ctrl 设计上为一个柔性数组，所以不需要专门进行赋值
			// 因为我们的 size 本身已经包含了 LayoutInfo 和 ControlSpec 数组的大小
			for (int j = 0; j < dwControls; j++)
			{
				rapidjson::Value& ctrl = layout["ctrls"][j];
				layouts[i]->ctrls[j].id = ctrl["id"].GetInt();
				layouts[i]->ctrls[j].type = ctrl["type"].GetString();
				layouts[i]->ctrls[j].label = ctrl["label"].GetString();
				layouts[i]->ctrls[j].text = ctrl["text"].GetString();
				layouts[i]->ctrls[j].event = ctrl["event"].GetString();
				// 读取尺寸
				SolverFactor(layouts[i]->ctrls[j].x, ctrl["x"]);
				SolverFactor(layouts[i]->ctrls[j].y, ctrl["y"]);
				SolverFactor(layouts[i]->ctrls[j].width, ctrl["width"]);
				SolverFactor(layouts[i]->ctrls[j].height, ctrl["height"]);
			}
		}
		// 解析完毕
		// 然后开始将其中所有出现的字符串放在安全缓冲区中，以便稍后销毁 doc 时不会出现内存泄漏
		for (int i = 0; i < this->dwLayouts; i++)
		{
			layouts[i]->name = CopyBuffer(layouts[i]->name, secbuf);
			layouts[i]->label = CopyBuffer(layouts[i]->label, secbuf);
			for (int j = 0; j < layouts[i]->dwControls; j++)
			{
				layouts[i]->ctrls[j].type = CopyBuffer(layouts[i]->ctrls[j].type, secbuf);
				layouts[i]->ctrls[j].label = CopyBuffer(layouts[i]->ctrls[j].label, secbuf);
				layouts[i]->ctrls[j].text = CopyBuffer(layouts[i]->ctrls[j].text, secbuf);
				layouts[i]->ctrls[j].event = CopyBuffer(layouts[i]->ctrls[j].event, secbuf);
			}
		}
	}
	// doc 生命周期结束对象在函数结束时会被自动销毁，因此不需要手动释放其内存
	bRet = TRUE;
END:
	return bRet;
}
// 函数: GetLayoutsCount
// 描述: 获取布局数量
DWORD LayoutSolver::GetLayoutsCount()
{
	return this->dwLayouts;
}
// 函数: GetLayout
// 描述: 获取指定索引的布局信息
CONST LayoutSolver::LayoutInfo* LayoutSolver::GetLayout(DWORD index)
{
	return this->layouts[index];
}
// 函数: CalcParams
// 描述: 静态函数，用以根据布局信息和参考尺寸以及固定偏移计算值
FLOAT LayoutSolver::CalcParams(SizeFactor& factor, AutoSize::SizeInfo<FLOAT>& refsize)
{
	DOUBLE sum = 0;
	FLOAT* ref = (FLOAT*)&refsize;
	FLOAT* coeff = (FLOAT*)&factor.coeff;
	for (int i = 0; i < 8; i++)
	{
		sum += ref[i] * coeff[i];
	}
	sum += factor.offset;
	return (FLOAT)sum;
}

// 函数: SolverFactor
// 描述: 解析尺寸因子
BOOL SolverFactor(LayoutSolver::SizeFactor& dest, rapidjson::Value& src)
{
	BOOL bRet = FALSE;
	// 从 src 中取出 coeff 数组
	auto arr = src["coeff"].GetArray();
	// 将 dest 的 coeff 视为一个 8 个元素的 FLOAT 数组
	FLOAT* pCoeff = (FLOAT*)&dest.coeff;
	// 然后直接读取
	for (int i = 0; i < 8; i++)
	{
		pCoeff[i] = arr[i].GetFloat();
	}
	// 解析固定偏移
	dest.offset = src["offset"].GetInt();
	// 解析成功
	bRet = TRUE;
	return bRet;
}

// 函数: CopyBuffer
// 描述: 复制字符串的缓冲区（便于 doc 释放后能够被持久保存）
LPCSTR CopyBuffer(LPCSTR text, SecureBuffer* secbuf)
{
	LPCSTR bRet = NULL;
	SIZE_T len = strlen(text) + 1;
	BufferInfo buf = secbuf->Allocate(len);
	if (buf.buffer)
	{
		strcpy_s((char*)buf.buffer, len, text);
		bRet = (LPCSTR)buf.buffer;
	}
	return bRet;
}