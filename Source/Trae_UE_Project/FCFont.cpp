// -*- coding: utf-8 -*-
#include "FCFont.h"

#include "Fonts/CompositeFont.h"
#include "UObject/Package.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"

UFont* FcGetFont()
{
	// simfang.ttf 以 INLINE 方式导入为 UFontFace 资产（运行期提供中文字形）。
	// 裸 UFontFace 不实现 IFontProviderInterface，Slate 无法据此解析 composite font，
	// 会回退到 LastResort（每个汉字都渲染成同一个占位字形）。
	// 这里把 face 包进一个 transient RUNTIME UFont（Default typeface 引用该 face），
	// 该 UFont 才是 FSlateFontInfo 期望的 IFontProviderInterface，可正确光栅化所有字形。
	static UFont* Font = []() -> UFont*
	{
		UFontFace* Face = LoadObject<UFontFace>(
			nullptr, TEXT("/Game/ForbiddenCity/F_FangSong.F_FangSong"));
		if (!Face)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FC] CJK FontFace load: NULL -> fallback small font"));
			return nullptr;
		}

		UPackage* Pkg = NewObject<UPackage>(nullptr, TEXT("/Temp/FC_FontPackage"),
			RF_Transient | RF_Standalone);
		Pkg->AddToRoot();
		UFont* Wrapper = NewObject<UFont>(Pkg, TEXT("FC_FangSongRT"), RF_Public | RF_Transient);
		Wrapper->FontCacheType = EFontCacheType::Runtime;

		const FName DefaultName(TEXT("Default"));
		FTypefaceEntry& Entry =
			Wrapper->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.AddDefaulted_GetRef();
		Entry.Name = DefaultName;
		Entry.Font = FFontData(Face, 0);

		Wrapper->AddToRoot();
		UE_LOG(LogTemp, Warning, TEXT("[FC] CJK runtime UFont built from FontFace: OK"));
		return Wrapper;
	}();
	return Font;
}

FSlateFontInfo FcMakeSlateFont(int32 Size, bool bBold)
{
	(void)bBold; // 只有 "Default" 一种字重，粗体由颜色/笔刷表达；保留形参兼容调用点。

	UFont* FontObj = FcGetFont();
	if (!FontObj)
	{
		// 只有引擎内置字体可用（无中文字形）。
		FSlateFontInfo Fallback;
		if (GEngine)
		{
			Fallback = FSlateFontInfo(GEngine->GetSmallFont(), Size);
		}
		else
		{
			Fallback.Size = Size;
		}
		return Fallback;
	}
	// 只有一个 ("Default") typeface，TypefaceFontName 保持 Default 以确保命中。
	return FSlateFontInfo(FontObj, Size, FName(TEXT("Default")));
}
