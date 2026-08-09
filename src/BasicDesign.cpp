#include <Windows.h>
#include "BasicDesign.h"
// 函数：GetScreenWidth
// 功能：获取屏幕宽度
DWORD RefSize::GetScreenWidth()
{
	return GetSystemMetrics(SM_CXSCREEN);
}
// 函数：GetScreenHeight
// 功能：获取屏幕高度
DWORD RefSize::GetScreenHeight()
{
	return GetSystemMetrics(SM_CYSCREEN);
}
// 函数：GetContrlHeight
// 功能：获取控件高度
DWORD RefSize::GetContrlHeight()
{
	return GetSystemMetrics(SM_CYSCREEN) / 40;
}
// 函数：GetFontSize
// 功能：获取字体大小
DWORD RefSize::GetFontSize()
{
	return GetSystemMetrics(SM_CYSCREEN) * 13 / 800;
}
// 函数：GetSpace
// 功能：获取设计间隔
DWORD RefSize::GetSpace()
{
	return GetSystemMetrics(SM_CYSCREEN) / 100;
}

// ================================
// 类名: AutoSize
// 描述: 根据参考大小自动调整尺寸的类 (与 RefSize 不同的是，可以传递经验参数)
// ================================

// ========================
// 辅助函数区域
// ========================
// 函数: CalculateSizeByRatio
// 描述: 根据比例参数计算尺寸
static AutoSize::SizeInfoDWORD CalculateSizeByRatio(DWORD dwBaseValue, const AutoSize::RatioInfo& ratioInfo)
{
	// 计算尺寸信息
	AutoSize::SizeInfoDWORD sizeInfo;
	sizeInfo.screenWidth = RefSize::GetScreenWidth();
	sizeInfo.screenHeight = RefSize::GetScreenHeight();
	sizeInfo.windowWidth = (dwBaseValue * ratioInfo.dwRatioWidth) / 1000;
	sizeInfo.windowHeight = (dwBaseValue * ratioInfo.dwRatioHeight) / 1000;
	sizeInfo.controlHeight = (dwBaseValue * ratioInfo.dwRatioControlHeight) / 1000;
	sizeInfo.fontSize = (dwBaseValue * ratioInfo.dwRatioFontSize) / 1000;
	sizeInfo.space = (dwBaseValue * ratioInfo.dwRatioSpace) / 1000;
	sizeInfo.padding = (dwBaseValue * ratioInfo.dwRatioPadding) / 1000;
	return sizeInfo;
}
// 函数: CalculateRatioBySize
// 描述: 根据尺寸参数计算比例
static AutoSize::RatioInfo CalculateRatioBySize(DWORD dwBaseValue, const AutoSize::SizeInfoDWORD& sizeInfo)
{
	AutoSize::RatioInfo ratioInfo;
	ratioInfo.dwRatioWidth = (sizeInfo.windowWidth * 1000) / dwBaseValue;
	ratioInfo.dwRatioHeight = (sizeInfo.windowHeight * 1000) / dwBaseValue;
	ratioInfo.dwRatioControlHeight = (sizeInfo.controlHeight * 1000) / dwBaseValue;
	ratioInfo.dwRatioFontSize = (sizeInfo.fontSize * 1000) / dwBaseValue;
	ratioInfo.dwRatioSpace = (sizeInfo.space * 1000) / dwBaseValue;
	ratioInfo.dwRatioPadding = (sizeInfo.padding * 1000) / dwBaseValue;
	return ratioInfo;
}
// 函数: ConvertSizeInfo
// 描述: 将 第一种 SizeInfo 类型 转换为 第二种 SizeInfo 类型
template<typename numerical_from, typename numerical_to>
static AutoSize::SizeInfo<numerical_to> ConvertSizeInfo(const AutoSize::SizeInfo<numerical_from>& sizeInfoFrom)
{
	AutoSize::SizeInfo<numerical_to> sizeInfoTo;
	sizeInfoTo.screenWidth = static_cast<numerical_to>(sizeInfoFrom.screenWidth);
	sizeInfoTo.screenHeight = static_cast<numerical_to>(sizeInfoFrom.screenHeight);
	sizeInfoTo.windowWidth = static_cast<numerical_to>(sizeInfoFrom.windowWidth);
	sizeInfoTo.windowHeight = static_cast<numerical_to>(sizeInfoFrom.windowHeight);
	sizeInfoTo.controlHeight = static_cast<numerical_to>(sizeInfoFrom.controlHeight);
	sizeInfoTo.fontSize = static_cast<numerical_to>(sizeInfoFrom.fontSize);
	sizeInfoTo.space = static_cast<numerical_to>(sizeInfoFrom.space);
	sizeInfoTo.padding = static_cast<numerical_to>(sizeInfoFrom.padding);
	return sizeInfoTo;
}
// 函数: AutoSize
// 描述: 将输入尺寸保存，并自动扩展为其他尺寸类型，同时保存比例信息
AutoSize::AutoSize(const SizeInfoDWORD& sizeInfo, RatioInfo& ratioInfo, DWORD dwBaseValue, AutoSizeMode sizeMode)
{
	this->sizeDWORD = sizeInfo;
	// 计算浮点数尺寸信息
	this->sizeFLOAT = ConvertSizeInfo<DWORD, FLOAT>(sizeInfo);
	// 保存比例信息
	this->ratioInfo = CalculateRatioBySize(dwBaseValue, sizeInfo);
	this->sizeMode = sizeMode;
	this->dwBaseValue = dwBaseValue;
}
AutoSize::AutoSize(const SizeInfoFLOAT& sizeInfo, RatioInfo& ratioInfo, DWORD dwBaseValue, AutoSizeMode sizeMode)
{
	this->sizeFLOAT = sizeInfo;
	// 计算整数尺寸信息
	this->sizeDWORD = ConvertSizeInfo<FLOAT, DWORD>(sizeInfo);
	// 保存比例信息
	this->ratioInfo = CalculateRatioBySize(dwBaseValue, this->sizeDWORD);
	this->sizeMode = sizeMode;
	this->dwBaseValue = dwBaseValue;
}
// 函数: CreateByRatio
// 描述: 通过比例参数创建 AutoSize 对象
AutoSize	 AutoSize::CreateByRatio(const RatioInfo& ratioInfo, AutoSizeMode mode)
{
	// 确定参考基准数值
	DWORD dwBaseValue = 0;
	switch (mode)
	{
	case AUTO_SIZE_BY_SCREEN_WIDTH:
		dwBaseValue = RefSize::GetScreenWidth();
		break;
	case AUTO_SIZE_BY_SCREEN_HEIGHT:
		dwBaseValue = RefSize::GetScreenHeight();
		break;
	case AUTO_SIZE_BY_WINDOW_WIDTH:
		// 默认窗口宽度为屏幕宽度的 0.5 倍
		dwBaseValue = RefSize::GetScreenWidth() / 2;
		break;
	case AUTO_SIZE_BY_WINDOW_HEIGHT:
		// 默认窗口高度为屏幕高度的 0.5 倍
		dwBaseValue = RefSize::GetScreenHeight() / 2;
		break;
	default:
		dwBaseValue = RefSize::GetScreenWidth();
		break;
	}
	// 计算尺寸信息
	AutoSize::SizeInfoDWORD sizeInfo = CalculateSizeByRatio(dwBaseValue, ratioInfo);
	return AutoSize(sizeInfo, const_cast<RatioInfo&>(ratioInfo), dwBaseValue, mode);
}
AutoSize AutoSize::CreateByRatio(
	DWORD dwRatioWidth, DWORD dwRatioHeight,
	DWORD dwRatioControlHeight, DWORD dwRatioFontSize,
	DWORD dwRatioSpace, DWORD dwRatioPadding,
	AutoSizeMode mode)
{
	// 构造比例信息
	RatioInfo ratioInfo;
	ratioInfo.dwRatioWidth = dwRatioWidth;
	ratioInfo.dwRatioHeight = dwRatioHeight;
	ratioInfo.dwRatioControlHeight = dwRatioControlHeight;
	ratioInfo.dwRatioFontSize = dwRatioFontSize;
	ratioInfo.dwRatioSpace = dwRatioSpace;
	ratioInfo.dwRatioPadding = dwRatioPadding;
	return CreateByRatio(ratioInfo, mode);
}
// 函数: CreateByFixedSize
// 描述: 通过固定尺寸参数创建 AutoSize 对象 (但是需要指定 sizeMode)
AutoSize AutoSize::CreateByFixedSize(const SizeInfoDWORD& sizeInfo, AutoSizeMode mode)
{
	// 确定参考基准数值
	DWORD dwBaseValue = 0;
	switch (mode)
	{
	case AUTO_SIZE_BY_SCREEN_WIDTH:
		dwBaseValue = RefSize::GetScreenWidth();
		break;
	case AUTO_SIZE_BY_SCREEN_HEIGHT:
		dwBaseValue = RefSize::GetScreenHeight();
		break;
	case AUTO_SIZE_BY_WINDOW_WIDTH:
		dwBaseValue = sizeInfo.windowWidth;
		break;
	case AUTO_SIZE_BY_WINDOW_HEIGHT:
		dwBaseValue = sizeInfo.windowHeight;
		break;
	default:
		dwBaseValue = RefSize::GetScreenWidth();
		break;
	}
	// 计算比例信息
	RatioInfo ratioInfo = CalculateRatioBySize(dwBaseValue, sizeInfo);
	return AutoSize(sizeInfo, ratioInfo, dwBaseValue, mode);
}
AutoSize AutoSize::CreateByFixedSize(
	DWORD dwWindowWidth, DWORD dwWindowHeight,
	DWORD dwControlHeight, DWORD dwFontSize,
	DWORD dwSpace, DWORD dwPadding,
	AutoSizeMode mode)
{
	// 构造尺寸信息
	SizeInfoDWORD sizeInfo;
	sizeInfo.screenWidth = RefSize::GetScreenWidth();
	sizeInfo.screenHeight = RefSize::GetScreenHeight();
	sizeInfo.windowWidth = dwWindowWidth;
	sizeInfo.windowHeight = dwWindowHeight;
	sizeInfo.controlHeight = dwControlHeight;
	sizeInfo.fontSize = dwFontSize;
	sizeInfo.space = dwSpace;
	sizeInfo.padding = dwPadding;
	return CreateByFixedSize(sizeInfo, mode);
}

// 函数: SetWindowRect
// 描述: 设置窗口矩形 (用于更新窗口宽度和高度)
BOOL AutoSize::SetWindowRect(DWORD dwWindowWidth, DWORD dwWindowHeight)
{
	// 仅在基于窗口尺寸的模式下有效
	if (sizeMode != AUTO_SIZE_BY_WINDOW_WIDTH && sizeMode != AUTO_SIZE_BY_WINDOW_HEIGHT)
	{
		return FALSE;
	}
	// 更新窗口尺寸
	sizeDWORD.windowWidth = dwWindowWidth;
	sizeDWORD.windowHeight = dwWindowHeight;
	sizeFLOAT.windowWidth = static_cast<FLOAT>(dwWindowWidth);
	sizeFLOAT.windowHeight = static_cast<FLOAT>(dwWindowHeight);
	// 重新计算参考基准数值
	if (sizeMode == AUTO_SIZE_BY_WINDOW_WIDTH)
	{
		dwBaseValue = dwWindowWidth;
	}
	else
	{
		dwBaseValue = dwWindowHeight;
	}
	// 重新计算其他尺寸信息
	sizeDWORD = CalculateSizeByRatio(dwBaseValue, ratioInfo);
	sizeFLOAT = ConvertSizeInfo<DWORD, FLOAT>(sizeDWORD);
	return TRUE;
}
// 函数: SetRatio
// 描述: 设置比例参数 (用于更新比例参数)
BOOL AutoSize::SetRatio(const RatioInfo& ratioInfo)
{
	// 更新比例信息
	this->ratioInfo = ratioInfo;
	// 重新计算尺寸信息
	sizeDWORD = CalculateSizeByRatio(dwBaseValue, ratioInfo);
	sizeFLOAT = ConvertSizeInfo<DWORD, FLOAT>(sizeDWORD);
	return TRUE;
}
BOOL AutoSize::SetRatio(
	DWORD dwRatioWidth, DWORD dwRatioHeight,
	DWORD dwRatioControlHeight, DWORD dwRatioFontSize,
	DWORD dwRatioSpace, DWORD dwRatioPadding)
{
	// 构造比例信息
	RatioInfo ratioInfo;
	ratioInfo.dwRatioWidth = dwRatioWidth;
	ratioInfo.dwRatioHeight = dwRatioHeight;
	ratioInfo.dwRatioControlHeight = dwRatioControlHeight;
	ratioInfo.dwRatioFontSize = dwRatioFontSize;
	ratioInfo.dwRatioSpace = dwRatioSpace;
	ratioInfo.dwRatioPadding = dwRatioPadding;
	return SetRatio(ratioInfo);
}
// 函数: SetSize
// 描述: 设置固定尺寸参数 (用于更新固定尺寸参数)
BOOL AutoSize::SetSize(const SizeInfoDWORD& sizeInfo)
{
	// 更新尺寸信息
	this->sizeDWORD = sizeInfo;
	// 重新计算比例信息
	this->ratioInfo = CalculateRatioBySize(dwBaseValue, sizeInfo);
	// 计算浮点数尺寸信息
	this->sizeFLOAT = ConvertSizeInfo<DWORD, FLOAT>(sizeInfo);
	return TRUE;
}
BOOL AutoSize::SetSize(const SizeInfoFLOAT& sizeInfo)
{
	// 更新尺寸信息
	this->sizeFLOAT = sizeInfo;
	// 计算整数尺寸信息
	this->sizeDWORD = ConvertSizeInfo<FLOAT, DWORD>(sizeInfo);
	// 重新计算比例信息
	this->ratioInfo = CalculateRatioBySize(dwBaseValue, this->sizeDWORD);
	return TRUE;
}
// 模板化函数区域，用于获取尺寸信息
// 每一个函数定义都在 BasicDesign.h 中进行了声明
// 然后在此处进行实现
// 并且每一个实现之后都紧跟着一个模板实例化声明（包含两个常用类型 DWORD 和 FLOAT）

template<typename numerical>
CONST AutoSize::SizeInfo<numerical>& AutoSize::GetSizeInfoRef() const
{
	static AutoSize::SizeInfo<numerical> defaultSizeInfo;
	defaultSizeInfo = ConvertSizeInfo<DWORD, numerical>(sizeDWORD);
	return defaultSizeInfo;
}
// 模板特例化: 获取 DWORD 类型尺寸信息引用
template<>
CONST AutoSize::SizeInfo<DWORD>& AutoSize::GetSizeInfoRef<DWORD>() const
{
	return sizeDWORD;
}
// 模板特例化: 获取 FLOAT 类型尺寸信息引用
template<>
CONST AutoSize::SizeInfo<FLOAT>& AutoSize::GetSizeInfoRef<FLOAT>() const
{
	return sizeFLOAT;
}

// 函数: GetSizeInfo
// 描述: 获取尺寸信息
template<typename numerical>
AutoSize::SizeInfo<numerical> AutoSize::GetSizeInfo() const
{
	return GetSizeInfoRef<numerical>();
}
// 函数: GetScreenWidth
// 描述: 获取屏幕宽度
template<typename numerical>
numerical AutoSize::GetScreenWidth() const
{
	return GetSizeInfoRef<numerical>().screenWidth;
}
// 函数: GetScreenHeight
// 描述: 获取屏幕高度
template<typename numerical>
numerical AutoSize::GetScreenHeight() const
{
	return GetSizeInfoRef<numerical>().screenHeight;
}
// 函数: GetWindowWidth
// 描述: 获取窗口宽度
template<typename numerical>
numerical AutoSize::GetWindowWidth() const
{
	return GetSizeInfoRef<numerical>().windowWidth;
}
// 函数: GetWindowHeight
// 描述: 获取窗口高度
template<typename numerical>
numerical AutoSize::GetWindowHeight() const
{
	return GetSizeInfoRef<numerical>().windowHeight;
}
// 函数: GetControlHeight
// 描述: 获取控件高度
template<typename numerical>
numerical AutoSize::GetControlHeight() const
{
	return GetSizeInfoRef<numerical>().controlHeight;
}
// 函数: GetFontSize
// 描述: 获取字体大小
template<typename numerical>
numerical AutoSize::GetFontSize() const
{
	return GetSizeInfoRef<numerical>().fontSize;
}
// 函数: GetSpace
// 描述: 获取设计间隔
template<typename numerical>
numerical AutoSize::GetSpace() const
{
	return GetSizeInfoRef<numerical>().space;
}
// 函数: GetPadding
// 描述: 获取设计内边距
template<typename numerical>
numerical AutoSize::GetPadding() const
{
	return GetSizeInfoRef<numerical>().padding;
}

// 模板实例化声明
template AutoSize::SizeInfo<DWORD> AutoSize::GetSizeInfo<DWORD>() const;
template AutoSize::SizeInfo<FLOAT> AutoSize::GetSizeInfo<FLOAT>() const;
template DWORD AutoSize::GetScreenWidth<DWORD>() const;
template FLOAT AutoSize::GetScreenWidth<FLOAT>() const;
template DWORD AutoSize::GetScreenHeight<DWORD>() const;
template FLOAT AutoSize::GetScreenHeight<FLOAT>() const;
template DWORD AutoSize::GetWindowWidth<DWORD>() const;
template FLOAT AutoSize::GetWindowWidth<FLOAT>() const;
template DWORD AutoSize::GetWindowHeight<DWORD>() const;
template FLOAT AutoSize::GetWindowHeight<FLOAT>() const;
template DWORD AutoSize::GetControlHeight<DWORD>() const;
template FLOAT AutoSize::GetControlHeight<FLOAT>() const;
template DWORD AutoSize::GetFontSize<DWORD>() const;
template FLOAT AutoSize::GetFontSize<FLOAT>() const;
template DWORD AutoSize::GetSpace<DWORD>() const;
template FLOAT AutoSize::GetSpace<FLOAT>() const;
template DWORD AutoSize::GetPadding<DWORD>() const;
template FLOAT AutoSize::GetPadding<FLOAT>() const;

// ==================== 实现 ====================
void MatchCR::InitFromMode(MatchCR::DEFAULT_MODE mode)
{
	// 默认亮色主题
	static const DWORD lightColors[8] =
	{
		0xFFEFEFEF,  // back
		0xFFE5E5E5,  // surface
		0xFFD1D1D1,  // border
		0xFFDBDBDB,  // hover
		0xFF9E9E9E,  // secondary
		0xFF6B6B6B,  // primary
		0xFF2F2F2F,  // emphasis
		0xFF2F2F2F   // overlay
	};
	// 默认暗色主题
	static const DWORD darkColors[8] =
	{
		0xFF2F2F2F,
		0xFF3A3A3A,
		0xFF505050,
		0xFF454545,
		0xFF7D7D7D,
		0xFFB0B0B0,
		0xFFEFEFEF,
		0xFFEFEFEF
	};

	const DWORD* src = nullptr;
	switch (mode)
	{
	case MatchCR::DEFAULT_MODE_LIGHT:
		src = lightColors;
		break;
	case MatchCR::DEFAULT_MODE_DARK:
		src = darkColors;
		break;
	case MatchCR::DEFAULT_MODE_CUSTOM:
		// 自定义模式不在此初始化，颜色应由外部传入
		return;
	}
	if (src) {
		for (int i = 0; i < 8; ++i) {
			m_colors[i] = src[i];
		}
	}
}

MatchCR::MatchCR(MatchCR::DEFAULT_MODE mode) : m_mode(mode)
{
	InitFromMode(mode);
}

MatchCR::MatchCR(const ColorsInfo& colors) : m_mode(MatchCR::DEFAULT_MODE_CUSTOM)
{
	m_colors[0] = colors.back;
	m_colors[1] = colors.surface;
	m_colors[2] = colors.border;
	m_colors[3] = colors.hover;
	m_colors[4] = colors.secondary;
	m_colors[5] = colors.primary;
	m_colors[6] = colors.emphasis;
	m_colors[7] = colors.overlay;
}

MatchCR::MatchCR(const DWORD preset[8]) : m_mode(MatchCR::DEFAULT_MODE_CUSTOM)
{
	for (int i = 0; i < 8; ++i)
	{
		m_colors[i] = preset[i];
	}
}

MatchCR::DEFAULT_MODE MatchCR::GetMode() const
{
	return m_mode;
}

MatchCR::ColorsInfo MatchCR::GetColorsInfo() const
{
	ColorsInfo info;
	info.back = m_colors[0];
	info.surface = m_colors[1];
	info.border = m_colors[2];
	info.hover = m_colors[3];
	info.secondary = m_colors[4];
	info.primary = m_colors[5];
	info.emphasis = m_colors[6];
	info.overlay = m_colors[7];
	return info;
}

void MatchCR::SetColorsInfo(const ColorsInfo& colors)
{
	m_mode = MatchCR::DEFAULT_MODE_CUSTOM;
	m_colors[0] = colors.back;
	m_colors[1] = colors.surface;
	m_colors[2] = colors.border;
	m_colors[3] = colors.hover;
	m_colors[4] = colors.secondary;
	m_colors[5] = colors.primary;
	m_colors[6] = colors.emphasis;
	m_colors[7] = colors.overlay;
}

DWORD MatchCR::GetBack() const { return m_colors[0]; }
void MatchCR::SetBack(DWORD color) { m_colors[0] = color; m_mode = MatchCR::DEFAULT_MODE_CUSTOM; }

DWORD MatchCR::GetSurface() const { return m_colors[1]; }
void MatchCR::SetSurface(DWORD color) { m_colors[1] = color; m_mode = MatchCR::DEFAULT_MODE_CUSTOM; }

DWORD MatchCR::GetBorder() const { return m_colors[2]; }
void MatchCR::SetBorder(DWORD color) { m_colors[2] = color; m_mode = MatchCR::DEFAULT_MODE_CUSTOM; }

DWORD MatchCR::GetHover() const { return m_colors[3]; }
void MatchCR::SetHover(DWORD color) { m_colors[3] = color; m_mode = MatchCR::DEFAULT_MODE_CUSTOM; }

DWORD MatchCR::GetSecondary() const { return m_colors[4]; }
void MatchCR::SetSecondary(DWORD color) { m_colors[4] = color; m_mode = MatchCR::DEFAULT_MODE_CUSTOM; }

DWORD MatchCR::GetPrimary() const { return m_colors[5]; }
void MatchCR::SetPrimary(DWORD color) { m_colors[5] = color; m_mode = MatchCR::DEFAULT_MODE_CUSTOM; }

DWORD MatchCR::GetEmphasis() const { return m_colors[6]; }
void MatchCR::SetEmphasis(DWORD color) { m_colors[6] = color; m_mode = MatchCR::DEFAULT_MODE_CUSTOM; }

DWORD MatchCR::GetOverlay() const { return m_colors[7]; }
void MatchCR::SetOverlay(DWORD color) { m_colors[7] = color; m_mode = MatchCR::DEFAULT_MODE_CUSTOM; }

const DWORD* MatchCR::GetColors() const
{
	return m_colors;
}