#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/AlphaAbilityComponent.h"
#include "Characters/AlphaCharacter.h"
#include "Characters/AlphaEnemy.h"
#include "Combat/AlphaCombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"

namespace AlphaAbilityTests
{
class FAbilityWorld
{
public:
    FAbilityWorld()
    {
        UWorld::InitializationValues Settings;
        Settings.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
            .CreateAISystem(false).RequiresHitProxies(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
            true, ERHIFeatureLevel::Num, &Settings);
    }

    ~FAbilityWorld()
    {
        if (World) World->DestroyWorld(false);
    }

    AAlphaCharacter* Spawn(bool bEnemy = false, float X = 0.0f)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AAlphaCharacter* Character = World->SpawnActor<AAlphaCharacter>(
            bEnemy ? AAlphaEnemy::StaticClass() : AAlphaCharacter::StaticClass(),
            FVector(X, 0, 100), FRotator::ZeroRotator, Params);
        if (Character && !Character->HasActorBegunPlay()) Character->DispatchBeginPlay();
        // These component tests have no floor; keep fixtures fixed while advancing cooldown time.
        if (Character) Character->GetCharacterMovement()->DisableMovement();
        return Character;
    }

    void Advance(float Seconds)
    {
        if (!World) return;
        while (Seconds > 0.0f)
        {
            const float Step = FMath::Min(0.1f, Seconds);
            World->Tick(LEVELTICK_All, Step);
            Seconds -= Step;
        }
    }

private:
    UWorld* World = nullptr;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaAbilityInvalidSlotTest,
    "AlphaGame.Abilities.Authority.InvalidSlot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaAbilityInvalidSlotTest::RunTest(const FString& Parameters)
{
    AlphaAbilityTests::FAbilityWorld Fixture;
    AAlphaCharacter* Character = Fixture.Spawn();
    if (!TestNotNull(TEXT("Character exists"), Character)) return false;
    UAlphaAbilityComponent* Ability = Character->GetAbilityComponent();
    if (!TestNotNull(TEXT("Ability component exists"), Ability)) return false;
    Ability->TryUseSkill(-1);
    Ability->TryUseSkill(3);
    Ability->TryUseSkill(99);
    TestEqual(TEXT("Invalid slots do not consume resonance"), Ability->GetResonance(), 0.0f);
    TestEqual(TEXT("Invalid slots do not put skill one on cooldown"), Ability->GetSkillRemaining(0), 0.0f);
    TestEqual(TEXT("Invalid slots do not put skill two on cooldown"), Ability->GetSkillRemaining(1), 0.0f);
    TestEqual(TEXT("Invalid slots do not put ultimate on cooldown"), Ability->GetSkillRemaining(2), 0.0f);
    TestFalse(TEXT("Invalid slots cannot be cast"), Ability->CanUseSkill(3));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaAbilityGuardCooldownTest,
    "AlphaGame.Abilities.Authority.GuardCooldown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaAbilityGuardCooldownTest::RunTest(const FString& Parameters)
{
    AlphaAbilityTests::FAbilityWorld Fixture;
    AAlphaCharacter* Character = Fixture.Spawn();
    if (!TestNotNull(TEXT("Character exists"), Character)) return false;
    UAlphaAbilityComponent* Ability = Character->GetAbilityComponent();
    UAlphaCombatComponent* Combat = Character->GetCombatComponent();
    if (!TestNotNull(TEXT("Ability exists"), Ability) || !TestNotNull(TEXT("Combat exists"), Combat)) return false;
    Ability->TryUseSkill(0);
    const float Remaining = Ability->GetSkillRemaining(0);
    TestTrue(TEXT("Guard starts its three-second cooldown"), Remaining > 2.9f && Remaining <= 3.0f);
    TestEqual(TEXT("A valid U cast earns exactly ten energy even with no target"), Ability->GetEnergy(), 10.0f);
    TestTrue(TEXT("Guard grants an authority defensive window"), Combat->IsInvulnerable());
    TestFalse(TEXT("Skill recovery blocks another action"), Combat->CanAct());
    Ability->TryUseSkill(0);
    Ability->TryUseSkill(1);
    TestEqual(TEXT("Spam does not extend cooldown"), Ability->GetSkillRemaining(0), Remaining);
    TestEqual(TEXT("A second skill cannot bypass action lock"), Ability->GetSkillRemaining(1), 0.0f);
    TestEqual(TEXT("Blocked input does not award additional energy"), Ability->GetEnergy(), 10.0f);
    Ability->ResetForRound();
    TestEqual(TEXT("Round reset clears cooldown"), Ability->GetSkillRemaining(0), 0.0f);
    TestEqual(TEXT("Round reset clears energy"), Ability->GetEnergy(), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaAbilityCoopPoiseTest,
    "AlphaGame.Abilities.Authority.CoopPoiseWithoutEnergy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaAbilityCoopPoiseTest::RunTest(const FString& Parameters)
{
    AlphaAbilityTests::FAbilityWorld Fixture;
    AAlphaCharacter* First = Fixture.Spawn(false, -400);
    AAlphaCharacter* Second = Fixture.Spawn(false, -200);
    AAlphaCharacter* Enemy = Fixture.Spawn(true, 500);
    if (!TestNotNull(TEXT("First player exists"), First) || !TestNotNull(TEXT("Second player exists"), Second)
        || !TestNotNull(TEXT("Enemy exists"), Enemy)) return false;
    UAlphaCombatComponent* Target = Enemy->GetCombatComponent();
    Target->ReceiveHit(10, 0, First);
    First->GetCombatComponent()->NotifyHitConfirmed(Enemy, false);
    TestEqual(TEXT("Ordinary confirmed hit does not award U-only energy"), First->GetAbilityComponent()->GetEnergy(), 0.0f);
    Target->ReceiveHit(10, 0, Second);
    Second->GetCombatComponent()->NotifyHitConfirmed(Enemy, false);
    TestEqual(TEXT("Cooperative bonus does not award partner energy"), First->GetAbilityComponent()->GetEnergy(), 0.0f);
    TestEqual(TEXT("Cooperative bonus does not award active attacker energy"), Second->GetAbilityComponent()->GetEnergy(), 0.0f);
    TestEqual(TEXT("Cooperative followup deals extra poise damage"), Target->GetPoise(), Target->GetMaxPoise() - 18.0f);
    Target->ReceiveHit(10, 0, First);
    First->GetCombatComponent()->NotifyHitConfirmed(Enemy, false);
    TestEqual(TEXT("Continued melee still does not award energy"), First->GetAbilityComponent()->GetEnergy(), 0.0f);
    TestEqual(TEXT("Cooldown prevents a second bonus poise hit"), Target->GetPoise(), Target->GetMaxPoise() - 18.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaAbilityEnergyFlowTest,
    "AlphaGame.Abilities.Authority.StrictEnergyFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaAbilityEnergyFlowTest::RunTest(const FString& Parameters)
{
    AlphaAbilityTests::FAbilityWorld Fixture;
    AAlphaCharacter* Character = Fixture.Spawn();
    if (!TestNotNull(TEXT("Character exists"), Character)) return false;
    UAlphaAbilityComponent* Ability = Character->GetAbilityComponent();
    if (!TestNotNull(TEXT("Ability exists"), Ability)) return false;
    TestTrue(TEXT("U is available at zero energy"), Ability->CanUseSkill(0));
    TestFalse(TEXT("I is unavailable below ten energy"), Ability->CanUseSkill(1));
    TestFalse(TEXT("O is unavailable below full energy"), Ability->CanUseSkill(2));
    Ability->TryUseSkill(1);
    Ability->TryUseSkill(2);
    TestEqual(TEXT("Rejected I cannot start cooldown"), Ability->GetSkillRemaining(1), 0.0f);
    TestEqual(TEXT("Rejected O cannot start cooldown"), Ability->GetSkillRemaining(2), 0.0f);
    TestEqual(TEXT("Rejected casts preserve zero energy"), Ability->GetEnergy(), 0.0f);
    for (int32 Cast = 0; Cast < 9; ++Cast)
    {
        TestTrue(TEXT("U becomes available after its cooldown"), Ability->CanUseSkill(0));
        Ability->TryUseSkill(0);
        TestEqual(TEXT("Each accepted U adds exactly ten energy"), Ability->GetEnergy(), (Cast + 1) * 10.0f);
        Fixture.Advance(3.05f);
    }
    Ability->TryUseSkill(2);
    TestEqual(TEXT("Ninety energy cannot activate O"), Ability->GetEnergy(), 90.0f);
    TestEqual(TEXT("Failed ninety-energy O leaves cooldown clear"), Ability->GetSkillRemaining(2), 0.0f);
    Ability->TryUseSkill(0);
    TestEqual(TEXT("Ten U casts fill energy"), Ability->GetEnergy(), 100.0f);
    Fixture.Advance(3.05f);
    Ability->TryUseSkill(0);
    TestEqual(TEXT("U at full energy caps at one hundred without auto-consuming it"), Ability->GetEnergy(), 100.0f);
    TestEqual(TEXT("Legacy resonance accessor agrees with energy"), Ability->GetResonance(), Ability->GetEnergy());
    Fixture.Advance(.4f);
    AAlphaCharacter* Enemy = Fixture.Spawn(true, 300.0f);
    if (!TestNotNull(TEXT("Enemy exists"), Enemy)) return false;
    Ability->TryUseSkill(1);
    TestEqual(TEXT("I consumes exactly ten even when energy was full"), Ability->GetEnergy(), 90.0f);
    TestEqual(TEXT("Full energy no longer automatically strengthens I"), Enemy->GetCombatComponent()->GetHealth(), 13.0f);
    TestFalse(TEXT("I's ten-energy cost makes O unavailable"), Ability->CanUseSkill(2));
    Fixture.Advance(3.05f);
    Ability->TryUseSkill(0);
    TestEqual(TEXT("Another accepted U restores full energy"), Ability->GetEnergy(), 100.0f);
    Fixture.Advance(.4f);
    TestTrue(TEXT("O is available with full energy and a clear action state"), Ability->CanUseSkill(2));
    Ability->TryUseSkill(2);
    TestEqual(TEXT("Accepted O consumes all one hundred energy"), Ability->GetEnergy(), 0.0f);
    TestTrue(TEXT("Accepted O starts its own cooldown"), Ability->GetSkillRemaining(2) > 19.9f);
    TestEqual(TEXT("O applies its actual high-damage attack"), Enemy->GetCombatComponent()->GetHealth(), 0.0f);
    Ability->ResetForRound();
    TestEqual(TEXT("Round reset clears the ultimate cooldown"), Ability->GetSkillRemaining(2), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaAbilityBlockedEnergyTest,
    "AlphaGame.Abilities.Authority.BlockedCastsPreserveEnergy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaAbilityBlockedEnergyTest::RunTest(const FString& Parameters)
{
    AlphaAbilityTests::FAbilityWorld Fixture;
    AAlphaCharacter* Character = Fixture.Spawn();
    AAlphaCharacter* Enemy = Fixture.Spawn(true, 500.0f);
    if (!TestNotNull(TEXT("Character exists"), Character) || !TestNotNull(TEXT("Enemy exists"), Enemy)) return false;
    UAlphaAbilityComponent* Ability = Character->GetAbilityComponent();
    UAlphaCombatComponent* Combat = Character->GetCombatComponent();
    Combat->SetActionLockFor(2.0f);
    Ability->TryUseSkill(0);
    TestEqual(TEXT("Staggered U does not earn energy"), Ability->GetEnergy(), 0.0f);
    TestEqual(TEXT("Staggered U does not enter cooldown"), Ability->GetSkillRemaining(0), 0.0f);
    Combat->ResetVitals();
    Ability->TryUseSkill(0);
    Ability->TryUseSkill(0);
    TestEqual(TEXT("Cooldown spam earns energy only once"), Ability->GetEnergy(), 10.0f);
    Fixture.Advance(3.05f);
    Combat->ReceiveHit(10000.0f, 0.0f, Enemy);
    TestFalse(TEXT("Dead characters cannot use a ready U"), Ability->CanUseSkill(0));
    Ability->TryUseSkill(0);
    Ability->TryUseSkill(1);
    Ability->TryUseSkill(2);
    TestEqual(TEXT("Death prevents all energy gains and costs"), Ability->GetEnergy(), 10.0f);
    TestEqual(TEXT("Dead U stays out of cooldown"), Ability->GetSkillRemaining(0), 0.0f);
    TestEqual(TEXT("Dead I stays out of cooldown"), Ability->GetSkillRemaining(1), 0.0f);
    return true;
}

#endif
