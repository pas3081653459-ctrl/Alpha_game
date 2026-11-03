#include "World/AlphaGameMode.h"
#include "World/AlphaGameState.h"
#include "World/AlphaPlayerController.h"
#include "Characters/AlphaCharacter.h"
#include "Characters/AlphaEnemy.h"
#include "Characters/Boss/AlphaForgeBoss.h"
#include "Rules/CombatRules.h"
#include "Combat/AlphaCombatComponent.h"
#include "UI/AlphaHUD.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "TimerManager.h"

AAlphaGameMode::AAlphaGameMode()
{
    DefaultPawnClass = AAlphaCharacter::StaticClass();
    PlayerControllerClass = AAlphaPlayerController::StaticClass();
    GameStateClass = AAlphaGameState::StaticClass();
    HUDClass = AAlphaHUD::StaticClass();
}

void AAlphaGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    // Entry has no authored PlayerStarts. Build before the first player logs in.
    for (int32 Index = 0; Index < 3; ++Index)
    {
        GetWorld()->SpawnActor<APlayerStart>(FVector(-650 + Index * 180, -100 + Index * 100, 110), FRotator::ZeroRotator);
    }
}

void AAlphaGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (ErrorMessage.IsEmpty() && GetNumPlayers() >= 3)
    {
        ErrorMessage = TEXT("Room full: maximum three players.");
    }
    const AAlphaGameState* State = GetGameState<AAlphaGameState>();
    if (ErrorMessage.IsEmpty() && State && (State->RoundState == EAlphaRoundState::Fighting || State->RoundState == EAlphaRoundState::Intermission))
    {
        ErrorMessage = TEXT("Run in progress: wait for preparation or the result screen.");
    }
}

void AAlphaGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    // Recheck after admission: near-simultaneous PreLogin calls can overlap.
    if (GetNumPlayers() > 3)
    {
        if (GameSession) GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("Room full: maximum three players.")));
        return;
    }
    // A run may have started between PreLogin and this callback.
    const AAlphaGameState* State = GetGameState<AAlphaGameState>();
    if (State && (State->RoundState == EAlphaRoundState::Fighting || State->RoundState == EAlphaRoundState::Intermission))
    {
        if (GameSession) GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("Run in progress: wait for preparation or the result screen.")));
        return;
    }
    EnsureRoundMonitor();
}

void AAlphaGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);
    if (GetNumPlayers() == 0)
    {
        GetWorldTimerManager().ClearTimer(WaveTimer);
        GetWorldTimerManager().ClearTimer(MonitorTimer);
        ClearEncounterActors();
        if (AAlphaGameState* State = GetGameState<AAlphaGameState>())
        {
            State->Wave = 0;
            State->RemainingEnemies = 0;
            State->BossPhase = 0;
            State->RoundState = EAlphaRoundState::Waiting;
            State->EncounterStage = EAlphaEncounterStage::Preparation;
        }
    }
    // A departing player never recalculates/heals an already spawned boss.
}

void AAlphaGameMode::EnsureRoundMonitor()
{
    if (!GetGameState<AAlphaGameState>())
    {
        GetWorldTimerManager().SetTimer(WaveTimer, this, &AAlphaGameMode::EnsureRoundMonitor, .25f, false);
        return;
    }
    if (!GetWorldTimerManager().IsTimerActive(MonitorTimer))
    {
        GetWorldTimerManager().SetTimer(MonitorTimer, this, &AAlphaGameMode::CheckRound, .25f, true);
    }
}

bool AAlphaGameMode::IsSelectionAllowed() const
{
    const AAlphaGameState* State = GetGameState<AAlphaGameState>();
    return State && (State->RoundState == EAlphaRoundState::Waiting
        || State->RoundState == EAlphaRoundState::Victory || State->RoundState == EAlphaRoundState::Defeat);
}

bool AAlphaGameMode::StartPrototypeRun()
{
    AAlphaGameState* State = GetGameState<AAlphaGameState>();
    if (!HasAuthority() || !State || State->RoundState != EAlphaRoundState::Waiting || GetNumPlayers() < 1) return false;
    // Capture once: joining/leaving must not mutate live encounter vitality.
    RunPlayerCount = FMath::Clamp(GetNumPlayers(), 1, 3);
    State->EncounterPlayerCount = RunPlayerCount;
    State->EncounterStage = EAlphaEncounterStage::Waves;
    EnsureRoundMonitor();
    StartNextWave();
    return State->RoundState == EAlphaRoundState::Fighting;
}

void AAlphaGameMode::MarkDefeat()
{
    GetWorldTimerManager().ClearTimer(WaveTimer);
    // Selection is allowed after defeat; no surviving AI may attack newly
    // configured/revived heroes while the host reviews the result.
    ClearEncounterActors();
    if (AAlphaGameState* State = GetGameState<AAlphaGameState>())
    {
        State->RemainingEnemies = 0;
        State->RoundState = EAlphaRoundState::Defeat;
        State->EncounterStage = EAlphaEncounterStage::Finished;
    }
}

void AAlphaGameMode::ClearEncounterActors()
{
    for (TActorIterator<AAlphaEnemy> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<AAlphaForgeBoss> It(GetWorld()); It; ++It) It->Destroy();
    WaveEnemies.Reset();
    if (AAlphaGameState* State = GetGameState<AAlphaGameState>()) State->ForgeBoss = nullptr;
}

void AAlphaGameMode::StartNextWave()
{
    AAlphaGameState* State = GetGameState<AAlphaGameState>();
    if (!State || GetNumPlayers() == 0) return;
    if (State->RoundState != EAlphaRoundState::Waiting && State->RoundState != EAlphaRoundState::Intermission) return;
    if (State->EncounterStage != EAlphaEncounterStage::Waves || State->Wave >= AAlphaGameState::TotalWaves) return;
    State->Wave++;
    State->RoundState = EAlphaRoundState::Fighting;
    WaveEnemies.Reset();
    const int32 Count = AlphaRules::WaveEnemyCount(State->Wave, RunPlayerCount);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        const FVector Location(650.f + (Index % 2) * 180.f, -240.f + (Index % 4) * 150.f, 110.f);
        AAlphaEnemy* Enemy = GetWorld()->SpawnActor<AAlphaEnemy>(AAlphaEnemy::StaticClass(), Location, FRotator(0, 180, 0), Params);
        if (Enemy) WaveEnemies.Add(Enemy);
    }
    State->RemainingEnemies = WaveEnemies.Num();
    if (WaveEnemies.IsEmpty())
    {
        // A failed spawn is a failed round, never a false-positive victory.
        MarkDefeat();
        UE_LOG(LogTemp, Error, TEXT("AlphaGame: no enemies could be spawned."));
    }
}

void AAlphaGameMode::StartBossEncounter()
{
    AAlphaGameState* State = GetGameState<AAlphaGameState>();
    if (!State || GetNumPlayers() == 0 || State->RoundState != EAlphaRoundState::Intermission) return;
    if (State->EncounterStage != EAlphaEncounterStage::Boss) return;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    AAlphaForgeBoss* Boss = GetWorld()->SpawnActor<AAlphaForgeBoss>(AAlphaForgeBoss::StaticClass(), FVector(800, 0, 150), FRotator(0, 180, 0), Params);
    if (!Boss)
    {
        MarkDefeat();
        UE_LOG(LogTemp, Error, TEXT("AlphaGame: forge boss spawn failed; encounter cannot complete."));
        return;
    }
    Boss->ConfigureForPlayers(RunPlayerCount);
    State->ForgeBoss = Boss;
    State->BossPhase = Boss->GetBossPhase();
    State->RemainingEnemies = 1;
    State->RoundState = EAlphaRoundState::Fighting;
}

void AAlphaGameMode::CheckRound()
{
    AAlphaGameState* State = GetGameState<AAlphaGameState>();
    if (!State) return;
    if (State->RoundState != EAlphaRoundState::Fighting && State->RoundState != EAlphaRoundState::Intermission) return;
    int32 LivingPlayers = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* Player = It->Get();
        AAlphaCharacter* Character = Player ? Cast<AAlphaCharacter>(Player->GetPawn()) : nullptr;
        if (Character && Character->GetCombatComponent() && Character->GetCombatComponent()->IsAlive()) ++LivingPlayers;
    }
    if (LivingPlayers == 0 && GetNumPlayers() > 0)
    {
        MarkDefeat();
        return;
    }
    if (State->EncounterStage == EAlphaEncounterStage::Boss)
    {
        if (State->RoundState == EAlphaRoundState::Intermission) return;
        if (!IsValid(State->ForgeBoss))
        {
            // Missing/destroyed actors and failed spawns are never a victory.
            MarkDefeat();
            return;
        }
        State->BossPhase = State->ForgeBoss->GetBossPhase();
        if (!State->ForgeBoss->GetCombatComponent()->IsAlive())
        {
            State->RemainingEnemies = 0;
            State->RoundState = EAlphaRoundState::Victory;
            State->EncounterStage = EAlphaEncounterStage::Finished;
        }
        return;
    }
    int32 LivingEnemies = 0;
    for (const TWeakObjectPtr<AAlphaEnemy>& Enemy : WaveEnemies)
    {
        if (Enemy.IsValid() && Enemy->GetCombatComponent() && Enemy->GetCombatComponent()->IsAlive()) ++LivingEnemies;
    }
    State->RemainingEnemies = LivingEnemies;
    if (LivingEnemies == 0 && State->RoundState == EAlphaRoundState::Fighting)
    {
        State->RoundState = EAlphaRoundState::Intermission;
        if (State->Wave >= AAlphaGameState::TotalWaves)
        {
            State->EncounterStage = EAlphaEncounterStage::Boss;
            GetWorldTimerManager().SetTimer(WaveTimer, this, &AAlphaGameMode::StartBossEncounter, 3.f, false);
        }
        else
        {
            GetWorldTimerManager().SetTimer(WaveTimer, this, &AAlphaGameMode::StartNextWave, 3.f, false);
        }
    }
}

void AAlphaGameMode::RestartPrototypeRound()
{
    if (!HasAuthority()) return;
    GetWorldTimerManager().ClearTimer(WaveTimer);
    ClearEncounterActors();
    if (AAlphaGameState* State = GetGameState<AAlphaGameState>())
    {
        State->Wave = 0;
        State->RemainingEnemies = 0;
        State->RoundState = EAlphaRoundState::Waiting;
        State->EncounterStage = EAlphaEncounterStage::Preparation;
        State->BossPhase = 0;
        State->EncounterPlayerCount = FMath::Clamp(GetNumPlayers(), 1, 3);
    }
    int32 Index = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* Player = It->Get();
        if (!Player) continue;
        // KillZ, destruction or a failed initial spawn can leave a controller
        // without a pawn. Recreate it before resetting the round.
        if (!Player->GetPawn()) RestartPlayer(Player);
        if (AAlphaCharacter* Character = Cast<AAlphaCharacter>(Player->GetPawn()))
        {
            Character->ResetForRound();
            Character->TeleportTo(FVector(-650 + Index * 180, -100 + Index * 100, 110), FRotator::ZeroRotator);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("AlphaGame: could not restore a player pawn during round restart."));
        }
        ++Index;
    }
    EnsureRoundMonitor();
}
