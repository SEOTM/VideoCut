#pragma once
#include <windows.h>

namespace ResourceLoader
{
	// 函数: Init
	// 描述: 缓存 exe 所在目录(含末尾 '\'; 应用启动期调用一次; 幂等)
	VOID Init();

	// 函数: GetFilePath
	// 描述: 拼接 exe 目录与文件名(内存池字符串; 每次调用新分配)
	LPCSTR GetFilePath(LPCSTR filename);

	// 函数: FileExists
	// 描述: 普通文件是否存在(加载链的"外部文件"级别判据)
	BOOL FileExists(LPCSTR path);
}
