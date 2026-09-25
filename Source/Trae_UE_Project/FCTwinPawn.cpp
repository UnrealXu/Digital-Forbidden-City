#include "FCTwinPawn.h"

#include "Camera/CameraComponent.h"
#include "FCFocusComponent.h"
#include "FCInfoCard.h"
#include "FCUiHud.h"
#include "FCPoiMarker.h"
#include "FCTaiheExploder.h"
#include "FCData.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMeshActor.h"

AFCTwinPawn::AFCTwinPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	SetRootComponent(Camera);
	Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
	Camera->SetFieldOfView(55.f);

	// 建筑高亮子系统：同一时刻只高亮点击选中的一栋建筑。
	FocusComponent = CreateDefaultSubobject<UFCFocusComponent>(TEXT("FocusComponent"));

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void AFCTwinPawn::BeginPlay()
{
	Super::BeginPlay();

	ApplyCamera();

	APlayerController* PC = Cast<APlayerController>(GetController());
	UE_LOG(LogTemp, Warning, TEXT("[FC] BeginPlay PC=%s LocalPlayer=%s"),
		PC ? TEXT("OK") : TEXT("NULL"),
		(PC && PC->GetLocalPlayer()) ? TEXT("OK") : TEXT("NULL"));
	if (PC)
	{
		RequestGameUiMode();
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
		BindControls();

		if (!InfoCard)
		{
			InfoCard = CreateWidget<UFCInfoCard>(PC, UFCInfoCard::StaticClass());
			UE_LOG(LogTemp, Warning, TEXT("[FC] CreateWidget InfoCard=%s"),
				InfoCard ? TEXT("OK") : TEXT("NULL"));
			if (InfoCard)
			{
				InfoCard->OnCardClosed.AddDynamic(this, &AFCTwinPawn::OnInfoCardClosed);
				InfoCard->OnOverviewRequested.AddDynamic(this, &AFCTwinPawn::OnOverviewRequested);
				InfoCard->OnExplodeToggle.AddDynamic(this, &AFCTwinPawn::OnExplodeToggle);
				InfoCard->AddToPlayerScreen(100);
				UE_LOG(LogTemp, Warning, TEXT("[FC] AddToPlayerScreen done IsInViewport=%d"),
					InfoCard->IsInViewport() ? 1 : 0);
			}
		}

		if (!HudShell)
		{
			HudShell = CreateWidget<UFCHudShell>(PC, UFCHudShell::StaticClass());
			if (HudShell)
			{
				HudShell->OnOverview.AddDynamic(this, &AFCTwinPawn::HandleHudOverview);
				HudShell->OnPrev.AddDynamic(this, &AFCTwinPawn::HandleHudPrev);
				HudShell->OnNext.AddDynamic(this, &AFCTwinPawn::HandleHudNext);
				HudShell->OnToggleTour.AddDynamic(this, &AFCTwinPawn::HandleToggleTour);
				HudShell->AddToPlayerScreen(200);
				HudShell->SetStatusText(FText::FromString(TEXT(
				"自由浏览 ｜ 62处 ｜ 左键旋转 · 右键/WASD 水平移动 · QE 升降 · 滚轮缩放")));
			}
		}

		if (!CatalogPanel)
		{
			CatalogPanel = CreateWidget<UFCCatalogPanel>(PC, UFCCatalogPanel::StaticClass());
			if (CatalogPanel)
			{
				CatalogPanel->OnCatalogPoiSelected.AddDynamic(this, &AFCTwinPawn::HandleCatalogSelected);
				CatalogPanel->AddToPlayerScreen(190);
			}
		}
	}
}

void AFCTwinPawn::RequestGameUiMode()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	FInputModeGameAndUI GameUiMode;
	GameUiMode.SetHideCursorDuringCapture(false);
	GameUiMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	PC->SetInputMode(GameUiMode);
}

void AFCTwinPawn::BindControls()
{
	if (!InputComponent)
	{
		InputComponent = NewObject<UInputComponent>(this, TEXT("FCInput"));
		InputComponent->RegisterComponent();
	}

	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AFCTwinPawn::OnLeftPressed);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AFCTwinPawn::OnLeftReleased);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AFCTwinPawn::OnRightPressed);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AFCTwinPawn::OnRightReleased);
	InputComponent->BindAxisKey(EKeys::MouseWheelAxis, this, &AFCTwinPawn::OnZoom);

	// WASD ground-plane movement, QE vertical movement, Home = overview reset
	InputComponent->BindKey(EKeys::W, IE_Pressed, this, &AFCTwinPawn::OnKeyFwdPressed);
	InputComponent->BindKey(EKeys::W, IE_Released, this, &AFCTwinPawn::OnKeyFwdReleased);
	InputComponent->BindKey(EKeys::S, IE_Pressed, this, &AFCTwinPawn::OnKeyBackPressed);
	InputComponent->BindKey(EKeys::S, IE_Released, this, &AFCTwinPawn::OnKeyBackReleased);
	InputComponent->BindKey(EKeys::A, IE_Pressed, this, &AFCTwinPawn::OnKeyLeftPressed);
	InputComponent->BindKey(EKeys::A, IE_Released, this, &AFCTwinPawn::OnKeyLeftReleased);
	InputComponent->BindKey(EKeys::D, IE_Pressed, this, &AFCTwinPawn::OnKeyRightPressed);
	InputComponent->BindKey(EKeys::D, IE_Released, this, &AFCTwinPawn::OnKeyRightReleased);
	InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AFCTwinPawn::OnKeyDownPressed);
	InputComponent->BindKey(EKeys::Q, IE_Released, this, &AFCTwinPawn::OnKeyDownReleased);
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AFCTwinPawn::OnKeyUpPressed);
	InputComponent->BindKey(EKeys::E, IE_Released, this, &AFCTwinPawn::OnKeyUpReleased);
	// Home key: reset to the overview pose.
	InputComponent->BindKey(EKeys::Home, IE_Pressed, this, &AFCTwinPawn::OnOverviewRequested);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		EnableInput(PC);
	}
}

void AFCTwinPawn::OnLeftPressed()
{
	if (bAutoTour)
	{
		StopAutoTour();
	}
	bLeftDown = true;
	LeftPressTime = FPlatformTime::Seconds();
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		PC->GetMousePosition(PressPos.X, PressPos.Y);
	}
}

void AFCTwinPawn::OnLeftReleased()
{
	bLeftDown = false;
	if (bDragging)
	{
		ReleaseMouseDrag();
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	FVector2D MousePos;
	PC->GetMousePosition(MousePos.X, MousePos.Y);

	double Now = FPlatformTime::Seconds();

	// 点击判定：位移在环绕阈值内（未进入 bDragging）且按住时长较短，才视为点击。
	const bool bQuickClick = (Now - LeftPressTime) <= MaxClickDuration;
	if (!bQuickClick)
	{
		// 长按松开：既不是点击也不参与双击判定，避免长按后误触发。
		return;
	}

	bool bDouble = (Now - LastLeftClickTime) <= DoubleClickTime
		&& FVector2D::Distance(MousePos, LastClickPos) <= DoubleClickPx;
	LastLeftClickTime = Now;
	LastClickPos = MousePos;

	if (bDouble)
	{
		HandleGroundDoubleClick(MousePos);
	}
	else
	{
		HandleClick(MousePos);
	}
}

void AFCTwinPawn::OnRightPressed()
{
	if (bAutoTour)
	{
		StopAutoTour();
	}
	bRightDown = true;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		PC->GetMousePosition(PressPos.X, PressPos.Y);
	}
}

void AFCTwinPawn::OnRightReleased()
{
	bRightDown = false;
	if (bDragging)
	{
		ReleaseMouseDrag();
	}
}

void AFCTwinPawn::OnZoom(float AxisValue)
{
	if (FMath::IsNearlyZero(AxisValue))
	{
		return;
	}
	if (bAutoTour)
	{
		StopAutoTour();
	}
	// Impulse the zoom velocity: wheel steps add/remove radius distance, which
	// then eases out through the coast velocity model (spring-arm feel).
	if (Mode == EFCCamMode::Fly)
	{
		Mode = EFCCamMode::Direct;
	}
	RadiusVel -= AxisValue * Radius * 1.2f;
	if (Mode == EFCCamMode::Direct)
	{
		Mode = EFCCamMode::Coast;
	}
}

void AFCTwinPawn::DebugInjectKeyboard(float Fwd, float Right, float Up, float DeltaSeconds)
{
	// Identical to the Tick WASD/QE block (flights ignore held-key movement).
	if (Mode == EFCCamMode::Fly)
	{
		return;
	}
	if (FMath::IsNearlyZero(Fwd) && FMath::IsNearlyZero(Right) && FMath::IsNearlyZero(Up))
	{
		return;
	}
	const float Scale = Radius * 0.9f * DeltaSeconds;
	Pivot += (ViewForward() * Fwd + ViewRight() * Right) * Scale;
	Pivot.Z += Up * Scale;
	Pivot.Z = FMath::Clamp(Pivot.Z, -5000.f, 120000.f);

	PivotVel = FVector::ZeroVector;
	YawVel = PitchVel = RadiusVel = 0.f;
	Mode = EFCCamMode::Direct;
	ApplyCamera();
}

void AFCTwinPawn::DebugInjectDrag(float DX, float DY, bool bLeftButton, float DeltaSeconds)
{
	// Identical orbit / ground-pan math as the per-frame branch of HandleDrag.
	if (Mode == EFCCamMode::Fly)
	{
		Mode = EFCCamMode::Direct;
	}
	if (bLeftButton)
	{
		// Drag right orbits left (scene follows the cursor); user-confirmed direction.
		Yaw = Yaw - DX * 0.25f;
		Pitch = FMath::Clamp(Pitch + DY * 0.25f, -89.f, -2.f);
	}
	else
	{
		const float Scale = Radius * 0.0018f;
		Pivot = Pivot + (-ViewRight() * DX + ViewForward() * DY) * Scale;
	}
	// The injected delta is applied directly, exactly like one frame of an
	// ongoing drag: no residual coast velocity afterwards.
	PivotVel = FVector::ZeroVector;
	YawVel = PitchVel = RadiusVel = 0.f;
	Mode = EFCCamMode::Direct;
	ApplyCamera();
}

void AFCTwinPawn::DebugInjectZoom(float AxisValue)
{
	OnZoom(AxisValue);
}

void AFCTwinPawn::OnKeyFwdPressed() { bKeyFwd = true; }
void AFCTwinPawn::OnKeyFwdReleased() { bKeyFwd = false; }
void AFCTwinPawn::OnKeyBackPressed() { bKeyBack = true; }
void AFCTwinPawn::OnKeyBackReleased() { bKeyBack = false; }
void AFCTwinPawn::OnKeyLeftPressed() { bKeyLeft = true; }
void AFCTwinPawn::OnKeyLeftReleased() { bKeyLeft = false; }
void AFCTwinPawn::OnKeyRightPressed() { bKeyRight = true; }
void AFCTwinPawn::OnKeyRightReleased() { bKeyRight = false; }
void AFCTwinPawn::OnKeyUpPressed() { bKeyUp = true; }
void AFCTwinPawn::OnKeyUpReleased() { bKeyUp = false; }
void AFCTwinPawn::OnKeyDownPressed() { bKeyDown = true; }
void AFCTwinPawn::OnKeyDownReleased() { bKeyDown = false; }

void AFCTwinPawn::ReleaseMouseDrag()
{
	bDragging = false;
	bSkipDelta = true;
	// Whatever velocity the drag carried keeps going and decays in Tick (coast).
	if (Mode != EFCCamMode::Fly)
	{
		Mode = EFCCamMode::Coast;
	}
	RequestGameUiMode();
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		PC->SetMouseLocation(FMath::RoundToInt(PressPos.X), FMath::RoundToInt(PressPos.Y));
	}
}

FVector AFCTwinPawn::YawForward() const
{
	// Pivot -> camera direction on the ground plane. Must match ApplyCamera's
	// CamDir: at Yaw=-90 the camera sits to the south (+Y) looking north.
	const float R = FMath::DegreesToRadians(Yaw);
	return FVector(FMath::Cos(R), -FMath::Sin(R), 0.f);
}

FVector AFCTwinPawn::ViewForward() const
{
	// Camera look direction on the ground plane, opposite the pivot->eye vector.
	// Standard UE yaw basis (cos yaw, sin yaw); at Yaw=-90 this is (0,-1): north.
	return -YawForward();
}

FVector AFCTwinPawn::ViewRight() const
{
	// Screen-right in the same handed frame as ViewForward (standard UE yaw basis).
	const FVector F = ViewForward();
	return FVector(-F.Y, F.X, 0.f);
}

void AFCTwinPawn::HandleDrag(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	if (!bLeftDown && !bRightDown)
	{
		return;
	}

	FVector2D MousePos;
	PC->GetMousePosition(MousePos.X, MousePos.Y);

	if (!bDragging)
	{
		if (FVector2D::Distance(MousePos, PressPos) <= ClickMovePx)
		{
			return;
		}
		bDragging = true;
		Mode = EFCCamMode::Direct;
		PivotVel = FVector::ZeroVector;
		YawVel = PitchVel = RadiusVel = 0.f;

		FInputModeGameOnly ModeGame;
		PC->SetInputMode(ModeGame);
		bSkipDelta = true;
		return;
	}

	float DX = 0.f, DY = 0.f;
	PC->GetInputMouseDelta(DX, DY);
	if (bSkipDelta)
	{
		bSkipDelta = false;
		return;
	}

	const float Dt = FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);

	if (bLeftDown)
	{
		// Orbit: drag follows the cursor 1:1; track per-frame velocity for the release fling.
		// Horizontal sign matches DebugInjectDrag (user-confirmed: drag right rotates left).
		const float NewYaw = Yaw - DX * 0.25f;
		const float NewPitch = FMath::Clamp(Pitch + DY * 0.25f, -89.f, -2.f);
		YawVel = (NewYaw - Yaw) / Dt;
		PitchVel = (NewPitch - Pitch) / Dt;
		Yaw = NewYaw;
		Pitch = NewPitch;
	}
	else
	{
		// Ground-plane pan, "grab the map" semantics (aligned with the reference
		// project): drag right pulls the scene right (pivot moves screen-left),
		// drag down pulls the scene down (pivot moves into the screen).
		const float Scale = Radius * 0.0018f;
		const FVector NewPivot = Pivot + (-ViewRight() * DX + ViewForward() * DY) * Scale;
		PivotVel = (NewPivot - Pivot) / Dt;
		Pivot = NewPivot;
	}
	ApplyCamera();
}

void AFCTwinPawn::HandleClick(const FVector2D& ScreenPos)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// 1) 屏幕矩形精确拾取悬浮名称标签：点哪个标签就飞到哪个建筑，
	//    与左侧目录列表同一机理；不会再被标签后面的远景网格“穿透”。
	const int32 PickedIdx = AFCPoiMarker::PickMarker(ScreenPos);
	if (PickedIdx != INDEX_NONE)
	{
		FlyToPoi(PickedIdx);
		return;
	}

	// 2) 其它任何点击（空白 / 地面 / 建筑网格）只清除已有高亮，
	//    不再因点中网格而触发金色描边闪烁。
	if (FocusComponent)
	{
		FocusComponent->ClearFocus();
	}
}

void AFCTwinPawn::HandleGroundDoubleClick(const FVector2D& ScreenPos)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	FVector WorldLoc, WorldDir;
	if (!PC->DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, WorldLoc, WorldDir))
	{
		return;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FCGroundDblClick), false, this);
	const FVector End = WorldLoc + WorldDir * TraceLength;
	if (GetWorld()->LineTraceSingleByChannel(Hit, WorldLoc, End, ECC_Visibility, Params))
	{
		Mode = EFCCamMode::Direct;
		PivotVel = FVector::ZeroVector;
		YawVel = PitchVel = RadiusVel = 0.f;
		Pivot.X = Hit.Location.X;
		Pivot.Y = Hit.Location.Y;
		Pivot.Z = 0.f;
		ApplyCamera();
	}
}

void AFCTwinPawn::StartFlight(const FVector& InTargetPivot, float InTargetYaw, float InTargetPitch,
	float InTargetRadius, bool bShowCard, int32 CardPoiIndex)
{
	PivotVel = FVector::ZeroVector;
	YawVel = PitchVel = RadiusVel = 0.f;

	StartPivot = Pivot;
	StartYaw = Yaw;
	StartPitch = Pitch;
	StartRadius = Radius;

	FlyPivot = InTargetPivot;
	FlyYaw = InTargetYaw;
	FlyPitch = InTargetPitch;
	FlyRadius = FMath::Clamp(InTargetRadius, MinRadius, MaxRadius);
	FlyElapsed = 0.f;
	bFlyShowCard = bShowCard;
	FlyPoiIndex = CardPoiIndex;
	Mode = EFCCamMode::Fly;
}

void AFCTwinPawn::FlyToPoi(int32 PoiIndex)
{
	if (PoiIndex < 0 || PoiIndex >= G_FCPoiCount)
	{
		return;
	}
	const FFCPoi& P = G_FCPois[PoiIndex];
	CurrentPoiIndex = PoiIndex;
	if (CatalogPanel)
	{
		CatalogPanel->SetSelectedIndex(PoiIndex);
	}

	// Leaving Taihe Hall: put the exploded hall back together before flying.
	if (AFCTaiheExploder* Ex = GetExploder())
	{
		Ex->AssembleNow();
	}

	// Remember the exact view the user clicked from; the card's x restores it.
	ReturnPivot = Pivot;
	ReturnYaw = Yaw;
	ReturnPitch = Pitch;
	ReturnRadius = Radius;
	bHasReturnView = true;

	// Keep the name labels visible along the approach.
	AFCPoiMarker::SetAllMarkersHidden(false);

	// Slightly above the baked pivot so the downward look clears the gate in
	// front of each hall (e.g. Taihe Gate between the viewer and Taihe Hall).
	StartFlight(FVector(P.X, P.Y, P.Z + 200.f), -90.f, -28.f, 26000.f, true, PoiIndex);

	if (HudShell)
	{
		HudShell->SetStatusText(FText::FromString(FString::Printf(
			TEXT("导览中 %d / %d ｜ %s"), CurrentPoiIndex + 1, G_FCPoiCount, P.Name)));
	}
}

void AFCTwinPawn::OnInfoCardClosed()
{
	// Flying away from Taihe Hall: reassemble first.
	if (AFCTaiheExploder* Ex = GetExploder())
	{
		Ex->AssembleNow();
	}

	AFCPoiMarker::SetAllMarkersHidden(false);

	if (bHasReturnView)
	{
		StartFlight(ReturnPivot, ReturnYaw, ReturnPitch, ReturnRadius, false, INDEX_NONE);
	}
}

void AFCTwinPawn::OnOverviewRequested()
{
	if (AFCTaiheExploder* Ex = GetExploder())
	{
		Ex->AssembleNow();
	}
	if (InfoCard)
	{
		InfoCard->HideCard();
	}
	AFCPoiMarker::SetAllMarkersHidden(false);
	// 午门以南沿中轴线向北的壮观全景（与 BeginPlay 默认构图一致）。
	StartFlight(FVector(0.f, 20000.f, 3200.f), -90.f, -9.2f, 47000.f, false, INDEX_NONE);
}

void AFCTwinPawn::OnExplodeToggle()
{
	if (AFCTaiheExploder* Ex = GetExploder())
	{
		Ex->Toggle();
	}
}

void AFCTwinPawn::HandleHudOverview()
{
	if (bAutoTour)
	{
		StopAutoTour();
	}
	CurrentPoiIndex = INDEX_NONE;
	if (CatalogPanel)
	{
		CatalogPanel->SetSelectedIndex(INDEX_NONE);
	}
	OnOverviewRequested();
}

void AFCTwinPawn::HandleHudPrev()
{
	StopAutoTour();
	if (G_FCPoiCount > 0)
	{
		// INDEX_NONE (free-roam) starts the wrap from the last entry.
		CurrentPoiIndex = (CurrentPoiIndex <= 0 || CurrentPoiIndex == INDEX_NONE)
			? G_FCPoiCount - 1 : CurrentPoiIndex - 1;
		FlyToPoi(CurrentPoiIndex);
	}
}

void AFCTwinPawn::HandleHudNext()
{
	StopAutoTour();
	GotoNextTour();
}

void AFCTwinPawn::HandleToggleTour()
{
	if (bAutoTour)
	{
		StopAutoTour();
	}
	else
	{
		StartAutoTour();
	}
}

void AFCTwinPawn::HandleCatalogSelected(int32 PoiIndex)
{
	if (bAutoTour)
	{
		StopAutoTour();
	}
	FlyToPoi(PoiIndex);
}

void AFCTwinPawn::GotoNextTour()
{
	if (G_FCPoiCount <= 0)
	{
		return;
	}
	CurrentPoiIndex = (CurrentPoiIndex + 1) % G_FCPoiCount;
	FlyToPoi(CurrentPoiIndex);
}

void AFCTwinPawn::StartAutoTour()
{
	bAutoTour = true;
	if (HudShell)
	{
		HudShell->SetTourButtonText(FText::FromString(TEXT("停止")));
	}
	GotoNextTour();
	GetWorldTimerManager().SetTimer(TourTimerHandle, this, &AFCTwinPawn::GotoNextTour,
		6.0f, true);
}

void AFCTwinPawn::StopAutoTour()
{
	bAutoTour = false;
	GetWorldTimerManager().ClearTimer(TourTimerHandle);
	if (HudShell)
	{
		HudShell->SetTourButtonText(FText::FromString(TEXT("自动导览")));
		HudShell->SetStatusText(FText::FromString(TEXT("自动导览已停止")));
	}
}

AFCTaiheExploder* AFCTwinPawn::GetExploder()
{
	if (!Exploder)
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFCTaiheExploder::StaticClass(), Found);
		if (Found.Num() > 0)
		{
			Exploder = Cast<AFCTaiheExploder>(Found[0]);
		}
	}
	return Exploder;
}

void AFCTwinPawn::DebugSetView(float InPivotZ, float InYaw, float InPitch, float InRadius)
{
	Mode = EFCCamMode::Direct;
	PivotVel = FVector::ZeroVector;
	YawVel = PitchVel = RadiusVel = 0.f;
	Pivot.Z = InPivotZ;
	Yaw = InYaw;
	Pitch = FMath::Clamp(InPitch, -89.f, 89.f);
	Radius = FMath::Clamp(InRadius, MinRadius, MaxRadius);
	ApplyCamera();
}

void AFCTwinPawn::ApplyCamera()
{
	const float Rad = FMath::DegreesToRadians(Yaw);
	const FVector CamDir(FMath::Cos(Rad), -FMath::Sin(Rad), 0.f); // Yaw=-90 -> (0,+1,0): south
	const float Horiz = Radius * FMath::Cos(FMath::DegreesToRadians(Pitch));
	const float Vert = -Radius * FMath::Sin(FMath::DegreesToRadians(Pitch));

	const FVector Loc = Pivot + CamDir * Horiz + FVector(0.f, 0.f, Vert);
	SetActorLocation(Loc);
	Camera->SetWorldRotation((Pivot - Loc).Rotation());
}

void AFCTwinPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	HandleDrag(DeltaSeconds);

	// Keyboard movement only while free-roaming (Direct / Coast); flights ignore it.
	if (Mode != EFCCamMode::Fly)
	{
		const float F = (bKeyFwd ? 1.f : 0.f) - (bKeyBack ? 1.f : 0.f);
		const float R = (bKeyRight ? 1.f : 0.f) - (bKeyLeft ? 1.f : 0.f);
		const float U = (bKeyUp ? 1.f : 0.f) - (bKeyDown ? 1.f : 0.f);
		if (!FMath::IsNearlyZero(F) || !FMath::IsNearlyZero(R) || !FMath::IsNearlyZero(U))
		{
			const float Scale = Radius * 0.9f * DeltaSeconds;
			Pivot += (ViewForward() * F + ViewRight() * R) * Scale;
			Pivot.Z += U * Scale;
			Pivot.Z = FMath::Clamp(Pivot.Z, -5000.f, 120000.f);

			// Direct control: kill any coast inertia so it does not fight the keys.
			PivotVel = FVector::ZeroVector;
			YawVel = PitchVel = RadiusVel = 0.f;
			Mode = EFCCamMode::Direct;
			ApplyCamera();
		}
	}

	if (Mode == EFCCamMode::Fly)
	{
		FlyElapsed += DeltaSeconds;
		float A = FMath::Clamp(FlyElapsed / FlyDuration, 0.f, 1.f);
		A = A * A * (3.f - 2.f * A); // smoothstep

		Pivot = FMath::Lerp(StartPivot, FlyPivot, A);
		Yaw = FMath::Lerp(StartYaw, FlyYaw, A);
		Pitch = FMath::Lerp(StartPitch, FlyPitch, A);
		Radius = FMath::Lerp(StartRadius, FlyRadius, A);
		ApplyCamera();

		if (FlyElapsed >= FlyDuration)
		{
			Pivot = FlyPivot;
			Yaw = FlyYaw;
			Pitch = FlyPitch;
			Radius = FlyRadius;
			ApplyCamera();

			if (bFlyShowCard && FlyPoiIndex != INDEX_NONE)
			{
				// Hide the name labels while the card is open so nearby labels
				// do not float across the close-up; they return when it closes.
				AFCPoiMarker::SetAllMarkersHidden(true);
				if (InfoCard)
				{
					InfoCard->ShowPoi(FlyPoiIndex);
				}
				UE_LOG(LogTemp, Warning, TEXT("[FC] Fly arrive idx=%d card shown"), FlyPoiIndex);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[FC] Fly arrive (no card)"));
			}

			Mode = EFCCamMode::Direct;
			PivotVel = FVector::ZeroVector;
			YawVel = PitchVel = RadiusVel = 0.f;
		}
	}
	else if (Mode == EFCCamMode::Coast)
	{
		// Exponential velocity decay: motion keeps going after release and the
		// speed eases off smoothly (spring-arm inertia).
		const float Fade = FMath::Exp(-CoastDamping * DeltaSeconds);
		YawVel *= Fade;
		PitchVel *= Fade;
		RadiusVel *= Fade;
		PivotVel *= Fade;

		Yaw += YawVel * DeltaSeconds;
		Pitch = FMath::Clamp(Pitch + PitchVel * DeltaSeconds, -89.f, -2.f);
		Radius = FMath::Clamp(Radius + RadiusVel * DeltaSeconds, MinRadius, MaxRadius);
		Pivot += PivotVel * DeltaSeconds;
		ApplyCamera();

		const float SpeedSq = YawVel * YawVel + PitchVel * PitchVel
			+ (RadiusVel / 1000.f) * (RadiusVel / 1000.f)
			+ PivotVel.SizeSquared() / 1000000.f;
		if (SpeedSq < 0.02f)
		{
			YawVel = PitchVel = RadiusVel = 0.f;
			PivotVel = FVector::ZeroVector;
			Mode = EFCCamMode::Direct;
		}
	}
}
