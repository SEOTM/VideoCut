#pragma once
#include <minwindef.h>
#include <windef.h>
#include <stdint.h>
#include "framework/base/ControlSpec.h"
#include "framework/base/BasicDesign.h"

constexpr DWORD RENDER_WND_BASE    = 0x00000001;
constexpr DWORD RENDER_WND_NOINPUT = 0x00000002;

using pfnAcquireWnd = BOOL(*)(CONST vec2& pos, CONST vec2& size, LPCSTR szTitle);

using pfnOpenWnd = BOOL(*)(LPCSTR szName, CONST vec2& pos, CONST vec2& size, DWORD dwStates);

using pfnCloseWnd = VOID(*)();

using pfnBringWndToFront = VOID(*)();

using pfnOpenPanel = BOOL(*)(LPCSTR szName, CONST vec2& pos, CONST vec2& size, DWORD crBack);

using pfnClosePanel = VOID(*)();

using pfnSetCursor = VOID(*)(CONST vec2& pos);

using pfnGetContentOrigin = vec2(*)();

using pfnPushId = VOID(*)(INT32 nId);

using pfnPushPtrId = VOID(*)(CONST VOID* pKey);

using pfnPopId = VOID(*)();

using pfnTakeItemId = uint64_t(*)();

using pfnAddHitArea = VOID(*)(LPCSTR szLabel, CONST vec2& pos, CONST vec2& size);

using pfnIsHitClicked = BOOL(*)();

using pfnIsRectClicked = BOOL(*)(CONST vec2& mn, CONST vec2& mx);

using pfnGetDrawVtxCount = INT64(*)();

struct RenderApi
{

	pfnAcquireWnd AcquireWnd;

	pfnOpenWnd OpenWnd;

	pfnCloseWnd CloseWnd;

	pfnBringWndToFront BringWndToFront;

	pfnOpenPanel OpenPanel;

	pfnClosePanel ClosePanel;

	pfnSetCursor SetCursor;

	pfnGetContentOrigin GetContentOrigin;

	pfnPushId PushId;

	pfnPushPtrId PushPtrId;

	pfnPopId PopId;

	pfnTakeItemId TakeItemId;

	pfnAddHitArea AddHitArea;

	pfnIsHitClicked IsHitClicked;

	pfnIsRectClicked IsRectClicked;

	pfnGetDrawVtxCount GetDrawVtxCount;
};

using pfnUpdateCtrlContext = BOOL(*)(CONST AutoSize& refsize, CONST ProjCR* pProj,
	CONST vec2* pWndPos, CONST vec2& orgPanel);

using pfnDrawCtrl = BOOL(*)(CtrlInfo* ctrl);

using pfnHasActiveInput = BOOL(*)();

struct CtrlApi
{
	pfnUpdateCtrlContext UpdateContext;

	pfnDrawCtrl DrawCtrl;

	pfnHasActiveInput HasActiveInput;
};

enum WndShowState
{
	WND_SHOW_NEVER = 0,
	WND_SHOW_VISIBLE,
	WND_SHOW_HIDDEN_BY_ACTIVE,
	WND_SHOW_HIDDEN_BY_HOST,
};

using pfnFrame = VOID(*)(VOID* pUser);

using pfnSegment = VOID(*)(DWORD index, VOID* pUser);

struct StyleDesc
{
	FLOAT fWindowRounding;
	FLOAT fFrameRounding;
	FLOAT fChildRounding;
	FLOAT fPopupRounding;
	DWORD crWindowBg;
	DWORD crChildBg;
	DWORD crFrameBg;
	DWORD crFrameBgHovered;
	DWORD crFrameBgActive;
	DWORD crButton;
	DWORD crButtonHovered;
	DWORD crButtonActive;
	DWORD crBorder;
	DWORD crText;
	DWORD crTextDisabled;
	DWORD crSliderGrab;
	DWORD crSliderGrabActive;
	DWORD crCheckMark;
	DWORD crHeader;
	DWORD crHeaderHovered;
	DWORD crHeaderActive;
};

struct HostApi
{
	VOID (*EnableDpiAwareness)();

	BOOL (*Start)(DWORD dwWndCount, LPCSTR szAppName, pfnSegment pfn, VOID* pUser);

	VOID (*Run)(pfnFrame pfnFrame, VOID* pUser, CONST BOOL* pbExit);

	VOID (*Release)();

	VOID (*DestroyWnds)();

	VOID (*SetDarkMode)(BOOL bDark);

	BOOL (*ApplyStyle)(CONST StyleDesc& desc);

	VOID (*SetIMEDisabled)(BOOL bDisabled);

	DWORD (*GetStandardWndCount)();

	WndShowState (*GetWndShowState)(DWORD index);

	BOOL (*GetWndRect)(DWORD index, RECT* prcWnd, RECT* prcClient);

	VOID (*GetRenderStats)(DWORD* pdwSegments, DWORD* pdwCmdLists, INT64* pllVtx, INT64* pllIdx);

	VOID (*GetResizeStats)(DWORD* pdwCount, DWORD* pdwLastW, DWORD* pdwLastH);

	VOID (*SetAppIcon)(VOID* hIcon);

	VOID* (*GetNativeDevice)();

	VOID* (*GetNativeDeviceContext)();
};
