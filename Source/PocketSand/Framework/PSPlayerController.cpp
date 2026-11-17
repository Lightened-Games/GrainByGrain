// Pocket Sand - player controller.

#include "Framework/PSPlayerController.h"
#include "Framework/PSEventSubsystem.h"
#include "Components/PSAutoplayComponent.h"
#include "Components/PSGameFeelComponent.h"
#include "Data/PSRoundConfig.h"
#include "UI/PSHUDWidget.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "PocketSand.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"

const FName APSPlayerController::KeyMove(TEXT("Move"));
const FName APSPlayerController::KeyPour(TEXT("Pour"));
const FName APSPlayerController::KeyLoad(TEXT("Load"));
const FName APSPlayerController::KeyRun(TEXT("Run"));
const FName APSPlayerController::KeyRestart(TEXT("Restart"));
const FName APSPlayerController::KeyLook(TEXT("Look"));

APSPlayerController::APSPlayerController()
{
	bShowMouseCursor = false;
	Autoplay = CreateDefaultSubobject<UPSAutoplayComponent>(TEXT("Autoplay"));
	GameFeel = CreateDefaultSubobject<UPSGameFeelComponent>(TEXT("GameFeel"));
	HUDWidgetClass = UPSHUDWidget::StaticClass();
}

IPSMatchInfo* APSPlayerController::Match() const
{
	return GetWorld() ? Cast<IPSMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
}

void APSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		SetupHUD();
		SetInputMode(FInputModeGameOnly());
	}
}

void APSPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (IsLocalController())
	{
		SetupHUD();
	}
}

void APSPlayerController::SetupHUD()
{
	if (HUDWidget || !HUDWidgetClass || !IsLocalController())
	{
		return;
	}
	HUDWidget = CreateWidget<UPSHUDWidget>(this, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(0);
	}
	else
	{
		UE_LOG(LogPocketSand, Error, TEXT("[PocketSand] Failed to create the HUD widget."));
	}
}

void APSPlayerController::BuildInput()
{
	if (Context)
	{
		return;
	}
	MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	PourAction = NewObject<UInputAction>(this, TEXT("IA_Pour"));
	LoadAction = NewObject<UInputAction>(this, TEXT("IA_Load"));
	RestartAction = NewObject<UInputAction>(this, TEXT("IA_Restart"));
	LookAction = NewObject<UInputAction>(this, TEXT("IA_Look"));
	LookAction->ValueType = EInputActionValueType::Axis1D;
	LookUpAction = NewObject<UInputAction>(this, TEXT("IA_LookUp"));
	LookUpAction->ValueType = EInputActionValueType::Axis1D;
	Context = NewObject<UInputMappingContext>(this, TEXT("IMC_PocketSand"));
	// W/S drive Y (forward), A/D drive X (right).
	auto Map2D = [this](const FKey& Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& M = Context->MapKey(MoveAction, Key);
		if (bSwizzle)
		{
			M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		}
		if (bNegate)
		{
			M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		}
	};
	Map2D(EKeys::W, true, false);
	Map2D(EKeys::S, true, true);
	Map2D(EKeys::D, false, false);
	Map2D(EKeys::A, false, true);
	Map2D(EKeys::Up, true, false);
	Map2D(EKeys::Down, true, true);
	Map2D(EKeys::Right, false, false);
	Map2D(EKeys::Left, false, true);
	Context->MapKey(PourAction, EKeys::SpaceBar);
	Context->MapKey(PourAction, EKeys::LeftMouseButton);
	Context->MapKey(LoadAction, EKeys::E);
	Context->MapKey(RestartAction, EKeys::R);
	RunAction = NewObject<UInputAction>(this, TEXT("IA_Run"));
	Context->MapKey(RunAction, EKeys::LeftShift);
	Context->MapKey(RunAction, EKeys::RightShift);
	MenuAction = NewObject<UInputAction>(this, TEXT("IA_Menu"));
	Context->MapKey(MenuAction, EKeys::Escape);
	Context->MapKey(LookAction, EKeys::MouseX);
	Context->MapKey(LookUpAction, EKeys::MouseY);
}

void APSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BuildInput();
	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Sub->AddMappingContext(Context, 0);
		}
	}
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APSPlayerController::OnMove);
		EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &APSPlayerController::OnMoveStop);
		EIC->BindAction(PourAction, ETriggerEvent::Started, this, &APSPlayerController::OnPourStart);
		EIC->BindAction(PourAction, ETriggerEvent::Completed, this, &APSPlayerController::OnPourStop);
		EIC->BindAction(LoadAction, ETriggerEvent::Started, this, &APSPlayerController::OnLoad);
		EIC->BindAction(RestartAction, ETriggerEvent::Started, this, &APSPlayerController::OnRestart);
		EIC->BindAction(MenuAction, ETriggerEvent::Started, this, &APSPlayerController::OnMenu);
	EIC->BindAction(RunAction, ETriggerEvent::Started, this, &APSPlayerController::OnRunStart);
	EIC->BindAction(RunAction, ETriggerEvent::Completed, this, &APSPlayerController::OnRunStop);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &APSPlayerController::OnLook);
		EIC->BindAction(LookUpAction, ETriggerEvent::Triggered, this, &APSPlayerController::OnLookUp);
	}
	else
	{
		UE_LOG(LogPocketSand, Error, TEXT("[PocketSand] InputComponent is not an EnhancedInputComponent; check DefaultInput.ini."));
	}
}

bool APSPlayerController::HumanBlocked() const
{
	// While the bot drives, stray desktop keystrokes must not interfere (design guide 15). FactoryKit take-over: the
	// first real key or mouse move hands the hero to the player; the bot resumes after the configured idle time.
	const IPSMatchInfo* M = Match();
	const UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this);
	return M && M->PS_IsAutoplay() && (!Flow || Flow->IsBotDriving());
}

void APSPlayerController::Visualize(FName Key, bool bSimulated)
{
	if (UPSEventSubsystem* Bus = UPSEventSubsystem::Get(this))
	{
		Bus->OnInput.Broadcast(Key, bSimulated);
	}
	// The FactoryKit legend is the video's visible input: light the row from both the player and the bot paths.
	if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
	{
		Flow->PulseLegend(Key);
	}
}

float APSPlayerController::CameraYaw() const
{
	const IPSMatchInfo* M = Match();
	const UPSRoundConfig* C = M ? M->PS_Config() : nullptr;
	return (C ? C->CameraBaseYaw : 90.0f) + YawOffset;
}

void APSPlayerController::OnMove(const FInputActionValue& Value)
{
	if (HumanBlocked())
	{
		return;
	}
	HumanMove = Value.Get<FVector2D>();
}

void APSPlayerController::OnMoveStop(const FInputActionValue& Value)
{
	HumanMove = FVector2D::ZeroVector;
}

void APSPlayerController::OnPourStart()
{
	if (!HumanBlocked())
	{
		bPourHeld = true;
		PS_SetPour(true, false);
	}
}

void APSPlayerController::OnPourStop()
{
	if (!HumanBlocked())
	{
		bPourHeld = false;
		PS_SetPour(false, false);
	}
}

void APSPlayerController::OnLoad()
{
	if (!HumanBlocked())
	{
		PS_Load(false);
	}
}

void APSPlayerController::OnRestart()
{
	if (HumanBlocked())
	{
		return;
	}
	Visualize(KeyRestart, false);
	if (IPSMatchInfo* M = Match())
	{
		if (M->PS_State() == EPSMatchState::Victory || M->PS_State() == EPSMatchState::Defeat || M->PS_State() == EPSMatchState::Active)
		{
			M->PS_RequestRestart();
		}
	}
}

void APSPlayerController::OnMenu()
{
	// Esc in play, countdown or an end screen: back to the start screen (reloads the level; the yard keeps living).
	const IPSMatchInfo* M = Match();
	UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this);
	if (HumanBlocked())
	{
		return; // the bot is driving (and in a recording it always is)
	}
	if (M && Flow && M->PS_State() != EPSMatchState::Menu)
	{
		Flow->ReturnToMenu();
	}
}

void APSPlayerController::OnLook(const FInputActionValue& Value)
{
	if (HumanBlocked())
	{
		return;
	}
	const IPSMatchInfo* M = Match();
	const UPSRoundConfig* C = M ? M->PS_Config() : nullptr;
	if (!C)
	{
		return;
	}
	if (LookIdle > 0.25f)
	{
		Visualize(KeyLook, false);
	}
	LookIdle = 0.0f;
	if (C->bThirdPerson)
	{
		YawOffset = FRotator::NormalizeAxis(YawOffset + Value.Get<float>() * C->MouseYawSpeed * UFactoryFlowSubsystem::GetMouseSensitivity());
	}
	else
	{
		YawOffset = FMath::Clamp(YawOffset + Value.Get<float>() * C->MouseYawSpeed * 0.6f, -C->CameraYawRange, C->CameraYawRange);
	}
}

void APSPlayerController::OnLookUp(const FInputActionValue& Value)
{
	const IPSMatchInfo* M = Match();
	const UPSRoundConfig* C = M ? M->PS_Config() : nullptr;
	if (HumanBlocked() || !C || !C->bThirdPerson)
	{
		return;
	}
	LookIdle = 0.0f;
	const float Invert = UFactoryFlowSubsystem::GetInvertY() ? -1.0f : 1.0f;
	Pitch = FMath::Clamp(Pitch + Value.Get<float>() * Invert * C->MousePitchSpeed * UFactoryFlowSubsystem::GetMouseSensitivity(), C->CameraPitchRange.X, C->CameraPitchRange.Y);
}

void APSPlayerController::UpdateCamera(float DeltaTime, const UPSRoundConfig* C)
{
	if (!bPitchInit)
	{
		Pitch = C->CameraPitch;
		bPitchInit = true;
	}
	if (!C->bThirdPerson)
	{
		Pitch = C->CameraPitch;
		if (!HumanBlocked())
		{
			YawOffset = FMath::FInterpTo(YawOffset, 0.0f, DeltaTime, YawRecenter);
		}
		return;
	}
	LookIdle += DeltaTime;
	const APawn* P = GetPawn();
	const FVector V = P ? P->GetVelocity() : FVector::ZeroVector;
	if (V.Size2D() < 60.0f)
	{
		// Autoplay, standing (pouring, loading): look across the yard toward its centre rather than at the wall the
		// hero last walked toward (the 35 s QA frame was two-thirds shaded wall).
		if (HumanBlocked() && P)
		{
			const FVector2D Centre = C->YardSize * 0.5f;
			const FVector2D ToCentre = Centre - FVector2D(P->GetActorLocation());
			if (ToCentre.Size() > C->IdleLookMinDistance)
			{
				const float Look = FMath::RadiansToDegrees(FMath::Atan2(ToCentre.Y, ToCentre.X));
				YawOffset = FRotator::NormalizeAxis(YawOffset + FMath::FindDeltaAngleDegrees(CameraYaw(), Look) * FMath::Min(1.0f, DeltaTime * C->IdleLookRate));
			}
		}
		return;
	}
	// Swing in behind the hero: always while the bot drives, and for a human only once the mouse has rested and they
	// are heading roughly away from the camera (strafing or backing up must not spin the view).
	// QA (-PSCamYawOff=, -PSCamPitch=): side-on leg shots while the bot drives.
	static float QAYawOff = 0.0f, QAPitch = 0.0f;
	static const bool bQAYaw = FParse::Value(FCommandLine::Get(), TEXT("PSCamYawOff="), QAYawOff);
	static const bool bQAPitch = FParse::Value(FCommandLine::Get(), TEXT("PSCamPitch="), QAPitch);
	const float Heading = V.Rotation().Yaw + (bQAYaw ? QAYawOff : 0.0f);
	const float Delta = FMath::FindDeltaAngleDegrees(CameraYaw(), Heading);
	float Rate = 0.0f;
	if (HumanBlocked())
	{
		Rate = bQAYaw ? 8.0f : C->AutoFollowRate;
		Pitch = bQAPitch ? QAPitch : FMath::FInterpTo(Pitch, C->CameraPitch, DeltaTime, 1.0f);
	}
	else if (C->FollowAfterIdle > 0.0f && LookIdle > C->FollowAfterIdle && FMath::Abs(Delta) < 100.0f)
	{
		Rate = C->HumanFollowRate;
	}
	YawOffset = FRotator::NormalizeAxis(YawOffset + Delta * FMath::Min(1.0f, DeltaTime * Rate));
}

void APSPlayerController::PS_SetMove(const FVector2D& WorldDir, bool bSimulated)
{
	WantMove = WorldDir.GetClampedToMaxSize(1.0f);
	if (!WantMove.IsNearlyZero())
	{
		if (MoveLightClock <= 0.0f)
		{
			Visualize(KeyMove, bSimulated);
		}
		MoveLightClock = 0.25f;
	}
}

void APSPlayerController::PS_SetPour(bool bPour, bool bSimulated)
{
	if (bPour)
	{
		Visualize(KeyPour, bSimulated);
	}
	if (IPSSandCarrier* C = Cast<IPSSandCarrier>(GetPawn()))
	{
		C->PS_SetWantPour(bPour);
	}
}

bool APSPlayerController::PS_Load(bool bSimulated)
{
	Visualize(KeyLoad, bSimulated);
	APawn* P = GetPawn();
	if (!P)
	{
		return false;
	}
	IPSInteractable* Best = nullptr;
	float BestD = BIG_NUMBER;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (IPSInteractable* I = Cast<IPSInteractable>(*It))
		{
			const float D = FVector::DistSquared2D(I->PS_InteractLocation(), P->GetActorLocation());
			if (D < BestD && I->PS_CanInteract(P))
			{
				BestD = D;
				Best = I;
			}
		}
	}
	return Best ? Best->PS_Interact(P) : false;
}

void APSPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	const IPSMatchInfo* M = Match();
	const UPSRoundConfig* C = M ? M->PS_Config() : nullptr;
	if (!C)
	{
		return;
	}
	MoveLightClock = FMath::Max(0.0f, MoveLightClock - DeltaTime);
	if (!HumanBlocked())
	{
		// Camera-relative WASD, and the camera eases back to centre when the mouse rests.
		const float Yaw = FMath::DegreesToRadians(CameraYaw());
		const FVector2D Fwd(FMath::Cos(Yaw), FMath::Sin(Yaw));
		const FVector2D Right(-Fwd.Y, Fwd.X);
		PS_SetMove(Fwd * HumanMove.Y - Right * HumanMove.X * -1.0f, false);
		if (!bPourHeld && M->PS_State() == EPSMatchState::Active)
		{
			PS_SetPour(false, false);
		}
	}
	UpdateCamera(DeltaTime, C);
	SetControlRotation(FRotator(Pitch, CameraYaw(), 0.0f));
	if (APawn* P = GetPawn())
	{
		if (!WantMove.IsNearlyZero())
		{
			P->AddMovementInput(FVector(WantMove, 0.0f), 1.0f);
		}
	}
	if (HumanBlocked())
	{
		WantMove = FVector2D::ZeroVector;
	}
}

void APSPlayerController::OnRunStart()
{
	if (!HumanBlocked())
	{
		PS_SetRun(true, false);
	}
}

void APSPlayerController::OnRunStop()
{
	if (!HumanBlocked())
	{
		PS_SetRun(false, false);
	}
}

void APSPlayerController::PS_SetRun(bool bRun, bool bSimulated)
{
	if (bRun && !bRunning)
	{
		Visualize(KeyRun, bSimulated);
	}
	bRunning = bRun;
	if (IPSSandCarrier* C = Cast<IPSSandCarrier>(GetPawn()))
	{
		C->PS_SetRun(bRun);
	}
}