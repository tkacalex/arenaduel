#include "ArenaDuelKnifeMesh.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"

namespace
{
	struct FKnifeBuilder
	{
		FMeshDescription Description;
		FStaticMeshAttributes Attributes;
		FKnifeBuilder() : Attributes(Description) { Attributes.Register(); }

		FPolygonGroupID AddGroup(FName Slot)
		{
			const FPolygonGroupID Group = Description.CreatePolygonGroup();
			Attributes.GetPolygonGroupMaterialSlotNames()[Group] = Slot;
			return Group;
		}

		/** Flat shaded triangle, turned so that it faces away from Inside. */
		void AddTriangle(FPolygonGroupID Group, const FVector& A, FVector B, FVector C, const FVector& Inside)
		{
			// Engine convention: the front face normal is (C - A) x (B - A).
			FVector Normal = FVector::CrossProduct(C - A, B - A);
			if (Normal.SizeSquared() < 1.e-8) return;
			Normal.Normalize();
			if (FVector::DotProduct(Normal, (A + B + C) / 3.0 - Inside) < 0.0) { Swap(B, C); Normal = -Normal; }
			const FVector Tangent = (B - A).GetSafeNormal();
			TArray<FVertexInstanceID> Corners;
			for (const FVector& Point : { A, B, C })
			{
				const FVertexID Vertex = Description.CreateVertex();
				Attributes.GetVertexPositions()[Vertex] = FVector3f(Point);
				const FVertexInstanceID Corner = Description.CreateVertexInstance(Vertex);
				Attributes.GetVertexInstanceNormals()[Corner] = FVector3f(Normal);
				Attributes.GetVertexInstanceTangents()[Corner] = FVector3f(Tangent);
				Attributes.GetVertexInstanceBinormalSigns()[Corner] = 1.0f;
				Attributes.GetVertexInstanceColors()[Corner] = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
				Attributes.GetVertexInstanceUVs().Set(Corner, 0, FVector2f(static_cast<float>(Point.X) * 0.05f, static_cast<float>(Point.Z) * 0.05f));
				Corners.Add(Corner);
			}
			Description.CreatePolygon(Group, Corners);
		}

		void AddQuad(FPolygonGroupID Group, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Inside)
		{
			AddTriangle(Group, A, B, C, Inside);
			AddTriangle(Group, A, C, D, Inside);
		}

		/** Prism along X between two cross sections given as Y/Z rings of equal size. */
		void AddPrism(FPolygonGroupID Group, float X0, float X1, const TArray<FVector2D>& Ring0, const TArray<FVector2D>& Ring1)
		{
			const int32 Count = Ring0.Num();
			const FVector Inside((X0 + X1) * 0.5, 0.0, 0.0);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const int32 Next = (Index + 1) % Count;
				AddQuad(Group, FVector(X0, Ring0[Index].X, Ring0[Index].Y), FVector(X1, Ring1[Index].X, Ring1[Index].Y), FVector(X1, Ring1[Next].X, Ring1[Next].Y), FVector(X0, Ring0[Next].X, Ring0[Next].Y), Inside);
				if (Index > 0 && Next != 0)
				{
					AddTriangle(Group, FVector(X0, Ring0[0].X, Ring0[0].Y), FVector(X0, Ring0[Index].X, Ring0[Index].Y), FVector(X0, Ring0[Next].X, Ring0[Next].Y), Inside);
					AddTriangle(Group, FVector(X1, Ring1[0].X, Ring1[0].Y), FVector(X1, Ring1[Index].X, Ring1[Index].Y), FVector(X1, Ring1[Next].X, Ring1[Next].Y), Inside);
				}
			}
		}

		static TArray<FVector2D> Oval(float HalfWidth, float HalfHeight, float CenterZ = 0.0f)
		{
			TArray<FVector2D> Ring;
			for (int32 Step = 0; Step < 8; ++Step)
			{
				const float Angle = UE_TWO_PI * (static_cast<float>(Step) + 0.5f) / 8.0f;
				Ring.Add(FVector2D(FMath::Cos(Angle) * HalfWidth, CenterZ + FMath::Sin(Angle) * HalfHeight));
			}
			return Ring;
		}
	};
}

UStaticMesh* ArenaDuelKnifeMesh::Build(UObject* Outer)
{
	// Centimetres, +X towards the tip, +Z towards the spine. The origin is the middle of the grip.
	FKnifeBuilder Builder;
	const FPolygonGroupID Blade = Builder.AddGroup(TEXT("Blade"));
	const FPolygonGroupID Grip = Builder.AddGroup(TEXT("Grip"));
	const FPolygonGroupID Guard = Builder.AddGroup(TEXT("Guard"));

	// Grip: three oval segments with a swell in the middle, and a pommel.
	Builder.AddPrism(Grip, -5.5f, -2.0f, FKnifeBuilder::Oval(1.05f, 1.45f), FKnifeBuilder::Oval(1.25f, 1.7f, -0.1f));
	Builder.AddPrism(Grip, -2.0f, 2.0f, FKnifeBuilder::Oval(1.25f, 1.7f, -0.1f), FKnifeBuilder::Oval(1.2f, 1.6f));
	Builder.AddPrism(Grip, 2.0f, 5.0f, FKnifeBuilder::Oval(1.2f, 1.6f), FKnifeBuilder::Oval(1.0f, 1.35f));
	Builder.AddPrism(Guard, -6.6f, -5.5f, FKnifeBuilder::Oval(1.2f, 1.75f), FKnifeBuilder::Oval(1.2f, 1.75f));

	// Cross guard, longer towards the edge side to protect the fingers.
	const TArray<FVector2D> GuardRing = { FVector2D(-0.9f, -3.1f), FVector2D(0.9f, -3.1f), FVector2D(0.9f, 2.3f), FVector2D(-0.9f, 2.3f) };
	Builder.AddPrism(Guard, 5.0f, 5.9f, GuardRing, GuardRing);

	// Blade: a wedge. The profile is a fan around its middle; the spine keeps its thickness, the edge and tip have none.
	struct FProfilePoint { float X; float Z; float HalfThickness; };
	const FProfilePoint Profile[] = {
		{ 5.9f, 1.55f, 0.22f }, { 15.5f, 1.55f, 0.20f }, { 19.5f, 0.95f, 0.12f }, { 23.0f, -0.35f, 0.0f },
		{ 20.5f, -1.25f, 0.0f }, { 17.0f, -1.7f, 0.0f }, { 8.0f, -1.75f, 0.0f }, { 5.9f, -1.3f, 0.10f } };
	const int32 PointCount = UE_ARRAY_COUNT(Profile);
	const FVector BladeInside(14.0, 0.0, 0.0);
	for (int32 Index = 0; Index < PointCount; ++Index)
	{
		const FProfilePoint& P = Profile[Index];
		const FProfilePoint& Q = Profile[(Index + 1) % PointCount];
		for (const float Side : { 1.0f, -1.0f })
		{
			// Each flank rises to a ridge along the middle of the blade.
			Builder.AddTriangle(Blade, FVector(14.0, Side * 0.26, 0.0), FVector(P.X, Side * P.HalfThickness, P.Z), FVector(Q.X, Side * Q.HalfThickness, Q.Z), BladeInside);
		}
		// Back of the spine and the flat at the guard, where the blade has thickness.
		Builder.AddQuad(Blade, FVector(P.X, P.HalfThickness, P.Z), FVector(Q.X, Q.HalfThickness, Q.Z), FVector(Q.X, -Q.HalfThickness, Q.Z), FVector(P.X, -P.HalfThickness, P.Z), BladeInside);
	}

	UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer ? Outer : GetTransientPackage(), NAME_None, RF_Transient);
	for (const FName Slot : { FName(TEXT("Blade")), FName(TEXT("Grip")), FName(TEXT("Guard")) }) Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, Slot, Slot));
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bBuildSimpleCollision = false;
	Params.bFastBuild = true;
	Params.bAllowCpuAccess = false;
	Mesh->BuildFromMeshDescriptions({ &Builder.Description }, Params);
	return Mesh;
}
