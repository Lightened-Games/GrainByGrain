// Pocket Sand - camera shakes (adapted from The Fix).

#include "Feedback/PSCameraShakes.h"
#include "Shakes/PerlinNoiseCameraShakePattern.h"

namespace PSShake
{
	UPerlinNoiseCameraShakePattern* Make(UCameraShakeBase* Owner, float Duration, float BlendOut, float Loc, float Rot, float Freq)
	{
		UPerlinNoiseCameraShakePattern* P = Owner->CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("Pattern"));
		P->Duration = Duration;
		P->BlendInTime = 0.02f;
		P->BlendOutTime = BlendOut;
		P->LocationAmplitudeMultiplier = 1.0f;
		P->LocationFrequencyMultiplier = 1.0f;
		P->X.Amplitude = Loc * 0.5f; P->X.Frequency = Freq;
		P->Y.Amplitude = Loc;        P->Y.Frequency = Freq * 1.1f;
		P->Z.Amplitude = Loc;        P->Z.Frequency = Freq * 1.3f;
		P->RotationAmplitudeMultiplier = 1.0f;
		P->RotationFrequencyMultiplier = 1.0f;
		P->Pitch.Amplitude = Rot;       P->Pitch.Frequency = Freq * 1.2f;
		P->Yaw.Amplitude = Rot * 0.6f;  P->Yaw.Frequency = Freq;
		P->Roll.Amplitude = Rot * 0.5f; P->Roll.Frequency = Freq * 0.9f;
		return P;
	}
}

UPSImpactShake::UPSImpactShake(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = false;
	SetRootShakePattern(PSShake::Make(this, 0.4f, 0.22f, 8.0f, 0.8f, 18.0f));
}

UPSMicroShake::UPSMicroShake(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = false;
	SetRootShakePattern(PSShake::Make(this, 0.16f, 0.1f, 2.5f, 0.25f, 26.0f));
}
