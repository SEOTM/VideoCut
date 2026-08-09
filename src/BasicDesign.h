#pragma once
#include <minwindef.h>

#define MAKE_CR_IMVEC4(dw) { ((dw) & 0xFF) / 255.0f, (((dw) >> 8) & 0xFF) / 255.0f, (((dw) >> 16) & 0xFF) / 255.0f, ((dw) >> 24) / 255.0f }

// ================================
// 类名：RefSize
// 描述：通过 RefSize 单例类可以获取当前窗口下的参考大小
// ================================
class RefSize
{
private:
	// 函数：RefSize
	// 描述：隐藏构造函数入口
	RefSize() {};
	// 函数：~RefSize
	// 描述：隐藏析构函数入口
	~RefSize() {};
public:
	// 函数：GetScreenWidth
	// 功能：获取屏幕宽度
	static DWORD GetScreenWidth();
	// 函数：GetScreenHeight
	// 功能：获取屏幕高度
	static DWORD GetScreenHeight();
	// 函数：GetContrlHeight
	// 功能：获取控件高度
	static DWORD GetContrlHeight();
	// 函数：GetFontSize
	// 功能：获取字体大小
	static DWORD GetFontSize();
	// 函数：GetSpace
	// 功能：获取设计间隔
	static DWORD GetSpace();
};

// ================================
// 类名: AutoSize
// 描述: 根据参考大小自动调整尺寸的类 (与 RefSize 不同的是，可以传递经验参数)
// ================================
class AutoSize
{
public:
	// 枚举类型，决定尺寸计算基于下述哪一情况
	enum AutoSizeMode
	{
		AUTO_SIZE_BY_SCREEN_WIDTH,	// 基于屏幕宽度
		AUTO_SIZE_BY_SCREEN_HEIGHT,	// 基于屏幕高度
		AUTO_SIZE_BY_WINDOW_WIDTH,	// 基于窗口宽度
		AUTO_SIZE_BY_WINDOW_HEIGHT	// 基于窗口高度
	};

	// 模板类型（以结构体的形式返回所有尺寸信息，方便结构化绑定）

	// 结构: SizeInfo
	// 描述: 存储尺寸信息的结构体模板
	template<typename numerical>
	struct SizeInfo
	{
		numerical screenWidth;		// 屏幕宽度
		numerical screenHeight;		// 屏幕高度
		numerical windowWidth;		// 窗口宽度
		numerical windowHeight;		// 窗口高度
		numerical controlHeight;		// 控件高度
		numerical fontSize;			// 字体大小
		numerical space;				// 设计间隔
		numerical padding;			// 设计内边距
	};
	// 为两种常见类型定义别名
	using SizeInfoDWORD = SizeInfo<DWORD>;	// 整型尺寸信息
	using SizeInfoFLOAT = SizeInfo<FLOAT>;	// 浮点型尺寸信息

	// 结构: RatioInfo
	// 描述: 存储比例信息的结构体
	struct RatioInfo
	{
		DWORD dwRatioWidth;			// 窗口宽度占参考基准数值的比例 (千分比，范围 0 - 1000)
		DWORD dwRatioHeight;		// 窗口高度占参考基准数值的比例 (千分比，范围 0 - 1000)
		DWORD dwRatioControlHeight; // 控件高度占参考基准数值的比例 (千分比，范围 0 - 1000)
		DWORD dwRatioFontSize;		// 字体大小占参考基准数值的比例 (千分比，范围 0 - 1000)
		DWORD dwRatioSpace;			// 设计间隔占参考基准数值的比例 (千分比，范围 0 - 1000)
		DWORD dwRatioPadding;		// 设计内边距占参考基准数值的比例 (千分比，范围 0 - 1000)
	};
private:
	// 整数成员变量
	SizeInfoDWORD sizeDWORD;
	// 浮点数成员变量
	SizeInfoFLOAT sizeFLOAT;

	// 比例信息
	AutoSizeMode sizeMode;	// 尺寸计算模式
	DWORD dwBaseValue;		// 参考基准数值 (根据 sizeMode 决定是屏幕宽度、屏幕高度、窗口宽度还是窗口高度)
	RatioInfo ratioInfo;	// 比例信息

	// 由于我们使用多种方式来初始化自动尺寸类，因此将初始化逻辑封装在私有函数中
	// 因此构造函数应当作为私有函数方式存在
	// 实际上初始化时由多种不同工厂函数来调用这些私有构造函数

	// 函数: AutoSize
	// 描述: 将输入尺寸保存，并自动扩展为其他尺寸类型，同时保存比例信息
	AutoSize(const SizeInfoDWORD& sizeInfo, RatioInfo& ratioInfo, DWORD dwBaseValue, AutoSizeMode sizeMode);
	AutoSize(const SizeInfoFLOAT& sizeInfo, RatioInfo& ratioInfo, DWORD dwBaseValue, AutoSizeMode sizeMode);
	// 函数: GetSizeInfoRef
	// 描述: 获取尺寸信息引用 (用于模板函数调用)
	template<typename numerical>
	CONST SizeInfo<numerical>& GetSizeInfoRef() const;
public:
	// 函数: CreateByRatio
	// 描述: 通过比例参数创建 AutoSize 对象
	// 参数:
	//		sizeMode: 尺寸计算模式，默认基于屏幕宽度
	//		dwRatioWidth: 窗口宽度占参考基准数值的比例 (千分比，范围 0 - 1000)
	//		dwRatioHeight: 窗口高度占参考基准数值的比例 (千分比，范围 0 - 1000)
	//		dwRatioControlHeight: 控件高度占参考基准数值的比例 (千分比，范围 0 - 1000)
	//		dwRatioFontSize: 字体大小占参考基准数值的比例 (千分比，范围 0 - 1000)
	// 		dwRatioSpace: 设计间隔占参考基准数值的比例 (千分比，范围 0 - 1000)
	//		dwRatioPadding: 设计内边距占参考基准数值的比例 (千分比，范围 0 - 1000)
	static AutoSize CreateByRatio(
		DWORD dwRatioWidth, DWORD dwRatioHeight,
		DWORD dwRatioControlHeight, DWORD dwRatioFontSize,
		DWORD dwRatioSpace, DWORD dwRatioPadding,
		AutoSizeMode sizeMode = AUTO_SIZE_BY_SCREEN_WIDTH);
	static AutoSize CreateByRatio(
		const RatioInfo& ratioInfo,
		AutoSizeMode sizeMode = AUTO_SIZE_BY_SCREEN_WIDTH);
	// 函数: CreateByFixedSize
	// 描述: 通过固定尺寸参数创建 AutoSize 对象 (但是需要指定 sizeMode)
	// 备注: 该函数会根据 sizeMode 自动计算参考基准数值，默认情况下参考基准数值为屏幕宽度
	static AutoSize CreateByFixedSize(
		DWORD dwWindowWidth, DWORD dwWindowHeight,
		DWORD dwControlHeight, DWORD dwFontSize,
		DWORD dwSpace, DWORD dwPadding,
		AutoSizeMode sizeMode = AUTO_SIZE_BY_SCREEN_WIDTH);
	static AutoSize CreateByFixedSize(
		const SizeInfoDWORD& sizeInfo,
		AutoSizeMode sizeMode = AUTO_SIZE_BY_SCREEN_WIDTH);
	// 函数: SetWindowRect
	// 描述: 设置窗口矩形 (用于更新窗口宽度和高度)
	// 备注: MODE == AUTO_SIZE_BY_WINDOW_WIDTH 或 AUTO_SIZE_BY_WINDOW_HEIGHT 时调用该函数有效
	BOOL SetWindowRect(DWORD dwWindowWidth, DWORD dwWindowHeight);

	// 函数: SetRatio
	// 描述: 设置比例参数 (用于更新比例参数)
	BOOL SetRatio(
		DWORD dwRatioWidth, DWORD dwRatioHeight,
		DWORD dwRatioControlHeight, DWORD dwRatioFontSize,
		DWORD dwRatioSpace, DWORD dwRatioPadding);
	BOOL SetRatio(const RatioInfo& ratioInfo);

	// 函数: SetSize
	// 描述: 设置固定尺寸参数 (用于更新固定尺寸参数)
	// 备注: 只接受 SizeInfoDWORD 类型 / SIZE_INFO_FLOAT 类型参数，并将 ScreenWidth 和 ScreenHeight 忽略
	BOOL SetSize(const SizeInfoDWORD& sizeInfo);
	BOOL SetSize(const SizeInfoFLOAT& sizeInfo);

	// 模板化函数区域，用于获取尺寸信息

	// 函数: GetSizeInfo
	// 描述: 获取尺寸信息
	template<typename numerical>
	SizeInfo<numerical> GetSizeInfo() const;
	// 函数: GetScreenWidth
	// 描述: 获取屏幕宽度
	template<typename numerical>
	numerical GetScreenWidth() const;
	// 函数: GetScreenHeight
	// 描述: 获取屏幕高度
	template<typename numerical>
	numerical GetScreenHeight() const;
	// 函数: GetWindowWidth
	// 描述: 获取窗口宽度
	template<typename numerical>
	numerical GetWindowWidth() const;
	// 函数: GetWindowHeight
	// 描述: 获取窗口高度
	template<typename numerical>
	numerical GetWindowHeight() const;
	// 函数: GetControlHeight
	// 描述: 获取控件高度
	template<typename numerical>
	numerical GetControlHeight() const;
	// 函数: GetFontSize
	// 描述: 获取字体大小
	template<typename numerical>
	numerical GetFontSize() const;
	// 函数: GetSpace
	// 描述: 获取设计间隔
	template<typename numerical>
	numerical GetSpace() const;
	// 函数: GetPadding
	// 描述: 获取设计内边距
	template<typename numerical>
	numerical GetPadding() const;
};

// ================================
// 类名: MatchCR
// 描述: 管理颜色主题的类，支持预设亮色/暗色模式及自定义颜色配置
// ================================
class MatchCR
{
public:
	// 枚举: DEFAULT_MODE
	// 描述: 主题模式枚举，用于指定颜色方案
	enum DEFAULT_MODE {
		DEFAULT_MODE_LIGHT = 0,	// 亮色主题（默认）
		DEFAULT_MODE_DARK = 1,		// 暗色主题
		DEFAULT_MODE_CUSTOM = 2	// 用户自定义模式
	};

	// 结构: ColorsInfo
	// 描述: 颜色信息结构体，方便整体传入和返回（类似 AutoSize::SizeInfo）
	struct ColorsInfo 
	{
		DWORD back;       // 背景色 (ARGB)
		DWORD surface;    // 表面色 (ARGB)
		DWORD border;     // 边框色 (ARGB)
		DWORD hover;      // 悬停色 (ARGB)
		DWORD secondary;  // 次要色 (ARGB)
		DWORD primary;    // 常规色 (ARGB)
		DWORD emphasis;   // 强调色 (ARGB)
		DWORD overlay;    // 叠加色 (ARGB)

		// 构造函数: ColorsInfo (默认)
		// 描述: 默认构造函数，初始化为亮色主题的预设值
		ColorsInfo() {
			back = 0xFFEFEFEF;
			surface = 0xFFE5E5E5;
			border = 0xFFD1D1D1;
			hover = 0xFFDBDBDB;
			secondary = 0xFF9E9E9E;
			primary = 0xFF6B6B6B;
			emphasis = 0xFF2F2F2F;
			overlay = 0xFF2F2F2F;
		}

		// 构造函数: ColorsInfo (从数组)
		// 描述: 从包含8个颜色的数组构造结构体
		// 参数:
		//		colors: 长度为8的DWORD数组，依次对应 back, surface, border, hover, secondary, primary, emphasis, overlay
		ColorsInfo(const DWORD colors[8]) {
			back = colors[0];
			surface = colors[1];
			border = colors[2];
			hover = colors[3];
			secondary = colors[4];
			primary = colors[5];
			emphasis = colors[6];
			overlay = colors[7];
		}

		// 函数: ToArray
		// 描述: 将颜色信息转换为数组
		// 参数:
		//		out: 输出数组，长度为8，依次存放 back, surface, border, hover, secondary, primary, emphasis, overlay
		VOID ToArray(DWORD out[8]) const {
			out[0] = back;
			out[1] = surface;
			out[2] = border;
			out[3] = hover;
			out[4] = secondary;
			out[5] = primary;
			out[6] = emphasis;
			out[7] = overlay;
		}
	};
private:
	DEFAULT_MODE m_mode;	// 当前主题模式
	DWORD m_colors[8];		// 颜色数组，按 ColorsInfo 顺序存储

	// 函数: InitFromMode
	// 描述: 内部初始化函数，根据给定模式设置颜色数组
	// 参数:
	//		mode: 主题模式（亮色/暗色）
	VOID InitFromMode(DEFAULT_MODE mode);
public:
	// 构造函数: MatchCR (指定模式)
	// 描述: 根据预设模式（亮色/暗色）构造对象，模式为自定义时需自行设置颜色
	// 参数:
	//		mode: 主题模式，默认为亮色主题
	MatchCR(DEFAULT_MODE mode = DEFAULT_MODE_LIGHT);

	// 构造函数: MatchCR (通过 ColorsInfo)
	// 描述: 通过 ColorsInfo 结构体自定义全部颜色，模式自动设为自定义模式
	// 参数:
	//		colors: 包含全部颜色信息的结构体
	explicit MatchCR(const ColorsInfo& colors);

	// 构造函数: MatchCR (通过数组)
	// 描述: 通过颜色数组自定义全部颜色，模式自动设为自定义模式
	// 参数:
	//		preset: 长度为8的DWORD数组，依次对应 back, surface, border, hover, secondary, primary, emphasis, overlay
	MatchCR(const DWORD preset[8]);

	// 函数: GetMode
	// 描述: 获取当前主题模式
	// 返回:
	//		当前模式枚举值
	DEFAULT_MODE GetMode() const;

	// 函数: GetColorsInfo
	// 描述: 获取整个颜色信息结构体
	// 返回:
	//		包含所有颜色的 ColorsInfo 结构体
	ColorsInfo GetColorsInfo() const;

	// 函数: SetColorsInfo
	// 描述: 设置整个颜色信息结构体，模式自动设为自定义模式
	// 参数:
	//		colors: 新的颜色信息结构体
	VOID SetColorsInfo(const ColorsInfo& colors);

	// 函数: GetBack
	// 描述: 获取背景色
	DWORD GetBack() const;
	// 函数: SetBack
	// 描述: 设置背景色，模式自动设为自定义模式
	VOID SetBack(DWORD color);

	// 函数: GetSurface
	// 描述: 获取表面色
	DWORD GetSurface() const;
	// 函数: SetSurface
	// 描述: 设置表面色，模式自动设为自定义模式
	VOID SetSurface(DWORD color);

	// 函数: GetBorder
	// 描述: 获取边框色
	DWORD GetBorder() const;
	// 函数: SetBorder
	// 描述: 设置边框色，模式自动设为自定义模式
	VOID SetBorder(DWORD color);

	// 函数: GetHover
	// 描述: 获取悬停色
	DWORD GetHover() const;
	// 函数: SetHover
	// 描述: 设置悬停色，模式自动设为自定义模式
	VOID SetHover(DWORD color);

	// 函数: GetSecondary
	// 描述: 获取次要色
	DWORD GetSecondary() const;
	// 函数: SetSecondary
	// 描述: 设置次要色，模式自动设为自定义模式
	VOID SetSecondary(DWORD color);

	// 函数: GetPrimary
	// 描述: 获取常规色
	DWORD GetPrimary() const;
	// 函数: SetPrimary
	// 描述: 设置常规色，模式自动设为自定义模式
	VOID SetPrimary(DWORD color);

	// 函数: GetEmphasis
	// 描述: 获取强调色
	DWORD GetEmphasis() const;
	// 函数: SetEmphasis
	// 描述: 设置强调色，模式自动设为自定义模式
	VOID SetEmphasis(DWORD color);

	// 函数: GetOverlay
	// 描述: 获取叠加色
	DWORD GetOverlay() const;
	// 函数: SetOverlay
	// 描述: 设置叠加色，模式自动设为自定义模式
	VOID SetOverlay(DWORD color);

	// 函数: GetColors
	// 描述: 获取全部颜色数组（只读）
	// 返回:
	//		指向内部颜色数组的常量指针，长度为8
	const DWORD* GetColors() const;
};


// 函数: 重载 SizeInfo 乘法运算
// 重载: 类型相同的 SizeInfo 逐成员相乘，返回相同数值类型的 SizeInfo
template<typename oper>
inline AutoSize::SizeInfo<oper> operator*(const AutoSize::SizeInfo<oper>& a, const AutoSize::SizeInfo<oper>& b)
{
	AutoSize::SizeInfo<oper> ret;
	ret.screenWidth = static_cast<oper>(a.screenWidth * b.screenWidth);
	ret.screenHeight = static_cast<oper>(a.screenHeight * b.screenHeight);
	ret.windowWidth = static_cast<oper>(a.windowWidth * b.windowWidth);
	ret.windowHeight = static_cast<oper>(a.windowHeight * b.windowHeight);
	ret.controlHeight = static_cast<oper>(a.controlHeight * b.controlHeight);
	ret.fontSize = static_cast<oper>(a.fontSize * b.fontSize);
	ret.space = static_cast<oper>(a.space * b.space);
	ret.padding = static_cast<oper>(a.padding * b.padding);
	return ret;
}
