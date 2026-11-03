#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AlphaReviveComponent.generated.h"

class AAlphaCharacter;

/** Held-input revive. All selection, claims, interruption and completion run on authority. */
UCLASS(ClassGroup=(AlphaGame), meta=(BlueprintSpawnableComponent))
class ALPHAGAME_API UAlphaReviveComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAlphaReviveComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category="Coop")
    void BeginRevive();

    UFUNCTION(BlueprintCallable, Category="Coop")
    void EndRevive();

    /** Authority-only cancellation, also used by combat, dodge and ability execution. */
    UFUNCTION(BlueprintCallable, Category="Coop")
    void CancelRevive();

    void RegisterDowned();
    void ResetForRound();

    UFUNCTION(BlueprintPure, Category="Coop")
    AAlphaCharacter* GetReviveTarget() const { return ReviveTarget; }

    UFUNCTION(BlueprintPure, Category="Coop")
    AAlphaCharacter* GetCurrentRescuer() const { return CurrentRescuer; }

    UFUNCTION(BlueprintPure, Category="Coop")
    float GetReviveProgress() const { return ReviveProgress; }

    UFUNCTION(BlueprintPure, Category="Coop")
    int32 GetDownCount() const { return DownCount; }

    UFUNCTION(BlueprintPure, Category="Coop")
    float GetRequiredReviveDuration() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION(Server, Reliable)
    void ServerSetReviveHeld(bool bHeld);

    UFUNCTION()
    void HandleOwnerHit(float ActualDamage, AActor* InstigatorActor);

    void SetReviveHeld(bool bHeld);
    void CancelIncomingRevive();
    AAlphaCharacter* FindNearestEligibleTarget() const;
    bool CanRescue(AAlphaCharacter* Character) const;
    bool IsEligibleTarget(AAlphaCharacter* Candidate) const;
    bool HasClearLineOfSight(const AAlphaCharacter* Candidate) const;
    bool IsRoundActive() const;

    UPROPERTY(Replicated)
    TObjectPtr<AAlphaCharacter> ReviveTarget;

    UPROPERTY(Replicated)
    TObjectPtr<AAlphaCharacter> CurrentRescuer;

    UPROPERTY(Replicated)
    float ReviveProgress = 0.0f;

    UPROPERTY(Replicated)
    int32 DownCount = 0;

    bool bReviveHeld = false;
    float ElapsedTime = 0.0f;
    int32 RescuerDamageRevision = 0;
    int32 TargetDamageRevision = 0;
    static constexpr float ReviveRange = 220.0f;
};
