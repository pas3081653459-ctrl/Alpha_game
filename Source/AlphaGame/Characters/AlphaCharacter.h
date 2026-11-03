#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Rules/PlayerControlRules.h"
#include "AlphaCharacter.generated.h"

class UAlphaCombatComponent;
class UAlphaReviveComponent;
class UAlphaAbilityComponent;
class UAlphaCharacterVisualComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/** Asset-free player pawn for the network combat prototype. */
UCLASS()
class ALPHAGAME_API AAlphaCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AAlphaCharacter();

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Combat")
    UAlphaCombatComponent* GetCombatComponent() const { return CombatComponent; }

    UFUNCTION(BlueprintPure, Category = "Coop")
    UAlphaReviveComponent* GetReviveComponent() const { return ReviveComponent; }

    UFUNCTION(BlueprintPure, Category = "Abilities")
    UAlphaAbilityComponent* GetAbilityComponent() const { return AbilityComponent; }

    UFUNCTION(BlueprintPure, Category = "Visual")
    UAlphaCharacterVisualComponent* GetVisualComponent() const { return VisualComponent; }

    UFUNCTION(BlueprintPure, Category = "Character")
    int32 GetArchetype() const { return Archetype; }

    /** Authority-only; controller/game mode must restrict selection to waiting or terminal rounds. */
    UFUNCTION(BlueprintCallable, Category = "Character")
    void SetArchetype(int32 NewArchetype);

    /** Combat must already have successfully revived; restores the character's replicated movement state. */
    bool RecoverFromRevive();
    /** Authority-only defense adjustment. Never grants damage authority to a client. */
    virtual void ModifyIncomingHit(float& Damage, float& PoiseDamage, AActor* Source) {}

    /** Called by the authority when restarting a round. */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ResetForRound();

    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsDefeated() const { return bDefeated; }

    /** Authority-side roster membership; disconnected or unpossessed pawns are excluded. */
    bool IsCoopParticipant() const;

    UFUNCTION(BlueprintPure, Category = "Movement")
    bool IsSprinting() const;

    UFUNCTION(BlueprintPure, Category = "Movement")
    float GetBaseWalkSpeed() const { return BaseWalkSpeed; }

    UFUNCTION(BlueprintCallable, Category = "Movement")
    void StopSprint();

    /** Pure XY facing intent; no position or velocity is accepted from clients. Also used by authority AI. */
    void FaceMoveIntent(const FVector& XYIntent);
    void BeginAirComboHold();
    void EndAirComboHold();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UAlphaCombatComponent> CombatComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coop")
    TObjectPtr<UAlphaReviveComponent> ReviveComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
    TObjectPtr<UAlphaAbilityComponent> AbilityComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
    TObjectPtr<UAlphaCharacterVisualComponent> VisualComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
    TObjectPtr<UStaticMeshComponent> PrototypeBody;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
    TObjectPtr<UStaticMeshComponent> PrototypeWeapon;

    UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.1"))
    float DodgeCooldown = 0.8f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.0"))
    float DodgeSpeed = 950.0f;

    UPROPERTY(ReplicatedUsing = OnRep_Defeated, BlueprintReadOnly, Category = "Combat")
    bool bDefeated = false;

    UFUNCTION()
    virtual void HandleDeath();

    /** Compatibility helper for existing lane-based callers. */
    void FaceAlongLane(float Direction);

private:
    void MoveForward(float Value);
    void MoveDepth(float Value);
    void BeginJump();
    void LightAttack();
    void HeavyAttack();
    void SyncAttackFacing();
    void Dodge();
    void BeginRevive();
    void EndRevive();
    void Skill1();
    void Skill2();
    void Skill3();
    bool IsMenuBlockingActions() const;

    void DirectionDPressed();
    void DirectionDReleased();
    void DirectionAPressed();
    void DirectionAReleased();
    void DirectionWPressed();
    void DirectionWReleased();
    void DirectionSPressed();
    void DirectionSReleased();
    void HandleDirectionKey(int32 KeyIndex, bool bPressed);
    void ProcessDirectionKey(int32 KeyIndex, bool bPressed, bool bAuthorityState);
    void SetSprintState(bool bEnabled, int32 KeyIndex);
    void RefreshSprintSpeed();
    void ResetTapHistory(bool bAuthorityState);
    void AcknowledgeSprintState();
    void UpdateMoveFacing();
    void ApplyAirComboHold();
    void AirComboHoldExpired();
    void CancelAirComboForAction();

    UFUNCTION(Server, Reliable)
    void ServerDirectionKey(int32 KeyIndex, bool bPressed);

    UFUNCTION(Server, Reliable)
    void ServerStopSprint();

    UFUNCTION(Server, Reliable)
    void ServerCancelAirCombo();

    UFUNCTION()
    void OnRep_SprintState();

    UFUNCTION()
    void OnRep_AirComboHeld();

    UFUNCTION()
    void HandleAirComboChanged(bool bHolding);

    UFUNCTION()
    void HandleComboFinished();

    UFUNCTION()
    void ShowCombo(int32 Stage, bool bAirborne);

    UFUNCTION(Server, Reliable)
    void ServerDodge();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastRoll(FVector Facing);

    UFUNCTION(Server, Reliable)
    void ServerSetFacing(FVector XYIntent);

    UFUNCTION()
    void OnRep_Defeated();

    void ApplyDefeatedState();

    UFUNCTION()
    void OnRep_Archetype();

    void ApplyArchetypeAppearance();

    UPROPERTY(ReplicatedUsing = OnRep_Archetype)
    int32 Archetype = 0;

    UFUNCTION()
    void ShowAttack(bool bHeavy);

    UFUNCTION()
    void CancelAttackVisuals();

    UFUNCTION()
    void ShowPoiseBreak();

    UFUNCTION()
    void ShowHit(float ActualDamage, AActor* InstigatorActor);

    void RestoreWeaponPose();
    void RestoreBodyAppearance();

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> WeaponMaterial;

    FTimerHandle WeaponFeedbackTimer;
    FTimerHandle PoiseFeedbackTimer;
    FTimerHandle AirComboHoldTimer;
    FLinearColor TeamColor;

    double LastDodgeTime = -1000.0;
    double LastFacingRequestTime = -1000.0;
    double VisualFacingLockedUntil = 0.0;
    FVector LastRequestedFacing = FVector::ForwardVector;
    FVector MoveInputIntent = FVector::ZeroVector;
    float BaseWalkSpeed = 480.0f;

    UPROPERTY(ReplicatedUsing = OnRep_SprintState)
    bool bSprinting = false;

    UPROPERTY(ReplicatedUsing = OnRep_SprintState)
    int32 SprintRevision = 0;

    UPROPERTY(Replicated)
    int32 SprintKey = -1;

    UPROPERTY(ReplicatedUsing = OnRep_AirComboHeld)
    bool bAirComboHeld = false;

    bool bPredictedSprinting = false;
    int32 PredictedSprintKey = -1;
    AlphaPlayerControl::TapState LocalTapStates[4];
    AlphaPlayerControl::TapState AuthorityTapStates[4];
    float SavedGravityScale = 1.0f;
    bool bSavedGravity = false;
};
