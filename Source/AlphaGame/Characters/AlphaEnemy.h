#pragma once

#include "CoreMinimal.h"
#include "Characters/AlphaCharacter.h"
#include "AlphaEnemy.generated.h"

/** Server-only decisions; CharacterMovement replicates movement to clients. */
UCLASS()
class ALPHAGAME_API AAlphaEnemy : public AAlphaCharacter
{
    GENERATED_BODY()

public:
    AAlphaEnemy();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;
    virtual void HandleDeath() override;

    UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0.0"))
    float AttackDistance = 145.0f;

    UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0.0"))
    float AttackDepthTolerance = 75.0f;

private:
    void FindTarget();
    TWeakObjectPtr<AAlphaCharacter> Target;
    float RetargetCountdown = 0.0f;
};
