#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/AlphaAbilityComponent.h"
#include "Characters/AlphaCharacter.h"
#include "Characters/AlphaEnemy.h"
#include "Combat/AlphaCombatComponent.h"
#include "Engine/World.h"
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
        return Character;
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
    Ability->TryUseSkill(99);
    TestEqual(TEXT("Invalid slots do not consume resonance"), Ability->GetResonance(), 0.0f);
    TestEqual(TEXT("Invalid slots do not put skill one on cooldown"), Ability->GetSkillRemaining(0), 0.0f);
    TestEqual(TEXT("Invalid slots do not put skill two on cooldown"), Ability->GetSkillRemaining(1), 0.0f);
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
    TestTrue(TEXT("Guard starts its six-second cooldown"), Remaining > 5.9f && Remaining <= 6.0f);
    TestTrue(TEXT("Guard grants an authority defensive window"), Combat->IsInvulnerable());
    TestFalse(TEXT("Skill recovery blocks another action"), Combat->CanAct());
    Ability->TryUseSkill(0);
    Ability->TryUseSkill(1);
    TestEqual(TEXT("Spam does not extend cooldown"), Ability->GetSkillRemaining(0), Remaining);
    TestEqual(TEXT("A second skill cannot bypass action lock"), Ability->GetSkillRemaining(1), 0.0f);
    Ability->ResetForRound();
    TestEqual(TEXT("Round reset clears cooldown"), Ability->GetSkillRemaining(0), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaAbilityCoopResonanceTest,
    "AlphaGame.Abilities.Authority.CoopResonance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaAbilityCoopResonanceTest::RunTest(const FString& Parameters)
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
    TestEqual(TEXT("Confirmed hit earns personal resonance"), First->GetAbilityComponent()->GetResonance(), 6.0f);
    Target->ReceiveHit(10, 0, Second);
    Second->GetCombatComponent()->NotifyHitConfirmed(Enemy, false);
    TestEqual(TEXT("Followup grants previous attacker partner resonance"), First->GetAbilityComponent()->GetResonance(), 14.0f);
    TestEqual(TEXT("Followup grants active attacker resonance"), Second->GetAbilityComponent()->GetResonance(), 18.0f);
    TestEqual(TEXT("Cooperative followup deals extra poise damage"), Target->GetPoise(), Target->GetMaxPoise() - 18.0f);
    Target->ReceiveHit(10, 0, First);
    First->GetCombatComponent()->NotifyHitConfirmed(Enemy, false);
    TestEqual(TEXT("Target-wide proc cooldown prevents immediate alternating retrigger"), First->GetAbilityComponent()->GetResonance(), 20.0f);
    TestEqual(TEXT("Cooldown prevents a second bonus poise hit"), Target->GetPoise(), Target->GetMaxPoise() - 18.0f);
    return true;
}

#endif
