#include "World/AlphaGameMode.h"
#include "World/AlphaGameState.h"
#include "World/AlphaBreakableProp.h"
#include "World/AlphaStoryNPC.h"
#include "Rules/StoryRules.h"
#include "World/AlphaPlayerController.h"
#include "Characters/AlphaCharacter.h"
#include "Characters/AlphaEnemy.h"
#include "Characters/AlphaCompanion.h"
#include "Characters/Boss/AlphaForgeBoss.h"
#include "Rules/CombatRules.h"
#include "Rules/CompanionRules.h"
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
    if (ErrorMessage.IsEmpty() && HumanControllers().Num() >= 3)
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
    if (HumanControllers().Num() > 3)
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
    // Immediately free the slot's bot collision before the new human pawn
    // chooses a PlayerStart; creation/role assignment waits for possession.
    RefreshCompanions();
    EnsureRoundMonitor();
    // PostLogin does not guarantee that RestartPlayer/possession has completed.
    GetWorldTimerManager().SetTimerForNextTick(this, &AAlphaGameMode::RefreshCompanions);
}

void AAlphaGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);
    if (HumanControllers(Exiting).IsEmpty())
    {
        GetWorldTimerManager().ClearTimer(WaveTimer);
        GetWorldTimerManager().ClearTimer(MonitorTimer);
        ClearEncounterActors();
        ClearCompanions();
        if (AAlphaGameState* State = GetGameState<AAlphaGameState>())
        {
            State->Wave = 0;
            State->RemainingEnemies = 0;
            State->BossPhase = 0;
            State->HumanPlayerCount = 0;
            State->CompanionCount = 0;
            State->RoundState = EAlphaRoundState::Waiting;
            State->StoryStage = EAlphaStoryStage::Arrival;
            State->EncounterStage = EAlphaEncounterStage::Preparation;
        }
    }
    else
    {
        GetWorldTimerManager().SetTimerForNextTick(this, &AAlphaGameMode::RefreshCompanions);
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

TArray<APlayerController*> AAlphaGameMode::HumanControllers(const AController* Ignore) const
{
    TArray<APlayerController*> Result;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* Player = It->Get();
        if (IsValid(Player) && Player != Ignore && !Player->IsActorBeingDestroyed()) Result.Add(Player);
    }
    return Result;
}

TArray<AAlphaCompanion*> AAlphaGameMode::CompanionActors() const
{
    TArray<AAlphaCompanion*> Result;
    for (TActorIterator<AAlphaCompanion> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && !It->IsActorBeingDestroyed()) Result.Add(*It);
    }
    return Result;
}

void AAlphaGameMode::UpdatePartyCounts()
{
    if (AAlphaGameState* State = GetGameState<AAlphaGameState>())
    {
        State->HumanPlayerCount = HumanControllers().Num();
        State->CompanionCount = CompanionActors().Num();
        State->bCompanionFillEnabled = bCompanionFillEnabled;
    }
}

void AAlphaGameMode::DestroyCompanion(AAlphaCompanion* Companion)
{
    if (!IsValid(Companion)) return;
    AController* Controller = Companion->GetController();
    // AAIController never occupies a human login slot or owns a PlayerState.
    if (Controller && !Controller->IsPlayerController()) Controller->UnPossess();
    Companion->Destroy();
    if (IsValid(Controller) && !Controller->IsPlayerController()) Controller->Destroy();
}

void AAlphaGameMode::ClearCompanions()
{
    for (AAlphaCompanion* Companion : CompanionActors()) DestroyCompanion(Companion);
    UpdatePartyCounts();
}

bool AAlphaGameMode::SetCompanionFillEnabled(bool bEnabled)
{
    AAlphaGameState* State = GetGameState<AAlphaGameState>();
    if (!HasAuthority() || !State || State->RoundState != EAlphaRoundState::Waiting) return false;
    bCompanionFillEnabled = bEnabled;
    RefreshCompanions();
    return true;
}

void AAlphaGameMode::RefreshCompanions()
{
    AAlphaGameState* State = GetGameState<AAlphaGameState>();
    if (!HasAuthority() || !State) return;
    UpdatePartyCounts();
    const bool bWaiting = State->RoundState == EAlphaRoundState::Waiting;
    const bool bTerminal = State->RoundState == EAlphaRoundState::Victory || State->RoundState == EAlphaRoundState::Defeat;
    if (!bWaiting && !bTerminal) return; // Active encounter population is locked.
    const TArray<APlayerController*> Humans = HumanControllers();
    const FVector ChapterOrigin = AAlphaGameState::GetChapterOrigin(State->ChapterIndex);
    // A late-joining pawn can be possessed after PostLogin, and Entry's initial
    // PlayerStart may still be used. Correct its chapter before admission ends.
    for (APlayerController* Human : Humans)
    {
        if (AAlphaCharacter* Character = Cast<AAlphaCharacter>(Human->GetPawn()))
        {
            if (FMath::Abs(Character->GetActorLocation().X - ChapterOrigin.X) > 1500.f)
                Character->TeleportTo(ChapterOrigin + FVector(-650, -100, 110), FRotator::ZeroRotator, false, true);
        }
    }
    const int32 Desired = AlphaCompanionRules::MissingCompanions(Humans.Num(), bCompanionFillEnabled);
    TArray<AAlphaCompanion*> Bots = CompanionActors();
    while (Bots.Num() > Desired)
    {
        DestroyCompanion(Bots.Pop());
    }
    // Result screens may admit humans, but never add or revive bots. Downed
    // bodies remain for results unless a new human needs their party slot.
    if (!bWaiting)
    {
        UpdatePartyCounts();
        return;
    }
    int32 HeroCounts[3] = {0, 0, 0};
    for (APlayerController* Human : Humans)
    {
        const AAlphaCharacter* Character = Cast<AAlphaCharacter>(Human->GetPawn());
        if (!Character || !Character->IsCoopParticipant() || !Character->GetCombatComponent()->IsAlive())
        {
            UpdatePartyCounts();
            return; // Wait for possession or an explicit round reset.
        }
        ++HeroCounts[FMath::Clamp(Character->GetArchetype(), 0, 2)];
    }
    while (Bots.Num() < Desired)
    {
        const int32 Slot = Humans.Num() + Bots.Num();
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        AAlphaCompanion* Bot = GetWorld()->SpawnActor<AAlphaCompanion>(AAlphaCompanion::StaticClass(),
            AAlphaGameState::GetChapterOrigin(State->ChapterIndex) + FVector(-650 + Slot * 180, -100 + Slot * 100, 110), FRotator::ZeroRotator, Params);
        if (!Bot)
        {
            UE_LOG(LogTemp, Warning, TEXT("AlphaGame: companion fill incomplete; spawn failed."));
            break;
        }
        Bots.Add(Bot);
    }
    for (AAlphaCompanion* Bot : Bots)
    {
        const int32 Hero = AlphaCompanionRules::ChooseHero(HeroCounts[0], HeroCounts[1], HeroCounts[2]);
        ++HeroCounts[Hero];
        if (Bot->GetArchetype() != Hero) Bot->SetArchetype(Hero);
        if (!Bot->GetController()) Bot->SpawnDefaultController();
    }
    UpdatePartyCounts();
}

bool AAlphaGameMode::StartPrototypeRun()
{
    AAlphaGameState* State = GetGameState<AAlphaGameState>();
    if (!HasAuthority() || !State || !AlphaStoryRules::CanStart(State->ChapterIndex,
        static_cast<AlphaStoryRules::Stage>(State->StoryStage), State->RoundState == EAlphaRoundState::Waiting)) return false;
    RefreshCompanions();
    const TArray<APlayerController*> Humans = HumanControllers();
    const TArray<AAlphaCompanion*> Bots = CompanionActors();
    if (Humans.IsEmpty() || Humans.Num() > 3
        || Bots.Num() != AlphaCompanionRules::MissingCompanions(Humans.Num(), bCompanionFillEnabled)) return false;
    for (APlayerController* Human : Humans)
    {
        AAlphaCharacter* Character = Cast<AAlphaCharacter>(Human->GetPawn());
        if (!Character || !Character->IsCoopParticipant() || !Character->GetCombatComponent()->IsAlive()) return false;
    }
    for (AAlphaCompanion* Bot : Bots)
    {
        if (!Bot->IsCoopParticipant() || !Bot->GetCombatComponent()->IsAlive()) return false;
    }
    // Capture the whole ready party once. Disconnects never heal a live boss.
    RunPlayerCount = Humans.Num() + Bots.Num();
    State->EncounterPlayerCount = RunPlayerCount;
    State->EncounterStage = EAlphaEncounterStage::Waves;
    State->StoryStage = EAlphaStoryStage::Combat;
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
    const auto DestroyEnemy = [](APawn* Enemy)
    {
        AController* Controller = Enemy->GetController();
        if (Controller && !Controller->IsPlayerController()) Controller->UnPossess();
        Enemy->Destroy();
        if (IsValid(Controller) && !Controller->IsPlayerController()) Controller->Destroy();
    };
    for (TActorIterator<AAlphaEnemy> It(GetWorld()); It; ++It) DestroyEnemy(*It);
    for (TActorIterator<AAlphaForgeBoss> It(GetWorld()); It; ++It) DestroyEnemy(*It);
    WaveEnemies.Reset();
    if (AAlphaGameState* State = GetGameState<AAlphaGameState>()) State->ForgeBoss = nullptr;
}

void AAlphaGameMode::StartNextWave()
{
    AAlphaGameState* State = GetGameState<AAlphaGameState>();
    if (!State || HumanControllers().IsEmpty()) return;
    if (State->RoundState != EAlphaRoundState::Waiting && State->RoundState != EAlphaRoundState::Intermission) return;
    if (State->EncounterStage != EAlphaEncounterStage::Waves || State->Wave >= State->GetChapterWaveCount()) return;
    State->Wave++;
    State->RoundState = EAlphaRoundState::Fighting;
    WaveEnemies.Reset();
    const int32 Count = AlphaRules::WaveEnemyCount(State->Wave, RunPlayerCount);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        const FVector Location = AAlphaGameState::GetChapterOrigin(State->ChapterIndex)
            + FVector(650.f + (Index % 2) * 180.f, -240.f + (Index % 4) * 150.f, 110.f);
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
    if (!State || HumanControllers().IsEmpty() || State->RoundState != EAlphaRoundState::Intermission) return;
    if (State->EncounterStage != EAlphaEncounterStage::Boss || !State->ChapterHasBoss()) return;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    AAlphaForgeBoss* Boss = GetWorld()->SpawnActor<AAlphaForgeBoss>(AAlphaForgeBoss::StaticClass(), AAlphaGameState::GetChapterOrigin(State->ChapterIndex) + FVector(800, 0, 150), FRotator(0, 180, 0), Params);
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
    RefreshCompanions();
    if (State->RoundState != EAlphaRoundState::Fighting && State->RoundState != EAlphaRoundState::Intermission) return;
    const TArray<APlayerController*> Humans = HumanControllers();
    bool bHasHumanPawn = false;
    for (APlayerController* Human : Humans)
    {
        const AAlphaCharacter* Character = Cast<AAlphaCharacter>(Human->GetPawn());
        if (IsValid(Character) && !Character->IsActorBeingDestroyed() && Character->IsCoopParticipant())
        {
            bHasHumanPawn = true;
            break;
        }
    }
    if (!Humans.IsEmpty() && !bHasHumanPawn)
    {
        // A downed pawn is still a participant and can be rescued. A destroyed
        // pawn is not: bots have no human anchor and would otherwise idle forever.
        MarkDefeat();
        return;
    }
    int32 LivingPlayers = 0;
    for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
    {
        if (It->IsCoopParticipant() && It->GetCombatComponent()->IsAlive()) ++LivingPlayers;
    }
    if (LivingPlayers == 0 && !HumanControllers().IsEmpty())
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
            MarkChapterVictory();
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
        if (State->Wave >= State->GetChapterWaveCount())
        {
            if (State->ChapterHasBoss())
            {
                State->EncounterStage = EAlphaEncounterStage::Boss;
                GetWorldTimerManager().SetTimer(WaveTimer, this, &AAlphaGameMode::StartBossEncounter, 3.f, false);
            }
            else MarkChapterVictory();
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
    const AAlphaGameState* CurrentState = GetGameState<AAlphaGameState>();
    const FVector ChapterOrigin = AAlphaGameState::GetChapterOrigin(CurrentState ? CurrentState->ChapterIndex : 0);
    GetWorldTimerManager().ClearTimer(WaveTimer);
    ClearEncounterActors();
    for (TActorIterator<AAlphaBreakableProp> It(GetWorld()); It; ++It) It->ResetForRound();
    if (AAlphaGameState* State = GetGameState<AAlphaGameState>())
    {
        State->Wave = 0;
        State->RemainingEnemies = 0;
        State->RoundState = EAlphaRoundState::Waiting;
        State->StoryRevision = State->StoryRevision == MAX_int32 ? 0 : State->StoryRevision + 1;
        State->StoryStage = EAlphaStoryStage::Arrival;
        State->EncounterStage = EAlphaEncounterStage::Preparation;
        State->BossPhase = 0;
        State->EncounterPlayerCount = FMath::Clamp(HumanControllers().Num(), 1, 3);
    }
    int32 StartIndex = 0;
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        It->SetActorLocation(ChapterOrigin + FVector(-650 + (StartIndex % 3) * 180, -100 + (StartIndex % 3) * 100, 110));
        ++StartIndex;
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
            Character->TeleportTo(ChapterOrigin + FVector(-650 + Index * 180, -100 + Index * 100, 110), FRotator::ZeroRotator, false, true);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("AlphaGame: could not restore a player pawn during round restart."));
        }
        ++Index;
    }
    for (AAlphaCompanion* Bot : CompanionActors())
    {
        Bot->ResetForRound();
        if (!Bot->GetController()) Bot->SpawnDefaultController();
        Bot->TeleportTo(ChapterOrigin + FVector(-650 + Index * 180, -100 + Index * 100, 110), FRotator::ZeroRotator, false, true);
        Bot->ResetCompanionBrain();
        ++Index;
    }
    RefreshCompanions();
    if (AAlphaGameState* State = GetGameState<AAlphaGameState>()) State->EncounterPlayerCount = State->GetPartyCount();
    EnsureRoundMonitor();
}

void AAlphaGameMode::MarkChapterVictory()
{
    GetWorldTimerManager().ClearTimer(WaveTimer);
    if (AAlphaGameState* State = GetGameState<AAlphaGameState>())
    {
        State->RemainingEnemies = 0;
        State->RoundState = EAlphaRoundState::Victory;
        State->EncounterStage = EAlphaEncounterStage::Finished;
        State->StoryStage = EAlphaStoryStage::Debrief;
        State->ForceNetUpdate();
    }
}

bool AAlphaGameMode::TalkToStoryNPC(AAlphaCharacter* Speaker)
{
    AAlphaGameState* State = GetGameState<AAlphaGameState>();
    if (!HasAuthority() || !State || !IsValid(Speaker) || Speaker->GetWorld() != GetWorld()) return false;
    const bool bSafeRound = State->RoundState == EAlphaRoundState::Waiting || State->RoundState == EAlphaRoundState::Victory;
    bool bEligible = false;
    for (TActorIterator<AAlphaStoryNPC> It(GetWorld()); It; ++It)
    {
        if (It->ChapterIndex == State->ChapterIndex && It->CanTalk(Speaker))
        {
            bEligible = true;
            break;
        }
    }
    if (!AlphaStoryRules::CanTalk(State->ChapterIndex, static_cast<AlphaStoryRules::Stage>(State->StoryStage), bEligible, bSafeRound)) return false;
    State->StoryStage = static_cast<EAlphaStoryStage>(AlphaStoryRules::AfterTalk(static_cast<AlphaStoryRules::Stage>(State->StoryStage)));
    State->ForceNetUpdate();
    return true;
}

bool AAlphaGameMode::AdvanceChapter()
{
    AAlphaGameState* State = GetGameState<AAlphaGameState>();
    if (!HasAuthority() || !State || !AlphaStoryRules::CanAdvance(State->ChapterIndex,
        static_cast<AlphaStoryRules::Stage>(State->StoryStage), State->RoundState == EAlphaRoundState::Victory)) return false;
    ++State->ChapterIndex;
    RestartPrototypeRound();
    State->ForceNetUpdate();
    return true;
}
