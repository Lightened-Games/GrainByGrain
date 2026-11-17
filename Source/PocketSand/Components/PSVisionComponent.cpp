// Pocket Sand - vision cone.

#include "Components/PSVisionComponent.h"
#include "World/PSArt.h"
#include "World/PSSandGrid.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

UPSVisionComponent::UPSVisionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPSVisionComponent::InitCone(float InRange, float InHalfAngle)
{
	Range = InRange;
	HalfAngle = InHalfAngle;
	BuildCone();
}

void UPSVisionComponent::BuildCone()
{
	if (!GetOwner())
	{
		return;
	}
	if (!Cone)
	{
		Cone = NewObject<UProceduralMeshComponent>(GetOwner(), TEXT("ViewCone"));
		Cone->SetupAttachment(this);
		Cone->SetUsingAbsoluteLocation(true);
		Cone->SetUsingAbsoluteRotation(true);
		Cone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cone->SetCastShadow(false);
		Cone->RegisterComponent();
		GetOwner()->AddInstanceComponent(Cone);
	}
	// Fan in local space pointing +X. UV.x = distance fraction (0 at the eye), UV.y = across the cone (0..1).
	TArray<FVector> V;
	TArray<int32> T;
	TArray<FVector> N;
	TArray<FVector2D> UV;
	const int32 Segs = FMath::Max(4, ConeSegments);
	const int32 Rings = 6;
	for (int32 R = 0; R <= Rings; ++R)
	{
		const float D = Range * R / Rings;
		for (int32 S = 0; S <= Segs; ++S)
		{
			const float A = FMath::DegreesToRadians(FMath::Lerp(-HalfAngle, HalfAngle, static_cast<float>(S) / Segs));
			V.Add(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.0f));
			N.Add(FVector::UpVector);
			UV.Add(FVector2D(static_cast<float>(R) / Rings, static_cast<float>(S) / Segs));
		}
	}
	const int32 Row = Segs + 1;
	for (int32 R = 0; R < Rings; ++R)
	{
		for (int32 S = 0; S < Segs; ++S)
		{
			const int32 A = R * Row + S, B = A + 1, C = A + Row, E = C + 1;
			// Front faces up (UE: Cross(B-A, C-A) points away from the visible side).
			T.Append({ A, B, C, B, E, C });
		}
	}
	Cone->CreateMeshSection(0, V, T, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	if (UMaterialInterface* M = PSArt::Mat(EPSMat::Cone))
	{
		ConeMID = UMaterialInstanceDynamic::Create(M, this);
		ConeMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.75f, 0.065f, 0.045f));
		ConeMID->SetScalarParameterValue(TEXT("Opacity"), ConeOpacity);
		Cone->SetMaterial(0, ConeMID);
	}
}

void UPSVisionComponent::SetGaze(const FVector& EyeWorld, float WorldYaw)
{
	EyePos = EyeWorld;
	Yaw = WorldYaw;
	if (Cone)
	{
		Cone->SetWorldLocationAndRotation(FVector(EyeWorld.X, EyeWorld.Y, ConeLift), FRotator(0.0f, WorldYaw, 0.0f));
	}
}

FVector UPSVisionComponent::Dir() const
{
	return FRotator(0.0f, Yaw, 0.0f).Vector();
}

bool UPSVisionComponent::CanSee(const FVector& Point, float ExposureMul) const
{
	const FVector2D To = FVector2D(Point) - FVector2D(EyePos);
	const float L = To.Size();
	if (L > Range * FMath::Max(ExposureMul, 0.1f))
	{
		return false;
	}
	if (L < 60.0f)
	{
		return true;
	}
	const float Cos = FVector2D::DotProduct(To / L, FVector2D(Dir()));
	// Exposure widens the cone a little too: a man standing in the pit is seen out of the corner of an eye.
	const float Half = FMath::Min(HalfAngle * FMath::Lerp(1.0f, 1.35f, FMath::Clamp(ExposureMul - 1.0f, 0.0f, 1.0f)), 89.0f);
	return Cos >= FMath::Cos(FMath::DegreesToRadians(Half));
}

bool UPSVisionComponent::ScanPatch(const APSSandGrid* Grid, float MinMass, float NoticeSeconds, float Dt, FVector& OutLocation)
{
	if (!Grid)
	{
		return false;
	}
	FVector2D Centre;
	float Mass = 0.0f;
	const bool bFound = Grid->Field().BestPatch(FVector2D(EyePos), FVector2D(Dir()), Range, FMath::Cos(FMath::DegreesToRadians(HalfAngle)), PatchBin, MinMass, Centre, Mass);
	if (!bFound)
	{
		Stare = FMath::Max(0.0f, Stare - Dt * 2.0f);
		return false;
	}
	if (Stare > 0.0f && FVector2D::Distance(Centre, StareAt) > PatchStickiness)
	{
		Stare = 0.0f;
	}
	StareAt = Centre;
	Stare += Dt;
	if (Stare >= NoticeSeconds)
	{
		Stare = 0.0f;
		OutLocation = FVector(Centre, 0.0f);
		return true;
	}
	return false;
}

void UPSVisionComponent::SetConeLook(float Alert, bool bShow)
{
	if (Cone)
	{
		Cone->SetVisibility(bShow);
	}
	if (ConeMID)
	{
		ConeMID->SetScalarParameterValue(TEXT("Opacity"), FMath::Lerp(ConeOpacity, AlertOpacity, FMath::Clamp(Alert, 0.0f, 1.0f)));
	}
}
