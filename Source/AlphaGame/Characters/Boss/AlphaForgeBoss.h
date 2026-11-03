#pragma once

#include "CoreMinimal.h"
#include "Characters/AlphaCharacter.h"
#include "Engine/NetSerialization.h"
#include "AlphaForgeBoss.generated.h"

UENUM(BlueprintType)
enum class EAlphaForgeAttack : uint8
{
    None,
    Sweep,
    FireLine,
    Hammer
};

UENUM(BlueprintType)
enum class EAlphaForgeAttackState : uint8
{
    Idle,
    Windup,
    Recovery,
    Defeated
};

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

/** Two-phase authority boss with cooked mesh floor telegraphs on every peer. */
UCLASS()
class ALPHAGAME_API AAlphaForgeBoss : public AAlphaCharacter
{
    GENERATED_BODY()
public:
    AAlphaForgeBoss();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Called once when the encounter spawns, never during disconnects. */
    void ConfigureForPlayers(int32 PlayerCount);
    int32 GetBossPhase() const { return BossPhase; }
    EAlphaForgeAttack GetAttackType() const { return AttackType; }
    EAlphaForgeAttackState GetAttackState() const { return AttackState; }
    double GetAttackResolveTime() const { return AttackResolveTime; }
    double GetAttackCooldownUntil() const { return AttackCooldownUntil; }
    FVector GetTelegraphOrigin() const { return TelegraphOrigin; }
    FVector GetTelegraphDirection() const { return TelegraphDirection; }

    /** Exact gameplay footprint shared by authority damage and automation tests. */
    static bool IsPointInAttackZone(EAlphaForgeAttack Attack, const FVector& Origin,
        const FVector& Direction, const FVector& Point);

protected:
    virtual void BeginPlay() override;
    virtual void HandleDeath() override;

private:
    AAlphaCharacter* FindClosestLivingPlayer() const;
    void BeginAttack(AAlphaCharacter* Target);
    void ResolveAttack();
    void UpdateTelegraph();
    void AddTelegraphSegment(const FVector& A, const FVector& B, float Width = 5.f);
    double ServerTime() const;

    UFUNCTION()
    void HandlePoiseBreak();

    UPROPERTY(Replicated)
    int32 BossPhase = 1;
    UPROPERTY(Replicated)
    EAlphaForgeAttack AttackType = EAlphaForgeAttack::None;
    UPROPERTY(Replicated)
    EAlphaForgeAttackState AttackState = EAlphaForgeAttackState::Idle;
    UPROPERTY(Replicated)
    double AttackServerStartTime = 0.0;
    UPROPERTY(Replicated)
    double AttackResolveTime = 0.0;
    UPROPERTY(Replicated)
    double AttackCooldownUntil = 0.0;
    UPROPERTY(Replicated)
    FVector_NetQuantize TelegraphOrigin;
    UPROPERTY(Replicated)
    FVector_NetQuantizeNormal TelegraphDirection;

    UPROPERTY(VisibleAnywhere, Category="Boss|Visual")
    TObjectPtr<UInstancedStaticMeshComponent> TelegraphMarkers;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> TelegraphMaterial;

    int32 AttackSequence = 0;
    bool bConfigured = false;
};
