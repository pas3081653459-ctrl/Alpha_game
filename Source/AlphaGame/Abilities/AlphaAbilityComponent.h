#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AlphaAbilityComponent.generated.h"

class AAlphaCharacter;
class UAlphaCombatComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAlphaSkillActivatedEvent, int32, Slot);

/** Asset-free class skills. Requests contain only a slot; the authority owns all targeting and values. */
UCLASS(ClassGroup=(AlphaGame), meta=(BlueprintSpawnableComponent))
class ALPHAGAME_API UAlphaAbilityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAlphaAbilityComponent();

    UFUNCTION(BlueprintCallable, Category="Abilities")
    void TryUseSkill(int32 Slot);

    UFUNCTION(BlueprintPure, Category="Abilities")
    float GetSkillRemaining(int32 Slot) const;

    UFUNCTION(BlueprintPure, Category="Abilities")
    bool CanUseSkill(int32 Slot) const;

    UFUNCTION(BlueprintPure, Category="Abilities")
    float GetEnergy() const { return Energy; }

    /** Compatibility accessor for previous HUDs; this is now U/I/O energy. */
    UFUNCTION(BlueprintPure, Category="Abilities")
    float GetResonance() const { return Energy; }

    /** Confirmed cosmetic event; never use multicast presentation to award energy. */
    UPROPERTY(BlueprintAssignable, Category="Abilities")
    FAlphaSkillActivatedEvent OnSkillActivated;

    /** Round reset and class selection only. Death/revival deliberately preserve cooldowns. */
    void ResetForRound();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    UPROPERTY(Replicated)
    double SkillOneReadyAt = 0.0;

    UPROPERTY(Replicated)
    double SkillTwoReadyAt = 0.0;

    UPROPERTY(Replicated)
    double UltimateReadyAt = 0.0;

    UPROPERTY(Replicated)
    float Energy = 0.0f;

    TWeakObjectPtr<AActor> LastHitTarget;
    double LastHitAt = -1000.0;
    TMap<TWeakObjectPtr<AActor>, double> LastCoopProcAt;

    UFUNCTION(Server, Reliable)
    void ServerUseSkill(int32 Slot);

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastSkill(int32 Archetype, int32 Slot, FVector Origin, FVector Facing);

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastCoopCue(FVector Location);

    UFUNCTION()
    void HandleConfirmedHit(AActor* Target, bool bPoiseBroken);

    void ExecuteSkill(int32 Slot);
    void AffectEnemies(float Radius, float Reach, float Damage, float PoiseDamage,
        float PullSpeed = 0.0f, float LiftSpeed = 0.0f);
    double AbilityTime() const;
};
