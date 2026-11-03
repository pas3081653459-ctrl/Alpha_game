#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AlphaGameMode.generated.h"

class AAlphaEnemy;
class AAlphaCharacter;
class AAlphaCompanion;

UCLASS()
class ALPHAGAME_API AAlphaGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AAlphaGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    void RestartPrototypeRound();
    /** Explicit host-only entry from training; never begins on a login timer. */
    bool StartPrototypeRun();
    bool TalkToStoryNPC(AAlphaCharacter* Speaker);
    bool AdvanceChapter();
    bool IsSelectionAllowed() const;
    bool SetCompanionFillEnabled(bool bEnabled);
    void RefreshCompanions();
    bool IsCompanionFillEnabled() const { return bCompanionFillEnabled; }
private:
    friend class FAlphaPartyLifecycleTest;
    friend class FAlphaStoryAuthorityTest;
    TArray<APlayerController*> HumanControllers(const AController* Ignore = nullptr) const;
    TArray<AAlphaCompanion*> CompanionActors() const;
    void UpdatePartyCounts();
    void ClearCompanions();
    static void DestroyCompanion(AAlphaCompanion* Companion);
    bool bCompanionFillEnabled = true;
    void StartNextWave();
    void MarkChapterVictory();
    void CheckRound();
    void EnsureRoundMonitor();
    void StartBossEncounter();
    void ClearEncounterActors();
    void MarkDefeat();
    int32 RunPlayerCount = 1;
    FTimerHandle WaveTimer;
    FTimerHandle MonitorTimer;
    TArray<TWeakObjectPtr<AAlphaEnemy>> WaveEnemies;
};
