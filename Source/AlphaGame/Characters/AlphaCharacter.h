#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
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

    /** Called by the authority when restarting a round. */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ResetForRound();

    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsDefeated() const { return bDefeated; }

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

    /** Yaw remains along the lane even while the pawn moves in depth. */
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
    bool IsMenuBlockingActions() const;

    UFUNCTION(Server, Reliable)
    void ServerDodge();

    UFUNCTION(Server, Reliable)
    void ServerSetFacing(bool bFaceRight);

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
    FLinearColor TeamColor;

    double LastDodgeTime = -1000.0;
    double LastFacingRequestTime = -1000.0;
    bool bLastRequestedFacingRight = true;
};
