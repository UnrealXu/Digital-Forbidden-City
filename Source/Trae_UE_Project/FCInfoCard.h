#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FCInfoCard.generated.h"

class UTextBlock;
class UVerticalBox;
class UCanvasPanel;
class UBorder;
class UButton;

/** Native UMG panel: persistent control hint + a POI information card.
 *  All layout is built in C++ (no UMG asset needed). */
UCLASS()
class TRAE_UE_PROJECT_API UFCInfoCard : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Broadcast when the user clicks the card's close button, so the pawn can
	 *  restore the POI beams. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCardClosed);
	UPROPERTY(BlueprintAssignable, Category = "ForbiddenCity")
	FOnCardClosed OnCardClosed;

	/** 点击"全局态势"按钮时广播（无参）。 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOverviewRequested);
	UPROPERTY(BlueprintAssignable, Category = "ForbiddenCity")
	FOnOverviewRequested OnOverviewRequested;

	/** 点击太和殿卡片上的"拆解/复位"按钮时广播（无参）。 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExplodeToggle);
	UPROPERTY(BlueprintAssignable, Category = "ForbiddenCity")
	FOnExplodeToggle OnExplodeToggle;

	/** Build the widget tree here: WidgetTree exists, but its Slate widgets are
	 *  generated (TakeWidget) only AFTER this call, so dynamically constructed
	 *  children must be added now rather than in NativeConstruct. */
	virtual void NativeOnInitialized() override;

	/** Show the card for a POI from the baked FCData table. */
	void ShowPoi(int32 PoiIndex);
	void HideCard();

private:
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnExplodeClicked();

	UTextBlock* TitleText = nullptr;
	UTextBlock* ZoneText = nullptr;
	UTextBlock* AliasesText = nullptr;
	UTextBlock* TagsText = nullptr;
	UTextBlock* BodyText = nullptr;
	UBorder* CardBorder = nullptr;

	UButton* ExplodeBtn = nullptr;
	UTextBlock* ExplodeLabel = nullptr;

	/** 太和殿拆解状态：false=未拆解（按钮文案"拆解建筑"），true=已拆解（"复位组装"）。 */
	bool bExploded = false;
};
