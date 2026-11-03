#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AlphaBreakableProp.generated.h"

class UStaticMeshComponent;

/** Small replicated scenery target. Never blocks players or changes combat damage. */
UCLASS()
class ALPHAGAME_API AAlphaBreakableProp : public AActor
{
    GENERATED_BODY()
public:
    AAlphaBreakableProp();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    static void AffectAttack(UWorld* World, AActor* DamageSource, const FVector& Start,
        const FVector& End, float Radius);
    void ResetForRound();
    bool IsBroken() const { return bBroken; }

private:
    UFUNCTION() void OnRep_Broken();
    void PresentState(bool bBurst);
    UPROPERTY(ReplicatedUsing=OnRep_Broken) bool bBroken = false;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Parts;
    bool bPresentationReady = false;
};
