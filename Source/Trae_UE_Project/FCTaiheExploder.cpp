#include "FCTaiheExploder.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AFCTaiheExploder::AFCTaiheExploder()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AFCTaiheExploder::BeginPlay()
{
	Super::BeginPlay();

	// 收集全部 StaticMeshActor 中 label 以 "Taihe_" 开头的太和殿部件。
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStaticMeshActor::StaticClass(), Actors);

	for (AActor* A : Actors)
	{
		AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(A);
		if (!MeshActor)
		{
			continue;
		}

		// 识别字符串来源：编辑器构建使用 ActorLabel。
		// fc_build_level.py 中 actor.set_actor_label(网格资产名)，故 ActorLabel 等于
		// 静态网格资产名（如 "Taihe_cols"）；GetActorLabel 本身是 WITH_EDITOR API，
		// 非编辑器构建回退使用 StaticMesh 资产名（与 ActorLabel 同源同值）。
		FString Label;
#if WITH_EDITOR
		Label = MeshActor->GetActorLabel();
#else
		if (UStaticMesh* Mesh = MeshActor->GetStaticMeshComponent()->GetStaticMesh())
		{
			Label = Mesh->GetName();
		}
#endif
		if (!Label.StartsWith(TEXT("Taihe_")))
		{
			continue;
		}

		// StaticMeshActor 默认 Static，需把网格组件改为 Movable 才能运行时挪位置。
		MeshActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);

		FPart Part;
		Part.Actor = MeshActor;
		Part.Base = MeshActor->GetActorLocation();
		Part.Offset = ResolveOffset(Label);
		Parts.Add(Part);
	}

	UE_LOG(LogTemp, Log, TEXT("[FCTaiheExploder] 找到太和殿部件数：%d（期望 13）"), Parts.Num());
}

void AFCTaiheExploder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Target = bTargetExploded ? 1.f : 0.f;
	Alpha = FMath::FInterpConstantTo(Alpha, Target, DeltaSeconds, LerpSpeed);
	if (FMath::Abs(Alpha - Target) < 1e-3f)
	{
		Alpha = Target;
	}

	ApplyParts();
}

FVector AFCTaiheExploder::ResolveOffset(const FString& Label) const
{
	// 屋顶体系（6 件：roof_skirt/roof_band/roof_upperwall/roof_top/
	// roof_top_ridge/roof_top_finials）
	if (Label.StartsWith(TEXT("Taihe_roof")))
	{
		return FVector(0.f, 0.f, 2400.f);
	}

	// 斗拱（2 件：dg、dg_g）
	if (Label.StartsWith(TEXT("Taihe_dg")))
	{
		return FVector(0.f, 0.f, 1200.f);
	}

	// 额枋
	if (Label == TEXT("Taihe_arch"))
	{
		return FVector(0.f, 0.f, 700.f);
	}

	// 柱
	if (Label == TEXT("Taihe_cols"))
	{
		return FVector(0.f, 0.f, 500.f);
	}

	// 其余（walls、dw、dw_g 墙身/门窗）保持不动，作为拆解基准层；
	// 任何未预期的额外 Taihe_ 件也归入此档。
	return FVector::ZeroVector;
}

void AFCTaiheExploder::ApplyParts() const
{
	for (const FPart& Part : Parts)
	{
		if (Part.Actor)
		{
			Part.Actor->SetActorLocation(Part.Base + Part.Offset * Alpha, false);
		}
	}
}

void AFCTaiheExploder::Explode()
{
	bTargetExploded = true;
}

void AFCTaiheExploder::Assemble()
{
	bTargetExploded = false;
}

void AFCTaiheExploder::Toggle()
{
	bTargetExploded = !bTargetExploded;
}

void AFCTaiheExploder::AssembleNow()
{
	bTargetExploded = false;
	Alpha = 0.f;
	ApplyParts();
}
