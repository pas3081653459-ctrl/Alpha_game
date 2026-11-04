#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AlphaScenicBackdrop.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/** One local, unlit painted vista beyond the playable geometry. No gameplay collision. */
UCLASS(NotBlueprintable)
class ALPHAGAME_API AAlphaScenicBackdrop : public AActor
{
    GENERATED_BODY()
public:
    AAlphaScenicBackdrop();
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void BeginPlay() override;
private:
    void RefreshChapter();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Vista;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> ChapterMaterials;
    int32 LastChapter = INDEX_NONE;
    float PanoramaAspect = 3.f;
    float LastWidth = 0.f;
};
