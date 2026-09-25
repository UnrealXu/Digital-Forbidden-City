// -*- coding: utf-8 -*-
// 共享中文字体 helper：把 INLINE 导入的仿宋 UFontFace 资产包装成
// transient RUNTIME UFont（Slate / UTextRenderComponent 均可渲染中文）。
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

class UFont;

/** 返回惰性创建、AddToRoot 保活的 RUNTIME UFont；加载失败返回 nullptr。 */
UFont* FcGetFont();

/** 成功则返回 FSlateFontInfo(Font, Size, "Default")；
 *  失败回退 GEngine->GetSmallFont()。bBold 不影响结果（只有 Default 一种字重）。 */
FSlateFontInfo FcMakeSlateFont(int32 Size, bool bBold = false);
