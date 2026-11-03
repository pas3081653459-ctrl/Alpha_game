#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "AlphaCombatCamera.generated.h"

class APawn;

/** Local presentation camera: frames all teammates, never drives movement. */
UCLASS()
class ALPHAGAME_API AAlphaCombatCamera : public ACameraActor
{
    GENERATED_BODY()
public:
    AAlphaCombatCamera();
    virtual void Tick(float DeltaSeconds) override;
    void SetShakeStrength(float Value);
private:
    FVector SmoothedFocus = FVector(0, 70, 120);
    double SmoothedDistance = 2800.;
    float ShakeStrength = .35f;
    float Impact = 0.f;
    float LastHealth = -1.f;
    TWeakObjectPtr<APawn> LastPawn;
    bool bInitialized = false;
};
