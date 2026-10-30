#pragma once
#include "framework/layout/LayoutModel.h"
#include "framework/base/BasicDesign.h"
#include "framework/base/mem/MemoryManager.h"

class LayoutSolver
{
private:
	DWORD dwLayouts;
	FileMidState state;
	Arena* memArena;
public:
	// 函数: LayoutSolver
	// 描述: 构造文档读入器(记录分配器, 计数清零, 中间态置空)
	LayoutSolver(Arena* arena);
	// 函数: ~LayoutSolver
	// 描述: 析构文档读入器(不释放分配器内存, 也不回收中间态块)
	~LayoutSolver();
	// 函数: LoadLayouts
	// 描述: 从文件加载布局信息(内存映射后转交内存入口)
	BOOL LoadLayouts(LPCSTR filename);
	// 函数: LoadLayouts
	// 描述: 从内存缓冲解析 JSON, 产物 = 中间态
	BOOL LoadLayouts(LPVOID data, DWORD size);
	// 函数: GetLayoutsCount
	// 描述: 获取布局数量
	DWORD GetLayoutsCount();
	// 函数: GetLayout
	// 描述: 获取指定索引布局的布局信息
	CONST LayoutInfo* GetLayout(DWORD index);
	// 函数: GetState
	// 描述: 取整份中间态(布局链 + 元素池)
	CONST FileMidState& GetState();

	// 函数: LoadLayoutsBin
	// 描述: 从文件按 bin 格式加载布局信息(整体读入后转交内存入口)
	BOOL LoadLayoutsBin(LPCSTR filename);
	// 函数: LoadLayoutsBin
	// 描述: 从内存缓冲按 bin 格式解析, 产物 = 中间态
	BOOL LoadLayoutsBin(LPCVOID data, DWORD size);
	// 函数: IsBinDocument
	// 描述: 嗅探缓冲的文档形态(TRUE = bin, FALSE = JSON)
	static BOOL IsBinDocument(LPCVOID data, DWORD size);

	// 函数: LoadLayoutsAuto
	// 描述: 嗅探文件形态后分派到 JSON / bin 读入口
	BOOL LoadLayoutsAuto(LPCSTR filename);
	// 函数: LoadLayoutsAuto
	// 描述: 嗅探内存缓冲形态后分派到 JSON / bin 读入口
	BOOL LoadLayoutsAuto(LPCVOID data, DWORD size);
};
