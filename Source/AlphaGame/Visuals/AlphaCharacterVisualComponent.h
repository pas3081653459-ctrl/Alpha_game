#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "AlphaCharacterVisualComponent.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class AAlphaCombatEffect;
enum class EAlphaPrimitive : uint8;

/** Local, collision-free modular silhouettes and procedural animation; never drives gameplay. */
UCLASS(ClassGroup=(AlphaGame), meta=(BlueprintSpawnableComponent))
class ALPHAGAME_API UAlphaCharacterVisualComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAlphaCharacterVisualComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Keep the original body/weapon transform anchors so existing hit/death poses still apply. */
    void InitializeAnchors(USceneComponent* BodyAnchor, USceneComponent* WeaponAnchor);
    void RefreshAppearance(int32 Hero, bool bEnemy, bool bBoss);
    void SetDefeated(bool bNewDefeated);
    void PlayAttack(bool bHeavy);
    /** Five-stage player chain; timing and extent come from PlayerControlRules. */
    void PlayCombo(int32 Stage, bool bAirborne);
    void PlayRoll();
    void CancelAttackPresentation();
    void PlayHit(float Damage);
    void PlayPoiseBreak();
    void SetBossTell(int32 Attack, float Progress, bool bWindingUp, bool bRecovering);

    int32 GetVisiblePartCount() const { return Parts.Num(); }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    USceneComponent* AddPivot(FName Name, USceneComponent* Parent, const FVector& Location);
    UStaticMeshComponent* AddPart(EAlphaPrimitive Shape, USceneComponent* Parent,
        const FVector& Location, const FVector& Dimensions, UMaterialInstanceDynamic* Material,
        const FRotator& Rotation = FRotator::ZeroRotator);
    UMaterialInstanceDynamic* MakeMaterial(FLinearColor Tint, float Metallic, float Roughness, float Glow = 0.0f);
    void BuildSilhouette();
    void BuildWeapon();
    void ClearParts();
    void UpdateTickBudget();
    void ApplyMaterialState(float HitFlash, float PoiseFlash);
    void UpdateComboPose(float AttackTime);
    double VisualTime() const;

    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> BodyRoot;
    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> WeaponRoot;
    UPROPERTY(Transient)
    TArray<TObjectPtr<USceneComponent>> Pivots;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> Parts;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> LeftLeg;
    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> RightLeg;
    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> LeftArm;
    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> RightArm;
    UPROPERTY(Transient)
    TObjectPtr<UStaticMeshComponent> ClothAccent;
    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> LanternRing;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> ClothMaterial;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> MetalMaterial;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> AccentMaterial;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> FaceMaterial;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> GlowMaterial;

    TArray<FLinearColor> BaseTints;
    TArray<float> BaseGlows;
    FTimerHandle TickBudgetTimer;
    int32 HeroType = 0;
    bool bEnemyVisual = false;
    bool bBossVisual = false;
    bool bDefeatedVisual = false;
    bool bHeavyAttack = false;
    bool bAttackCueSpawned = true;
    int32 ComboStage = 0;
    bool bComboAirborne = false;
    FVector AttackFacing = FVector::ForwardVector;
    TWeakObjectPtr<AAlphaCombatEffect> AttackCue;
    bool bBossWindup = false;
    int32 BossAttackType = 0;
    float BossWindupProgress = 0.0f;
    float GaitPhase = 0.0f;
    double AttackStartedAt = -1000.0;
    double HitUntil = -1000.0;
    double PoiseUntil = -1000.0;
    double LastSprintCueAt = -1000.0;
    double RollStartedAt = -1000.0;
    float AttackWindup = 0.12f;
    float AttackDuration = 0.3f;
    float LastHitFlash = -1.0f;
    float LastPoiseFlash = -1.0f;
    bool bLastMaterialDefeated = false;
};
