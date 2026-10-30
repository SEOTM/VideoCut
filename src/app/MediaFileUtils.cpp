#include <Windows.h>
#include <shobjidl.h>
#include <stdio.h>
#include "MediaFileUtils.h"

// 静态成员变量定义
WCHAR MediaFileUtils::widebuf[MAX_PATH];
WCHAR MediaFileUtils::findbuf[MAX_PATH];
CHAR MediaFileUtils::utf8buf[MAX_PATH * 3];

// 函数: ConvWideInBuffer
// 描述: 转换 Unicode 编码字符串到 UTF8 缓冲区(传入参数为 Unicode 编码字符串)
BOOL MediaFileUtils::ConvWideInBuffer(LPCWSTR lpWide)
{
	BOOL ret = 0;
	if (lpWide == NULL) goto END;

	// 清空缓冲区
	ZeroMemory(utf8buf, sizeof(utf8buf));

	// 将宽字符转换为UTF-8
	{
		int result = WideCharToMultiByte(
			CP_UTF8,            // 目标编码为UTF-8
			0,                  // 转换标志
			lpWide,             // 源宽字符串
			-1,                 // 自动计算长度
			utf8buf,            // 目标缓冲区
			sizeof(utf8buf),    // 缓冲区大小
			NULL,               // 默认字符（失败时使用）
			NULL                // 是否使用了默认字符
		);
		ret = result > 0;
	}
END:
	return ret;
}

// 函数: ConvUTF8InBuffer
// 描述: 转换 UTF8 编码字符串到 Unicode 缓冲区(传入参数为 UTF-8 编码字符串)
BOOL MediaFileUtils::ConvUTF8InBuffer(LPCSTR lpUTF8)
{
	BOOL ret = FALSE;
	if (lpUTF8 == NULL) goto END;
	// 清空缓冲区
	ZeroMemory(widebuf, sizeof(widebuf));
	// 将UTF-8转换为宽字符
	{
		int result = MultiByteToWideChar(
			CP_UTF8,            // 源编码为UTF-8
			0,                  // 转换标志
			lpUTF8,             // 源UTF-8字符串
			-1,                 // 自动计算长度
			widebuf,            // 目标缓冲区
			MAX_PATH            // 缓冲区大小（以WCHAR为单位）
		);
		ret = result > 0;
	}
END:
	return ret;
}
// 函数: IsDirectoryExists
// 描述: 文件夹是否存在(传入参数为 UTF-8 编码字符串)
BOOL MediaFileUtils::IsDirectoryExists(LPCSTR lpFolderpath)
{
	BOOL ret = FALSE;
	if (lpFolderpath == NULL)  goto END;
	// 将UTF-8路径转换为宽字符
	if (!ConvUTF8InBuffer(lpFolderpath)) goto END;
	// 检查目录是否存在
	{
		DWORD dwAttrib = GetFileAttributesW(widebuf);
		ret = (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
	}
END:
	return ret;
}

// 函数: IsFileExists
// 描述: 文件是否存在(传入参数为 UTF-8 编码字符串)
BOOL MediaFileUtils::IsFileExists(LPCSTR lpFilepath)
{
	BOOL ret = FALSE;
	if (lpFilepath == NULL) goto END;
	// 将UTF-8路径转换为宽字符
	if (!ConvUTF8InBuffer(lpFilepath)) goto END;
	// 检查文件是否存在
	{
		DWORD dwAttrib = GetFileAttributesW(widebuf);
		ret = (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
	}
END:
	return ret;
}

// 函数: FolderBrowse
// 功能: 调用系统文件夹浏览器
BOOL MediaFileUtils::FolderBrowse(LPSTR lpBuffer, size_t size, LPCSTR lpDefault)
{
	// 初始化
	BOOL ret = FALSE;
	IFileDialog* pFileDialog = NULL;
	HRESULT hr = NULL;
	BufferInfo buf = { lpBuffer,size };
	// 接口
	hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileDialog));
	if (FAILED(hr)) goto END;
	{
		// 设置为文件夹选择模式
		DWORD dwOptions;
		hr = pFileDialog->GetOptions(&dwOptions);
		if (FAILED(hr)) goto CLEANUP;
		hr = pFileDialog->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
		if (FAILED(hr)) goto CLEANUP;
		// 设置默认路径
		if (lpDefault && IsDirectoryExists(lpDefault))
		{
			ConvUTF8InBuffer(lpDefault);
		}
		else
		{
			GetCurrentDirectory(MAX_PATH, widebuf);
		}
		if (widebuf[0])
		{
			IShellItem* pDefaultFolder = NULL;
			hr = SHCreateItemFromParsingName((WCHAR*)widebuf, NULL, IID_PPV_ARGS(&pDefaultFolder));
			if (SUCCEEDED(hr))
			{
				pFileDialog->SetFolder(pDefaultFolder);
				pDefaultFolder->Release();
			}
		}
		// 显示对话框
		pFileDialog->SetTitle(L"请选择文件夹");
		hr = pFileDialog->Show(NULL);
		if (FAILED(hr)) goto CLEANUP;
		// 获取结果
		IShellItem* pShellItem = NULL;
		hr = pFileDialog->GetResult(&pShellItem);
		if (FAILED(hr)) goto CLEANUP;
		LPWSTR path = NULL;
		hr = pShellItem->GetDisplayName(SIGDN_FILESYSPATH, &path);
		if (SUCCEEDED(hr))
		{
			WideCharToMultiByte(CP_UTF8, 0, path, -1, (char*)buf.buffer, (int)buf.size, NULL, NULL);
			CoTaskMemFree(path);
		}
		pShellItem->Release();
		ret = TRUE;
	};
CLEANUP:
	pFileDialog->Release();
END:
	return TRUE;
}
// 函数: SearchVideos
// 功能: 搜索目录下所有的文件 (传入 UTF-8 路径)
MediaOrganizer* MediaFileUtils::SearchVideos(LPCSTR lpDirectory)
{
	// 返回值
	MediaOrganizer* ret = NULL;
	// 将UTF-8路径转换为宽字符
	ConvUTF8InBuffer(lpDirectory);
	WCHAR dirPath[MAX_PATH];
	wcscpy_s(dirPath, MAX_PATH, widebuf);
	// 确保以反斜杠结尾
	size_t qwLen = wcslen(dirPath);
	if (dirPath[qwLen - 1] != L'\\' && dirPath[qwLen - 1] != L'/')
	{
		wcscat_s(dirPath, MAX_PATH, L"\\");
		qwLen++;
	}
	DWORD dwAttrib = GetFileAttributesW(widebuf);
	// 构建搜索扩展名
	const WCHAR* exts[] = { L"*.mpg", L"*.mpeg", L"*.mp4", L"*.avi", L"*.mkv", L"*.wmv",L"*.ts"};
	const int extCount = sizeof(exts) / sizeof(exts[0]);
	// 检查是否为无效目录
	if (dwAttrib == INVALID_FILE_ATTRIBUTES || !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) goto END;

	// 构建搜索模式
	wcscat_s(widebuf, MAX_PATH, L"*.mpg");
	// 开始查找文件
	{
		WIN32_FIND_DATAW findData;
		MediaOrganizer* org = NULL;
		DWORD dwError{};
		DWORD dwBytes = sizeof(WCHAR) * MAX_PATH;
		DWORD dwNums = 0;
		// 第一次遍历：统计总文件数
		for (int i = 0; i < extCount; i++)
		{
			// 构造搜索模式：目录 + 扩展名
			wcscpy_s(widebuf, MAX_PATH, dirPath);
			wcscat_s(widebuf, MAX_PATH, exts[i]);
			// 获取文件数量
			HANDLE hFind = FindFirstFileW(widebuf, &findData);
			if (hFind == INVALID_HANDLE_VALUE)  continue;
			do
			{
				// 跳过 "." 和 ".." 目录
				if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
				{
					continue;
				}
				// 跳过子目录，只显示文件
				if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					continue;
				}
				dwNums++;
			} while (FindNextFileW(hFind, &findData) != 0);
			// 清理资源
			FindClose(hFind);
		}
		dwError = GetLastError();
		if (dwError != ERROR_NO_MORE_FILES)
		{
			ret = FALSE;
		}
		// 如果没有文件，直接返回 NULL
		if (dwNums == 0)
		{
			ret = FALSE;
			goto END;
		}
		org = new MediaOrganizer(dwNums, FALSE);
		if (org == NULL) goto END;
		// 第二次遍历：填充文件路径
		for (int i = 0; i < extCount; ++i)
		{
			wcscpy_s(widebuf, MAX_PATH, dirPath);
			wcscat_s(widebuf, MAX_PATH, exts[i]);

			HANDLE hFind = FindFirstFileW(widebuf, &findData);
			if (hFind == INVALID_HANDLE_VALUE)  continue;
			do
			{
				// 跳过 "." 和 ".." 目录
				if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
				{
					continue;
				}
				// 跳过子目录，只显示文件
				if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					continue;
				}
				RtlZeroMemory(findbuf, dwBytes);
				wcsncpy_s(findbuf, MAX_PATH, widebuf, qwLen);
				wcscat_s(findbuf, MAX_PATH, findData.cFileName);
				ConvWideInBuffer(findbuf);
				// printf("%s\n", utf8buf);
				org->SaveFilename(utf8buf);
				
			} while (FindNextFileW(hFind, &findData) != 0);
			FindClose(hFind);
		}
		dwError = GetLastError();
		if (dwError != ERROR_NO_MORE_FILES)
		{
			ret = FALSE;
		}
		ret = org;
	}
END:
	return ret;
}