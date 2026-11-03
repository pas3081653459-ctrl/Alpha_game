#pragma once

#include "CoreMinimal.h"
#include "Characters/AlphaCharacter.h"
#include "AlphaEnemy.generated.h"

UENUM(BlueprintType)
enum class EAlphaEnemyRole : uint8 { Melee, Shield, Gunner };

UENUM(BlueprintType)
enum class EAlphaEnemyTellState : uint8 { Idle, Windup, Recovery };

/** Server-only decisions; CharacterMovement replicates movement to clients. */
UCLASS()
class ALPHAGAME_API AAlphaEnemy : public AAlphaCharacter
{
    GENERATED_BODY()

public:
    AAlphaEnemy();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void ModifyIncomingHit(float& Damage, float& PoiseDamage, AActor* Source) override;
    void ConfigureRole(int32 RoleIndex);
    EAlphaEnemyRole GetEnemyRole() const { return EnemyRole; }
    EAlphaEnemyTellState GetTellState() const { return TellState; }
    FVector GetTellOrigin() const { return TellOrigin; }
    FVector GetTellDirection() const { return TellDirection; }
    double GetTellStartedAt() const { return TellStartedAt; }
    double GetResolveAt() const { return TellResolveAt; }
    float GetTellProgress() const;
    bool IsGuardActive() const;
    int32 GetShotRevision() const { return ShotRevision; }

protected:
    virtual void BeginPlay() override;
    virtual void HandleDeath() override;

    UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0.0"))
    float AttackDistance = 145.0f;

    UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0.0"))
    float AttackDepthTolerance = 75.0f;

private:
    void FindTarget();
    bool HasAttackSlot(AAlphaCharacter* Candidate) const;
    bool HasLineOfSightTo(AAlphaCharacter* Candidate) const;
    void BeginTacticalAttack();
    void UpdateTacticalAttack();
    void ResolveGunnerShot();
    void EnterRecovery(float Duration);
    double ServerTime() const;
    UFUNCTION() void OnRep_Role();
    UFUNCTION() void HandleShieldBroken();
    UPROPERTY(ReplicatedUsing=OnRep_Role) EAlphaEnemyRole EnemyRole = EAlphaEnemyRole::Melee;
    UPROPERTY(Replicated) EAlphaEnemyTellState TellState = EAlphaEnemyTellState::Idle;
    UPROPERTY(Replicated) FVector TellOrigin = FVector::ZeroVector;
    UPROPERTY(Replicated) FVector TellDirection = FVector::ForwardVector;
    UPROPERTY(Replicated) double TellStartedAt = 0.;
    UPROPERTY(Replicated) double TellResolveAt = 0.;
    UPROPERTY(Replicated) double ShieldOpenUntil = 0.;
    UPROPERTY(Replicated) int32 ShotRevision = 0;
    double TacticalRecoveryUntil = 0.;
    int32 AttackDamageRevision = 0;
    FVector AttackBodyOrigin = FVector::ZeroVector;
    TWeakObjectPtr<AAlphaCharacter> AttackTarget;
    TWeakObjectPtr<AAlphaCharacter> Target;
    float RetargetCountdown = 0.0f;
    float RouteRefreshCountdown = 0.0f;
    FVector RouteWaypoint = FVector::ZeroVector;
    TWeakObjectPtr<AAlphaCharacter> RoutedTarget;
};
