#include "FCPoiMarker.h"

#include "FCData.h"
#include "FCPoiLabelWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// 三级距离剔除：城市级地标全城常驻，重点建筑全景常驻，普通建筑靠近才出现。
	constexpr float CityRadius = 600000.f;
	constexpr float KeyRadius = 150000.f;
	constexpr float LocalRadius = 40000.f;

	// 屏幕空间稀疏去重：同一帧内通过距离剔除的标签候选（屏幕像素矩形）。
	struct FScreenCandidate
	{
		AFCPoiMarker* Marker;
		FVector2D Min;
		FVector2D Max;
		float Dist3D;
		int32 Tier; // 2=城市级 1=重点 0=普通
	};

	/** 两个像素矩形的重叠面积占较小矩形的比例，超过阈值即判定“挤在一起”。 */
	float OverlapRatio(const FScreenCandidate& A, const FScreenCandidate& B)
	{
		const float IX = FMath::Max(0.f, FMath::Min(A.Max.X, B.Max.X) - FMath::Max(A.Min.X, B.Min.X));
		const float IY = FMath::Max(0.f, FMath::Min(A.Max.Y, B.Max.Y) - FMath::Max(A.Min.Y, B.Min.Y));
		const float Inter = IX * IY;
		const float AA = (A.Max.X - A.Min.X) * (A.Max.Y - A.Min.Y);
		const float BB = (B.Max.X - B.Min.X) * (B.Max.Y - B.Min.Y);
		const float Smaller = FMath::Min(AA, BB);
		return Smaller > KINDA_SMALL_NUMBER ? Inter / Smaller : 0.f;
	}

	// 34px 字号 RT 行高约 50px，加上内框纵向 Padding(4+4) 与外框描边，
	// 整个底框 desired 高度约 62px，作为四边形像素高度。
	constexpr float LabelQuadPixelH = 62.f;
	// 标签目标屏幕高度：无论远近都保持约 32px 的恒定屏幕尺寸，
	// 远景不抢戏、近景不遮楼。WorldScale 每帧按实际距离反推。
	constexpr float LabelTargetScreenH = 28.f;

	/** 中轴线 / 重点建筑名称集合：名称完全相等即视为重点 POI。 */
	bool IsKeyPoi(int32 Idx)
	{
		static const TCHAR* const KeyNames[] = {
			TEXT("午门"), TEXT("太和门"), TEXT("太和殿"), TEXT("中和殿"), TEXT("保和殿"),
			TEXT("乾清门"), TEXT("乾清宫"), TEXT("交泰殿"), TEXT("坤宁宫"), TEXT("神武门"),
			TEXT("东华门"), TEXT("西华门"), TEXT("养心殿"), TEXT("太和殿广场"), TEXT("御花园"),
			TEXT("奉先殿"), TEXT("宁寿宫"), TEXT("慈宁宫")
		};

		const TCHAR* Name = G_FCPois[Idx].Name;
		for (const TCHAR* Key : KeyNames)
		{
			if (FCString::Strcmp(Name, Key) == 0)
			{
				return true;
			}
		}
		// 角楼数据可能拆成四座，名称包含"角楼"即算。
		return FCString::Strstr(Name, TEXT("角楼")) != nullptr;
	}

	/** 城市级地标：全城视野下仍需标注的北京著名建筑。 */
	bool IsCityPoi(int32 Idx)
	{
		static const TCHAR* const CityNames[] = {
			TEXT("天安门"), TEXT("天安门广场"), TEXT("人民英雄纪念碑"), TEXT("毛主席纪念堂"),
			TEXT("正阳门"), TEXT("正阳门箭楼"), TEXT("永定门"),
			TEXT("景山万春亭"), TEXT("鼓楼"), TEXT("钟楼"),
			TEXT("北海白塔"), TEXT("五龙亭"),
			TEXT("祈年殿"), TEXT("皇穹宇"), TEXT("圜丘坛"),
			TEXT("太岁殿"), TEXT("观耕台")
		};

		const TCHAR* Name = G_FCPois[Idx].Name;
		for (const TCHAR* City : CityNames)
		{
			if (FCString::Strcmp(Name, City) == 0)
			{
				return true;
			}
		}
		return false;
	}

	// 屏幕空间稀疏去重的逐帧候选列表：各 marker 在 Tick 中登记，
	// GameMode 在 PostPhysics 阶段调用 ResolveScreenCulling 统一排序剔除并清空。
	TArray<FScreenCandidate> G_FCScreenCandidates;
}

bool AFCPoiMarker::bAllHidden = false;

AFCPoiMarker::AFCPoiMarker()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Label = CreateDefaultSubobject<UWidgetComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetMobility(EComponentMobility::Movable);
	Label->SetWidgetSpace(EWidgetSpace::World);
	// 关键：必须显式给出 DrawSize。世界空间 WidgetComponent 的四边形尺寸
	// = DrawSize × 相对缩放；默认 DrawSize(500×500) 会让标签放大到 100m 级。
	Label->SetDrawAtDesiredSize(false);
	Label->SetDrawSize(FVector2D(200.f, LabelQuadPixelH));
	// 标签完全不参与碰撞：点击拾取唯一入口是 HitBox。
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetCollisionResponseToAllChannels(ECR_Ignore);
	Label->SetGenerateOverlapEvents(false);

	HitBox = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(Root);
	HitBox->SetMobility(EComponentMobility::Movable);
	HitBox->SetVisibility(false);
	HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitBox->SetCollisionObjectType(ECC_Visibility);
	HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AFCPoiMarker::Init(int32 InIndex)
{
	PoiIndex = InIndex;
	const FFCPoi& P = G_FCPois[InIndex];

	SetActorLocation(FVector(P.X, P.Y, 0.f));

	static UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	HitBox->SetStaticMesh(SphereMesh);

	// 世界空间 UMG 标签：字体走 Slate 运行时字体路径（Runtime UFont 可正常光栅化中文）。
	UFCPoiLabelWidget* W = CreateWidget<UFCPoiLabelWidget>(GetWorld(), UFCPoiLabelWidget::StaticClass());
	Label->SetWidget(W);
	W->SetLabelText(FText::FromString(P.Name));

	// 按名称字数估算底框像素宽：34px 字号约 34px/字，加菱形(约36px)、内框左右 Padding 与外描边。
	const int32 NameLen = FMath::Max(2, FCString::Strlen(P.Name));
	const float PixelW = 34.f * NameLen + 92.f;
	Label->SetDrawSize(FVector2D(PixelW, LabelQuadPixelH));
	// 缓存像素宽；世界尺寸在 Tick 中按"恒定屏幕高度"逐帧反推。
	BaseW = PixelW;

	// 屋顶上方的悬浮位置。
	Label->SetRelativeLocation(FVector(0.f, 0.f, 4000.f));
	HitBox->SetRelativeLocation(FVector(0.f, 0.f, 4000.f));

	// 缓存档位，供 Tick 三级剔除使用。
	Tier = IsCityPoi(InIndex) ? 2 : (IsKeyPoi(InIndex) ? 1 : 0);

	// 引擎 Sphere 半径 50cm，scale 即半径倍数；显示后 Tick 会按实际缩放覆盖。
	HitBox->SetWorldScale3D(FVector(15.f, 15.f, 5.f));

#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("FC_POI_%d"), InIndex), false);
#endif
}

void AFCPoiMarker::SetAllMarkersHidden(bool bHidden)
{
	bAllHidden = bHidden;
}

void AFCPoiMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P)
	{
		return;
	}

	const FVector CamLoc = P->GetActorLocation();

	// 1) 三级距离显隐（用 pawn 位置做 2D 距离裁剪）。
	const float Dist2D = FVector::Dist2D(CamLoc, GetActorLocation());
	const float Radius = Tier == 2 ? CityRadius : (Tier == 1 ? KeyRadius : LocalRadius);
	bShown = !bAllHidden && Dist2D <= Radius;

	// 距离剔除 / 全局隐藏：直接隐藏并关碰撞，本帧不登记候选。
	if (!bShown)
	{
		SetActorHiddenInGame(true);
		HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	// 2) Billboard：四边形 +X 法线朝向相机，且文字顶边对齐屏幕上方
	//    （屏幕对齐 billboard：陡俯角下也不会出现南侧标签文字倒置）。
	const FVector ToCam = (CamLoc - Label->GetComponentLocation()).GetSafeNormal();
	const FVector CamUp = PC->PlayerCameraManager
		? PC->PlayerCameraManager->GetCameraRotation().RotateVector(FVector::UpVector)
		: FVector::UpVector;
	const FRotator R = FRotationMatrix::MakeFromXZ(ToCam, CamUp).Rotator();
	Label->SetWorldRotation(R);

	// 3) 恒定屏幕尺寸缩放（解析公式）：
	//    UE 的 FOV 为水平 FOV，距离 D 处水平世界跨度 = 2·D·tan(FOV/2)，
	//    对应视口宽度 ViewportX 像素，故每像素世界尺度 = 2·D·tan(FOV/2)/ViewportX。
	//    D 取标签相对相机的视线纵深（沿相机前向投影）。
	//    远距陡俯角下旧方案用 1000cm 偏移投影反推，亚像素除法会严重失稳，
	//    解析公式无此问题。
	FVector2D ScrCenter;
	const FVector LabelWorld = Label->GetComponentLocation();
	const bool bProjected = PC->ProjectWorldLocationToScreen(LabelWorld, ScrCenter, true);

	float WorldH = 1000.f;
	if (bProjected)
	{
		int32 ViewportX = 0;
		int32 ViewportY = 0;
		PC->GetViewportSize(ViewportX, ViewportY);

		const float FOVDeg = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : 90.f;
		const FVector CamForward = PC->PlayerCameraManager
			? PC->PlayerCameraManager->GetCameraRotation().Vector()
			: (LabelWorld - CamLoc).GetSafeNormal();
		const float Depth = FMath::Max(1.f, FVector::DotProduct(LabelWorld - CamLoc, CamForward));

		if (ViewportX > 0)
		{
			const float WorldPerPixel = 2.f * Depth * FMath::Tan(FMath::DegreesToRadians(FOVDeg) * 0.5f) / ViewportX;
			WorldH = LabelTargetScreenH * WorldPerPixel;
		}
	}
	const float S = WorldH / LabelQuadPixelH;
	Label->SetWorldScale3D(FVector(S));
	const float WorldW = BaseW * S;
	// HitBox 跟随真实世界尺寸（引擎 Sphere 半径 50cm，scale = 目标半径/50）。
	HitBox->SetWorldScale3D(FVector(
		FMath::Max(4.f, WorldW * 0.5f / 50.f),
		FMath::Max(4.f, WorldW * 0.5f / 50.f),
		FMath::Max(2.f, WorldH * 0.5f / 50.f)));

	// 4) 登记屏幕空间候选，最终由 ResolveScreenCulling 统一排序、重叠剔除。
	if (bProjected)
	{
		// 标签像素宽 = 纹理宽高比 × 目标像素高（恒定，与距离无关）。
		const float ScreenW = LabelTargetScreenH * (BaseW / LabelQuadPixelH);
		FScreenCandidate Cand;
		Cand.Marker = this;
		Cand.Min = ScrCenter - FVector2D(ScreenW, LabelTargetScreenH) * 0.5f;
		Cand.Max = ScrCenter + FVector2D(ScreenW, LabelTargetScreenH) * 0.5f;
		Cand.Dist3D = FVector::Dist(CamLoc, LabelWorld);
		Cand.Tier = Tier;
		G_FCScreenCandidates.Add(Cand);
	}
}

int32 AFCPoiMarker::PickMarker(const FVector2D& ScreenPos)
{
	UWorld* World = nullptr;
	APlayerController* PC = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (UWorld* W = Ctx.World())
			{
				if (APlayerController* C = W->GetFirstPlayerController())
				{
					World = W;
					PC = C;
					break;
				}
			}
		}
	}
	if (!PC)
	{
		return INDEX_NONE;
	}

	return PickMarkerInWorld(World, PC, ScreenPos);
}

int32 AFCPoiMarker::PickMarkerInWorld(UWorld* World, APlayerController* PC, const FVector2D& ScreenPos)
{
	int32 BestIdx = INDEX_NONE;
	float BestDepth = TNumericLimits<float>::Max();

	TArray<AActor*> AllMarkers;
	UGameplayStatics::GetAllActorsOfClass(World, AFCPoiMarker::StaticClass(), AllMarkers);

	for (AActor* MA : AllMarkers)
	{
		AFCPoiMarker* M = Cast<AFCPoiMarker>(MA);
		if (!M)
		{
			continue;
		}
		// 只拾取当前真正显示、未被屏幕剔除的标签。
		if (!M->bShown || M->bScreenCulled || M->bAllHidden || M->PoiIndex == INDEX_NONE)
		{
			continue;
		}

		const FVector LabelWorld = M->Label->GetComponentLocation();
		FVector2D ScrCenter;
		if (!PC->ProjectWorldLocationToScreen(LabelWorld, ScrCenter, true))
		{
			continue;
		}

		// 与 Tick 中相同的屏幕矩形：宽 = 纹理宽高比 × 目标屏幕高。
		const float ScreenW = LabelTargetScreenH * (M->BaseW / LabelQuadPixelH);
		const FVector2D Half(ScreenW * 0.5f, LabelTargetScreenH * 0.5f);

		if (ScreenPos.X >= ScrCenter.X - Half.X && ScreenPos.X <= ScrCenter.X + Half.X
			&& ScreenPos.Y >= ScrCenter.Y - Half.Y && ScreenPos.Y <= ScrCenter.Y + Half.Y)
		{
			// 多个标签矩形重叠时取离相机更近者。
			const FVector CamLoc = PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector::ZeroVector;
			const float Depth = FVector::DistSquared(CamLoc, LabelWorld);
			if (Depth < BestDepth)
			{
				BestDepth = Depth;
				BestIdx = M->PoiIndex;
			}
		}
	}

	return BestIdx;
}

TArray<FString> AFCPoiMarker::RunPickSelfTest()
{
	TArray<FString> Lines;

	UWorld* World = nullptr;
	APlayerController* PC = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (UWorld* W = Ctx.World())
			{
				if (APlayerController* C = W->GetFirstPlayerController())
				{
					World = W;
					PC = C;
					break;
				}
			}
		}
	}
	if (!PC)
	{
		Lines.Add(TEXT("NO PLAYER CONTROLLER"));
		return Lines;
	}

	TArray<AActor*> AllMarkers;
	UGameplayStatics::GetAllActorsOfClass(World, AFCPoiMarker::StaticClass(), AllMarkers);

	int32 Tested = 0;
	int32 Matched = 0;
	for (AActor* MA : AllMarkers)
	{
		AFCPoiMarker* M = Cast<AFCPoiMarker>(MA);
		if (!M || !M->bShown || M->bScreenCulled || M->bAllHidden || M->PoiIndex == INDEX_NONE)
		{
			continue;
		}

		const FVector LabelWorld = M->Label->GetComponentLocation();
		FVector2D ScrCenter;
		if (!PC->ProjectWorldLocationToScreen(LabelWorld, ScrCenter, true))
		{
			continue;
		}

		const int32 Picked = PickMarkerInWorld(World, PC, ScrCenter);
		const bool bMatch = (Picked == M->PoiIndex);
		++Tested;
		if (bMatch)
		{
			++Matched;
		}
		Lines.Add(FString::Printf(TEXT("poi=%d picked=%d match=%s screen=(%.0f,%.0f)"),
			M->PoiIndex, Picked, bMatch ? TEXT("true") : TEXT("false"), ScrCenter.X, ScrCenter.Y));
	}

	Lines.Insert(FString::Printf(TEXT("SUMMARY tested=%d matched=%d"), Tested, Matched), 0);
	return Lines;
}

void AFCPoiMarker::ResolveScreenCulling()
{
	// 高档位优先保留，同档按距离由近到远。
	G_FCScreenCandidates.Sort([](const FScreenCandidate& A, const FScreenCandidate& B)
	{
		if (A.Tier != B.Tier)
		{
			return A.Tier > B.Tier;
		}
		return A.Dist3D < B.Dist3D;
	});

	TArray<const FScreenCandidate*> Kept;
	for (const FScreenCandidate& Cand : G_FCScreenCandidates)
	{
		const FString CandName = G_FCPois[Cand.Marker->PoiIndex].Name;

		bool bOverlap = false;
		for (const FScreenCandidate* Other : Kept)
		{
			// 同名 POI（如重复/拆分组）只留最近的一个，杜绝重复名称堆叠。
			const bool bSameName = CandName == G_FCPois[Other->Marker->PoiIndex].Name;
			if (bSameName || OverlapRatio(Cand, *Other) > 0.10f)
			{
				bOverlap = true;
				break;
			}
		}

		Cand.Marker->ApplyScreenCull(bOverlap);
		if (!bOverlap)
		{
			Kept.Add(&Cand);
		}
	}

	G_FCScreenCandidates.Reset();
}

void AFCPoiMarker::ApplyScreenCull(bool bCulled)
{
	bScreenCulled = bCulled;
	SetActorHiddenInGame(bCulled);
	HitBox->SetCollisionEnabled(bCulled ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}
