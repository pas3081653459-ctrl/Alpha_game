#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "AlphaCombatComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAlphaCombatEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAlphaAttackEvent, bool, bHeavy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAlphaHitEvent, float, ActualDamage, AActor*, InstigatorActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAlphaHitConfirmedEvent, AActor*, Target, bool, bPoiseBroken);

/** Server-authoritative timed attacks, hit reactions, defensive windows and vitals. */
UCLASS(ClassGroup=(AlphaGame), meta=(BlueprintSpawnableComponent))
class ALPHAGAME_API UAlphaCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAlphaCombatComponent();

    UFUNCTION(BlueprintCallable, Category="Combat")
    void TryAttack(bool bHeavy = false);

    /** Only effective on authority. Clients cannot submit damage or positions. */
    UFUNCTION(BlueprintCallable, Category="Combat")
    void ReceiveHit(float Damage, float PoiseDamage, AActor* InstigatorActor);

    UFUNCTION(BlueprintCallable, Category="Combat")
    void ResetVitals();

    UFUNCTION(BlueprintPure, Category="Combat")
    bool CanAct() const;

    UFUNCTION(BlueprintCallable, Category="Combat")
    void SetInvulnerableFor(float Duration);

    UFUNCTION(BlueprintCallable, Category="Combat")
    void SetActionLockFor(float Duration);

    UFUNCTION(BlueprintCallable, Category="Combat")
    void ConfigureVitals(float NewMaxHealth, float NewMaxPoise, float InPoiseBreakStunDuration = 0.65f,
        float InNormalHitStunDuration = 0.08f);

    /** Authority-only interaction cancellation; preserves recovery and does not add a stagger. */
    UFUNCTION(BlueprintCallable, Category="Combat")
    void CancelCurrentAttack();

    UFUNCTION(BlueprintPure, Category="Combat")
    bool IsInvulnerable() const;

    UFUNCTION(BlueprintCallable, Category="Combat")
    bool Revive(float HealthFraction = 0.4f);

    /** Configure only while entering/restarting a round; resets vitals. Server-only. */
    UFUNCTION(BlueprintCallable, Category="Combat")
    void ConfigureArchetype(int32 Archetype);

    UFUNCTION(BlueprintPure, Category="Combat")
    int32 GetDamageRevision() const { return DamageRevision; }

    UFUNCTION(BlueprintPure, Category="Combat")
    int32 GetPoiseBreakRevision() const { return PoiseBreakRevision; }

    /** Server-only notification for skills after independently confirming accepted damage. */
    void NotifyHitConfirmed(AActor* Target, bool bPoiseBroken);

    UFUNCTION(BlueprintPure, Category="Combat")
    bool IsAlive() const { return Health > 0.0f; }

    UFUNCTION(BlueprintPure, Category="Combat")
    float GetHealth() const { return Health; }

    UFUNCTION(BlueprintPure, Category="Combat")
    float GetMaxHealth() const { return MaxHealth; }

    UFUNCTION(BlueprintPure, Category="Combat")
    float GetPoise() const { return Poise; }

    UFUNCTION(BlueprintPure, Category="Combat")
    float GetMaxPoise() const { return MaxPoise; }

    /** Same nonnegative team ID means friendly. Configure players as 1 and enemies as 2. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Combat")
    int32 TeamId = 1;

    UPROPERTY(BlueprintAssignable, Category="Combat")
    FAlphaCombatEvent OnDeath;

    UPROPERTY(BlueprintAssignable, Category="Combat")
    FAlphaCombatEvent OnVitalsChanged;

    UPROPERTY(BlueprintAssignable, Category="Combat")
    FAlphaCombatEvent OnPoiseBroken;

    /** Presentation event on the server and relevant clients; never drives hit detection. */
    UPROPERTY(BlueprintAssignable, Category="Combat")
    FAlphaAttackEvent OnAttack;

    UPROPERTY(BlueprintAssignable, Category="Combat")
    FAlphaCombatEvent OnAttackCancelled;

    UPROPERTY(BlueprintAssignable, Category="Combat")
    FAlphaHitEvent OnHit;

    /** Authority only: resource systems must never award resources from client presentation events. */
    UPROPERTY(BlueprintAssignable, Category="Combat")
    FAlphaHitConfirmedEvent OnHitConfirmed;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditAnywhere, Replicated, Category="Combat|Vitals", meta=(ClampMin="1.0"))
    float MaxHealth = 100.0f;

    UPROPERTY(EditAnywhere, Replicated, Category="Combat|Vitals", meta=(ClampMin="1.0"))
    float MaxPoise = 60.0f;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Combat|Vitals")
    float Health = 100.0f;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Poise, Category="Combat|Vitals")
    float Poise = 60.0f;

    UPROPERTY(EditAnywhere, Category="Combat|Attack", meta=(ClampMin="0.05"))
    float LightCooldown = 0.35f;

    UPROPERTY(EditAnywhere, Category="Combat|Attack", meta=(ClampMin="0.05"))
    float HeavyCooldown = 0.8f;

    UPROPERTY(EditAnywhere, Category="Combat|Attack", meta=(ClampMin="0.01"))
    float LightWindup = 0.12f;

    UPROPERTY(EditAnywhere, Category="Combat|Attack", meta=(ClampMin="0.01"))
    float HeavyWindup = 0.32f;

    UPROPERTY(EditAnywhere, Category="Combat|Attack", meta=(ClampMin="0.01"))
    float InputBufferWindow = 0.12f;

    UPROPERTY(EditAnywhere, Category="Combat|Attack", meta=(ClampMin="0.01"))
    float PoiseBreakStunDuration = 0.65f;

    UPROPERTY(EditAnywhere, Category="Combat|Attack", meta=(ClampMin="0.0", ClampMax="1.0"))
    float NormalHitStunDuration = 0.08f;

    UPROPERTY(EditAnywhere, Category="Combat|Attack", meta=(ClampMin="0.0"))
    float LightDamage = 15.0f;

    UPROPERTY(EditAnywhere, Category="Combat|Attack", meta=(ClampMin="0.0"))
    float HeavyDamage = 30.0f;

    UPROPERTY(EditAnywhere, Category="Combat|Attack", meta=(ClampMin="0.0"))
    float LightPoiseDamage = 10.0f;

    UPROPERTY(EditAnywhere, Category="Combat|Attack", meta=(ClampMin="0.0"))
    float HeavyPoiseDamage = 35.0f;

    UPROPERTY(EditAnywhere, Category="Combat|Attack", meta=(ClampMin="1.0"))
    float AttackReach = 175.0f;

    UPROPERTY(EditAnywhere, Category="Combat|Attack", meta=(ClampMin="1.0"))
    float AttackRadius = 65.0f;

private:
    UPROPERTY(Replicated)
    double RecoveryUntil = 0.0;

    UPROPERTY(Replicated)
    double StaggerUntil = 0.0;

    UPROPERTY(Replicated)
    double InvulnerableUntil = 0.0;

    UPROPERTY(Replicated)
    int32 DamageRevision = 0;

    UPROPERTY(Replicated)
    int32 PoiseBreakRevision = 0;

    FTimerHandle WindupTimer;
    FTimerHandle RecoveryTimer;
    bool bPendingHeavy = false;
    bool bAttackPending = false;
    bool bBufferedAttack = false;
    bool bBufferedHeavy = false;
    double BufferedAttackExpires = 0.0;

    UFUNCTION(Server, Reliable)
    void ServerTryAttack(bool bHeavy);

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastAttack(bool bHeavy);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastAttackCancelled();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastPoiseBroken();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastHit(float ActualDamage, AActor* InstigatorActor);

    UFUNCTION()
    void OnRep_Health(float PreviousHealth);

    UFUNCTION()
    void OnRep_Poise();

    void ExecuteAttack(bool bHeavy);
    void ReleaseAttack();
    void FinishRecovery();
    void CancelPendingAttack();
    double CombatTime() const;
    bool IsFriendly(const AActor* OtherActor) const;
    bool bEndingPlay = false;
};
