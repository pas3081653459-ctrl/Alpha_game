#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AlphaGameMode.generated.h"

class AAlphaEnemy;

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
    bool IsSelectionAllowed() const;
private:
    void StartNextWave();
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
