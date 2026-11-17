// Pocket Sand - hero pawn.

#include "Player/PSCharacter.h"
#include "AI/PSCrowdDirectorComponent.h"
#include "Components/PSHumanBodyComponent.h"
#include "Components/PSPourComponent.h"
#include "EngineUtils.h"
#include "Components/PSSuspicionComponent.h"
#include "Data/PSRoundConfig.h"
#include "Feedback/PSFeedback.h"
#include "Framework/PSEventSubsystem.h"
#include "World/PSArt.h"
#include "World/PSSandGrid.h"
#include "PocketSand.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

APSCharacter::APSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 90.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.0f, RotationRate, 0.0f);
	Move->MaxWalkSpeed = 430.0f;
	// Gentler start and stop so the walk clip shows between idle and run (user 2026-11-09: movement looks weird).
	Move->MaxAcceleration = 1200.0f;
	Move->BrakingDecelerationWalking = 1500.0f;
	Move->GroundFriction = 8.0f;
	Move->MaxStepHeight = 30.0f;
	Move->bConstrainToPlane = false;

	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(RootComponent);
	Boom->TargetArmLength = 1500.0f;
	Boom->bDoCollisionTest = false;
	Boom->bUsePawnControlRotation = true;
	Boom->bInheritRoll = false;
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 6.0f;
	Boom->SocketOffset = FVector(0.0f, 0.0f, 0.0f);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	Body = CreateDefaultSubobject<UPSHumanBodyComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	// Native Rocketbox mocap run (D22): 288 cm/s at rate 1 (probe_mocap.py); the hero runs at ~1.2x, waddles at ~1.0x.
	// Walk below jogging speed, blending into the native run (RBM_Walk 101 cm/s, RBM_Run 288): no slow-motion run
	// while accelerating or slowing down.
	// Brisk walk by default (Rocketbox m_walk_fast_01, 150 cm/s native), Shift runs (RBM_Run, 288 cm/s native):
	// each gait plays near its own speed, so there is no slow-motion jog (user 2026-11-10).
	Body->WalkClip = TEXT("RBM_WalkFast");
	Body->RunClip = TEXT("RBM_Run");
	Body->IdleClip = TEXT("RBM_Idle");
	Body->WalkAnimSpeed = 150.0f;
	Body->RunAnimSpeed = 288.0f;
	Pour = CreateDefaultSubobject<UPSPourComponent>(TEXT("Pour"));
	Suspicion = CreateDefaultSubobject<UPSSuspicionComponent>(TEXT("Suspicion"));

	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(RootComponent);
	Ring->SetUsingAbsoluteRotation(true);
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->SetCastShadow(false);
	Ring->SetRelativeLocation(FVector(0.0f, 0.0f, -87.0f));

	// Hero shirt: washed grey-blue, a shade lighter than the yard's denim, so he reads inside a crowd.
	Outfit.Top = FLinearColor(0.34f, 0.40f, 0.48f);
	Outfit.Bottom = FLinearColor(0.10f, 0.13f, 0.20f);
	Outfit.Shoes = FLinearColor(0.05f, 0.045f, 0.04f);
	Outfit.SkinTone = 0.3f;
	Outfit.Hair = 1;
	Outfit.HairTone = 0.35f;
	Outfit.SleeveEnd = 50.0f;
}

IPSMatchInfo* APSCharacter::Match() const
{
	return GetWorld() ? Cast<IPSMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
}

void APSCharacter::BeginPlay()
{
	Super::BeginPlay();
	Setup();
}

void APSCharacter::Setup()
{
	const IPSMatchInfo* M = Match();
	Cfg = M ? M->PS_Config() : nullptr;
	if (!Cfg)
	{
		return;
	}
	Pour->InitFromConfig(Cfg);
	Suspicion->InitFromConfig(Cfg);
	Boom->TargetArmLength = Cfg->CameraArm;
	if (Cfg->bThirdPerson)
	{
		// Chase camera: shoulder pivot, pulled in by walls and the cell block (props ignore the camera channel).
		Boom->TargetOffset = FVector(0.0f, 0.0f, Cfg->CameraPivot.Z);
		Boom->SocketOffset = FVector(0.0f, Cfg->CameraPivot.Y, 0.0f);
		Boom->bDoCollisionTest = true;
		Boom->ProbeChannel = ECC_Camera;
		Boom->ProbeSize = 14.0f;
		Boom->CameraLagSpeed = 10.0f;
		Boom->bEnableCameraRotationLag = true;
		Boom->CameraRotationLagSpeed = 14.0f;
	}
	float QAArm = 0.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("PSCamArm="), QAArm) && QAArm > 50.0f)
	{
		// QA only: portrait shots of the characters.
		Boom->TargetArmLength = QAArm;
	}
	Camera->SetFieldOfView(Cfg->CameraFOV);
	if (Outfit.Avatar.IsEmpty())
	{
		Outfit.Avatar = Cfg->HeroAvatar;
	}
	Body->Dress(Cfg, Outfit);
	if (UStaticMesh* Plane = PSArt::PlaneMesh())
	{
		Ring->SetStaticMesh(Plane);
		Ring->SetRelativeScale3D(FVector(RingRadius * 2.0f / 100.0f));
		if (UMaterialInterface* M2 = PSArt::Mat(EPSMat::Marker))
		{
			RingMID = UMaterialInstanceDynamic::Create(M2, this);
			RingMID->SetScalarParameterValue(TEXT("Shape"), 0.0f);
			Ring->SetMaterial(0, RingMID);
		}
	}
	// Two trouser bags: bulges strapped to the thighs, visible while full.
	for (const FName Bone : { Body->ThighBone(true), Body->ThighBone(false) })
	{
		if (UStaticMeshComponent* Bag = Body->AttachProp(PSArt::SphereMesh(), PSArt::Mat(EPSMat::Cloth), Bone, FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 0.0f), FVector(BagScale))))
		{
			Bag->SetVisibility(false);
			Bags.Add(Bag);
		}
	}
	Pour->OnBagEmptied.AddUObject(this, &APSCharacter::HandleBagEmptied);
	Pour->OnPourChanged.AddUObject(this, &APSCharacter::HandlePourChanged);
	if (UPSEventSubsystem* StateBus = UPSEventSubsystem::Get(this))
	{
		StateBus->OnStateChanged.AddUniqueDynamic(this, &APSCharacter::HandleMatchState);
	}
	StartLoc = GetActorLocation();
	StartYaw = GetActorRotation().Yaw;
	LastLoc = GetActorLocation();
	UpdateSpeed();
}

void APSCharacter::SetCameraYawOffset(float Offset)
{
	YawOffset = Offset;
}

FVector APSCharacter::FootLocation() const
{
	const FVector Back = -GetActorForwardVector() * (Pour ? Pour->PourBehind : 20.0f);
	const FVector L = GetActorLocation() + Back;
	return FVector(L.X, L.Y, GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

void APSCharacter::UpdateSpeed()
{
	if (!Cfg)
	{
		return;
	}
	const bool bHold = bLocked || LoadLeft > 0.0f;
	GetCharacterMovement()->MaxWalkSpeed = bHold ? 0.0f : (bRun ? Cfg->RunSpeed : Cfg->WalkSpeed) * (PS_IsWaddling() ? Cfg->WaddleSpeedMul : 1.0f);
}

void APSCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Cfg)
	{
		Setup();
		if (!Cfg)
		{
			return;
		}
	}
	IPSMatchInfo* M = Match();
	if (LoadLeft > 0.0f)
	{
		LoadLeft -= DeltaSeconds;
		if (LoadLeft <= 0.0f && !bCrouching)
		{
			Body->StopAction();
		}
	}
	if (LoadLeft > 0.0f && bHasLoadFacing)
	{
		const FVector To = LoadFacing - GetActorLocation();
		if (To.Size2D() > 20.0f)
		{
			const float Yaw = FMath::FixedTurn(GetActorRotation().Yaw, To.Rotation().Yaw, LoadTurnRate * DeltaSeconds);
			SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
		}
	}
	// Bag pickup: bend forward over the first third, hold while the hands take the bag, straighten over the last third.
	{
		const float Dur = FMath::Max(Cfg->LoadSecondsPerBag, 0.05f);
		const float U = LoadLeft > 0.0f ? 1.0f - LoadLeft / Dur : 1.0f;
		static const bool bBendTest = FParse::Param(FCommandLine::Get(), TEXT("PSBendTest")); // QA: hold the full bend
		const float Want = bBendTest ? 1.0f : (LoadLeft > 0.0f && CrouchClip.IsEmpty() ? FMath::SmoothStep(0.0f, 0.33f, U) * (1.0f - FMath::SmoothStep(0.7f, 1.0f, U)) : 0.0f);
		Bend = FMath::FInterpTo(Bend, Want, DeltaSeconds, 18.0f);
		Body->SetBend(Bend);
	}
	UpdateSpeed();
	const float Speed = GetVelocity().Size2D();
	// QA (-PSNoWaddle): isolates the clip from the procedural waddle in gait measurements.
	static const bool bNoWaddle = FParse::Param(FCommandLine::Get(), TEXT("PSNoWaddle"));
	Body->SetLocomotion(Speed, PS_IsWaddling() && !bNoWaddle ? 1.0f : 0.0f);

	// Footsteps (audio + dust): heavier and shorter when waddling.
	const FVector Loc = GetActorLocation();
	StepAccum += FVector::Dist2D(Loc, LastLoc);
	LastLoc = Loc;
	const float StepLen = StepDistance * (PS_IsWaddling() ? 0.8f : 1.0f);
	if (StepAccum >= StepLen)
	{
		StepAccum = 0.0f;
		if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
		{
			Bus->OnFootstep.Broadcast(FVector(Loc.X, Loc.Y, Loc.Z - 88.0f));
		}
	}

	const bool bActive = M && M->PS_State() == EPSMatchState::Active;
	APSSandGrid* Grid = M ? M->PS_Grid() : nullptr;
	const FVector Foot = FootLocation();
	Pour->Step(DeltaSeconds, Grid, Foot, bActive && LoadLeft <= 0.0f);
	// Sand work crouched: down while taking bags at the door and while pouring on the spot; up as soon as he walks.
	if (!CrouchClip.IsEmpty())
	{
		StillTime = Speed < StillSpeed ? StillTime + DeltaSeconds : 0.0f;
		const bool bWork = bActive && (LoadLeft > 0.0f || (Pour->IsPouring() && StillTime >= StillToCrouch));
		CrouchLinger = bWork ? CrouchLingerSeconds : (Speed > StandSpeed ? 0.0f : FMath::Max(0.0f, CrouchLinger - DeltaSeconds));
		const bool bWant = bWork || CrouchLinger > 0.0f;
		if (bCrouching && bWant)
		{
			if (LoadLeft > 0.0f)
			{
				CrouchYaw = GetActorRotation().Yaw; // still turning to face the door
			}
			else
			{
				SetActorRotation(FRotator(0.0f, CrouchYaw, 0.0f));
			}
		}
		if (bWant && !bCrouching)
		{
			CrouchYaw = GetActorRotation().Yaw;
			Body->SetActionBlendSpeed(CrouchBlendSpeed);
			Body->PlayAction(CrouchClip, true, 1.0f);
			bCrouching = true;
		}
		else if (!bWant && bCrouching)
		{
			Body->StopAction();
			bCrouching = false;
		}
	}
	if (Pour->IsPouring())
	{
		FxClock += DeltaSeconds;
		if (FxClock >= TrickleFxInterval)
		{
			FxClock = 0.0f;
			PSFeedback::Vfx(this, TEXT("Trickle"), Foot + FVector(0.0f, 0.0f, 8.0f), FRotator(-90.0f, 0.0f, 0.0f), TrickleFxScale);
		}
	}
	if (UPSCrowdDirectorComponent* Crowd = M ? M->PS_Crowd() : nullptr)
	{
		bCrowdCover = Crowd->CountNear(Loc, Cfg->CrowdCoverRadius) >= Cfg->CrowdCoverCount;
	}
	if (bActive)
	{
		Suspicion->Step(DeltaSeconds, ComputeSuspicionGain(DeltaSeconds));
	}
	UpdateRing(DeltaSeconds);
	UpdateBags(DeltaSeconds);
}

float APSCharacter::ComputeSuspicionGain(float Dt)
{
	IPSMatchInfo* M = Match();
	if (!M || !Cfg)
	{
		return 0.0f;
	}
	const APSSandGrid* Grid = M->PS_Grid();
	float Exposure = 1.0f;
	if (Grid)
	{
		const int32 Idx = Grid->Field().IndexOf(FVector2D(GetActorLocation()));
		if (Idx != INDEX_NONE)
		{
			Exposure = Grid->Field().RowAt(Idx).ExposureMul;
		}
	}
	float Gain = 0.0f;
	const FVector Eye = GetActorLocation();
	for (const TWeakObjectPtr<AActor>& W : M->PS_Watchers())
	{
		IPSWatcher* Watcher = Cast<IPSWatcher>(W.Get());
		if (!Watcher || !Watcher->PS_CanSee(Eye, Exposure))
		{
			continue;
		}
		float G = 0.0f;
		if (Pour->IsPouring())
		{
			G = Watcher->PS_PourSeenGain() * (bCrowdCover ? Cfg->CrowdCoverMul : 1.0f);
			Watcher->PS_Watch(this);
		}
		else if (PS_IsWaddling())
		{
			G = Cfg->WaddleSeenGain;
			Watcher->PS_Watch(this);
		}
		Gain = FMath::Max(Gain, G);
	}
	return Gain;
}

void APSCharacter::UpdateRing(float Dt)
{
	IPSMatchInfo* M = Match();
	const APSSandGrid* Grid = M ? M->PS_Grid() : nullptr;
	if (!Grid || !RingMID)
	{
		return;
	}
	// The ring previews the rule under your feet: solid off-white = hidden here, broken red = it shows.
	const FVector2D Foot(FootLocation());
	const bool bHide = Grid->Field().FootprintHides(Foot, Pour->SpreadRadius) || bCrowdCover;
	bShowsHere = !bHide;
	const bool bShowRing = M->PS_State() == EPSMatchState::Active || M->PS_State() == EPSMatchState::Ready;
	const float Carry = Pour->Carried() > 0 ? 1.0f : 0.45f;
	RingMID->SetScalarParameterValue(TEXT("Broken"), bShowsHere ? 1.0f : 0.0f);
	RingMID->SetVectorParameterValue(TEXT("Color"), bShowsHere ? FLinearColor(0.75f, 0.065f, 0.045f) : FLinearColor(0.85f, 0.82f, 0.77f));
	RingMID->SetScalarParameterValue(TEXT("Opacity"), bShowRing ? (Pour->IsPouring() ? 0.95f : 0.7f) * Carry : 0.0f);
	const float Z = Grid->SurfaceTopZ(Foot);
	Ring->SetWorldLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Z + 2.0f));
}

void APSCharacter::UpdateBags(float Dt)
{
	BagPop = FMath::Max(0.0f, BagPop - Dt * 3.0f);
	const int32 N = Pour->Carried();
	for (int32 I = 0; I < Bags.Num(); ++I)
	{
		const bool bOn = I < N;
		Bags[I]->SetVisibility(bOn);
		if (bOn)
		{
			const float Fill = (I == N - 1) ? FMath::Lerp(0.55f, 1.0f, Pour->CurrentBag()) : 1.0f;
			Bags[I]->SetRelativeScale3D(FVector(BagScale * Fill * (1.0f + 0.25f * BagPop)));
		}
	}
}

void APSCharacter::HandleBagEmptied(const FVector& Where)
{
	BagPop = 1.0f;
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnBagEmptied.Broadcast(Pour->Carried(), Where);
	}
}

void APSCharacter::HandlePourChanged(bool bOn, const FVector& Where)
{
	// Hands: brushing the trouser legs while the sand trickles out (Rocketbox mocap, upper body over the walk).
	if (bOn && LoadLeft <= 0.0f && !PourClip.IsEmpty())
	{
		Body->PlayAction(PourClip, true, PourClipRate);
	}
	else if (!bOn && LoadLeft <= 0.0f && !bCrouching)
	{
		Body->StopAction();
	}
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnPourChanged.Broadcast(bOn, Where);
	}
}

int32 APSCharacter::PS_Carried() const { return Pour ? Pour->Carried() : 0; }
float APSCharacter::PS_CurrentBag() const { return Pour ? Pour->CurrentBag() : 0.0f; }
bool APSCharacter::PS_IsPouring() const { return Pour && Pour->IsPouring(); }
bool APSCharacter::PS_IsWaddling() const { return Pour && Pour->Carried() >= FMath::Max(2, Pour->MaxCarried); }
float APSCharacter::PS_Suspicion() const { return Suspicion ? Suspicion->Value() : 0.0f; }

void APSCharacter::PS_AddBag()
{
	if (Pour->AddBag())
	{
		LoadLeft = Cfg ? Cfg->LoadSecondsPerBag : 0.5f;
		// Face the bag store while taking a bag (not the yard behind you).
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (const IPSInteractable* I = Cast<IPSInteractable>(*It))
			{
				LoadFacing = I->PS_InteractLocation();
				bHasLoadFacing = true;
			}
		}
		// Reach out and take the bag (Rocketbox mocap; the clip's reach-and-take is its first ~1.6 s, then it blends out).
		if (CrouchClip.IsEmpty())
		{
			Body->PlayAction(LoadClip, false, LoadClipRate);
		}
		BagPop = 1.0f;
		if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
		{
			Bus->OnBagLoaded.Broadcast(Pour->Carried(), GetActorLocation());
		}
	}
}

void APSCharacter::PS_SetWantPour(bool bWant)
{
	Pour->SetWantPour(bWant && !bLocked);
}

void APSCharacter::PS_ResetCarrier(const FVector& Location, float Yaw)
{
	Pour->ResetBags();
	Suspicion->ResetValue();
	LoadLeft = 0.0f;
	bCrouching = false;
	CrouchLinger = 0.0f;
	Body->StopAction();
	GetCharacterMovement()->StopMovementImmediately();
	SetActorLocationAndRotation(Location, FRotator(0.0f, Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	StartLoc = Location;
	StartYaw = Yaw;
	LastLoc = Location;
	YawOffset = 0.0f;
}

void APSCharacter::PS_LockMovement(bool bLock)
{
	bLocked = bLock;
	if (bLock)
	{
		GetCharacterMovement()->StopMovementImmediately();
		Pour->SetWantPour(false);
	}
	UpdateSpeed();
}

void APSCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	// Never fall into a void: back to the cell-block door with zero velocity.
	UE_LOG(LogPocketSand, Warning, TEXT("[PocketSand] Hero fell out of the world - respawning at the door."));
	GetCharacterMovement()->StopMovementImmediately();
	SetActorLocationAndRotation(StartLoc + FVector(0.0f, 0.0f, 20.0f), FRotator(0.0f, StartYaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
}

void APSCharacter::HandleMatchState(EPSMatchState State, EPSDefeat Reason)
{
	// Ending poses for the victory orbit / defeat push-in: dust off the trousers, or shrug.
	bCrouching = false;
	CrouchLinger = 0.0f;
	Body->SetActionBlendSpeed(ActionBlendSpeedDefault);
	if (State == EPSMatchState::Victory)
	{
		Body->PlayAction(VictoryClip, false, 1.0f);
	}
	else if (State == EPSMatchState::Defeat)
	{
		Body->PlayAction(DefeatClip, false, 1.0f);
	}
	else if (State == EPSMatchState::Ready || State == EPSMatchState::Menu)
	{
		Body->StopAction();
	}
}

void APSCharacter::PS_SetRun(bool bInRun)
{
	bRun = bInRun;
}
