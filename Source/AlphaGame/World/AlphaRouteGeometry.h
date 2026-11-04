#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AlphaRouteGeometry.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;

/** Deterministic collision and visible surfaces built from the same route union. */
UCLASS(NotBlueprintable)
class ALPHAGAME_API AAlphaRouteGeometry : public AActor
{
    GENERATED_BODY()
public:
    AAlphaRouteGeometry();
    virtual void BeginPlay() override;
    UPROPERTY(EditAnywhere, Category="Route") int32 ChapterIndex = 0;
    int32 GetFloorPieceCount() const { return FloorPieceCount; }
    int32 GetBoundaryPieceCount() const { return BoundaryPieceCount; }
private:
    void BuildRoute();
    void AddCollisionBox(const FVector& Center, const FVector& Extent, const FRotator& Rotation = FRotator::ZeroRotator);
    UInstancedStaticMeshComponent* MakeBatch(FLinearColor Tint, int32 Style, float Strength);
    UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> CollisionPieces;
    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> SurfaceBatches;
    int32 FloorPieceCount = 0;
    int32 BoundaryPieceCount = 0;
};
