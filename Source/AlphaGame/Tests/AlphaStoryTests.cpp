#if WITH_DEV_AUTOMATION_TESTS

#include "World/AlphaGameMode.h"
#include "World/AlphaGameState.h"
#include "World/AlphaStoryNPC.h"
#include "Characters/AlphaCharacter.h"
#include "Characters/AlphaEnemy.h"
#include "Characters/Boss/AlphaForgeBoss.h"
#include "Combat/AlphaCombatComponent.h"
#include "Rules/LevelLayoutRules.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaStoryAuthorityTest,
    "AlphaGame.Story.Authority.BriefingVictoryAndChapterTravel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaStoryAuthorityTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Settings;
    Settings.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
        .CreateAISystem(false).RequiresHitProxies(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
        true, ERHIFeatureLevel::Num, &Settings);
    if (!TestNotNull(TEXT("Authority world exists"), World)) return false;
    struct FWorldScope { UWorld* World; ~FWorldScope() { World->DestroyWorld(false); } } Scope{World};
    AAlphaGameMode* Mode = World->SpawnActor<AAlphaGameMode>();
    AAlphaGameState* State = World->SpawnActor<AAlphaGameState>();
    APlayerController* Player = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("GameMode exists"), Mode) || !TestNotNull(TEXT("GameState exists"), State)
        || !TestNotNull(TEXT("Player exists"), Player)) return false;
    Mode->GameState = State;
    World->SetGameState(State);
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AAlphaCharacter* Hero = World->SpawnActor<AAlphaCharacter>(AAlphaCharacter::StaticClass(),
        FVector(-800, 0, 96), FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Human hero exists"), Hero)) return false;
    Player->Possess(Hero);
    Hero->DispatchBeginPlay();
    Mode->SetCompanionFillEnabled(false); // The separate party fixture verifies AI fill.
    for (int32 Chapter = 0; Chapter < 3; ++Chapter)
    {
        const FTransform Transform(FRotator::ZeroRotator,
            AAlphaGameState::GetChapterOrigin(Chapter) + AAlphaStoryNPC::GetLocalPosition());
        AAlphaStoryNPC* NPC = World->SpawnActorDeferred<AAlphaStoryNPC>(AAlphaStoryNPC::StaticClass(), Transform);
        if (!TestNotNull(TEXT("Chapter NPC exists"), NPC)) return false;
        NPC->ChapterIndex = Chapter;
        NPC->FinishSpawning(Transform);
    }
    TestFalse(TEXT("Remote story call without speaker is rejected"), Mode->TalkToStoryNPC(nullptr));
    TestFalse(TEXT("Starting before the briefing is rejected"), Mode->StartPrototypeRun());
    for (int32 Chapter = 0; Chapter < 3; ++Chapter)
    {
        TestEqual(TEXT("Chapter order cannot skip"), State->ChapterIndex, Chapter);
        TestTrue(TEXT("New chapter begins at arrival"), State->StoryStage == EAlphaStoryStage::Arrival);
        TestFalse(TEXT("Arrival cannot advance to another chapter"), Mode->AdvanceChapter());
        Hero->SetActorLocation(AAlphaGameState::GetChapterOrigin(Chapter) + FVector(-800, 0, 96));
        TestTrue(TEXT("Nearby NPC briefing readies the chapter"), Mode->TalkToStoryNPC(Hero));
        TestFalse(TEXT("Repeated briefing cannot skip combat"), Mode->TalkToStoryNPC(Hero));
        if (!TestTrue(TEXT("Briefed human can begin encounters"), Mode->StartPrototypeRun())) return false;
        TestTrue(TEXT("Starting moves story into combat"), State->StoryStage == EAlphaStoryStage::Combat);
        TestTrue(TEXT("Starting marks a real route objective before spawning"), State->bAwaitingRouteAdvance);
        Mode->RouteReadyAt = 0.; // Skip only the presentation pause, retaining spatial validation.
        Mode->CheckRound();
        TestEqual(TEXT("Remaining near the guide does not spawn a remote encounter"), State->Wave, 0);
        const auto MoveToEncounter = [&]()
        {
            Hero->SetActorLocation(State->RouteObjective + FVector(0, 0, 110));
            Mode->RouteReadyAt = 0.;
            Mode->CheckRound();
        };
        MoveToEncounter();
        TestFalse(TEXT("A living human at the objective activates the encounter"), State->bAwaitingRouteAdvance);
        TestFalse(TEXT("Combat cannot accept story progression"), Mode->TalkToStoryNPC(Hero));
        TestFalse(TEXT("Combat cannot advance a chapter"), Mode->AdvanceChapter());
        for (int32 Wave = 1; Wave <= State->GetChapterWaveCount(); ++Wave)
        {
            TestEqual(TEXT("Expected chapter wave has spawned"), State->Wave, Wave);
            for (TActorIterator<AAlphaEnemy> It(World); It; ++It)
                It->GetCombatComponent()->ReceiveHit(10000.f, 0.f, nullptr);
            Mode->CheckRound();
            // Exercise authoritative transition methods without advancing the
            // global engine frame or relying on a real-time three-second wait.
            World->GetTimerManager().ClearTimer(Mode->WaveTimer);
            if (Wave < State->GetChapterWaveCount())
            {
                TestTrue(TEXT("A cleared wave queues a regroup objective"), State->bAwaitingRouteAdvance);
                MoveToEncounter();
            }
        }
        if (Chapter == 1)
        {
            TestTrue(TEXT("Forge reserves its boss after the third wave"), State->EncounterStage == EAlphaEncounterStage::Boss);
            MoveToEncounter();
            if (!TestNotNull<AAlphaForgeBoss>(TEXT("Forge boss spawned"), State->ForgeBoss)) return false;
            TestTrue(TEXT("Boss uses the current chapter origin"), State->ForgeBoss->GetActorLocation().X > 6000.f);
            State->ForgeBoss->GetCombatComponent()->ReceiveHit(10000.f, 0.f, nullptr);
            Mode->CheckRound();
        }
        else TestFalse(TEXT("Non-forge chapter does not spawn a duplicate forge boss"), IsValid(State->ForgeBoss));
        TestTrue(TEXT("Victory awaits an NPC debrief"), State->RoundState == EAlphaRoundState::Victory
            && State->StoryStage == EAlphaStoryStage::Debrief);
        TestFalse(TEXT("Victory alone cannot skip the debrief"), Mode->AdvanceChapter());
        Hero->SetActorLocation(AAlphaGameState::GetChapterOrigin(Chapter) + FVector(-800, 0, 96));
        TestTrue(TEXT("Debrief completes the chapter"), Mode->TalkToStoryNPC(Hero));
        TestTrue(TEXT("Completion is shared state"), State->StoryStage == EAlphaStoryStage::Complete);
        if (Chapter < 2)
        {
            TestTrue(TEXT("Host advances a completed chapter"), Mode->AdvanceChapter());
            TestTrue(TEXT("Party is moved to the next chapter"), FMath::Abs(Hero->GetActorLocation().X
                - AAlphaGameState::GetChapterOrigin(Chapter + 1).X) < 1000.f);
        }
        else TestFalse(TEXT("Final ending cannot create a fourth chapter"), Mode->AdvanceChapter());
    }
    const int32 PreviousRevision = State->StoryRevision;
    Mode->RestartPrototypeRound();
    TestEqual(TEXT("Retry invalidates old conversation snapshots"), State->StoryRevision, PreviousRevision + 1);
    TestEqual(TEXT("Retry preserves current chapter"), State->ChapterIndex, 2);
    TestTrue(TEXT("Retry requires a fresh briefing"), State->StoryStage == EAlphaStoryStage::Arrival);
    TestTrue(TEXT("Retry returns to preparation"), State->RoundState == EAlphaRoundState::Waiting);
    TestEqual(TEXT("Retry clears wave count"), State->Wave, 0);
    return true;
}

#endif
