#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AlphaArtLibrary.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

enum class EAlphaPrimitive : uint8 { Cube, Sphere, Cylinder, Cone, Plane };

/** Hard references retain primitives during cooking. Generated color materials
 * are prepared by the editor startup hook and build scripts; missing assets are reported. */
UCLASS()
class ALPHAGAME_API UAlphaArtLibrary : public UObject
{
    GENERATED_BODY()
public:
    UAlphaArtLibrary();
    UStaticMesh* GetMesh(EAlphaPrimitive Shape) const;
    UMaterialInterface* GetSurface();
    UMaterialInterface* GetEffect();
    /** Logs real asset/parameter readiness; does not claim shader/render success. */
    void ReportStatus();
private:
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
    UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
    UPROPERTY() TObjectPtr<UStaticMesh> Cone;
    UPROPERTY() TObjectPtr<UStaticMesh> Plane;
    UPROPERTY() TObjectPtr<UMaterialInterface> FallbackSurface;
    UPROPERTY() TObjectPtr<UMaterialInterface> GeneratedSurface;
    UPROPERTY() TObjectPtr<UMaterialInterface> GeneratedEffect;
    double NextSurfaceRetryAt = 0.;
    double NextEffectRetryAt = 0.;
    bool bWarnedSurface = false;
    bool bWarnedEffect = false;
};

namespace AlphaArt
{
    ALPHAGAME_API UStaticMesh* Mesh(EAlphaPrimitive Shape);
    ALPHAGAME_API UMaterialInstanceDynamic* Material(UObject* Outer, FLinearColor Tint,
        float Metallic = 0.f, float Roughness = .6f, float Glow = 0.f, float AnimeShade = .35f,
        int32 SurfaceStyle = 0, float TextureStrength = 0.f);
    /** Unlit opaque energy effects when generated; unverified engine fallback otherwise. */
    ALPHAGAME_API UMaterialInstanceDynamic* EffectMaterial(UObject* Outer, FLinearColor Tint, float Glow = 1.f);
}
