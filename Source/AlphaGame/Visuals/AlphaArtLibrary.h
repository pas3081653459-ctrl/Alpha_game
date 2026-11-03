#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AlphaArtLibrary.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

enum class EAlphaPrimitive : uint8 { Cube, Sphere, Cylinder, Cone, Plane };

/** Hard references retain built-in primitives during cooking. Optional generated
 * master is created by Scripts/create_art_assets.py inside the Unreal editor. */
UCLASS()
class ALPHAGAME_API UAlphaArtLibrary : public UObject
{
    GENERATED_BODY()
public:
    UAlphaArtLibrary();
    UStaticMesh* GetMesh(EAlphaPrimitive Shape) const;
    UMaterialInterface* GetSurface();
    UMaterialInterface* GetEffect();
private:
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
    UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
    UPROPERTY() TObjectPtr<UStaticMesh> Cone;
    UPROPERTY() TObjectPtr<UStaticMesh> Plane;
    UPROPERTY() TObjectPtr<UMaterialInterface> FallbackSurface;
    UPROPERTY() TObjectPtr<UMaterialInterface> GeneratedSurface;
    UPROPERTY() TObjectPtr<UMaterialInterface> GeneratedEffect;
    bool bLookedUpSurface = false;
    bool bLookedUpEffect = false;
};

namespace AlphaArt
{
    ALPHAGAME_API UStaticMesh* Mesh(EAlphaPrimitive Shape);
    ALPHAGAME_API UMaterialInstanceDynamic* Material(UObject* Outer, FLinearColor Tint,
        float Metallic = 0.f, float Roughness = .6f, float Glow = 0.f, float AnimeShade = .35f);
    /** Unlit opaque energy effects when generated; visible lit-color fallback otherwise. */
    ALPHAGAME_API UMaterialInstanceDynamic* EffectMaterial(UObject* Outer, FLinearColor Tint, float Glow = 1.f);
}
