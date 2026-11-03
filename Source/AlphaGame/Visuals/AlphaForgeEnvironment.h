#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AlphaForgeEnvironment.generated.h"

class UInstancedStaticMeshComponent;
enum class EAlphaPrimitive : uint8;

/** Local-only fixed-seed workshop set dressing; gameplay collision lives in GameState. */
UCLASS(NotBlueprintable)
class ALPHAGAME_API AAlphaForgeEnvironment : public AActor
{
    GENERATED_BODY()
public:
    AAlphaForgeEnvironment();
    virtual void BeginPlay() override;

private:
    UInstancedStaticMeshComponent* MakeBatch(EAlphaPrimitive Shape, const FLinearColor& Tint,
        float Metallic = 0.f, float Roughness = .6f, float Glow = 0.f);
    static void Add(UInstancedStaticMeshComponent* Batch, const FVector& Position,
        const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator);
    static void Bar(UInstancedStaticMeshComponent* Batch, const FVector& Start,
        const FVector& End, float Width, float Height);
    static void Pipe(UInstancedStaticMeshComponent* Batch, const FVector& Start,
        const FVector& End, float Radius);
    static void Ring(UInstancedStaticMeshComponent* Batch, const FVector& Center,
        float Radius, int32 Segments, float Width, float Depth);
    void BuildWorkshop();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UInstancedStaticMeshComponent>> Batches;
};
