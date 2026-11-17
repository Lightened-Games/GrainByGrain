// Pocket Sand - project material paths (built by Tools/Editor/build_content.py) and engine basic shapes.
// Soft paths, loaded at runtime, so no constructor hard references root the materials (design guide 15).

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UStaticMesh;

enum class EPSMat : uint8
{
	Gravel, Soil, Court, Pit, Apron, Outer, Mound,
	Concrete, ConcreteDark, ConcretePaint, Steel, SteelDark, Wood, Rubber,
	Water, ChainLink, Net, Razor, Lamp, Window,
	Marker, Cone, Chevron, Cloth, Ball, Cap, Stick,
	Door, DoorFrame,
	Count
};

namespace PSArt
{
	POCKETSAND_API UMaterialInterface* Mat(EPSMat Which);
	POCKETSAND_API const TCHAR* MatPath(EPSMat Which);
	/** /Engine/BasicShapes: geometry bases under our materials (Unreal Engine built-in). */
	POCKETSAND_API UStaticMesh* PlaneMesh();
	POCKETSAND_API UStaticMesh* SphereMesh();
	POCKETSAND_API UStaticMesh* CylinderMesh();
	POCKETSAND_API UStaticMesh* CubeMesh();
}
