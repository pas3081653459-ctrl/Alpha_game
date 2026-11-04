#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "TimerManager.h"
#include "AlphaGameInstance.generated.h"

class UNetDriver;
class UAlphaProfileSave;
namespace AlphaProfileRules { struct Data; }

/** Keeps connection errors visible after returning to the local prototype arena. */
UCLASS()
class ALPHAGAME_API UAlphaGameInstance : public UGameInstance
{
    GENERATED_BODY()
public:
    virtual void Init() override;
    virtual void Shutdown() override;
    void BeginConnectionAttempt();

    int32 GetHighestUnlockedChapter() const;
    int32 GetLastChapter() const;
    int32 GetPreferredHero() const;
    float GetAudioVolume() const;
    float GetCameraShake() const;
    FString GetProfileStatusError() const { return ProfileStatusError; }
    bool RecordChapterComplete(int32 Chapter);
    bool SetPreferredChapter(int32 Chapter);
    bool SetPreferredHero(int32 Hero);
    bool SetPresentationSettings(float Volume, float Shake);
    /** Explicit replacement only. A failed/corrupt load never invokes this automatically. */
    bool ResetLocalProfile();

    UPROPERTY(BlueprintReadOnly, Category="Network")
    FString LastConnectionError;
private:
    void LoadLocalProfile();
    bool CommitProfile(const AlphaProfileRules::Data& Data, bool bExplicitReset = false);
    UPROPERTY(Transient) TObjectPtr<UAlphaProfileSave> LocalProfile;
    FString ProfileStatusError;
    bool bProfileReadOnly = false;
    bool bProfileLoaded = false;
    bool bProfileNeedsSave = false;
    void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error);
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
    void ScheduleLocalRecovery(UWorld* FailedWorld);
    void ReturnToLocalArena();

    FDelegateHandle NetworkFailureHandle;
    FDelegateHandle TravelFailureHandle;
    FTimerHandle RecoveryTimer;
    bool bRecoveryAttempted = false;
};
