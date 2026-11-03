#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AlphaCharacter.h"
#include "Combat/AlphaCombatComponent.h"
#include "Coop/AlphaReviveComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

namespace AlphaCoopTests
{
class FCoopWorld
{
public:
    FCoopWorld()
    {
        UWorld::InitializationValues Settings;
        Settings.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
            .CreateAISystem(false).RequiresHitProxies(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
            true, ERHIFeatureLevel::Num, &Settings);
        if (!World) return;
        DamageSource = World->SpawnActor<AActor>();
        if (!DamageSource) return;
        UAlphaCombatComponent* EnemyCombat = NewObject<UAlphaCombatComponent>(DamageSource);
        DamageSource->AddInstanceComponent(EnemyCombat);
        EnemyCombat->TeamId = 2;
        EnemyCombat->RegisterComponent();
        EnemyCombat->ResetVitals();
    }

    ~FCoopWorld()
    {
        if (World) World->DestroyWorld(false);
    }

    AAlphaCharacter* SpawnPlayer(FVector Location)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AAlphaCharacter* Character = World->SpawnActor<AAlphaCharacter>(AAlphaCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
        APlayerController* Controller = World->SpawnActor<APlayerController>();
        if (!Character || !Controller) return nullptr;
        Controller->Possess(Character);
        // Dispatch the real actor/component BeginPlay without introducing a round game mode.
        Character->DispatchBeginPlay();
        return Character;
    }

    void Down(AAlphaCharacter* Character) const
    {
        Character->GetCombatComponent()->ReceiveHit(100000.0f, 0.0f, DamageSource);
    }

    void Step(UAlphaReviveComponent* Revive, float Seconds) const
    {
        Revive->TickComponent(Seconds, LEVELTICK_All, nullptr);
    }

    AActor* AddWall(FVector Location) const
    {
        AActor* Wall = World->SpawnActor<AActor>();
        if (!Wall) return nullptr;
        UBoxComponent* Collision = NewObject<UBoxComponent>(Wall);
        Wall->SetRootComponent(Collision);
        Wall->AddInstanceComponent(Collision);
        Collision->SetBoxExtent(FVector(10.0f, 100.0f, 100.0f));
        Collision->SetCollisionProfileName(TEXT("BlockAll"));
        Collision->RegisterComponent();
        Wall->SetActorLocation(Location);
        return Wall;
    }

    UWorld* World = nullptr;
    AActor* DamageSource = nullptr;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaReviveCompletionTest,
    "AlphaGame.Coop.Authority.CompletionAndRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaReviveCompletionTest::RunTest(const FString& Parameters)
{
    AlphaCoopTests::FCoopWorld Fixture;
    AAlphaCharacter* Helper = Fixture.SpawnPlayer(FVector(0.0f, 0.0f, 100.0f));
    AAlphaCharacter* Target = Fixture.SpawnPlayer(FVector(120.0f, 0.0f, 100.0f));
    if (!TestNotNull(TEXT("Helper exists"), Helper) || !TestNotNull(TEXT("Target exists"), Target)
        || !TestNotNull(TEXT("Damage source exists"), Fixture.DamageSource)) return false;
    Fixture.Down(Target);
    TestTrue(TEXT("Zero health is downed, not alive"), Target->IsDefeated() && !Target->GetCombatComponent()->IsAlive());
    TestEqual(TEXT("First down recorded"), Target->GetReviveComponent()->GetDownCount(), 1);
    Helper->GetReviveComponent()->BeginRevive();
    TestTrue(TEXT("Nearest downed ally claimed"), Helper->GetReviveComponent()->GetReviveTarget() == Target);
    TestTrue(TEXT("Target exposes its rescuer"), Target->GetReviveComponent()->GetCurrentRescuer() == Helper);
    Fixture.Step(Helper->GetReviveComponent(), 1.0f);
    TestTrue(TEXT("One second is partial progress"), FMath::IsNearlyEqual(Helper->GetReviveComponent()->GetReviveProgress(), 1.0f / 3.0f));
    TestFalse(TEXT("Partial progress does not heal"), Target->GetCombatComponent()->IsAlive());
    Fixture.Step(Helper->GetReviveComponent(), 2.0f);
    TestTrue(TEXT("Three seconds restores living state"), Target->GetCombatComponent()->IsAlive());
    TestFalse(TEXT("Character downed state clears explicitly"), Target->IsDefeated());
    TestEqual(TEXT("Revival restores 40 percent health"), Target->GetCombatComponent()->GetHealth(), Target->GetCombatComponent()->GetMaxHealth() * 0.4f);
    TestTrue(TEXT("Movement restored"), Target->GetCharacterMovement()->MovementMode == MOVE_Walking);
    TestNull(TEXT("Completed claim released"), Target->GetReviveComponent()->GetCurrentRescuer());
    TestNull(TEXT("Helper target clears"), Helper->GetReviveComponent()->GetReviveTarget());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaReviveInterruptionTest,
    "AlphaGame.Coop.Authority.ExclusiveClaimAndDamageInterruption",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaReviveInterruptionTest::RunTest(const FString& Parameters)
{
    AlphaCoopTests::FCoopWorld Fixture;
    AAlphaCharacter* Helper = Fixture.SpawnPlayer(FVector(0.0f, 0.0f, 100.0f));
    AAlphaCharacter* OtherHelper = Fixture.SpawnPlayer(FVector(0.0f, 100.0f, 100.0f));
    AAlphaCharacter* Target = Fixture.SpawnPlayer(FVector(120.0f, 0.0f, 100.0f));
    if (!TestNotNull(TEXT("Helper exists"), Helper) || !TestNotNull(TEXT("Second helper exists"), OtherHelper)
        || !TestNotNull(TEXT("Target exists"), Target) || !TestNotNull(TEXT("Damage source exists"), Fixture.DamageSource)) return false;
    Fixture.Down(Target);
    Helper->GetReviveComponent()->BeginRevive();
    OtherHelper->GetReviveComponent()->BeginRevive();
    TestNull(TEXT("Second helper cannot take existing claim"), OtherHelper->GetReviveComponent()->GetReviveTarget());
    OtherHelper->GetReviveComponent()->EndRevive();
    Fixture.Step(Helper->GetReviveComponent(), 2.0f);
    Helper->GetCombatComponent()->ReceiveHit(10.0f, 0.0f, Fixture.DamageSource);
    Fixture.Step(Helper->GetReviveComponent(), 1.0f);
    TestNull(TEXT("Damage interrupts and releases claim"), Target->GetReviveComponent()->GetCurrentRescuer());
    TestEqual(TEXT("Interruption loses partial progress"), Helper->GetReviveComponent()->GetReviveProgress(), 0.0f);
    TestFalse(TEXT("Damage cannot be bypassed at completion"), Target->GetCombatComponent()->IsAlive());
    OtherHelper->GetReviveComponent()->BeginRevive();
    Fixture.Step(OtherHelper->GetReviveComponent(), 1.0f);
    OtherHelper->GetReviveComponent()->EndRevive();
    TestNull(TEXT("Releasing interact clears claim"), Target->GetReviveComponent()->GetCurrentRescuer());
    TestEqual(TEXT("Release resets progress"), OtherHelper->GetReviveComponent()->GetReviveProgress(), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaReviveRangeDisconnectResetTest,
    "AlphaGame.Coop.Authority.RangeDisconnectAndRoundReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaReviveRangeDisconnectResetTest::RunTest(const FString& Parameters)
{
    AlphaCoopTests::FCoopWorld Fixture;
    AAlphaCharacter* Helper = Fixture.SpawnPlayer(FVector(0.0f, 0.0f, 100.0f));
    AAlphaCharacter* Target = Fixture.SpawnPlayer(FVector(120.0f, 0.0f, 100.0f));
    if (!TestNotNull(TEXT("Helper exists"), Helper) || !TestNotNull(TEXT("Target exists"), Target)
        || !TestNotNull(TEXT("Damage source exists"), Fixture.DamageSource)) return false;
    Fixture.Down(Target);
    Helper->GetReviveComponent()->BeginRevive();
    Helper->SetActorLocation(FVector(1000.0f, 0.0f, 100.0f));
    Fixture.Step(Helper->GetReviveComponent(), 1.0f);
    TestNull(TEXT("Leaving range cancels claim"), Target->GetReviveComponent()->GetCurrentRescuer());
    Helper->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
    Helper->GetReviveComponent()->BeginRevive();
    AController* Controller = Helper->GetController();
    if (!TestNotNull(TEXT("Possessing controller exists"), Controller)) return false;
    Controller->UnPossess();
    Fixture.Step(Helper->GetReviveComponent(), 1.0f);
    TestNull(TEXT("Disconnect or unpossession cancels claim"), Target->GetReviveComponent()->GetCurrentRescuer());
    Controller->Possess(Helper);
    Helper->GetReviveComponent()->BeginRevive();
    Target->ResetForRound();
    TestNull(TEXT("Reset cancels incoming rescue"), Helper->GetReviveComponent()->GetReviveTarget());
    TestEqual(TEXT("Reset clears repeated down penalty"), Target->GetReviveComponent()->GetDownCount(), 0);
    TestFalse(TEXT("Reset clears downed state"), Target->IsDefeated());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaReviveVisibilityTest,
    "AlphaGame.Coop.Authority.BlockedLineOfSight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaReviveVisibilityTest::RunTest(const FString& Parameters)
{
    AlphaCoopTests::FCoopWorld Fixture;
    AAlphaCharacter* Helper = Fixture.SpawnPlayer(FVector(0.0f, 0.0f, 100.0f));
    AAlphaCharacter* Target = Fixture.SpawnPlayer(FVector(160.0f, 0.0f, 100.0f));
    if (!TestNotNull(TEXT("Helper exists"), Helper) || !TestNotNull(TEXT("Target exists"), Target)
        || !TestNotNull(TEXT("Damage source exists"), Fixture.DamageSource)) return false;
    Fixture.Down(Target);
    AActor* Wall = Fixture.AddWall(FVector(80.0f, 0.0f, 100.0f));
    if (!TestNotNull(TEXT("Occluding wall exists"), Wall)) return false;
    Helper->GetReviveComponent()->BeginRevive();
    Fixture.Step(Helper->GetReviveComponent(), 4.0f);
    TestNull(TEXT("Cannot claim through wall"), Helper->GetReviveComponent()->GetReviveTarget());
    TestFalse(TEXT("Blocked target remains downed"), Target->GetCombatComponent()->IsAlive());
    Helper->GetReviveComponent()->EndRevive();
    Wall->Destroy();
    Helper->GetReviveComponent()->BeginRevive();
    TestTrue(TEXT("Removing wall permits claim"), Helper->GetReviveComponent()->GetReviveTarget() == Target);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaReviveDeathAndTargetDisconnectTest,
    "AlphaGame.Coop.Authority.DeathAndTargetDisconnect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaReviveDeathAndTargetDisconnectTest::RunTest(const FString& Parameters)
{
    AlphaCoopTests::FCoopWorld Fixture;
    AAlphaCharacter* Helper = Fixture.SpawnPlayer(FVector(0.0f, 0.0f, 100.0f));
    AAlphaCharacter* Target = Fixture.SpawnPlayer(FVector(120.0f, 0.0f, 100.0f));
    if (!TestNotNull(TEXT("Helper exists"), Helper) || !TestNotNull(TEXT("Target exists"), Target)
        || !TestNotNull(TEXT("Damage source exists"), Fixture.DamageSource)) return false;
    Fixture.Down(Target);
    Helper->GetReviveComponent()->BeginRevive();
    Fixture.Step(Helper->GetReviveComponent(), 1.0f);
    Fixture.Down(Helper);
    TestTrue(TEXT("Rescuer dies"), Helper->IsDefeated());
    TestNull(TEXT("Rescuer death clears incoming claim"), Target->GetReviveComponent()->GetCurrentRescuer());
    TestNull(TEXT("Rescuer death clears outgoing target"), Helper->GetReviveComponent()->GetReviveTarget());
    TestFalse(TEXT("Both downed pawns stay at zero health"), Target->GetCombatComponent()->IsAlive());
    Helper->ResetForRound();
    Helper->GetReviveComponent()->BeginRevive();
    AController* TargetController = Target->GetController();
    if (!TestNotNull(TEXT("Target controller exists"), TargetController)) return false;
    TargetController->UnPossess();
    Fixture.Step(Helper->GetReviveComponent(), 1.0f);
    TestNull(TEXT("Target disconnect cancels rescue"), Helper->GetReviveComponent()->GetReviveTarget());
    TestEqual(TEXT("Target disconnect clears progress"), Helper->GetReviveComponent()->GetReviveProgress(), 0.0f);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
