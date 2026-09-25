// -*- coding: utf-8 -*-
// 世界空间 POI 名称标签的内容控件：青色描边深色底框 + "◆ 名称"。
// 必须走 UMG/Slate 路径：UTextRenderComponent 的 SceneProxy 对 RUNTIME
// UFont 会在渲染线程直接早退，中文一个字都画不出来。
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FCPoiLabelWidget.generated.h"

class UTextBlock;

UCLASS()
class TRAE_UE_PROJECT_API UFCPoiLabelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	void SetLabelText(const FText& InText);

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> LabelText;
};
