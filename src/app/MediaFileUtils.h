#pragma once
#include <minwindef.h>
#include "MediaOrganizer.h"
class MediaFileUtils
{
private:
	static WCHAR widebuf[MAX_PATH];
	static WCHAR findbuf[MAX_PATH];
	static CHAR utf8buf[MAX_PATH * 3];
	// 函数: ConvWideInBuffer
	// 描述: 转换 Unicode 编码字符串到 UTF8 缓冲区(传入参数为 Unicode 编码字符串)
	static BOOL ConvWideInBuffer(LPCWSTR lpWide);
	// 函数: ConvWideInBuffer
	// 描述: 转换 UTF8 编码字符串到 Unicode 缓冲区(传入参数为 UTF-8 编码字符串)
	static BOOL ConvUTF8InBuffer(LPCSTR lpUTF8);
public:
	// 函数: IsDirectoryExists
	// 描述: 文件夹是否存在(传入参数为 UTF-8 编码字符串)
	static BOOL IsDirectoryExists(LPCSTR lpFolderpath);
	// 函数: IsFileExists
	// 描述: 文件是否存在(传入参数为 UTF-8 编码字符串)
	static BOOL IsFileExists(LPCSTR lpFilepath);
	// 函数: FolderBrowse
	// 功能: 调用系统文件夹浏览器
	static BOOL FolderBrowse(LPSTR lpBuffer, size_t size, LPCSTR lpDefault = NULL);
	// 函数: SearchVideos
	// 功能: 搜索目录下所有的视频 (传入 UTF-8 路径)
	static MediaOrganizer* SearchVideos(LPCSTR lpDirectory);
};

