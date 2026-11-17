// Pocket Sand - procedural geometry.

#include "World/PSMeshLib.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

void FPSMeshData::Append(const FPSMeshData& Other)
{
	const int32 Base = Verts.Num();
	Verts.Append(Other.Verts);
	Normals.Append(Other.Normals);
	UVs.Append(Other.UVs);
	Tangents.Append(Other.Tangents);
	Tris.Reserve(Tris.Num() + Other.Tris.Num());
	for (int32 I : Other.Tris)
	{
		Tris.Add(Base + I);
	}
}

namespace
{
	/** Planar projection axes for a face normal: U and V directions in world space. */
	void PlanarAxes(const FVector& N, FVector& OutU, FVector& OutV)
	{
		const FVector A = N.GetAbs();
		if (A.Z >= A.X && A.Z >= A.Y)
		{
			OutU = FVector(1, 0, 0);
			OutV = FVector(0, N.Z >= 0.0f ? -1.0f : 1.0f, 0);
		}
		else if (A.X >= A.Y)
		{
			OutU = FVector(0, N.X >= 0.0f ? 1.0f : -1.0f, 0);
			OutV = FVector(0, 0, -1);
		}
		else
		{
			OutU = FVector(N.Y >= 0.0f ? -1.0f : 1.0f, 0, 0);
			OutV = FVector(0, 0, -1);
		}
	}

	int32 PushVert(FPSMeshData& D, const FVector& P, const FVector& N, const FVector& U, const FVector& V, float UVPerM)
	{
		const int32 Index = D.Verts.Add(P);
		D.Normals.Add(N);
		D.UVs.Add(FVector2D(FVector::DotProduct(P, U), FVector::DotProduct(P, V)) * (0.01f * UVPerM));
		D.Tangents.Add(FProcMeshTangent(U, false));
		return Index;
	}

	/** UE front faces have Cross(B-A, C-A) pointing away from the normal (see GenerateBoxMesh). */
	void PushTri(FPSMeshData& D, int32 IA, int32 IB, int32 IC, const FVector& N)
	{
		const FVector& A = D.Verts[IA];
		const FVector& B = D.Verts[IB];
		const FVector& C = D.Verts[IC];
		if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), N) > 0.0f)
		{
			D.Tris.Append({ IA, IC, IB });
		}
		else
		{
			D.Tris.Append({ IA, IB, IC });
		}
	}
}

void PSMesh::AddTri(FPSMeshData& D, const FVector& A, const FVector& B, const FVector& C, const FVector& Normal, float UVPerM)
{
	FVector N = Normal.GetSafeNormal();
	if (N.IsNearlyZero())
	{
		N = FVector::CrossProduct(C - A, B - A).GetSafeNormal();
	}
	FVector U, V;
	PlanarAxes(N, U, V);
	const int32 IA = PushVert(D, A, N, U, V, UVPerM);
	const int32 IB = PushVert(D, B, N, U, V, UVPerM);
	const int32 IC = PushVert(D, C, N, U, V, UVPerM);
	PushTri(D, IA, IB, IC, N);
}

void PSMesh::AddQuad(FPSMeshData& D, const FVector& A, const FVector& B, const FVector& C, const FVector& E, const FVector& Normal, float UVPerM)
{
	FVector N = Normal.GetSafeNormal();
	FVector U, V;
	PlanarAxes(N, U, V);
	const int32 IA = PushVert(D, A, N, U, V, UVPerM);
	const int32 IB = PushVert(D, B, N, U, V, UVPerM);
	const int32 IC = PushVert(D, C, N, U, V, UVPerM);
	const int32 IE = PushVert(D, E, N, U, V, UVPerM);
	PushTri(D, IA, IB, IC, N);
	PushTri(D, IA, IC, IE, N);
}

void PSMesh::AddBox(FPSMeshData& D, const FTransform& Xf, const FVector& Half, float Bevel, float UVPerM)
{
	const float B = FMath::Clamp(Bevel, 0.0f, Half.GetMin() * 0.45f);
	const FVector H = Half;
	auto P = [&](float X, float Y, float Z) { return Xf.TransformPosition(FVector(X, Y, Z)); };
	auto NW = [&](const FVector& LocalN) { return Xf.TransformVectorNoScale(LocalN).GetSafeNormal(); };

	// Six inset faces.
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		for (int32 S = -1; S <= 1; S += 2)
		{
			const int32 A1 = (Axis + 1) % 3;
			const int32 A2 = (Axis + 2) % 3;
			FVector Corner[4];
			const float Sx[4] = { -1, 1, 1, -1 };
			const float Sy[4] = { -1, -1, 1, 1 };
			for (int32 k = 0; k < 4; ++k)
			{
				FVector L;
				L[Axis] = S * H[Axis];
				L[A1] = Sx[k] * (H[A1] - B);
				L[A2] = Sy[k] * (H[A2] - B);
				Corner[k] = P(L.X, L.Y, L.Z);
			}
			FVector LN = FVector::ZeroVector;
			LN[Axis] = S;
			AddQuad(D, Corner[0], Corner[1], Corner[2], Corner[3], NW(LN), UVPerM);
		}
	}
	if (B <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	// Twelve edge chamfers.
	for (int32 C = 0; C < 3; ++C)
	{
		const int32 A = (C + 1) % 3;
		const int32 Bx = (C + 2) % 3;
		for (int32 Sa = -1; Sa <= 1; Sa += 2)
		{
			for (int32 Sb = -1; Sb <= 1; Sb += 2)
			{
				FVector Q[4];
				for (int32 k = 0; k < 4; ++k)
				{
					const bool bOnA = k < 2;
					const float Sc = (k == 0 || k == 3) ? -1.0f : 1.0f;
					FVector L;
					L[C] = Sc * (H[C] - B);
					L[A] = Sa * (bOnA ? H[A] : H[A] - B);
					L[Bx] = Sb * (bOnA ? H[Bx] - B : H[Bx]);
					Q[k] = P(L.X, L.Y, L.Z);
				}
				FVector LN = FVector::ZeroVector;
				LN[A] = Sa;
				LN[Bx] = Sb;
				AddQuad(D, Q[0], Q[1], Q[2], Q[3], NW(LN.GetSafeNormal()), UVPerM);
			}
		}
	}
	// Eight corner triangles.
	for (int32 Sx = -1; Sx <= 1; Sx += 2)
	{
		for (int32 Sy = -1; Sy <= 1; Sy += 2)
		{
			for (int32 Sz = -1; Sz <= 1; Sz += 2)
			{
				const FVector V0 = P(Sx * H.X, Sy * (H.Y - B), Sz * (H.Z - B));
				const FVector V1 = P(Sx * (H.X - B), Sy * H.Y, Sz * (H.Z - B));
				const FVector V2 = P(Sx * (H.X - B), Sy * (H.Y - B), Sz * H.Z);
				AddTri(D, V0, V1, V2, NW(FVector(Sx, Sy, Sz).GetSafeNormal()), UVPerM);
			}
		}
	}
}

void PSMesh::AddBlock(FPSMeshData& D, const FTransform& Xf, const FVector Corners[8], float UVPerM)
{
	FVector W[8];
	FVector Centre = FVector::ZeroVector;
	for (int32 i = 0; i < 8; ++i)
	{
		W[i] = Xf.TransformPosition(Corners[i]);
		Centre += W[i] / 8.0f;
	}
	const int32 Faces[6][4] = {
		{ 0, 1, 2, 3 }, // bottom
		{ 4, 5, 6, 7 }, // top
		{ 0, 1, 5, 4 }, // -y
		{ 1, 2, 6, 5 }, // +x
		{ 2, 3, 7, 6 }, // +y
		{ 3, 0, 4, 7 }, // -x
	};
	for (const auto& F : Faces)
	{
		const FVector& A = W[F[0]];
		const FVector& B = W[F[1]];
		const FVector& C = W[F[2]];
		const FVector& E = W[F[3]];
		FVector N = FVector::CrossProduct(C - A, E - B);
		if (N.SizeSquared() < 1e-4f)
		{
			N = FVector::CrossProduct(B - A, E - A);
		}
		N.Normalize();
		const FVector FaceCentre = (A + B + C + E) * 0.25f;
		if (FVector::DotProduct(N, FaceCentre - Centre) < 0.0f)
		{
			N = -N;
		}
		if (FVector::DistSquared(A, B) < 1.0f || FVector::DistSquared(C, E) < 1.0f || FVector::DistSquared(B, C) < 1.0f || FVector::DistSquared(E, A) < 1.0f)
		{
			// Degenerate quad (collapsed edge): emit as a triangle.
			TArray<FVector> Unique;
			for (const FVector& Pt : { A, B, C, E })
			{
				bool bDup = false;
				for (const FVector& U : Unique) { bDup |= FVector::DistSquared(U, Pt) < 1.0f; }
				if (!bDup) { Unique.Add(Pt); }
			}
			if (Unique.Num() == 3)
			{
				AddTri(D, Unique[0], Unique[1], Unique[2], N, UVPerM);
			}
			continue;
		}
		AddQuad(D, A, B, C, E, N, UVPerM);
	}
}

void PSMesh::AddTapered(FPSMeshData& D, const FTransform& Xf, const FVector& Half, const FVector2D& TopScale, const FVector2D& TopOffset, float UVPerM)
{
	const FVector C[8] = {
		FVector(-Half.X, -Half.Y, -Half.Z), FVector(Half.X, -Half.Y, -Half.Z), FVector(Half.X, Half.Y, -Half.Z), FVector(-Half.X, Half.Y, -Half.Z),
		FVector(-Half.X * TopScale.X + TopOffset.X, -Half.Y * TopScale.Y + TopOffset.Y, Half.Z),
		FVector(Half.X * TopScale.X + TopOffset.X, -Half.Y * TopScale.Y + TopOffset.Y, Half.Z),
		FVector(Half.X * TopScale.X + TopOffset.X, Half.Y * TopScale.Y + TopOffset.Y, Half.Z),
		FVector(-Half.X * TopScale.X + TopOffset.X, Half.Y * TopScale.Y + TopOffset.Y, Half.Z),
	};
	AddBlock(D, Xf, C, UVPerM);
}

void PSMesh::AddCylinder(FPSMeshData& D, const FTransform& Xf, float Radius, float HalfHeight, int32 Segments, bool bCaps, float TopRadius, float UVPerM)
{
	const float R1 = TopRadius < 0.0f ? Radius : TopRadius;
	const int32 Segs = FMath::Max(3, Segments);
	const float Circumference = UE_TWO_PI * FMath::Max(Radius, R1);
	const int32 Base = D.Verts.Num();
	for (int32 i = 0; i <= Segs; ++i)
	{
		const float A = UE_TWO_PI * i / Segs;
		const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.0f);
		const FVector Slope = FVector(Dir.X, Dir.Y, (Radius - R1) / FMath::Max(2.0f * HalfHeight, 1.0f)).GetSafeNormal();
		const FVector N = Xf.TransformVectorNoScale(Slope).GetSafeNormal();
		const FVector T = Xf.TransformVectorNoScale(FVector(-Dir.Y, Dir.X, 0.0f)).GetSafeNormal();
		for (int32 j = 0; j < 2; ++j)
		{
			const float R = j == 0 ? Radius : R1;
			const FVector P = Xf.TransformPosition(FVector(Dir.X * R, Dir.Y * R, j == 0 ? -HalfHeight : HalfHeight));
			D.Verts.Add(P);
			D.Normals.Add(N);
			D.UVs.Add(FVector2D(Circumference * i / Segs, j == 0 ? 0.0f : -2.0f * HalfHeight) * (0.01f * UVPerM));
			D.Tangents.Add(FProcMeshTangent(T, false));
		}
	}
	for (int32 i = 0; i < Segs; ++i)
	{
		const int32 A0 = Base + i * 2, A1 = A0 + 1, B0 = A0 + 2, B1 = A0 + 3;
		const FVector N = (D.Normals[A0] + D.Normals[B0]).GetSafeNormal();
		PushTri(D, A0, B0, B1, N);
		PushTri(D, A0, B1, A1, N);
	}
	if (!bCaps)
	{
		return;
	}
	for (int32 j = 0; j < 2; ++j)
	{
		const float R = j == 0 ? Radius : R1;
		if (R <= 0.5f)
		{
			continue;
		}
		const float Z = j == 0 ? -HalfHeight : HalfHeight;
		const FVector N = Xf.TransformVectorNoScale(FVector(0, 0, j == 0 ? -1.0f : 1.0f)).GetSafeNormal();
		const FVector Centre = Xf.TransformPosition(FVector(0, 0, Z));
		for (int32 i = 0; i < Segs; ++i)
		{
			const float A = UE_TWO_PI * i / Segs;
			const float B = UE_TWO_PI * (i + 1) / Segs;
			AddTri(D, Centre, Xf.TransformPosition(FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z)), Xf.TransformPosition(FVector(FMath::Cos(B) * R, FMath::Sin(B) * R, Z)), N, UVPerM);
		}
	}
}

void PSMesh::AddLathe(FPSMeshData& D, const FTransform& Xf, const TArray<FVector2D>& Profile, int32 Segments, float UVPerM)
{
	const int32 Segs = FMath::Max(3, Segments);
	const int32 NP = Profile.Num();
	if (NP < 2)
	{
		return;
	}
	// Profile normals (2D, outward).
	TArray<FVector2D> PN;
	PN.SetNum(NP);
	for (int32 k = 0; k < NP; ++k)
	{
		const FVector2D Prev = Profile[FMath::Max(k - 1, 0)];
		const FVector2D Next = Profile[FMath::Min(k + 1, NP - 1)];
		const FVector2D T = (Next - Prev).GetSafeNormal();
		PN[k] = FVector2D(T.Y, -T.X);
		if (PN[k].X < 0.0f && Profile[k].X > 1.0f)
		{
			PN[k] = -PN[k];
		}
	}
	float Arc = 0.0f;
	TArray<float> ArcAt;
	ArcAt.SetNum(NP);
	for (int32 k = 0; k < NP; ++k)
	{
		if (k > 0) { Arc += FVector2D::Distance(Profile[k], Profile[k - 1]); }
		ArcAt[k] = Arc;
	}
	float MaxR = 1.0f;
	for (const FVector2D& Pt : Profile) { MaxR = FMath::Max(MaxR, Pt.X); }
	const int32 Base = D.Verts.Num();
	for (int32 i = 0; i <= Segs; ++i)
	{
		const float A = UE_TWO_PI * i / Segs;
		const float C = FMath::Cos(A), S = FMath::Sin(A);
		const FVector T = Xf.TransformVectorNoScale(FVector(-S, C, 0.0f)).GetSafeNormal();
		for (int32 k = 0; k < NP; ++k)
		{
			D.Verts.Add(Xf.TransformPosition(FVector(C * Profile[k].X, S * Profile[k].X, Profile[k].Y)));
			D.Normals.Add(Xf.TransformVectorNoScale(FVector(C * PN[k].X, S * PN[k].X, PN[k].Y)).GetSafeNormal());
			D.UVs.Add(FVector2D(UE_TWO_PI * MaxR * i / Segs, -ArcAt[k]) * (0.01f * UVPerM));
			D.Tangents.Add(FProcMeshTangent(T, false));
		}
	}
	for (int32 i = 0; i < Segs; ++i)
	{
		for (int32 k = 0; k + 1 < NP; ++k)
		{
			const int32 A0 = Base + i * NP + k;
			const int32 A1 = A0 + 1;
			const int32 B0 = A0 + NP;
			const int32 B1 = B0 + 1;
			const FVector N = (D.Normals[A0] + D.Normals[B1]).GetSafeNormal();
			PushTri(D, A0, B0, B1, N);
			PushTri(D, A0, B1, A1, N);
		}
	}
}

void PSMesh::AddCard(FPSMeshData& D, const FVector& Base, float Width, float Height, float YawDeg, const FVector2D& UVMin, const FVector2D& UVMax)
{
	const FVector Right = FRotator(0.0f, YawDeg, 0.0f).RotateVector(FVector(0, 1, 0)) * (Width * 0.5f);
	const FVector N = FRotator(0.0f, YawDeg, 0.0f).RotateVector(FVector(1, 0, 0));
	const FVector Up(0, 0, Height);
	const int32 I0 = D.Verts.Add(Base - Right);
	const int32 I1 = D.Verts.Add(Base + Right);
	const int32 I2 = D.Verts.Add(Base + Right + Up);
	const int32 I3 = D.Verts.Add(Base - Right + Up);
	// Foliage normals lean up so cards light like a canopy instead of flat boards.
	const FVector LitN = (N * 0.35f + FVector(0, 0, 1.0f)).GetSafeNormal();
	for (int32 k = 0; k < 4; ++k)
	{
		D.Normals.Add(LitN);
		D.Tangents.Add(FProcMeshTangent(Right.GetSafeNormal(), false));
	}
	D.UVs.Add(FVector2D(UVMin.X, UVMax.Y));
	D.UVs.Add(FVector2D(UVMax.X, UVMax.Y));
	D.UVs.Add(FVector2D(UVMax.X, UVMin.Y));
	D.UVs.Add(FVector2D(UVMin.X, UVMin.Y));
	PushTri(D, I0, I1, I2, N);
	PushTri(D, I0, I2, I3, N);
}

void PSMesh::Commit(UProceduralMeshComponent* Mesh, int32 Section, const FPSMeshData& D, UMaterialInterface* Material, bool bCollision)
{
	if (!Mesh || D.IsEmpty())
	{
		return;
	}
	Mesh->CreateMeshSection(Section, D.Verts, D.Tris, D.Normals, D.UVs, TArray<FColor>(), D.Tangents, bCollision);
	if (Material)
	{
		Mesh->SetMaterial(Section, Material);
	}
}

bool PSMesh::ExportGLTF(const FString& FilePath, const TArray<TPair<FString, const FPSMeshData*>>& Meshes)
{
	TArray<uint8> Buffer;
	FString MeshJson, AccessorJson, ViewJson, NodeJson, SceneNodes;
	int32 AccessorIndex = 0;
	int32 ViewIndex = 0;
	int32 MeshIndex = 0;
	auto Align4 = [&Buffer]() { while (Buffer.Num() % 4) { Buffer.Add(0); } };
	auto AddView = [&](int32 Offset, int32 Length, int32 Target)
	{
		ViewJson += FString::Printf(TEXT("%s{\"buffer\":0,\"byteOffset\":%d,\"byteLength\":%d,\"target\":%d}"), ViewIndex ? TEXT(",") : TEXT(""), Offset, Length, Target);
		return ViewIndex++;
	};
	auto AddAccessor = [&](int32 View, int32 ComponentType, int32 Count, const TCHAR* Type, const FString& Extra)
	{
		AccessorJson += FString::Printf(TEXT("%s{\"bufferView\":%d,\"componentType\":%d,\"count\":%d,\"type\":\"%s\"%s}"), AccessorIndex ? TEXT(",") : TEXT(""), View, ComponentType, Count, Type, *Extra);
		return AccessorIndex++;
	};
	for (const TPair<FString, const FPSMeshData*>& Pair : Meshes)
	{
		const FPSMeshData* D = Pair.Value;
		if (!D || D->IsEmpty())
		{
			continue;
		}
		FVector Min(BIG_NUMBER), Max(-BIG_NUMBER);
		// Positions (glTF: x, z, y in metres; mirror flips handedness, so swap triangle winding).
		Align4();
		const int32 PosOffset = Buffer.Num();
		for (const FVector& P : D->Verts)
		{
			const FVector3f G(P.X * 0.01f, P.Z * 0.01f, P.Y * 0.01f);
			Min = FVector(FMath::Min(Min.X, G.X), FMath::Min(Min.Y, G.Y), FMath::Min(Min.Z, G.Z));
			Max = FVector(FMath::Max(Max.X, G.X), FMath::Max(Max.Y, G.Y), FMath::Max(Max.Z, G.Z));
			Buffer.Append(reinterpret_cast<const uint8*>(&G), sizeof(G));
		}
		const int32 PosView = AddView(PosOffset, Buffer.Num() - PosOffset, 34962);
		const int32 PosAcc = AddAccessor(PosView, 5126, D->Verts.Num(), TEXT("VEC3"), FString::Printf(TEXT(",\"min\":[%f,%f,%f],\"max\":[%f,%f,%f]"), Min.X, Min.Y, Min.Z, Max.X, Max.Y, Max.Z));
		const int32 NrmOffset = Buffer.Num();
		for (const FVector& N : D->Normals)
		{
			const FVector3f G(N.X, N.Z, N.Y);
			Buffer.Append(reinterpret_cast<const uint8*>(&G), sizeof(G));
		}
		const int32 NrmAcc = AddAccessor(AddView(NrmOffset, Buffer.Num() - NrmOffset, 34962), 5126, D->Normals.Num(), TEXT("VEC3"), FString());
		const int32 UVOffset = Buffer.Num();
		for (const FVector2D& UV : D->UVs)
		{
			const FVector2f G(UV.X, UV.Y);
			Buffer.Append(reinterpret_cast<const uint8*>(&G), sizeof(G));
		}
		const int32 UVAcc = AddAccessor(AddView(UVOffset, Buffer.Num() - UVOffset, 34962), 5126, D->UVs.Num(), TEXT("VEC2"), FString());
		const int32 IdxOffset = Buffer.Num();
		for (int32 t = 0; t + 2 < D->Tris.Num(); t += 3)
		{
			const uint32 Tri[3] = { static_cast<uint32>(D->Tris[t]), static_cast<uint32>(D->Tris[t + 1]), static_cast<uint32>(D->Tris[t + 2]) };
			Buffer.Append(reinterpret_cast<const uint8*>(Tri), sizeof(Tri));
		}
		const int32 IdxAcc = AddAccessor(AddView(IdxOffset, Buffer.Num() - IdxOffset, 34963), 5125, D->Tris.Num(), TEXT("SCALAR"), FString());
		MeshJson += FString::Printf(TEXT("%s{\"name\":\"%s\",\"primitives\":[{\"attributes\":{\"POSITION\":%d,\"NORMAL\":%d,\"TEXCOORD_0\":%d},\"indices\":%d}]}"),
			MeshIndex ? TEXT(",") : TEXT(""), *Pair.Key, PosAcc, NrmAcc, UVAcc, IdxAcc);
		NodeJson += FString::Printf(TEXT("%s{\"name\":\"%s\",\"mesh\":%d}"), MeshIndex ? TEXT(",") : TEXT(""), *Pair.Key, MeshIndex);
		SceneNodes += FString::Printf(TEXT("%s%d"), MeshIndex ? TEXT(",") : TEXT(""), MeshIndex);
		++MeshIndex;
	}
	if (MeshIndex == 0)
	{
		return false;
	}
	const FString Json = FString::Printf(
		TEXT("{\"asset\":{\"version\":\"2.0\",\"generator\":\"PocketSand PSMeshLib (original, generated)\"},\"scene\":0,\"scenes\":[{\"nodes\":[%s]}],")
		TEXT("\"nodes\":[%s],\"meshes\":[%s],\"accessors\":[%s],\"bufferViews\":[%s],")
		TEXT("\"buffers\":[{\"byteLength\":%d,\"uri\":\"data:application/octet-stream;base64,%s\"}]}"),
		*SceneNodes, *NodeJson, *MeshJson, *AccessorJson, *ViewJson, Buffer.Num(), *FBase64::Encode(Buffer));
	return FFileHelper::SaveStringToFile(Json, *FilePath);
}
