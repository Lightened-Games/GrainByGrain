// Pocket Sand - project material lookup.

#include "World/PSArt.h"
#include "PocketSand.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/StrongObjectPtr.h"

const TCHAR* PSArt::MatPath(EPSMat Which)
{
	switch (Which)
	{
	case EPSMat::Gravel:        return TEXT("/Game/PocketSand/Materials/MI_PS_Gravel.MI_PS_Gravel");
	case EPSMat::Soil:          return TEXT("/Game/PocketSand/Materials/MI_PS_Soil.MI_PS_Soil");
	case EPSMat::Court:         return TEXT("/Game/PocketSand/Materials/MI_PS_Court.MI_PS_Court");
	case EPSMat::Pit:           return TEXT("/Game/PocketSand/Materials/MI_PS_Pit.MI_PS_Pit");
	case EPSMat::Apron:         return TEXT("/Game/PocketSand/Materials/MI_PS_Apron.MI_PS_Apron");
	case EPSMat::Outer:         return TEXT("/Game/PocketSand/Materials/MI_PS_Outer.MI_PS_Outer");
	case EPSMat::Mound:         return TEXT("/Game/PocketSand/Materials/MI_PS_Mound.MI_PS_Mound");
	case EPSMat::Concrete:      return TEXT("/Game/PocketSand/Materials/MI_PS_Concrete.MI_PS_Concrete");
	case EPSMat::ConcreteDark:  return TEXT("/Game/PocketSand/Materials/MI_PS_ConcreteDark.MI_PS_ConcreteDark");
	case EPSMat::ConcretePaint: return TEXT("/Game/PocketSand/Materials/MI_PS_ConcretePaint.MI_PS_ConcretePaint");
	case EPSMat::Steel:         return TEXT("/Game/PocketSand/Materials/MI_PS_Steel.MI_PS_Steel");
	case EPSMat::SteelDark:     return TEXT("/Game/PocketSand/Materials/MI_PS_SteelDark.MI_PS_SteelDark");
	case EPSMat::Wood:          return TEXT("/Game/PocketSand/Materials/MI_PS_Wood.MI_PS_Wood");
	case EPSMat::Rubber:        return TEXT("/Game/PocketSand/Materials/MI_PS_Rubber.MI_PS_Rubber");
	case EPSMat::Water:         return TEXT("/Game/PocketSand/Materials/MI_PS_Water.MI_PS_Water");
	case EPSMat::ChainLink:     return TEXT("/Game/PocketSand/Materials/MI_PS_ChainLink.MI_PS_ChainLink");
	case EPSMat::Net:           return TEXT("/Game/PocketSand/Materials/MI_PS_Net.MI_PS_Net");
	case EPSMat::Razor:         return TEXT("/Game/PocketSand/Materials/MI_PS_Razor.MI_PS_Razor");
	case EPSMat::Lamp:          return TEXT("/Game/PocketSand/Materials/MI_PS_Lamp.MI_PS_Lamp");
	case EPSMat::Window:        return TEXT("/Game/PocketSand/Materials/MI_PS_Window.MI_PS_Window");
	case EPSMat::Marker:        return TEXT("/Game/PocketSand/Materials/MI_PS_Marker.MI_PS_Marker");
	case EPSMat::Cone:          return TEXT("/Game/PocketSand/Materials/MI_PS_Cone.MI_PS_Cone");
	case EPSMat::Chevron:       return TEXT("/Game/PocketSand/Materials/MI_PS_Chevron.MI_PS_Chevron");
	case EPSMat::Cap:           return TEXT("/Game/PocketSand/Materials/MI_PS_Cap.MI_PS_Cap");
	case EPSMat::Stick:         return TEXT("/Game/PocketSand/Materials/MI_PS_Stick.MI_PS_Stick");
	case EPSMat::Cloth:         return TEXT("/Game/PocketSand/Materials/MI_PS_Cloth.MI_PS_Cloth");
	case EPSMat::Ball:          return TEXT("/Game/PocketSand/Materials/MI_PS_Ball.MI_PS_Ball");
	case EPSMat::Door:          return TEXT("/Game/PocketSand/Materials/MI_PS_Door.MI_PS_Door");
	case EPSMat::DoorFrame:     return TEXT("/Game/PocketSand/Materials/MI_PS_DoorFrame.MI_PS_DoorFrame");
	default:                    return TEXT("");
	}
}

UMaterialInterface* PSArt::Mat(EPSMat Which)
{
	static TStrongObjectPtr<UMaterialInterface> Cache[static_cast<int32>(EPSMat::Count)];
	const int32 Index = static_cast<int32>(Which);
	if (Index < 0 || Index >= static_cast<int32>(EPSMat::Count))
	{
		return nullptr;
	}
	if (!Cache[Index].IsValid())
	{
		UMaterialInterface* Loaded = LoadObject<UMaterialInterface>(nullptr, MatPath(Which));
		if (!Loaded)
		{
			UE_LOG(LogPocketSand, Error, TEXT("[PocketSand] Missing project material %s - run Tools/build_content.ps1"), MatPath(Which));
			return nullptr;
		}
		Cache[Index].Reset(Loaded);
	}
	return Cache[Index].Get();
}

namespace
{
	UStaticMesh* Shape(const TCHAR* Path, TStrongObjectPtr<UStaticMesh>& Slot)
	{
		if (!Slot.IsValid())
		{
			Slot.Reset(LoadObject<UStaticMesh>(nullptr, Path));
		}
		return Slot.Get();
	}
}

UStaticMesh* PSArt::PlaneMesh()
{
	static TStrongObjectPtr<UStaticMesh> S;
	return Shape(TEXT("/Engine/BasicShapes/Plane.Plane"), S);
}

UStaticMesh* PSArt::SphereMesh()
{
	static TStrongObjectPtr<UStaticMesh> S;
	return Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere"), S);
}

UStaticMesh* PSArt::CylinderMesh()
{
	static TStrongObjectPtr<UStaticMesh> S;
	return Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), S);
}

UStaticMesh* PSArt::CubeMesh()
{
	static TStrongObjectPtr<UStaticMesh> S;
	return Shape(TEXT("/Engine/BasicShapes/Cube.Cube"), S);
}
