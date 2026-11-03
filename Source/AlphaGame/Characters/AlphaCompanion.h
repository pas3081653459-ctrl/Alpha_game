#pragma once

#include "CoreMinimal.h"
#include "Characters/AlphaCharacter.h"
#include "AlphaCompanion.generated.h"

class AAlphaForgeBoss;

UENUM(BlueprintType)
enum class EAlphaCompanionAction : uint8
{
    Idle,
    Follow,
    Attack,
    Revive,
    Evade
};

/** Authority-only companion decisions; uses the same movement, damage, skills and rescue as a human. */
UCLASS()
class ALPHAGAME_API AAlphaCompanion : public AAlphaCharacter
{
    GENERATED_BODY()

public:
    AAlphaCompanion();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category="Companion")
    EAlphaCompanionAction GetCompanionAction() const { return BotAction; }

    UFUNCTION(BlueprintPure, Category="Companion")
    FString GetBehaviorLabel() const;

    /** Read-only destination for diagnostics/automation; it is not a teleport or client command. */
    UFUNCTION(BlueprintPure, Category="Companion")
    FVector GetDesiredDestination() const { return DesiredDestination; }

    /** Called after the game mode restores/positions a reused pawn; never revives or heals it. */
    void ResetCompanionBrain();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void HandleDeath() override;

private:
    void Decide();
    void SetAction(EAlphaCompanionAction NewAction);
    void StopMoving();
    void MoveToward(const FVector& Destination, float AcceptanceRadius);
    FVector AddPersonalSpace(const FVector& Direction) const;
    AAlphaCharacter* FindHumanAnchor() const;
    AAlphaCharacter* FindDownedAlly() const;
    AAlphaCharacter* FindEnemy(const AAlphaCharacter* Human) const;
    void GatherBossThreats();
    bool IsDangerous(const FVector& Location, bool bMargin = true) const;
    bool HasDirectPath(const FVector& Destination) const;
    bool ChooseEvadeDestination(FVector& Destination) const;
    bool TrySkill(AAlphaCharacter* Enemy);
    void Follow(AAlphaCharacter* Human);
    void Attack(AAlphaCharacter* Enemy);
    void Rescue(AAlphaCharacter* Ally);
    double ServerTime() const;

    UFUNCTION()
    void HandleCompanionHit(float ActualDamage, AActor* InstigatorActor);

    UPROPERTY(Replicated)
    EAlphaCompanionAction BotAction = EAlphaCompanionAction::Idle;

    TArray<TWeakObjectPtr<AAlphaForgeBoss>> Threats;
    FVector DesiredDestination = FVector::ZeroVector;
    FVector DesiredMove = FVector::ZeroVector;
    float DecisionAccumulator = 0.1f;
    double RescueRetryAt = 0.0;
    double NextAttackAt = 0.0;
    double EvadeUntil = 0.0;
    bool bNextHeavy = false;
};
