#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Characters/AlphaCharacter.h"
#include "Characters/AlphaCompanion.h"
#include "Characters/AlphaEnemy.h"
#include "Combat/AlphaCombatComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "World/AlphaGameMode.h"
#include "World/AlphaGameState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaPartyLifecycleTest,
    "AlphaGame.Party.Authority.FillRolesScalingAndCleanup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaPartyLifecycleTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Settings;
    Settings.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
        .CreateAISystem(false).RequiresHitProxies(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
        true, ERHIFeatureLevel::Num, &Settings);
    if (!TestNotNull(TEXT("Authority world exists"), World)) return false;
    struct FWorldScope
    {
        UWorld* World;
        ~FWorldScope() { World->DestroyWorld(false); }
    } Scope{World};
    AAlphaGameMode* Mode = World->SpawnActor<AAlphaGameMode>();
    AAlphaGameState* State = World->SpawnActor<AAlphaGameState>();
    if (!TestNotNull(TEXT("GameMode exists"), Mode) || !TestNotNull(TEXT("GameState exists"), State)) return false;
    // Isolated authority fixture: no real network driver, game instance or map.
    // Friendship grants test wiring without a production state-injection API.
    Mode->GameState = State;
    World->SetGameState(State);
    const auto AddHuman = [World](int32 Slot, int32 Hero)
    {
        APlayerController* Controller = World->SpawnActor<APlayerController>();
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AAlphaCharacter* Character = World->SpawnActor<AAlphaCharacter>(AAlphaCharacter::StaticClass(),
            FVector(-650 + Slot * 180, -100 + Slot * 100, 110), FRotator::ZeroRotator, Params);
        if (Controller && Character)
        {
            Controller->Possess(Character);
            Character->SetArchetype(Hero);
        }
        return Controller;
    };
    const auto ControllerCount = [World]()
    {
        int32 Count = 0;
        for (TActorIterator<AAIController> It(World); It; ++It)
        {
            if (IsValid(*It) && !It->IsActorBeingDestroyed()) ++Count;
        }
        return Count;
    };
    APlayerController* First = AddHuman(0, 0);
    if (!TestNotNull(TEXT("First human controller exists"), First)
        // UE5.6 can return TObjectPtr here; explicit APawn allows its pointer conversion.
        || !TestNotNull<APawn>(TEXT("First human pawn exists"), First->GetPawn())) return false;
    const int32 OriginalPlayerStateCount = State->PlayerArray.Num();
    Mode->RefreshCompanions();
    TestEqual(TEXT("One human receives two companions"), State->CompanionCount, 2);
    TestEqual(TEXT("Replicated human count excludes AI"), State->HumanPlayerCount, 1);
    TestEqual(TEXT("Derived party count is three"), State->GetPartyCount(), 3);
    TestEqual(TEXT("Companions do not add PlayerState entries"), State->PlayerArray.Num(), OriginalPlayerStateCount);
    TestEqual(TEXT("Exactly two AI controllers are created"), ControllerCount(), 2);
    Mode->RefreshCompanions();
    TestEqual(TEXT("Repeated refresh does not accumulate AI controllers"), ControllerCount(), 2);
    TSet<int32> Heroes;
    Heroes.Add(CastChecked<AAlphaCharacter>(First->GetPawn())->GetArchetype());
    for (AAlphaCompanion* Bot : Mode->CompanionActors())
    {
        Heroes.Add(Bot->GetArchetype());
        TestTrue(TEXT("Spawned bot has authoritative possession"), Bot->IsCoopParticipant());
    }
    TestEqual(TEXT("A solo party fills the two missing roles"), Heroes.Num(), 3);
    CastChecked<AAlphaCharacter>(First->GetPawn())->SetArchetype(2);
    Mode->RefreshCompanions();
    Heroes.Reset();
    Heroes.Add(2);
    for (AAlphaCompanion* Bot : Mode->CompanionActors()) Heroes.Add(Bot->GetArchetype());
    TestEqual(TEXT("Human hero changes resynchronize missing roles"), Heroes.Num(), 3);
    TestTrue(TEXT("Fill can be disabled in preparation"), Mode->SetCompanionFillEnabled(false));
    TestEqual(TEXT("Disabled fill removes companions"), State->CompanionCount, 0);
    TestEqual(TEXT("Disabling fill destroys the AI controllers too"), ControllerCount(), 0);
    TestTrue(TEXT("Fill can be enabled again in preparation"), Mode->SetCompanionFillEnabled(true));
    APlayerController* Second = AddHuman(1, 0);
    if (!TestNotNull(TEXT("Second human exists"), Second)
        || !TestNotNull<APawn>(TEXT("Second human pawn exists"), Second->GetPawn())) return false;
    Mode->RefreshCompanions();
    TestEqual(TEXT("Second human replaces one companion"), State->CompanionCount, 1);
    TestEqual(TEXT("Two human seats are reported"), State->HumanPlayerCount, 2);
    APlayerController* Third = AddHuman(2, 1);
    if (!TestNotNull(TEXT("Third human exists"), Third)
        || !TestNotNull<APawn>(TEXT("Third human pawn exists"), Third->GetPawn())) return false;
    Mode->RefreshCompanions();
    TestEqual(TEXT("Three humans need no AI"), State->CompanionCount, 0);
    TestEqual(TEXT("All companion controllers were cleaned"), ControllerCount(), 0);
    if (APawn* Pawn = Third->GetPawn()) Pawn->Destroy();
    Third->Destroy();
    Mode->RefreshCompanions();
    TestEqual(TEXT("Returning to two humans fills the spare slot"), State->CompanionCount, 1);
    TestTrue(TEXT("Ready party starts an actual run"), Mode->StartPrototypeRun());
    TestEqual(TEXT("Run scaling locks the full party, including AI"), State->EncounterPlayerCount, 3);
    TestFalse(TEXT("Fill cannot be changed during combat"), Mode->SetCompanionFillEnabled(false));
    CastChecked<AAlphaCharacter>(First->GetPawn())->GetCombatComponent()->ReceiveHit(10000, 0, nullptr);
    CastChecked<AAlphaCharacter>(Second->GetPawn())->GetCombatComponent()->ReceiveHit(10000, 0, nullptr);
    Mode->CheckRound();
    TestFalse(TEXT("Living bot keeps the run alive while humans await rescue"), State->RoundState == EAlphaRoundState::Defeat);
    TArray<AAlphaCompanion*> Bots = Mode->CompanionActors();
    if (!TestEqual(TEXT("One bot is available for rescue"), Bots.Num(), 1)) return false;
    Bots[0]->GetCombatComponent()->ReceiveHit(10000, 0, nullptr);
    Mode->CheckRound();
    TestTrue(TEXT("A wholly downed party loses the run"), State->RoundState == EAlphaRoundState::Defeat);
    TestEqual(TEXT("Defeat retains the downed companion body"), Mode->CompanionActors().Num(), 1);
    Mode->RestartPrototypeRound();
    TestTrue(TEXT("Restart returns to preparation"), State->RoundState == EAlphaRoundState::Waiting);
    TestEqual(TEXT("Restart restores the full party"), State->GetPartyCount(), 3);
    TestEqual(TEXT("Restart reuses rather than accumulating controllers"), ControllerCount(), 1);
    Mode->ClearCompanions();
    TestEqual(TEXT("Explicit cleanup removes all companion pawns"), Mode->CompanionActors().Num(), 0);
    TestEqual(TEXT("Explicit cleanup removes all companion controllers"), ControllerCount(), 0);
    TestEqual(TEXT("Only human controllers occupy connection seats"), Mode->HumanControllers().Num(), 2);
    Mode->RefreshCompanions();
    TestEqual(TEXT("Preparation can refill after cleanup"), ControllerCount(), 1);
    Mode->Logout(Second);
    if (APawn* Pawn = Second->GetPawn()) Pawn->Destroy();
    Second->Destroy();
    Mode->RefreshCompanions();
    TestEqual(TEXT("One remaining human refills two bots in preparation"), State->CompanionCount, 2);
    TestTrue(TEXT("Solo plus bots starts a new run"), Mode->StartPrototypeRun());
    if (APawn* Pawn = First->GetPawn()) Pawn->Destroy();
    Mode->CheckRound();
    TestTrue(TEXT("A destroyed last human pawn ends the run instead of leaving anchorless bots"), State->RoundState == EAlphaRoundState::Defeat);
    Mode->Logout(First);
    First->Destroy();
    TestEqual(TEXT("Last human logout removes all companions"), Mode->CompanionActors().Num(), 0);
    TestEqual(TEXT("Last human logout removes all AI controllers"), ControllerCount(), 0);
    TestEqual(TEXT("Empty room resets replicated human count"), State->HumanPlayerCount, 0);
    return true;
}

#endif
