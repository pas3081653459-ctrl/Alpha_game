#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AlphaChapterEnvironment.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UTextRenderComponent;
enum class EAlphaPrimitive : uint8;

/** Local-only chapter set dressing. Authority-owned floor/walls live in GameState. */
UCLASS(NotBlueprintable)
class ALPHAGAME_API AAlphaChapterEnvironment : public AActor
{
    GENERATED_BODY()
public:
    AAlphaChapterEnvironment();
    /** Call after spawning at the chapter origin. Supports chapter 0 and 2. */
    void BuildForChapter(int32 Chapter);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chapter")
    int32 ChapterIndex = -1;

private:
    void ClearSet();
    void BuildDock();
    void BuildCanal();
    void BuildRouteMarkers(UInstancedStaticMeshComponent* Trim, UInstancedStaticMeshComponent* Light);
    void AddLamp(const FVector& Location, const FLinearColor& Color);
    UInstancedStaticMeshComponent* MakeBatch(EAlphaPrimitive Shape, const FLinearColor& Tint,
        float Glow = 0.f, bool bShadow = true, int32 SurfaceStyle = 0, float TextureStrength = 0.f);
    static void Add(UInstancedStaticMeshComponent* Batch, const FVector& Position, const FVector& Dimensions,
        const FRotator& Rotation = FRotator::ZeroRotator);
    static void Beam(UInstancedStaticMeshComponent* Batch, const FVector& Start, const FVector& End, float Width);

    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Batches;
    UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lamps;
    UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> ChapterLabel;
};
