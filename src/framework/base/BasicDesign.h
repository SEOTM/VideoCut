#pragma once
#include <minwindef.h>
#include <windef.h>
#include "framework/base/mem/MemoryManager.h"

#define MAKE_CR_IMVEC4(dw) { ((dw) & 0xFF) / 255.0f, (((dw) >> 8) & 0xFF) / 255.0f, (((dw) >> 16) & 0xFF) / 255.0f, ((dw) >> 24) / 255.0f }

class ScreenRef
{
private:
	// 函数: ScreenRef
	// 描述: 隐藏构造函数入口
	ScreenRef() {};
	// 函数: ~ScreenRef
	// 描述: 隐藏析构函数入口
	~ScreenRef() {};
public:
	// 函数: GetScreenWidth
	// 描述: 取主显示器的屏幕宽度(像素)
	static DWORD GetScreenWidth();
	// 函数: GetScreenHeight
	// 描述: 取主显示器的屏幕高度(像素)
	static DWORD GetScreenHeight();
	// 函数: GetPrimaryWorkArea
	// 描述: 取主显示器的工作区矩形(桌面坐标; 已排除任务栏等保留区)
	static BOOL GetPrimaryWorkArea(RECT* pWork);
	// 函数: IsRectOnAnyMonitor
	// 描述: 判断矩形是否与任一显示器的工作区相交
	static BOOL IsRectOnAnyMonitor(CONST RECT& rc);
	// 函数: GetContrlHeight
	// 描述: 取控件高度基准(屏高 / 40)
	static DWORD GetContrlHeight();
	// 函数: GetFontSize
	// 描述: 取字号基准(屏高 * 13 / 800)
	static DWORD GetFontSize();
	// 函数: GetSpace
	// 描述: 取设计间隔基准(屏高 / 100)
	static DWORD GetSpace();
};

class AutoSize
{
public:
	enum AutoSizeMode
	{
		AUTO_SIZE_BY_SCREEN_WIDTH,
		AUTO_SIZE_BY_SCREEN_HEIGHT,
		AUTO_SIZE_BY_WINDOW_WIDTH,
		AUTO_SIZE_BY_WINDOW_HEIGHT
	};

	template<typename numerical>
	struct SizeInfo
	{
		numerical screenWidth;
		numerical screenHeight;
		numerical windowWidth;
		numerical windowHeight;
		numerical controlHeight;
		numerical fontSize;
		numerical space;
		numerical padding;
	};
	using SizeInfoDWORD = SizeInfo<DWORD>;
	using SizeInfoFLOAT = SizeInfo<FLOAT>;

	struct RatioInfo
	{
		DWORD dwRatioWidth;
		DWORD dwRatioHeight;
		DWORD dwRatioControlHeight;
		DWORD dwRatioFontSize;
		DWORD dwRatioSpace;
		DWORD dwRatioPadding;
	};
private:
	SizeInfoDWORD sizeDWORD;
	SizeInfoFLOAT sizeFLOAT;

	AutoSizeMode sizeMode;
	DWORD dwBaseValue;
	RatioInfo ratioInfo;

	// 函数: AutoSize
	// 描述: 将输入尺寸保存，并自动扩展为其他尺寸类型，同时保存比例信息
	AutoSize(const SizeInfoDWORD& sizeInfo, RatioInfo& ratioInfo, DWORD dwBaseValue, AutoSizeMode sizeMode);
	AutoSize(const SizeInfoFLOAT& sizeInfo, RatioInfo& ratioInfo, DWORD dwBaseValue, AutoSizeMode sizeMode);
	// 函数: GetSizeInfoRef
	// 描述: 获取尺寸信息引用 (用于模板函数调用)
	template<typename numerical>
	CONST SizeInfo<numerical>& GetSizeInfoRef() const;
public:
	// 函数: AutoSize
	// 描述: 默认构造函数, 各字段零初始化
	AutoSize();

	// 函数: CreateByRatio
	// 描述: 按比例参数创建 AutoSize 对象(千分比换算为八项像素值)
	static AutoSize CreateByRatio(
		DWORD dwRatioWidth, DWORD dwRatioHeight,
		DWORD dwRatioControlHeight, DWORD dwRatioFontSize,
		DWORD dwRatioSpace, DWORD dwRatioPadding,
		AutoSizeMode sizeMode = AUTO_SIZE_BY_SCREEN_WIDTH);
	static AutoSize CreateByRatio(
		const RatioInfo& ratioInfo,
		AutoSizeMode sizeMode = AUTO_SIZE_BY_SCREEN_WIDTH);
	// 函数: CreateByFixedSize
	// 描述: 按固定像素参数创建 AutoSize 对象(屏宽屏高取系统值, 基准随模式)
	static AutoSize CreateByFixedSize(
		DWORD dwWindowWidth, DWORD dwWindowHeight,
		DWORD dwControlHeight, DWORD dwFontSize,
		DWORD dwSpace, DWORD dwPadding,
		AutoSizeMode sizeMode = AUTO_SIZE_BY_SCREEN_WIDTH);
	static AutoSize CreateByFixedSize(
		const SizeInfoDWORD& sizeInfo,
		AutoSizeMode sizeMode = AUTO_SIZE_BY_SCREEN_WIDTH);
	// 函数: SetWindowRect
	// 描述: 设置窗口宽高(仅窗口模式有效; 重定基准并重算各参考量)
	BOOL SetWindowRect(DWORD dwWindowWidth, DWORD dwWindowHeight);

	// 函数: SetRatio
	// 描述: 设置比例参数 (用于更新比例参数)
	BOOL SetRatio(
		DWORD dwRatioWidth, DWORD dwRatioHeight,
		DWORD dwRatioControlHeight, DWORD dwRatioFontSize,
		DWORD dwRatioSpace, DWORD dwRatioPadding);
	BOOL SetRatio(const RatioInfo& ratioInfo);

	// 函数: SetSize
	// 描述: 设置固定尺寸参数(按基准反算比例; 入参屏宽屏高原样写入)
	BOOL SetSize(const SizeInfoDWORD& sizeInfo);
	BOOL SetSize(const SizeInfoFLOAT& sizeInfo);

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

class ProjCR
{
public:
	enum SAMPLE_DIRECTION {
		PROJ_DARK_TO_LIGHT = 0,
		PROJ_LIGHT_TO_DARK = 1
	};

	enum CR_SLOT {
		PROJ_SLOT_SURFACE = 0,
		PROJ_SLOT_HOVER = 1,
		PROJ_SLOT_ACTIVE = 2,
		PROJ_SLOT_CONTRAST = 3
	};

	using SampleFunc = FLOAT (*)(DWORD dwParam, DWORD dwLayers);

	static constexpr FLOAT DEFAULT_CONTRAST_STRENGTH = 1.0f;
	static constexpr FLOAT DEFAULT_BORDER_STRENGTH = 0.25f;
	static constexpr FLOAT SLOT_OFFSET_RATIO = 0.75f;
private:
	DWORD crBase;
	DWORD crLight;
	SAMPLE_DIRECTION eDirection;
	DWORD dwLayers;
	FLOAT fContrastStrength;
	FLOAT fBorderStrength;
	SampleFunc pfnSample;
	BufferInfo layers;

	// 函数: ProjCR
	// 描述: 私有构造(仅工厂 Create 可用 ⇒ 未经校验无法构造)
	ProjCR();
	// 函数: ~ProjCR
	// 描述: 私有析构(归还轴位置表; 对象本体由 Release 释放)
	~ProjCR();

	// 函数: IsSampleFuncValid
	// 描述: 逐层调用校验采样函数(值域 0..1 且序列非严格单调)
	static BOOL IsSampleFuncValid(SampleFunc pfnSample, DWORD dwLayers);

	// 函数: PushToFarEnd
	// 描述: 远端法: 把轴位置朝"离它更远的那一端"推进 ρ 比例(对比槽与边框共用)
	static FLOAT PushToFarEnd(FLOAT fAxis, FLOAT fStrength);

	// 函数: AxisToColor
	// 描述: 轴位置 → 颜色(按底色端/光色端逐字节线性插值)
	DWORD AxisToColor(FLOAT fAxis) const;

	// 函数: BlendColor
	// 描述: 两色逐字节线性插值(比例 0 = 前者; 1 = 后者; 含 α 字节)
	static DWORD BlendColor(DWORD crFrom, DWORD crTo, FLOAT fRatio);

	// 函数: AllocLayers
	// 描述: 按层数分配轴位置表(小对象池优先, 超出池上限回落线性分配器)
	static BufferInfo AllocLayers(DWORD dwLayers);

	// 函数: FreeLayers
	// 描述: 归还轴位置表(仅池分配者可单独回收)
	static VOID FreeLayers(BufferInfo info);

	// 函数: RebuildLayers
	// 描述: 重算轴位置表(方向/层数/采样函数变更后调用)
	VOID RebuildLayers();
public:
	// 函数: SetAllocators
	// 描述: 一次性静态注入两个分配器(全进程一份; 传 NULL = 解绑)
	static VOID SetAllocators(Arena* arena, MemPool* pool);

	// 函数: Create
	// 描述: 工厂构造(先校验层数与采样函数, 再分配表并构造)
	static BOOL Create(ProjCR*& pOut, DWORD crBase, DWORD crLight, SAMPLE_DIRECTION eDirection,
		DWORD dwLayers, SampleFunc pfnSample = NULL);

	// 函数: Release
	// 描述: 与 Create 成对: 归还轴位置表并释放对象本体(pOut 置 NULL)
	static VOID Release(ProjCR*& pOut);

	// 函数: Sample
	// 描述: 取某层的槽位色(取色正门)
	DWORD Sample(DWORD dwLayer, CR_SLOT eSlot = PROJ_SLOT_SURFACE) const;

	// 函数: SampleBorder
	// 描述: 取两个绘图层之间的边框色(表面色取中后复用对比槽的远端法)
	DWORD SampleBorder(DWORD dwLayerA, DWORD dwLayerB) const;

	// 函数: SetBase
	// 描述: 设置底色端(恒成功; 重算轴位置表)
	VOID SetBase(DWORD crBase);
	// 函数: SetLight
	// 描述: 设置光色端(恒成功; 重算轴位置表)
	VOID SetLight(DWORD crLight);
	// 函数: SetDirection
	// 描述: 设置采样方向(恒成功; 重算轴位置表) —— 亮暗模式切换入口
	VOID SetDirection(SAMPLE_DIRECTION eDirection);
	// 函数: SetLayers
	// 描述: 设置层数(先按新层数重校验采样函数, 再换块并重算)
	BOOL SetLayers(DWORD dwLayers);
	// 函数: SetSampleFunc
	// 描述: 设置单调采样函数(逐层校验; 失败沿用旧值)
	BOOL SetSampleFunc(SampleFunc pfnSample);
	// 函数: SetContrastStrength
	// 描述: 设置对比槽强度 ρ(钳到 0..1; 不改表 —— 派生是取色时现算的)
	VOID SetContrastStrength(FLOAT fStrength);
	// 函数: SetBorderStrength
	// 描述: 设置边框强度 ρ(钳到 0..1; 不改表)
	VOID SetBorderStrength(FLOAT fStrength);

	// 函数: GetBase
	// 描述: 取底色端
	DWORD GetBase() const;
	// 函数: GetLight
	// 描述: 取光色端
	DWORD GetLight() const;
	// 函数: GetDirection
	// 描述: 取采样方向
	SAMPLE_DIRECTION GetDirection() const;
	// 函数: GetLayers
	// 描述: 取层数
	DWORD GetLayers() const;
	// 函数: GetContrastStrength
	// 描述: 取对比槽强度
	FLOAT GetContrastStrength() const;
	// 函数: GetBorderStrength
	// 描述: 取边框强度
	FLOAT GetBorderStrength() const;
	// 函数: GetSampleFunc
	// 描述: 取采样函数(NULL = 线性均匀)
	SampleFunc GetSampleFunc() const;
	// 函数: GetAxis
	// 描述: 取某层的轴位置(0 = 底色端; 1 = 光色端)
	FLOAT GetAxis(DWORD dwLayer) const;
	// 函数: IsInvalid
	// 描述: 取无效标志(未构造成功者恒 TRUE)
	BOOL IsInvalid() const;
};

// 函数: operator*
// 描述: 逐字段相乘两份尺寸信息(八项参考量各自相乘)
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
