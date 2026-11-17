// Pocket Sand - procedural geometry (bevelled boxes, tapered blocks, cylinders, lathes, cards) for code-built art.
// Every piece gets planar UVs in metres so the textured surface master tiles at a constant texel density.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

struct POCKETSAND_API FPSMeshData
{
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;

	int32 Num() const { return Verts.Num(); }
	bool IsEmpty() const { return Tris.Num() == 0; }
	void Append(const FPSMeshData& Other);
};

namespace PSMesh
{
	/** UV scale: UV units per metre (1 = texture repeats every metre before the material's own tiling). */
	POCKETSAND_API void AddTri(FPSMeshData& D, const FVector& A, const FVector& B, const FVector& C, const FVector& Normal, float UVPerM = 1.0f);
	POCKETSAND_API void AddQuad(FPSMeshData& D, const FVector& A, const FVector& B, const FVector& C, const FVector& E, const FVector& Normal, float UVPerM = 1.0f);

	/** Chamfered box: Half extents in local space, Bevel in world units. */
	POCKETSAND_API void AddBox(FPSMeshData& D, const FTransform& Xf, const FVector& Half, float Bevel = 0.0f, float UVPerM = 1.0f);

	/** Six-sided block from 8 corners: bottom (-x-y, +x-y, +x+y, -x+y) then top in the same order (local space). */
	POCKETSAND_API void AddBlock(FPSMeshData& D, const FTransform& Xf, const FVector Corners[8], float UVPerM = 1.0f);

	/** Box whose top is scaled by TopScale (x, y) and shifted by TopOffset: turrets, roofs (TopScale.Y = 0 makes a gable prism). */
	POCKETSAND_API void AddTapered(FPSMeshData& D, const FTransform& Xf, const FVector& Half, const FVector2D& TopScale, const FVector2D& TopOffset = FVector2D::ZeroVector, float UVPerM = 1.0f);

	/** Cylinder along local Z, centred. Smooth sides. */
	POCKETSAND_API void AddCylinder(FPSMeshData& D, const FTransform& Xf, float Radius, float HalfHeight, int32 Segments, bool bCaps = true, float TopRadius = -1.0f, float UVPerM = 1.0f);

	/** Surface of revolution around local Z from (radius, z) profile points, bottom to top. */
	POCKETSAND_API void AddLathe(FPSMeshData& D, const FTransform& Xf, const TArray<FVector2D>& Profile, int32 Segments, float UVPerM = 1.0f);

	/** Upright card (quad) with explicit UV rectangle, for foliage. Width/height in world units, base at Centre. */
	POCKETSAND_API void AddCard(FPSMeshData& D, const FVector& Base, float Width, float Height, float YawDeg, const FVector2D& UVMin, const FVector2D& UVMax);

	/** Writes the data into a mesh section. */
	POCKETSAND_API void Commit(UProceduralMeshComponent* Mesh, int32 Section, const FPSMeshData& D, UMaterialInterface* Material, bool bCollision);

	/** Minimal glTF 2.0 writer (embedded buffer) for asset-library exports. Meshes are named; positions go UE -> glTF axes. */
	POCKETSAND_API bool ExportGLTF(const FString& FilePath, const TArray<TPair<FString, const FPSMeshData*>>& Meshes);
}
