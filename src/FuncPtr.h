#pragma once
#include <minwindef.h>
// 前向声明 Application
class Application;

// ================================
// 描述: 预定义需要使用的函数指针类型
// ================================

// 类型: pfnCtrlEvent
// 描述: 即时UI模式下的控件响应函数指针类型
using pfnCtrlEvent = VOID(*)();

// 结构: EventInfo
// 描述: 事件信息结构体
struct EventInfo
{
	pfnCtrlEvent pfnEvent;	// 指向控件响应函数的指针
	DWORD bufferSize;		// 事件本身所需缓冲区大小
							// 如: 一个用以输入文件路径的事件，则其需要一定的缓冲来保存路径
};

// =================
// 下述编解码器属于即时编解码，不会再持久化存储明文数据
// 二进制 RAW 数据: 编码后存储到计算机内存以及后续存储在磁盘文件中的数据
// 二进制 RAW 数据大小允许为零，表示空数据
// 编解码器使用的数据索引在受保护的内存中执行操作，允许对新增数据进行在原有的 RAW 数据基础上进行增量编码
// 特别的，此处定义一个结构体用于传递 RAW 数据和其大小

// 结构: RawDataInfo
// 描述: 二进制 RAW 数据信息结构体
struct RawDataInfo
{
	CONST BYTE * data;	// 指向二进制 RAW 数据的指针
	size_t size;		// 二进制 RAW 数据的大小（字节数）
	BOOL isUseful;		// 从 RAW 中解码不会影响 RAW 有效性，编码后由于会生成新的 RAW 数据，因此会影响原 RAW 有效性
						// 主要是因为编码后 RAW 数据可能会变大或变小，从而影响原有数据的完整性
};
// 结构: OperDataInfo
// 描述: 编码/解码数据结构体
struct OperDataInfo
{
	BYTE* data;		// 指向编码/解码数据的指针
	size_t size;	// 编码/解码数据的大小（字节数）
};

// 类型: pfnEncoder
// 描述: 数据编码函数指针类型，根据二进制 RAW 数据、32 字节操作哈希指针、编码数据和 INDEX 进行编码
// 输入:
//   RawDataInfo* pRawDataInfo	- 指向二进制 RAW 数据信息的指针
//   CONST BYTE* pOperHash	- 指向 32 字节操作哈希的指针
//   OperDataInfo* pOperDataInfo	- 指向编码数据的指针
//   DWORD index			- 数据索引
// 返回值:
//   RawDataInfo - 返回新的二进制 RAW 数据信息
//				   如果编码失败，则返回的 size 为 0，data 指针为 nullptr，isUseful 为 FALSE
// 备注:
//   编码函数需要根据传入的编码数据和索引，对原有的 RAW 数据进行编码，生成新的 RAW 数据
using pfnEncoder = RawDataInfo(*)(RawDataInfo* pRawDataInfo, CONST BYTE* pOperHash, OperDataInfo* pOperDataInfo, DWORD index);

// 类型: pfnDecoder
// 描述: 数据解码函数指针类型，根据二进制 RAW 数据、32 字节操作哈希指针和 INDEX 进行编码
// 输入:
//   RawDataInfo* pRawDataInfo	- 指向二进制 RAW 数据信息的指针
//   CONST BYTE* pOperHash	- 指向 32 字节操作哈希的指针
//   DWORD index			- 数据索引
// 返回值:
//   OperDataInfo - 返回解码数据
//				    如果解码失败，则返回的 size 为 0，data 指针为 nullptr
using pfnDecoder = OperDataInfo(*)(RawDataInfo* pRawDataInfo, CONST BYTE* pOperHash, DWORD index);