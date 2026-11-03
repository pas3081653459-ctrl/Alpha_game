#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "TimerManager.h"
#include "AlphaGameInstance.generated.h"

class UNetDriver;

/** Keeps connection errors visible after returning to the local prototype arena. */
UCLASS()
class ALPHAGAME_API UAlphaGameInstance : public UGameInstance
{
    GENERATED_BODY()
public:
    virtual void Init() override;
    virtual void Shutdown() override;
    void BeginConnectionAttempt();

    UPROPERTY(BlueprintReadOnly, Category="Network")
    FString LastConnectionError;
private:
    void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error);
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
    void ScheduleLocalRecovery(UWorld* FailedWorld);
    void ReturnToLocalArena();

    FDelegateHandle NetworkFailureHandle;
    FDelegateHandle TravelFailureHandle;
    FTimerHandle RecoveryTimer;
    bool bRecoveryAttempted = false;
};
