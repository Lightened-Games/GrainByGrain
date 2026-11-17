// Pocket Sand - cell-block door stash.

#include "World/PSDoorStash.h"
#include "Camera/PlayerCameraManager.h"
#include "Data/PSRoundConfig.h"
#include "World/PSArt.h"
#include "World/PSMeshLib.h"
#include "ProceduralMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

APSDoorStash::APSDoorStash()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Chevron = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Chevron"));
	Chevron->SetupAttachment(RootComponent);
	Chevron->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Chevron->SetCastShadow(false);
}

IPSMatchInfo* APSDoorStash::Match() const
{
	return GetWorld() ? Cast<IPSMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
}

void APSDoorStash::Setup(const UPSRoundConfig* Config)
{
	Cfg = Config;
	if (!Cfg)
	{
		return;
	}
	SetActorLocation(FVector(Cfg->DoorLocation.X, Cfg->DoorLocation.Y - StandOff, 0.0f));
	// A downward chevron: two bevelled bars meeting in a point, in the X-Z plane (faces the camera, which looks north).
	FPSMeshData V;
	const float Arm = ChevronSize * 0.55f;
	const float Thick = ChevronSize * 0.13f;
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		PSMesh::AddBox(V, FTransform(FRotator(Side * 40.0f, 0.0f, 0.0f), FVector(Side * Arm * 0.36f, 0.0f, Arm * 0.3f)), FVector(Arm * 0.5f, Thick * 0.5f, Thick * 0.5f), Thick * 0.2f, 1.0f);
	}
	PSMesh::Commit(Chevron, 0, V, nullptr, false);
	if (UMaterialInterface* M = PSArt::Mat(EPSMat::Chevron))
	{
		ChevronMID = UMaterialInstanceDynamic::Create(M, this);
		Chevron->SetMaterial(0, ChevronMID);
	}
}

void APSDoorStash::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	const IPSMatchInfo* M = Match();
	const bool bAny = M && M->PS_BagsInCell() > 0 && M->PS_State() == EPSMatchState::Active;
	APawn* Hero = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
	const IPSSandCarrier* Carrier = Cast<IPSSandCarrier>(Hero);
	const bool bWanted = bAny && Carrier && Carrier->PS_Carried() == 0;
	// Chevron bobs (ease) and is loud only when the trousers are empty.
	const float Bob = FMath::Sin(Clock * BobSpeed) * BobAmplitude;
	Chevron->SetRelativeLocation(FVector(0.0f, 0.0f, ChevronHeight + Bob));
	if (ChevronMID)
	{
		// Close up, the "E  LOAD" prompt does the talking: the chevron fades so it never fills the centre of the frame.
		const float Near = Hero ? FMath::Clamp((FVector::Dist2D(Hero->GetActorLocation(), GetActorLocation()) - NearFadeStart) / FMath::Max(NearFadeRange, 1.0f), 0.0f, 1.0f) : 1.0f;
		// Also fade when the camera itself is close (just out of the door, the chevron hangs right in front of the lens).
		const APlayerCameraManager* Cam = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->PlayerCameraManager.Get() : nullptr;
		const float CamNear = Cam ? FMath::Clamp((FVector::Dist(Cam->GetCameraLocation(), Chevron->GetComponentLocation()) - CameraFadeStart) / FMath::Max(CameraFadeRange, 1.0f), 0.0f, 1.0f) : 1.0f;
		const float Base = bAny ? (bWanted ? 0.95f : 0.35f) : 0.0f;
		ChevronMID->SetScalarParameterValue(TEXT("Opacity"), Base * FMath::Lerp(NearFadeMin, 1.0f, FMath::Min(Near, CamNear)));
	}
}

bool APSDoorStash::PS_CanInteract(const AActor* User) const
{
	const IPSMatchInfo* M = Match();
	const IPSSandCarrier* Carrier = Cast<IPSSandCarrier>(User);
	if (!M || !Carrier || !Cfg || M->PS_BagsInCell() <= 0 || M->PS_State() != EPSMatchState::Active)
	{
		return false;
	}
	if (Carrier->PS_Carried() >= Cfg->MaxCarried || Carrier->PS_IsLoading())
	{
		return false;
	}
	return FVector::Dist2D(User->GetActorLocation(), PS_InteractLocation()) <= PS_InteractRadius();
}

bool APSDoorStash::PS_Interact(AActor* User)
{
	if (!PS_CanInteract(User))
	{
		return false;
	}
	IPSMatchInfo* M = Match();
	IPSSandCarrier* Carrier = Cast<IPSSandCarrier>(User);
	if (M && Carrier && M->PS_TakeBagFromCell())
	{
		Carrier->PS_AddBag();
		return true;
	}
	return false;
}

FVector APSDoorStash::PS_InteractLocation() const
{
	return GetActorLocation();
}

float APSDoorStash::PS_InteractRadius() const
{
	return Cfg ? Cfg->LoadRadius : 260.0f;
}
