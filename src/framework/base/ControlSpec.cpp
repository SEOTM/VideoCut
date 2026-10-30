#include "framework/base/ControlSpec.h"

// 函数: IsKnownType
// 描述: 判断渲染层是否认识本控件类型(未知类型与未使用槽位都不认识)
BOOL CtrlInfo::IsKnownType() const
{
	return (type != CTRL_TYPE_INVALID && type != CTRL_TYPE_UNKNOWN) ? TRUE : FALSE;
}
