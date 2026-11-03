#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AlphaCharacter.h"
#include "Combat/AlphaCombatComponent.h"
#include "CoreGlobals.h"
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

#endif
