// Pocket Sand - controls: WASD move (camera-relative), hold Space pour, E load a bag at the door, R restart, mouse looks
// around the third-person camera. Input actions are created at runtime (no template Input content).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/PSInterfaces.h"
#include "PSPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UPSAutoplayComponent;
class UPSGameFeelComponent;
class UPSHUDWidget;
struct FInputActionValue;

UCLASS()
class POCKETSAND_API APSPlayerController : public APlayerController, public IPSPlayerInput
{
	GENERATED_BODY()

public:
	APSPlayerController();

	static const FName KeyMove;
	static const FName KeyPour;
	static const FName KeyLoad;
	static const FName KeyRun;
	static const FName KeyRestart;
	static const FName KeyLook;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UPSAutoplayComponent> Autoplay;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PocketSand") TObjectPtr<UPSGameFeelComponent> GameFeel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PocketSand") TSubclassOf<UPSHUDWidget> HUDWidgetClass;
	/** Overhead mode: the camera drifts back to centre when the mouse rests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float YawRecenter = 0.35f;

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	// IPSPlayerInput
	virtual void PS_SetMove(const FVector2D& WorldDir, bool bSimulated) override;
	virtual void PS_SetPour(bool bPour, bool bSimulated) override;
	virtual bool PS_Load(bool bSimulated) override;
	virtual void PS_SetRun(bool bRun, bool bSimulated) override;

	/** Camera yaw in the world (base + mouse offset). The bot and the HUD convert directions with it. */
	float CameraYaw() const;

private:
	UPROPERTY() TObjectPtr<UPSHUDWidget> HUDWidget;
	UPROPERTY() TObjectPtr<UInputMappingContext> Context;
	UPROPERTY() TObjectPtr<UInputAction> MoveAction;
	UPROPERTY() TObjectPtr<UInputAction> PourAction;
	UPROPERTY() TObjectPtr<UInputAction> LoadAction;
	UPROPERTY() TObjectPtr<UInputAction> RestartAction;
	UPROPERTY() TObjectPtr<UInputAction> LookAction;
	UPROPERTY() TObjectPtr<UInputAction> LookUpAction;
	UPROPERTY() TObjectPtr<UInputAction> MenuAction;
	UPROPERTY() TObjectPtr<UInputAction> RunAction;
	void OnRunStart();
	bool bRunning = false;
	void OnRunStop();
	FVector2D HumanMove = FVector2D::ZeroVector;
	FVector2D WantMove = FVector2D::ZeroVector;
	float YawOffset = 0.0f;
	float Pitch = 0.0f;
	bool bPitchInit = false;
	float LookIdle = 0.0f;
	float MoveLightClock = 0.0f;
	bool bPourHeld = false;

	void SetupHUD();
	void BuildInput();
	bool HumanBlocked() const;
	class IPSMatchInfo* Match() const;
	void Visualize(FName Key, bool bSimulated);

	void OnMove(const FInputActionValue& Value);
	void OnMoveStop(const FInputActionValue& Value);
	void OnPourStart();
	void OnPourStop();
	void OnLoad();
	void OnRestart();
	void OnLook(const FInputActionValue& Value);
	void OnMenu();
	void OnLookUp(const FInputActionValue& Value);
	void UpdateCamera(float DeltaTime, const class UPSRoundConfig* C);
};
