#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FCTwinPawn.generated.h"

class UCameraComponent;
class UFCInfoCard;
class UFCHudShell;
class UFCCatalogPanel;
class AFCTaiheExploder;
class UFCFocusComponent;

/** How the camera pose is being advanced this frame. */
enum class EFCCamMode : uint8
{
	Direct,	// exactly at the described pose (idle, dragging, instant snaps)
	Coast,	// no input: velocities decay exponentially (inertia / spring-arm ease-out)
	Fly		// programmatic smoothstep flight to a target pose
};

/** Overview / digital-twin camera.
 *  LMB drag  = orbit around the pivot target
 *  RMB drag  = pan on the ground plane
 *  Wheel     = zoom (pivot distance)
 *  Double-LMB on empty ground = move pivot there
 *  Click on a POI name label  = smooth fly-to + info card. */
UCLASS()
class TRAE_UE_PROJECT_API AFCTwinPawn : public APawn
{
	GENERATED_BODY()

public:
	AFCTwinPawn();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity")
	void FlyToPoi(int32 PoiIndex);

	/** Runtime framing helper: keep the current pivot XY, override its Z / orbit pose (no recompile). */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity")
	void DebugSetView(float PivotZ, float InYaw, float InPitch, float InRadius);

/** Test helper: current horizontal orbit yaw / pitch / radius. */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity")
	void DebugGetState(float& OutYaw, float& OutPitch, float& OutRadius, bool& bOutFlying) const
	{
		OutYaw = Yaw; OutPitch = Pitch; OutRadius = Radius; bOutFlying = (Mode == EFCCamMode::Fly);
	}

	/** Test helper: one held-keyboard frame (F/R/U in [-1,1]), same math as Tick WASD/QE. */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity")
	void DebugInjectKeyboard(float Fwd, float Right, float Up, float DeltaSeconds);

	/** Test helper: one mouse-drag frame (pixels), same math as left/right HandleDrag. */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity")
	void DebugInjectDrag(float DX, float DY, bool bLeftButton, float DeltaSeconds);

	/** Test helper: one wheel notch, same math as OnZoom. */
	UFUNCTION(BlueprintCallable, Category = "ForbiddenCity")
	void DebugInjectZoom(float AxisValue);

	/** 建筑高亮/脉冲管理组件（点击建筑后调用其 SetFocus）。 */
	UFUNCTION(BlueprintPure, Category = "ForbiddenCity")
	UFCFocusComponent* GetFocusComponent() const { return FocusComponent; }

protected:
	virtual void BeginPlay() override;

	/** APawn has no SetupInputComponent override; bind manually after possession. */
	void BindControls();

private:
	void OnLeftPressed();
	void OnLeftReleased();
	void OnRightPressed();
	void OnRightReleased();
	UFUNCTION() void OnZoom(float AxisValue);

	// WASD / QE keyboard movement (paired press/release handlers)
	void OnKeyFwdPressed();
	void OnKeyFwdReleased();
	void OnKeyBackPressed();
	void OnKeyBackReleased();
	void OnKeyLeftPressed();
	void OnKeyLeftReleased();
	void OnKeyRightPressed();
	void OnKeyRightReleased();
	void OnKeyUpPressed();
	void OnKeyUpReleased();
	void OnKeyDownPressed();
	void OnKeyDownReleased();

	// HudShell (FCUiHud) button handlers
	UFUNCTION() void HandleHudOverview();
	UFUNCTION() void HandleHudPrev();
	UFUNCTION() void HandleHudNext();
	UFUNCTION() void HandleToggleTour();

	/** Left catalog list entry clicked: stop any tour and fly there. */
	UFUNCTION() void HandleCatalogSelected(int32 PoiIndex);

	/** Advance the guided tour to the next POI (also the 6s timer callback). */
	UFUNCTION() void GotoNextTour();
	void StartAutoTour();
	void StopAutoTour();

	void HandleDrag(float DeltaSeconds);
	void HandleClick(const FVector2D& ScreenPos);
	void HandleGroundDoubleClick(const FVector2D& ScreenPos);

	/** Info card close button: fly back to the view the camera had when the name was clicked. */
	UFUNCTION() void OnInfoCardClosed();

	/** "全局态势" button: fly to the initial city-wide overview. */
	UFUNCTION() void OnOverviewRequested();

	/** Taihe Hall card button: delegate the explode/assemble toggle to the exploder. */
	UFUNCTION() void OnExplodeToggle();

	/** Configure a smoothstep flight from the current pose to the given target. */
	void StartFlight(const FVector& InTargetPivot, float InTargetYaw, float InTargetPitch,
		float InTargetRadius, bool bShowCard, int32 CardPoiIndex);

	/** Horizontal ground-plane basis for the current yaw. */
	FVector YawForward() const; // pivot -> camera (screen-bottom direction on the ground)
	/** View-aligned horizontal basis: forward is where the camera looks on the ground. */
	FVector ViewForward() const; // camera look direction (into the screen, screen-top)
	FVector ViewRight() const;   // screen-right, same handedness as the view basis
	void ApplyCamera();
	void ReleaseMouseDrag();
	void RequestGameUiMode();
	AFCTaiheExploder* GetExploder();

	UPROPERTY(VisibleAnywhere, Category = "ForbiddenCity")
	UCameraComponent* Camera;

	UPROPERTY(Transient)
	UFCInfoCard* InfoCard = nullptr;

	UPROPERTY(Transient)
	UFCHudShell* HudShell = nullptr;

	UPROPERTY(Transient)
	UFCCatalogPanel* CatalogPanel = nullptr;

	UPROPERTY(Transient)
	AFCTaiheExploder* Exploder = nullptr;

	/** 点击建筑高亮子系统（默认子对象，BeginPlay 自动挂描边后处理材质）。 */
	UPROPERTY(VisibleAnywhere, Category = "ForbiddenCity")
	UFCFocusComponent* FocusComponent;

	// Current orbit pose
	// 默认：相机在午门以南沿中轴线向北仰视，注视点落在太和殿(X=0,Y=18000)。
	FVector Pivot = FVector(0.f, 20000.f, 3200.f);
	float Yaw = -90.f;
	float Pitch = -9.2f;
	float Radius = 47000.f;

	/** [DBG] throttled logging accumulator. */

	// Keyboard movement state
	bool bKeyFwd = false;
	bool bKeyBack = false;
	bool bKeyLeft = false;
	bool bKeyRight = false;
	bool bKeyUp = false;
	bool bKeyDown = false;

	// Guided tour state
	int32 CurrentPoiIndex = INDEX_NONE;
	bool bAutoTour = false;
	FTimerHandle TourTimerHandle;

	EFCCamMode Mode = EFCCamMode::Direct;

	// Coast velocities (cm/s and deg/s)
	FVector PivotVel = FVector::ZeroVector;
	float YawVel = 0.f;
	float PitchVel = 0.f;
	float RadiusVel = 0.f;

	// Drag state
	bool bLeftDown = false;
	bool bRightDown = false;
	bool bDragging = false;
	FVector2D PressPos = FVector2D::ZeroVector;
	/** 左键按下时刻（秒），松开时按时长区分"点击"与"长按/拖动"。 */
	double LeftPressTime = 0.0;
	bool bSkipDelta = false;
	double LastLeftClickTime = -100.0;
	FVector2D LastClickPos = FVector2D::ZeroVector;

	// View the camera had when a name label was clicked (restored when the card closes).
	bool bHasReturnView = false;
	FVector ReturnPivot = FVector::ZeroVector;
	float ReturnYaw = 0.f, ReturnPitch = 0.f, ReturnRadius = 0.f;

	// Fly interpolation (Start* -> Fly*)
	int32 FlyPoiIndex = INDEX_NONE;
	bool bFlyShowCard = false;
	FVector StartPivot = FVector::ZeroVector;
	float StartYaw = 0.f, StartPitch = 0.f, StartRadius = 0.f;
	FVector FlyPivot;
	float FlyYaw = 0.f, FlyPitch = 0.f, FlyRadius = 0.f;
	float FlyElapsed = 0.f;
	static constexpr float FlyDuration = 1.6f;

	// Tunables
	static constexpr float MinRadius = 2500.f;
	static constexpr float MaxRadius = 600000.f;
	static constexpr float ClickMovePx = 8.f;
	/** 视为"点击"的最大按住时长（秒）：超时即使没拖动也不触发点击，避免长按误选。 */
	static constexpr double MaxClickDuration = 0.5;
	static constexpr float DoubleClickTime = 0.35f;
	static constexpr float DoubleClickPx = 12.f;
	static constexpr float TraceLength = 1000000.f;
	static constexpr float CoastDamping = 2.4f; // exponential velocity decay while coasting
};
