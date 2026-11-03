#include "Visuals/AlphaArtLibrary.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

UAlphaArtLibrary::UAlphaArtLibrary()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeAsset(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SurfaceAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    Cube = CubeAsset.Object;
    Sphere = SphereAsset.Object;
    Cylinder = CylinderAsset.Object;
    Cone = ConeAsset.Object;
    Plane = PlaneAsset.Object;
    FallbackSurface = SurfaceAsset.Object;
}

UStaticMesh* UAlphaArtLibrary::GetMesh(EAlphaPrimitive Shape) const
{
    switch (Shape)
    {
    case EAlphaPrimitive::Sphere: return Sphere;
    case EAlphaPrimitive::Cylinder: return Cylinder;
    case EAlphaPrimitive::Cone: return Cone;
    case EAlphaPrimitive::Plane: return Plane;
    default: return Cube;
    }
}

UMaterialInterface* UAlphaArtLibrary::GetSurface()
{
    if (!bLookedUpSurface)
    {
        bLookedUpSurface = true;
        // Optional generated material: fallback keeps a clean checkout usable.
        GeneratedSurface = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Art/Materials/M_AlphaSurface.M_AlphaSurface"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    }
    return GeneratedSurface ? GeneratedSurface.Get() : FallbackSurface.Get();
}

UStaticMesh* AlphaArt::Mesh(EAlphaPrimitive Shape)
{
    return GetDefault<UAlphaArtLibrary>()->GetMesh(Shape);
}

UMaterialInstanceDynamic* AlphaArt::Material(UObject* Outer, FLinearColor Tint,
    float Metallic, float Roughness, float Glow)
{
    if (!Outer) return nullptr;
    UMaterialInterface* Surface = GetMutableDefault<UAlphaArtLibrary>()->GetSurface();
    if (!Surface) return nullptr;
    UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Surface, Outer);
    if (Instance)
    {
        Instance->SetVectorParameterValue(TEXT("Tint"), Tint);
        Instance->SetVectorParameterValue(TEXT("Color"), Tint); // Engine fallback parameter.
        Instance->SetScalarParameterValue(TEXT("Metallic"), FMath::Clamp(Metallic, 0.f, 1.f));
        Instance->SetScalarParameterValue(TEXT("Roughness"), FMath::Clamp(Roughness, .05f, 1.f));
        Instance->SetScalarParameterValue(TEXT("Glow"), FMath::Clamp(Glow, 0.f, 8.f));
    }
    return Instance;
}
