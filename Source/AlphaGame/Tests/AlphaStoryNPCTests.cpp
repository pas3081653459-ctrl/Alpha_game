#if WITH_DEV_AUTOMATION_TESTS

#include "World/AlphaStoryNPC.h"
#include "World/AlphaGameState.h"
#include "Characters/AlphaCharacter.h"
#include "Combat/AlphaCombatComponent.h"
#include "AIController.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaStoryNPCEligibilityTest,
    "AlphaGame.Story.NPC.ProximityObstructionAndHumanAuthority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaStoryNPCEligibilityTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Settings;
    Settings.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
        .CreateAISystem(false).RequiresHitProxies(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
        true, ERHIFeatureLevel::Num, &Settings);
    if (!TestNotNull(TEXT("Authority world exists"), World)) return false;
    struct FScope { UWorld* World; ~FScope() { World->DestroyWorld(false); } } Scope{World};
    AAlphaGameState* State = World->SpawnActor<AAlphaGameState>();
    AAlphaStoryNPC* NPC = World->SpawnActor<AAlphaStoryNPC>();
    APlayerController* Player = World->SpawnActor<APlayerController>();
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AAlphaCharacter* Hero = World->SpawnActor<AAlphaCharacter>(AAlphaCharacter::StaticClass(),
        FVector(150, 0, 88), FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Story state exists"), State) || !TestNotNull(TEXT("NPC exists"), NPC)
        || !TestNotNull(TEXT("Human controller exists"), Player) || !TestNotNull(TEXT("Hero exists"), Hero)) return false;
    World->SetGameState(State);
    Player->Possess(Hero);
    Hero->GetCombatComponent()->ConfigureVitals(100.f, 100.f);
    TestTrue(TEXT("Living nearby human can talk"), NPC->CanTalk(Hero));
    TestFalse(TEXT("Missing speaker is rejected"), NPC->CanTalk(nullptr));
    TestNull(TEXT("Story NPC is not a combat target"), NPC->FindComponentByClass<UAlphaCombatComponent>());
    Hero->SetActorLocation(FVector(241, 0, 88));
    TestFalse(TEXT("Out-of-range speaker is rejected"), NPC->CanTalk(Hero));
    Hero->SetActorLocation(FVector(150, 0, 161));
    TestFalse(TEXT("Speaker above interaction height is rejected"), NPC->CanTalk(Hero));
    Hero->SetActorLocation(FVector(150, 0, 88));
    State->ChapterIndex = 1;
    TestFalse(TEXT("A different chapter's NPC cannot advance story"), NPC->CanTalk(Hero));
    State->ChapterIndex = 0;

    AActor* Wall = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Wall actor exists"), Wall)) return false;
    UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
    Wall->AddInstanceComponent(Box);
    Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(10, 50, 100));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionObjectType(ECC_WorldStatic);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->RegisterComponent();
    Wall->SetActorLocation(FVector(75, 0, 100));
    TestFalse(TEXT("Solid obstruction prevents conversation"), NPC->CanTalk(Hero));
    Wall->Destroy();
    TestTrue(TEXT("Removing obstruction restores interaction"), NPC->CanTalk(Hero));
    Player->UnPossess();
    TestFalse(TEXT("Unpossessed pawn cannot trigger story"), NPC->CanTalk(Hero));
    AAIController* AI = World->SpawnActor<AAIController>();
    if (!TestNotNull(TEXT("AI controller exists"), AI)) return false;
    AI->Possess(Hero);
    TestFalse(TEXT("AI cannot consume human story progression"), NPC->CanTalk(Hero));
    AI->UnPossess();
    Player->Possess(Hero);
    Hero->GetCombatComponent()->ReceiveHit(1000.f, 0.f, nullptr);
    TestFalse(TEXT("Downed human cannot talk"), NPC->CanTalk(Hero));
    TestTrue(TEXT("Unknown chapter has no dialogue"), AAlphaStoryNPC::GetDialogue(3, 0).IsEmpty());
    TestTrue(TEXT("Out-of-range page has no dialogue"), AAlphaStoryNPC::GetDialogue(0, 3).IsEmpty());
    TestTrue(TEXT("Debrief has different chapter-specific dialogue"),
        AAlphaStoryNPC::GetDialogue(1, 0).ToString() != AAlphaStoryNPC::GetDialogue(1, 0, true).ToString());
    return true;
}

#endif
