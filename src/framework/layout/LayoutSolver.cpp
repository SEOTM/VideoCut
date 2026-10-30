#include "framework/layout/LayoutSolver.h"
#include "framework/base/util/FileCodec.h"
#include <Windows.h>
#include <stdio.h>
#include <new>
#include "rapidjson/document.h"

// 函数: SolverFactor
// 描述: 解析 JSON 的尺寸因子(coeff[8] + offset)到 SizeFactor
BOOL SolverFactor(SizeFactor& dest, rapidjson::Value& src, LPCSTR szWhere);
// 函数: SolverRefSize
// 描述: 解析布局级 "autosize"(参考尺寸/经验参数)到 RefSize
VOID SolverRefSize(RefSize& dest, rapidjson::Value& src);
// 函数: SolverStr
// 描述: 把 JSON 字符串拷入中间态的定长字段(超长截断并打印诊断)
VOID SolverStr(char* dst, size_t cap, LPCSTR src, LPCSTR szWhere, LPCSTR szField);

// 函数: LayoutSolver
// 描述: 构造文档读入器(记录分配器, 计数清零, 中间态置空)
LayoutSolver::LayoutSolver(Arena* arena)
{
	this->memArena = arena;
	this->dwLayouts = 0;
	this->state = FileMidState{};
}

// 函数: ~LayoutSolver
// 描述: 析构文档读入器(不释放分配器内存, 也不回收中间态块)
LayoutSolver::~LayoutSolver()
{
	memArena = nullptr;
	this->state = FileMidState{};
}
// 函数: LoadLayouts
// 描述: 从文件加载布局信息(内存映射后转交内存入口)
BOOL LayoutSolver::LoadLayouts(LPCSTR filename)
{

	BOOL bRet = FALSE;
	DWORD size = 0;
	HANDLE hMap = NULL;
	LPVOID data = NULL;
	HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD err = GetLastError();
	if (hFile == INVALID_HANDLE_VALUE)
	{
		goto END;
	}
	size = GetFileSize(hFile, NULL);
	if (size == 0)
	{
		goto END;
	}
	hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, size, NULL);
	if (hMap == NULL)
	{
		goto END;
	}
	data = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, size);
	if (data == NULL)
	{
		goto CLOSEMAP;
	}
	bRet = LoadLayouts(data, size);
	if (data != NULL)
	{
		UnmapViewOfFile(data);
	}
CLOSEMAP:
	if (hMap != NULL)
	{
		CloseHandle(hMap);
	}
	CloseHandle(hFile);
END:
	return bRet;
}
// 函数: LoadLayouts
// 描述: 从内存缓冲解析 JSON, 产物 = 中间态
BOOL LayoutSolver::LoadLayouts(LPVOID data, DWORD size)
{
	BOOL bRet = FALSE;
	if (data == NULL || memArena == NULL)
	{
		return FALSE;
	}
	rapidjson::Document doc;
	doc.Parse((char*)data);
	if (doc.HasParseError())
	{
		printf("[JSON] 解析错误(偏移 %u, 代码 %u): 拒绝加载\n",
			(unsigned)doc.GetErrorOffset(), (unsigned)doc.GetParseError());
		goto END;
	}

	if (!doc.HasMember("layouts") || !doc["layouts"].IsArray())
	{
		printf("[JSON] 缺少 \"layouts\" 数组: 拒绝加载\n");
		goto END;
	}
	{
		const rapidjson::SizeType nLayouts = doc["layouts"].Size();
		if (nLayouts == 0 || nLayouts > LAYOUT_MAX)
		{
			printf("[JSON] \"layouts\" 数量非法(%u, 允许 1..%u): 拒绝加载\n",
				(unsigned)nLayouts, (unsigned)LAYOUT_MAX);
			goto END;
		}
		this->dwLayouts = (DWORD)nLayouts;
		DWORD* Counts = NULL;
		{
			BufferInfo cbuf = memArena->Allocate(sizeof(DWORD) * this->dwLayouts);
			if (cbuf.buffer == NULL)
			{
				goto END;
			}
			Counts = (DWORD*)cbuf.buffer;
		}
		size_t qwTotalElements = 0;
		for (DWORD i = 0; i < this->dwLayouts; i++)
		{
			rapidjson::Value& layout = doc["layouts"][i];
			if (!layout.IsObject())
			{
				printf("[JSON] layouts[%u] 不是对象: 拒绝加载\n", (unsigned)i);
				goto END;
			}
			if (!layout.HasMember("ctrls") || !layout["ctrls"].IsArray())
			{
				printf("[JSON] layouts[%u] 缺少 \"ctrls\" 数组: 拒绝加载\n", (unsigned)i);
				goto END;
			}
			const rapidjson::SizeType nCtrls = layout["ctrls"].Size();
			if (nCtrls > CTRL_MAX)
			{
				printf("[JSON] layouts[%u] 控件数非法(%u, 上限 %u): 拒绝加载\n",
					(unsigned)i, (unsigned)nCtrls, (unsigned)CTRL_MAX);
				goto END;
			}
			Counts[i] = (DWORD)nCtrls;
			qwTotalElements += (size_t)nCtrls;
		}
		if (qwTotalElements > (0xFFFFFFFFull / sizeof(ElementSpec)))
		{
			printf("[JSON] 控件总数过大导致尺寸溢出: 拒绝加载\n");
			goto END;
		}
		LayoutModel::StateClear(this->state);
		if (!LayoutModel::StateInit(this->state, this->dwLayouts, Counts))
		{
			printf("[JSON] 中间态块分配失败: 拒绝加载\n");
			goto END;
		}
		for (DWORD i = 0; i < this->dwLayouts; i++)
		{
			rapidjson::Value& layout = doc["layouts"][i];
			LayoutNode* pNode = LayoutModel::LayoutAt(this->state, i);
			if ((pNode == NULL) || (pNode->info == NULL))
			{
				goto END;
			}
			LayoutInfo* info = pNode->info;
			CONST DWORD dwControls = info->dwElements;
			{
				CHAR szLay[64]{};
				LPCSTR lpName = (layout.HasMember("name") && layout["name"].IsString())
					? layout["name"].GetString() : "";
				LPCSTR lpLabel = (layout.HasMember("label") && layout["label"].IsString())
					? layout["label"].GetString() : "";
				info->IsMaxiable = (layout.HasMember("IsMaxiable") && layout["IsMaxiable"].IsBool())
					? layout["IsMaxiable"].GetBool() : FALSE;
				info->IsResizeable = (layout.HasMember("IsResizeable") && layout["IsResizeable"].IsBool())
					? layout["IsResizeable"].GetBool() : FALSE;
				SolverRefSize(info->refSize, layout);
				info->wndX = 0;
				info->wndY = 0;
				if (layout.HasMember("wndX") && layout["wndX"].IsInt()) { info->wndX = layout["wndX"].GetInt(); }
				if (layout.HasMember("wndY") && layout["wndY"].IsInt()) { info->wndY = layout["wndY"].GetInt(); }
				SolverStr(info->name, sizeof(info->name), lpName, "layouts[] 布局 name", "name");
				SolverStr(info->label, sizeof(info->label), lpLabel, "layouts[] 布局 label", "label");
				snprintf(szLay, sizeof(szLay), "layouts[%u] name=%s", (unsigned)i, info->name);
				if (!layout.HasMember("width") || !layout.HasMember("height"))
				{
					printf("[JSON] %s 缺少 width/height 因子: 拒绝加载\n", szLay);
					goto END;
				}
				if (!SolverFactor(info->width, layout["width"], szLay) ||
					!SolverFactor(info->height, layout["height"], szLay))
				{
					goto END;
				}
			}
			ElementNode* pElemNode = LayoutModel::ElementFirst(pNode);
			for (DWORD j = 0; j < dwControls; j++)
			{
				rapidjson::Value& ctrl = layout["ctrls"][j];
				if ((pElemNode == NULL) || (pElemNode->data == NULL))
				{
					printf("[JSON] layouts[%u].ctrls[%u] 元素链长度不足: 拒绝加载\n", (unsigned)i, (unsigned)j);
					goto END;
				}
				ElementSpec& es = *(pElemNode->data);
				pElemNode = LayoutModel::ElementNext(pElemNode);
				if (!ctrl.IsObject())
				{
					printf("[JSON] layouts[%u].ctrls[%u] 不是对象: 拒绝加载\n", (unsigned)i, (unsigned)j);
					goto END;
				}
				if (!ctrl.HasMember("type") || !ctrl["type"].IsString())
				{
					printf("[JSON] layouts[%u].ctrls[%u] 缺少字符串 \"type\": 拒绝加载\n",
						(unsigned)i, (unsigned)j);
					goto END;
				}
				es.id = (ctrl.HasMember("id") && ctrl["id"].IsInt())
					? ctrl["id"].GetInt() : (int)j;
				if (!(ctrl.HasMember("id") && ctrl["id"].IsInt()))
				{
					printf("[JSON] layouts[%u].ctrls[%u] 缺少整数 \"id\": 按数组下标 %u 兜底\n",
						(unsigned)i, (unsigned)j, (unsigned)j);
				}
				es.parentID = (ctrl.HasMember("parent") && ctrl["parent"].IsInt())
					? ctrl["parent"].GetInt() : -1;
				if (es.parentID < -1)
				{
					es.parentID = -1;
				}
				{
					CHAR szWhere[128]{};
					snprintf(szWhere, sizeof(szWhere), "layouts[%u].ctrls[%u]", (unsigned)i, (unsigned)j);
					SolverStr(es.type, sizeof(es.type), ctrl["type"].GetString(), szWhere, "type");
					SolverStr(es.label, sizeof(es.label),
						(ctrl.HasMember("label") && ctrl["label"].IsString()) ? ctrl["label"].GetString() : "",
						szWhere, "label");
					SolverStr(es.text, sizeof(es.text),
						(ctrl.HasMember("text") && ctrl["text"].IsString()) ? ctrl["text"].GetString() : "",
						szWhere, "text");
					SolverStr(es.event, sizeof(es.event),
						(ctrl.HasMember("event") && ctrl["event"].IsString()) ? ctrl["event"].GetString() : "",
						szWhere, "event");
					{
						CHAR szFac[160]{};
						snprintf(szFac, sizeof(szFac), "%s type=%s label=%s",
							szWhere, es.type, es.label);
						if (!ctrl.HasMember("x") || !ctrl.HasMember("y") ||
							!ctrl.HasMember("width") || !ctrl.HasMember("height"))
						{
							printf("[JSON] %s 缺少几何因子(x/y/width/height): 拒绝加载\n", szFac);
							goto END;
						}
						if (!SolverFactor(es.x, ctrl["x"], szFac) ||
							!SolverFactor(es.y, ctrl["y"], szFac) ||
							!SolverFactor(es.width, ctrl["width"], szFac) ||
							!SolverFactor(es.height, ctrl["height"], szFac))
						{
							goto END;
						}
					}
				}
			}
		}
	}
	bRet = TRUE;
END:
	if (bRet == FALSE)
	{
		fflush(stdout);
	}
	return bRet;
}
// 函数: GetLayoutsCount
// 描述: 获取布局数量
DWORD LayoutSolver::GetLayoutsCount()
{
	return this->dwLayouts;
}
// 函数: GetLayout
// 描述: 获取指定索引布局的布局信息
CONST LayoutInfo* LayoutSolver::GetLayout(DWORD index)
{
	LayoutNode* pNode = LayoutModel::LayoutAt(this->state, index);
	return (pNode != NULL) ? pNode->info : NULL;
}
// 函数: GetState
// 描述: 取整份中间态(布局链 + 元素池)
CONST FileMidState& LayoutSolver::GetState()
{
	return this->state;
}

constexpr DWORD BIN_SNIFF_BYTES = 16;

// 函数: LoadLayoutsBin
// 描述: 从文件按 bin 格式加载布局信息(整体读入后转交内存入口)
BOOL LayoutSolver::LoadLayoutsBin(LPCSTR filename)
{
	BOOL bRet = FALSE;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	DWORD cbSize = 0;
	DWORD dwRead = 0;
	LPBYTE lpBuf = NULL;
	if (filename == NULL || filename[0] == '\0')
	{
		return bRet;
	}
	hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("[BIN] 打开文件失败(%s; GetLastError=%u): 拒绝加载\n", filename, (unsigned)GetLastError());
		goto END;
	}
	cbSize = GetFileSize(hFile, NULL);
	if ((cbSize == 0) || (cbSize == INVALID_FILE_SIZE))
	{
		printf("[BIN] 文件大小非法(%s; %u 字节): 拒绝加载\n", filename, (unsigned)cbSize);
		goto END;
	}
	lpBuf = new (std::nothrow) BYTE[cbSize];
	if (lpBuf == NULL)
	{
		goto END;
	}
	if (!ReadFile(hFile, lpBuf, cbSize, &dwRead, NULL) || (dwRead != cbSize))
	{
		printf("[BIN] 读取文件未完成(%s; %u / %u 字节): 拒绝加载\n", filename, (unsigned)dwRead, (unsigned)cbSize);
		goto END;
	}
	bRet = LoadLayoutsBin((LPCVOID)lpBuf, cbSize);
END:
	if (lpBuf != NULL)
	{
		delete[] lpBuf;
	}
	if (hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFile);
	}
	return bRet;
}

// 函数: LoadLayoutsBin
// 描述: 从内存缓冲按 bin 格式解析, 产物 = 中间态
BOOL LayoutSolver::LoadLayoutsBin(LPCVOID data, DWORD size)
{
	BOOL bRet = FALSE;
	CONST BYTE* pData = (CONST BYTE*)data;
	DWORD* Counts = NULL;
	DWORD dwSegs = 0;
	DWORD dwVer = 0;
	DWORD dwStored = 0;
	DWORD dwCalc = 0;
	size_t cbOff = 0;
	if (pData == NULL || memArena == NULL)
	{
		goto END;
	}
	if (size < (DWORD)BIN_FILE_HEAD)
	{
		printf("[BIN] 文件过短(%u 字节, 至少 %u): 拒绝加载\n", (unsigned)size, (unsigned)BIN_FILE_HEAD);
		goto END;
	}
	memcpy(&dwStored, pData + 0, 4);
	memcpy(&dwVer, pData + 4, 4);
	memcpy(&dwSegs, pData + 8, 4);
	{
		CONST BYTE Zeros[BIN_CRC_SIZE] = {};
		DWORD dwState = FileCodec::Crc32Update(FileCodec::Crc32Begin(), (LPCVOID)Zeros, (size_t)BIN_CRC_SIZE);
		dwState = FileCodec::Crc32Update(dwState, (LPCVOID)(pData + BIN_CRC_SIZE),
			(size_t)size - (size_t)BIN_CRC_SIZE);
		dwCalc = FileCodec::Crc32Finish(dwState);
	}
	if (dwStored != dwCalc)
	{
		printf("[BIN] 校验码不符(文件 %08X, 计算 %08X): 拒绝加载\n", (unsigned)dwStored, (unsigned)dwCalc);
		goto END;
	}
	if ((dwVer & BIN_VER_GEN_MASK) != (BIN_VERSION & BIN_VER_GEN_MASK))
	{
		printf("[BIN] 文件代际版本号不支持(%u, 需 %u): 拒绝加载\n",
			(unsigned)((dwVer & BIN_VER_GEN_MASK) >> 8), (unsigned)((BIN_VERSION & BIN_VER_GEN_MASK) >> 8));
		goto END;
	}
	{
		DWORD dwFlags = dwVer & BIN_VER_FLAG_MASK;
		if (dwFlags != 0)
		{
			LPCSTR lpShape = "含未定义位的标记";
			if (dwFlags == BIN_VER_FLAG_COMPRESS)
			{
				lpShape = "压缩形态";
			}
			else if (dwFlags == BIN_VER_FLAG_ENCRYPT)
			{
				lpShape = "加密形态";
			}
			else if (dwFlags == (BIN_VER_FLAG_COMPRESS | BIN_VER_FLAG_ENCRYPT))
			{
				lpShape = "加密且压缩形态";
			}
			printf("[BIN] 文件格式标记不支持(0x%02X = %s, 尚未实现): 拒绝加载\n",
				(unsigned)dwFlags, lpShape);
			goto END;
		}
	}
	if (dwSegs == 0 || dwSegs > (DWORD)LAYOUT_MAX)
	{
		printf("[BIN] 段数量非法(%u, 允许 1..%u): 拒绝加载\n", (unsigned)dwSegs, (unsigned)LAYOUT_MAX);
		goto END;
	}
	this->dwLayouts = dwSegs;
	{
		BufferInfo cbuf = memArena->Allocate(sizeof(DWORD) * dwSegs);
		if (cbuf.buffer == NULL)
		{
			goto END;
		}
		Counts = (DWORD*)cbuf.buffer;
	}
	cbOff = (size_t)BIN_FILE_HEAD;
	for (DWORD i = 0; i < dwSegs; i++)
	{
		DWORD cbSeg = 0;
		DWORD dwHeadCtrls = 0;
		DWORD dwUse = 0;
		size_t cbRem = 0;
		size_t nSegCtrls = 0;
		if (cbOff + (size_t)BIN_SEG_HEAD > (size_t)size)
		{
			printf("[BIN] 段[%u] 段头越界(偏移 %u, 文件 %u 字节): 拒绝加载\n",
				(unsigned)i, (unsigned)cbOff, (unsigned)size);
			goto END;
		}
		memcpy(&cbSeg, pData + cbOff + 0, 4);
		if ((size_t)cbSeg < (size_t)(BIN_SEG_HEAD + BIN_LAYOUT_SIZE))
		{
			printf("[BIN] 段[%u] 本段大小过小(声明 %u, 至少 %u): 拒绝加载\n",
				(unsigned)i, (unsigned)cbSeg, (unsigned)(BIN_SEG_HEAD + BIN_LAYOUT_SIZE));
			goto END;
		}
		if (cbOff + (size_t)cbSeg > (size_t)size)
		{
			printf("[BIN] 段[%u] 本段越界(偏移 %u, 声明 %u, 文件 %u 字节): 拒绝加载\n",
				(unsigned)i, (unsigned)cbOff, (unsigned)cbSeg, (unsigned)size);
			goto END;
		}
		cbRem = (size_t)cbSeg - (size_t)(BIN_SEG_HEAD + BIN_LAYOUT_SIZE);
		if ((cbRem % (size_t)BIN_CTRL_SIZE) != 0)
		{
			printf("[BIN] 段[%u] 本段大小与控件记录不符(余量 %u 非 %u 的整数倍): 拒绝加载\n",
				(unsigned)i, (unsigned)cbRem, (unsigned)BIN_CTRL_SIZE);
			goto END;
		}
		nSegCtrls = cbRem / (size_t)BIN_CTRL_SIZE;
		memcpy(&dwHeadCtrls, pData + cbOff + BIN_SEG_HEAD + BIN_LAYOUT_NELEM, 4);
		if (dwHeadCtrls > (DWORD)CTRL_MAX)
		{
			printf("[BIN] 段[%u] 控件数超上限(头部声明 %u, 上限 %u): 拒绝加载\n",
				(unsigned)i, (unsigned)dwHeadCtrls, (unsigned)CTRL_MAX);
			goto END;
		}
		dwUse = (dwHeadCtrls < (DWORD)nSegCtrls) ? dwHeadCtrls : (DWORD)nSegCtrls;
		if (dwHeadCtrls != (DWORD)nSegCtrls)
		{
			printf("[BIN] 段[%u] 控件数量与段大小不符(头部声明 %u, 段内容纳 %u) ⇒ 按较小值 %u 解析\n",
				(unsigned)i, (unsigned)dwHeadCtrls, (unsigned)nSegCtrls, (unsigned)dwUse);
		}
		Counts[i] = dwUse;
		cbOff += (size_t)cbSeg;
	}
	if (cbOff != (size_t)size)
	{
		printf("[BIN] 段区总长与文件长度不符(%u / %u 字节): 拒绝加载\n", (unsigned)cbOff, (unsigned)size);
		goto END;
	}
	LayoutModel::StateClear(this->state);
	if (!LayoutModel::StateInit(this->state, dwSegs, Counts))
	{
		printf("[BIN] 中间态块分配失败: 拒绝加载\n");
		goto END;
	}
	cbOff = (size_t)BIN_FILE_HEAD;
	for (DWORD i = 0; i < dwSegs; i++)
	{
		CONST BYTE* pSeg = pData + cbOff;
		LayoutNode* pNode = LayoutModel::LayoutAt(this->state, i);
		LayoutInfo* info = NULL;
		CONST BYTE* pCtrl = NULL;
		if ((pNode == NULL) || (pNode->info == NULL))
		{
			goto END;
		}
		info = pNode->info;
		memcpy(info, pSeg + BIN_SEG_HEAD, sizeof(LayoutInfo));
		info->dwElements = Counts[i];
		info->name[STR_NAME - 1] = '\0';
		info->label[STR_LABEL - 1] = '\0';
		info->refSize.mode[STR_MODE - 1] = '\0';
		pCtrl = pSeg + BIN_SEG_HEAD + BIN_LAYOUT_SIZE;
		for (ElementNode* pElem = LayoutModel::ElementFirst(pNode);
			pElem != NULL; pElem = LayoutModel::ElementNext(pElem))
		{
			ElementSpec* es = pElem->data;
			if (es == NULL)
			{
				goto END;
			}
			memcpy(es, pCtrl, sizeof(ElementSpec));
			es->label[STR_LABEL - 1] = '\0';
			es->text[STR_TEXT - 1] = '\0';
			es->type[STR_TYPE - 1] = '\0';
			es->event[STR_EVENT - 1] = '\0';
			if (es->parentID < -1)
			{
				es->parentID = -1;
			}
			pCtrl += BIN_CTRL_SIZE;
		}
		cbOff += BinSegBytes(Counts[i]);
	}
	bRet = TRUE;
END:
	if (!bRet)
	{
		fflush(stdout);
	}
	return bRet;
}

// 函数: IsBinDocument
// 描述: 嗅探缓冲的文档形态(TRUE = bin, FALSE = JSON)
BOOL LayoutSolver::IsBinDocument(LPCVOID data, DWORD size)
{
	CONST BYTE* pData = (CONST BYTE*)data;
	size_t i = 0;
	if (pData == NULL || size == 0)
	{
		return FALSE;
	}
	if ((size >= 3) && (pData[0] == 0xEF) && (pData[1] == 0xBB) && (pData[2] == 0xBF))
	{
		i = 3;
	}
	while ((i < (size_t)size) &&
		(pData[i] == ' ' || pData[i] == '\t' || pData[i] == '\r' || pData[i] == '\n'))
	{
		i++;
	}
	if (i >= (size_t)size)
	{
		return FALSE;
	}
	return (pData[i] == '{') ? FALSE : TRUE;
}

// 函数: LoadLayoutsAuto
// 描述: 嗅探文件形态后分派到 JSON / bin 读入口
BOOL LayoutSolver::LoadLayoutsAuto(LPCSTR filename)
{
	BOOL bRet = FALSE;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	DWORD dwRead = 0;
	BYTE Heads[BIN_SNIFF_BYTES]{};
	if (filename == NULL || filename[0] == '\0')
	{
		return bRet;
	}
	hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		if (!ReadFile(hFile, Heads, (DWORD)sizeof(Heads), &dwRead, NULL))
		{
			dwRead = 0;
		}
		CloseHandle(hFile);
	}
	if ((dwRead > 0) && IsBinDocument((LPCVOID)Heads, dwRead))
	{
		bRet = LoadLayoutsBin(filename);
	}
	else
	{
		bRet = LoadLayouts(filename);
	}
	return bRet;
}

// 函数: LoadLayoutsAuto
// 描述: 嗅探内存缓冲形态后分派到 JSON / bin 读入口
BOOL LayoutSolver::LoadLayoutsAuto(LPCVOID data, DWORD size)
{
	if ((data != NULL) && (size > 0) && IsBinDocument(data, size))
	{
		return LoadLayoutsBin(data, size);
	}
	return LoadLayouts((LPVOID)data, size);
}

// 函数: SolverRefSize
// 描述: 解析布局级 autosize(参考尺寸与经验参数)到 RefSize
VOID SolverRefSize(RefSize& dest, rapidjson::Value& src)
{
	dest = LayoutModel::DefaultRefSize();
	if (!src.HasMember("autosize") || !src["autosize"].IsObject())
	{
		return;
	}
	rapidjson::Value& a = src["autosize"];
	{
		static const LPCSTR Modes[4] = {
			"AUTO_SIZE_BY_SCREEN_WIDTH",
			"AUTO_SIZE_BY_SCREEN_HEIGHT",
			"AUTO_SIZE_BY_WINDOW_WIDTH",
			"AUTO_SIZE_BY_WINDOW_HEIGHT"
		};
		INT nMode = AutoSize::AUTO_SIZE_BY_SCREEN_HEIGHT;
		if (a.HasMember("mode"))
		{
			if (a["mode"].IsString())
			{
				LPCSTR szMode = a["mode"].GetString();
				for (INT m = 0; m < 4; m++)
				{
					if (strcmp(szMode, Modes[m]) == 0)
					{
						nMode = m;
						break;
					}
				}
			}
			else if (a["mode"].IsInt())
			{
				nMode = a["mode"].GetInt();
			}
		}
		if (nMode < 0 || nMode > 3) { nMode = 0; }
		LayoutModel::StrCopy(dest.mode, sizeof(dest.mode), Modes[nMode]);
	}
	if (a.HasMember("unit") && a["unit"].IsInt())
	{
		INT unit = a["unit"].GetInt();
		if (unit >= 0 && unit <= 1) { dest.unit = unit; }
	}
	static const LPCSTR Flds[6] = {
		"width", "height", "controlHeight", "fontSize", "space", "padding"
	};
	for (int i = 0; i < 6; i++)
	{
		if (a.HasMember(Flds[i]) && a[Flds[i]].IsInt())
		{
			INT v = a[Flds[i]].GetInt();
			if (v < 0) { v = 0; }
			if (v > 10000) { v = 10000; }
			LayoutModel::SetRefSizeValueAt(dest, i, v);
		}
	}
}

// 函数: SolverStr
// 描述: 把 JSON 字符串拷入中间态的定长字段, 超长按截断处理并打印诊断
VOID SolverStr(char* dst, size_t cap, LPCSTR src, LPCSTR szWhere, LPCSTR szField)
{
	if (!LayoutModel::StrCopy(dst, cap, src))
	{
		printf("[JSON] %s 字段 \"%s\" 超长(上限 %u 字节): 已截断\n",
			(szWhere != NULL) ? szWhere : "?", (szField != NULL) ? szField : "?", (unsigned)cap);
	}
}

// 函数: SolverFactor
// 描述: 解析尺寸因子(coeff[8] 与整数 offset)到 SizeFactor
BOOL SolverFactor(SizeFactor& dest, rapidjson::Value& src, LPCSTR szWhere)
{
	BOOL bRet = TRUE;
	LPCSTR szAt = (szWhere != NULL) ? szWhere : "?";
	FLOAT* pCoeff = (FLOAT*)&dest.coeff;
	for (INT i = 0; i < 8; i++)
	{
		pCoeff[i] = 0.0f;
	}
	dest.offset = 0;
	if (!src.IsObject())
	{
		printf("[JSON] %s 尺寸因子不是对象: 按 0 处理\n", szAt);
		return FALSE;
	}
	if (src.HasMember("coeff") && src["coeff"].IsArray())
	{
		rapidjson::Value& srcCoeffs = src["coeff"];
		const rapidjson::SizeType nSize = srcCoeffs.Size();
		for (rapidjson::SizeType i = 0; i < 8; i++)
		{
			if (i >= nSize)
			{
				printf("[JSON] %s coeff 只有 %u 项(需 8): 缺项按 0 处理\n", szAt, (unsigned)nSize);
				bRet = FALSE;
				break;
			}
			if (!srcCoeffs[i].IsNumber())
			{
				printf("[JSON] %s coeff[%u] 不是数字: 按 0 处理\n", szAt, (unsigned)i);
				bRet = FALSE;
				continue;
			}
			pCoeff[i] = srcCoeffs[i].GetFloat();
		}
	}
	else
	{
		printf("[JSON] %s 缺少 coeff 数组: 按 0 处理\n", szAt);
		bRet = FALSE;
	}
	if (src.HasMember("offset") && src["offset"].IsInt())
	{
		dest.offset = src["offset"].GetInt();
	}
	else
	{
		printf("[JSON] %s 缺少整数 offset: 按 0 处理\n", szAt);
		bRet = FALSE;
	}
	return bRet;
}
