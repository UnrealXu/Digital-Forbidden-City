#include "FCFocusComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FCPoiMarker.h"
#include "FCData.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// 后处理描边/金色遮罩材质（由 Saved/create_outline_material.py 在编辑器中生成）。
	const TCHAR* GOutlineMaterialPath = TEXT("/Game/ForbiddenCity/Materials/MPP_Outline.MPP_Outline");

}

UFCFocusComponent::UFCFocusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFCFocusComponent::BeginPlay()
{
	Super::BeginPlay();

	// 确保 CustomDepth-Stencil 通道启用（3 = 启用 CustomDepth 并写入 Stencil），
	// 否则 SetCustomStencilValue 写入的值在后处理材质里读不到。
	static IConsoleVariable* CVarCustomDepth =
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
	if (CVarCustomDepth && CVarCustomDepth->GetInt() < 3)
	{
		CVarCustomDepth->Set(3, ECVF_SetByCode);
	}

	// 将描边后处理材质挂到拥有者相机上。
	if (UObject* MatObj = StaticLoadObject(UMaterialInterface::StaticClass(),
		nullptr, GOutlineMaterialPath))
	{
		if (UMaterialInterface* OutlineMat = Cast<UMaterialInterface>(MatObj))
		{
			if (const AActor* Owner = GetOwner())
			{
				if (UCameraComponent* Cam = Owner->FindComponentByClass<UCameraComponent>())
				{
					Cam->PostProcessSettings.WeightedBlendables.Array.Add(
						FWeightedBlendable(1.f, OutlineMat));
					Cam->PostProcessBlendWeight = 1.0f;
				}
			}
		}
	}
}

void UFCFocusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearFocus();
	Super::EndPlay(EndPlayReason);
}

void UFCFocusComponent::SetFocus(AActor* Target)
{
	TArray<AActor*> Targets;
	if (Target)
	{
		Targets.Add(Target);
	}
	SetFocusActors(Targets);
}

void UFCFocusComponent::SetFocusActors(const TArray<AActor*>& Targets)
{
	ClearFocus();

	TArray<AActor*> Valid;
	for (AActor* A : Targets)
	{
		if (IsValid(A))
		{
			Valid.AddUnique(A);
		}
	}

	if (Valid.Num() > 0)
	{
		ApplyHighlight(Valid);
	}
}

void UFCFocusComponent::ClearFocus()
{
	for (const FFocusedMesh& Fm : FocusedMeshes)
	{
		if (!Fm.Mesh)
		{
			continue;
		}
		// 关闭 CustomDepth 渲染。
		Fm.Mesh->SetRenderCustomDepth(false);

	}

	FocusedMeshes.Reset();
	FocusedActors.Reset();
}

bool UFCFocusComponent::SetFocusByPoiIndex(int32 PoiIndex)
{
	if (PoiIndex < 0)
	{
		return false;
	}
	TArray<AActor*> Found;
	if (FindPoiActors(PoiIndex, Found))
	{
		SetFocusActors(Found);
		return true;
	}
	return false;
}

bool UFCFocusComponent::SetFocusByName(const FString& Name)
{
	if (Name.IsEmpty())
	{
		return false;
	}

	// 若传入的是 FCData 中的中文 POI 名，查出其索引后走 Tag/前缀匹配。
	int32 KnownIdx = INDEX_NONE;
	for (int32 i = 0; i < G_FCPoiCount; ++i)
	{
		if (Name == FString(G_FCPois[i].Name))
		{
			KnownIdx = i;
			break;
		}
	}

	TArray<AActor*> Found;
	if (FindPoiActors(KnownIdx, Found))
	{
		SetFocusActors(Found);
		return true;
	}
	return false;
}

void UFCFocusComponent::ApplyHighlight(const TArray<AActor*>& Targets)
{
	for (AActor* Target : Targets)
	{
		FocusedActors.Add(Target);

		// 收集目标身上（含附加子组件）的全部静态网格。
		TArray<UStaticMeshComponent*> Meshes;
		Target->GetComponents<UStaticMeshComponent>(Meshes, true);

		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (!Mesh || Mesh->GetStaticMesh() == nullptr)
			{
				continue;
			}

			FFocusedMesh Fm;
			Fm.Mesh = Mesh;

			// 只写 CustomDepth/Stencil，由后处理材质描外轮廓。
			Mesh->SetRenderCustomDepth(true);
			Mesh->SetCustomDepthStencilValue(FocusStencilValue);

			FocusedMeshes.Add(Fm);
		}
	}
}

bool UFCFocusComponent::FindPoiActors(int32 KnownIdx, TArray<AActor*>& OutActors) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FString TagWanted = KnownIdx != INDEX_NONE
		? FString::Printf(TEXT("poi_%d"), KnownIdx) : FString();
	const FString PrefixWanted = KnownIdx != INDEX_NONE
		? FString::Printf(TEXT("POI%d_"), KnownIdx) : FString();

	for (AActor* A : TActorRange<AActor>(World))
	{
		if (!IsValid(A) || A->IsA(AFCPoiMarker::StaticClass()))
		{
			continue;
		}

		// 1) Tag 精确匹配：拼合建筑的全部部件 Actor 统一收集（最高优先级）。
		if (!TagWanted.IsEmpty() && A->ActorHasTag(FName(*TagWanted)))
		{
			OutActors.AddUnique(A);
		}
	}

	if (OutActors.Num() > 0)
	{
		return true;
	}

	// 2) 名字前缀 POI<idx>_（大小写不敏感；无 Tag 时的兜底）。
	for (AActor* A : TActorRange<AActor>(World))
	{
		if (IsValid(A) && !PrefixWanted.IsEmpty()
			&& A->GetName().StartsWith(PrefixWanted, ESearchCase::IgnoreCase))
		{
			OutActors.AddUnique(A);
		}
	}

	return OutActors.Num() > 0;
}
