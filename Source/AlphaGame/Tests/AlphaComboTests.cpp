#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AlphaCharacter.h"
#include "Combat/AlphaCombatComponent.h"
#include "CoreGlobals.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"

namespace AlphaComboTests
{
struct FWorldFixture
{
    UWorld* World = nullptr;
    AAlphaCharacter* Character = nullptr;

    FWorldFixture()
    {
        UWorld::InitializationValues Settings;
        Settings.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
            .CreateAISystem(false).RequiresHitProxies(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
            true, ERHIFeatureLevel::Num, &Settings);
        if (!World) return;
        Character = World->SpawnActor<AAlphaCharacter>();
        if (!Character) return;
        Character->DispatchBeginPlay();
        Character->GetCharacterMovement()->SetComponentTickEnabled(false);
    }
    ~FWorldFixture() { if (World) World->DestroyWorld(false); }
};

class FSequenceCommand : public IAutomationLatentCommand
{
public:
    FSequenceCommand(FAutomationTestBase* InTest, TSharedPtr<FWorldFixture> InFixture)
        : Test(InTest), Fixture(InFixture), StartedAt(FPlatformTime::Seconds()) {}

    virtual bool Update() override
    {
        // Let the real engine advance its frame counter. Never alter global
        // GFrameCounter or tick a timer manager twice in one engine frame.
        if (LastFrame == GFrameCounter) return false;
        LastFrame = GFrameCounter;
        if (FPlatformTime::Seconds() - StartedAt > 20.)
        {
            Test->AddError(TEXT("Five-stage combo did not cycle back to one within the test deadline."));
            return true;
        }
        Fixture->World->Tick(LEVELTICK_All, .02f);
        UAlphaCombatComponent* Combat = Fixture->Character->GetCombatComponent();
        const int32 Stage = Combat->GetComboStage();
        if (Stage > 0 && Stage != LastStage)
        {
            const int32 Expected = Observed < 5 ? Observed + 1 : 1;
            Test->TestEqual(TEXT("Accepted attacks follow 1,2,3,4,5,1"), Stage, Expected);
            LastStage = Stage;
            ++Observed;
            if (Observed == 6) return true;
        }
        if (++Ticks % 4 == 0) Combat->TryAttack(false); // Repeated clicks, not an automatic chain.
        return false;
    }
private:
    FAutomationTestBase* Test;
    TSharedPtr<FWorldFixture> Fixture;
    double StartedAt;
    uint64 LastFrame = MAX_uint64;
    int32 LastStage = 0;
    int32 Observed = 0;
    int32 Ticks = 0;
};

class FAirCoverageCommand : public IAutomationLatentCommand
{
public:
    FAirCoverageCommand(FAutomationTestBase* InTest, TSharedPtr<FWorldFixture> InFixture, AAlphaCharacter* InTarget)
        : Test(InTest), Fixture(InFixture), Target(InTarget), StartedAt(FPlatformTime::Seconds()) {}
    virtual bool Update() override
    {
        if (LastFrame == GFrameCounter) return false;
        LastFrame = GFrameCounter;
        if (FPlatformTime::Seconds() - StartedAt > 10.)
        {
            Test->AddError(TEXT("Aerial hit window did not advance within its deadline."));
            return true;
        }
        Fixture->World->Tick(LEVELTICK_All, .02f);
        Elapsed += .02f;
        if (Elapsed < .22f) return false;
        Test->TestEqual(TEXT("Downward air crescent reaches a grounded enemy once"),
            Target->GetCombatComponent()->GetHealth(), 982.f);
        Test->TestEqual(TEXT("One air swing advances only one damage revision"),
            Target->GetCombatComponent()->GetDamageRevision(), 1);
        Test->TestTrue(TEXT("Aerial hit coverage does not grant invulnerability"),
            !Fixture->Character->GetCombatComponent()->IsInvulnerable());
        return true;
    }
private:
    FAutomationTestBase* Test;
    TSharedPtr<FWorldFixture> Fixture;
    AAlphaCharacter* Target;
    double StartedAt;
    uint64 LastFrame = MAX_uint64;
    float Elapsed = 0.f;
};


// The same public input sequence runs against real query shapes with/without
// floor and with an optional wall. Targets enter only after the fifth windup.
class FFinisherContactCommand : public IAutomationLatentCommand
{
public:
    FFinisherContactCommand(FAutomationTestBase* InTest, TSharedPtr<FWorldFixture> InFixture,
        AAlphaCharacter* InNear, AAlphaCharacter* InSide, bool bInExpectSide)
        : Test(InTest), Fixture(InFixture), Near(InNear), Side(InSide),
          bExpectSide(bInExpectSide), StartedAt(FPlatformTime::Seconds()) {}
    virtual bool Update() override
    {
        if (LastFrame == GFrameCounter) return false;
        LastFrame = GFrameCounter;
        if (FPlatformTime::Seconds() - StartedAt > 20.)
        {
            Test->AddError(TEXT("Ground finisher input sequence exceeded its deadline."));
            return true;
        }
        Fixture->World->Tick(LEVELTICK_All, .02f);
        UAlphaCombatComponent* Combat = Fixture->Character->GetCombatComponent();
        if (!bFifthStarted && Combat->GetComboStage() == 5)
        {
            bFifthStarted = true;
            Near->SetActorLocation(FVector(150.f, 0, 96.f));
            Side->SetActorLocation(FVector(110.f, 225.f, 96.f));
        }
        if (bFifthStarted)
        {
            SinceFifth += .02f;
            if (SinceFifth < .36f) return false;
            Test->TestEqual(TEXT("Overlapping slash and floor explosion cause exactly one accepted hit"),
                Near->GetCombatComponent()->GetDamageRevision(), 1);
            Test->TestTrue(TEXT("Finisher damaged the overlapping enemy"),
                Near->GetCombatComponent()->GetHealth() < 1000.f);
            Test->TestEqual(TEXT("Side enemy is hit only by a real unobstructed floor explosion"),
                Side->GetCombatComponent()->GetDamageRevision(), bExpectSide ? 1 : 0);
            return true;
        }
        if (++Ticks % 4 == 0) Combat->TryAttack(false);
        return false;
    }
private:
    FAutomationTestBase* Test;
    TSharedPtr<FWorldFixture> Fixture;
    AAlphaCharacter* Near;
    AAlphaCharacter* Side;
    bool bExpectSide;
    double StartedAt;
    uint64 LastFrame = MAX_uint64;
    int32 Ticks = 0;
    bool bFifthStarted = false;
    float SinceFifth = 0.f;
};

bool QueueFinisherContactTest(FAutomationTestBase* Test, bool bWithFloor, bool bWithWall)
{
    TSharedPtr<FWorldFixture> Fixture = MakeShared<FWorldFixture>();
    if (!Test->TestNotNull(TEXT("Fixture character exists"), Fixture->Character)) return false;
    Fixture->Character->SetActorLocation(FVector(0, 0, 96.f));
    Fixture->Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Fixture->Character->GetCombatComponent()->ConfigureArchetype(0);
    const auto Block = [&Fixture](const FVector& Position, const FVector& Extent)
    {
        AActor* Actor = Fixture->World->SpawnActor<AActor>();
        if (!Actor) return false;
        UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
        Actor->AddInstanceComponent(Box);
        Actor->SetRootComponent(Box);
        Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->RegisterComponent();
        Actor->SetActorLocation(Position);
        return true;
    };
    if (bWithFloor && !Test->TestTrue(TEXT("Real query floor exists"),
        Block(FVector(0, 0, -25.f), FVector(2000.f, 1000.f, 25.f)))) return false;
    if (bWithWall && !Test->TestTrue(TEXT("Side cover wall exists"),
        Block(FVector(110.f, 120.f, 150.f), FVector(300.f, 10.f, 150.f)))) return false;
    const auto Enemy = [&Fixture](float Y)
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AAlphaCharacter* Target = Fixture->World->SpawnActor<AAlphaCharacter>(FVector(1000.f, Y, 96.f), FRotator::ZeroRotator, Spawn);
        if (!Target) return Target;
        Target->SetActorLocation(FVector(1000.f, Y, 96.f));
        Target->DispatchBeginPlay();
        Target->GetCharacterMovement()->SetComponentTickEnabled(false);
        Target->GetCombatComponent()->TeamId = 2;
        Target->GetCombatComponent()->ConfigureVitals(1000.f, 1000.f, .65f, 0.f);
        return Target;
    };
    AAlphaCharacter* Near = Enemy(0.f);
    AAlphaCharacter* Side = Enemy(225.f);
    if (!Test->TestNotNull(TEXT("Near enemy exists"), Near)
        || !Test->TestNotNull(TEXT("Side enemy exists"), Side)) return false;
    Fixture->Character->GetCombatComponent()->TryAttack(false);
    ADD_LATENT_AUTOMATION_COMMAND(FFinisherContactCommand(Test, Fixture, Near, Side, bWithFloor && !bWithWall));
    return true;
}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaFiveStageCycleTest,
    "AlphaGame.Combat.Combo.FiveStagesAndBufferedCycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaFiveStageCycleTest::RunTest(const FString& Parameters)
{
    TSharedPtr<AlphaComboTests::FWorldFixture> Fixture = MakeShared<AlphaComboTests::FWorldFixture>();
    if (!TestNotNull(TEXT("World exists"), Fixture->World)
        || !TestNotNull(TEXT("Character exists"), Fixture->Character)) return false;
    Fixture->Character->GetCombatComponent()->TryAttack(false);
    ADD_LATENT_AUTOMATION_COMMAND(AlphaComboTests::FSequenceCommand(this, Fixture));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaAirComboCancellationTest,
    "AlphaGame.Combat.Combo.AirHoldCancellationAndPerFlightLatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaAirComboCancellationTest::RunTest(const FString& Parameters)
{
    AlphaComboTests::FWorldFixture Fixture;
    if (!TestNotNull(TEXT("Character exists"), Fixture.Character)) return false;
    UCharacterMovementComponent* Movement = Fixture.Character->GetCharacterMovement();
    UAlphaCombatComponent* Combat = Fixture.Character->GetCombatComponent();
    Movement->SetMovementMode(MOVE_Falling);
    const float OriginalGravity = Movement->GravityScale;
    Combat->TryAttack(false);
    TestTrue(TEXT("Air attack starts a bounded hold"), Combat->IsAirComboActive());
    TestEqual(TEXT("Hold actually disables gravity"), Movement->GravityScale, 0.f);
    Combat->SetActionLockFor(.1f);
    TestFalse(TEXT("A skill or roll action lock cancels air hold"), Combat->IsAirComboActive());
    TestEqual(TEXT("Cancellation restores original gravity"), Movement->GravityScale, OriginalGravity);
    Fixture.World->Tick(LEVELTICK_All, .2f);
    Combat->TryAttack(false);
    TestFalse(TEXT("Another click in the same flight cannot renew hovering"), Combat->IsAirComboActive());
    Combat->ResetVitals();
    Movement->SetMovementMode(MOVE_Walking);
    // ResetVitals already clears the per-flight latch. Exercise the public
    // reset path without manually invoking a protected engine tick callback.
    Movement->SetMovementMode(MOVE_Falling);
    Combat->TryAttack(false);
    TestTrue(TEXT("A reset character can start a new air hold"), Combat->IsAirComboActive());
    Combat->ReceiveHit(10.f, 0.f, nullptr);
    TestFalse(TEXT("Damage interrupts air hold"), Combat->IsAirComboActive());
    TestEqual(TEXT("Damage restores gravity"), Movement->GravityScale, OriginalGravity);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaAirComboGroundCoverageTest,
    "AlphaGame.Combat.Combo.AerialVariantHitsGroundedEnemyOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaAirComboGroundCoverageTest::RunTest(const FString& Parameters)
{
    TSharedPtr<AlphaComboTests::FWorldFixture> Fixture = MakeShared<AlphaComboTests::FWorldFixture>();
    if (!TestNotNull(TEXT("Character exists"), Fixture->Character)) return false;
    Fixture->Character->SetActorLocation(FVector(0, 0, 330.f));
    Fixture->Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    AAlphaCharacter* Target = Fixture->World->SpawnActor<AAlphaCharacter>();
    if (!TestNotNull(TEXT("Grounded target exists"), Target)) return false;
    Target->SetActorLocation(FVector(150.f, 0, 96.f));
    Target->DispatchBeginPlay();
    Target->GetCharacterMovement()->SetComponentTickEnabled(false);
    Target->GetCombatComponent()->TeamId = 2;
    Target->GetCombatComponent()->ConfigureVitals(1000.f, 1000.f, .65f, 0.f);
    Fixture->Character->GetCombatComponent()->ConfigureArchetype(0);
    Fixture->Character->GetCombatComponent()->TryAttack(false);
    ADD_LATENT_AUTOMATION_COMMAND(AlphaComboTests::FAirCoverageCommand(this, Fixture, Target));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaFinisherImpactDedupTest,
    "AlphaGame.Combat.Combo.GroundImpactMeleeAndExplosionDeduplicate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaFinisherImpactDedupTest::RunTest(const FString& Parameters)
{
    return AlphaComboTests::QueueFinisherContactTest(this, true, false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaFinisherRequiresFloorTest,
    "AlphaGame.Combat.Combo.GroundImpactRequiresRealFloor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaFinisherRequiresFloorTest::RunTest(const FString& Parameters)
{
    return AlphaComboTests::QueueFinisherContactTest(this, false, false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaFinisherWallTest,
    "AlphaGame.Combat.Combo.GroundImpactCannotDamageThroughWall",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaFinisherWallTest::RunTest(const FString& Parameters)
{
    return AlphaComboTests::QueueFinisherContactTest(this, true, true);
}

#endif
