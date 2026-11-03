#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/AlphaCombatComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

namespace AlphaCombatTests
{
/** A real standalone authority world; no editor module or content assets are required. */
class FCombatWorld
{
public:
    FCombatWorld()
    {
        UWorld::InitializationValues Settings;
        Settings.AllowAudioPlayback(false)
            .CreatePhysicsScene(false)
            .CreateNavigation(false)
            .CreateAISystem(false)
            .RequiresHitProxies(false)
            .ShouldSimulatePhysics(false)
            .SetTransactional(false);
        // CreateWorld initializes the world using Settings; do not initialize it a second time.
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
            true, ERHIFeatureLevel::Num, &Settings);
    }

    ~FCombatWorld()
    {
        if (World)
        {
            World->DestroyWorld(false);
        }
    }

    UAlphaCombatComponent* SpawnCombat(int32 Team)
    {
        if (!World)
        {
            return nullptr;
        }
        AActor* Actor = World->SpawnActor<AActor>();
        if (!Actor)
        {
            return nullptr;
        }
        Actor->SetReplicates(true);
        UAlphaCombatComponent* Combat = NewObject<UAlphaCombatComponent>(Actor);
        Actor->AddInstanceComponent(Combat);
        Combat->TeamId = Team;
        Combat->RegisterComponent();
        Combat->ResetVitals();
        return Combat;
    }

    void Advance(float DeltaSeconds)
    {
        if (World) World->Tick(LEVELTICK_All, DeltaSeconds);
    }

private:
    UWorld* World = nullptr;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaCombatDamageTest,
    "AlphaGame.Combat.Authority.DamageAndPoise",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaCombatDamageTest::RunTest(const FString& Parameters)
{
    AlphaCombatTests::FCombatWorld Fixture;
    UAlphaCombatComponent* Player = Fixture.SpawnCombat(1);
    UAlphaCombatComponent* Enemy = Fixture.SpawnCombat(2);
    if (!TestNotNull(TEXT("Player combat component exists"), Player)
        || !TestNotNull(TEXT("Enemy combat component exists"), Enemy))
    {
        return false;
    }
    TestTrue(TEXT("Standalone actor has authority"), Player->GetOwner()->HasAuthority());
    Player->ReceiveHit(25.0f, 15.0f, Enemy->GetOwner());
    TestEqual(TEXT("Enemy damage is applied once"), Player->GetHealth(), 75.0f);
    TestEqual(TEXT("Enemy attack reduces poise"), Player->GetPoise(), 45.0f);
    Player->ReceiveHit(0.0f, 50.0f, Enemy->GetOwner());
    TestEqual(TEXT("Poise break refills the prototype poise meter"), Player->GetPoise(), Player->GetMaxPoise());
    TestEqual(TEXT("Poise-only hit does not alter health"), Player->GetHealth(), 75.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaCombatFriendlyFireTest,
    "AlphaGame.Combat.Authority.FriendlyFire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaCombatFriendlyFireTest::RunTest(const FString& Parameters)
{
    AlphaCombatTests::FCombatWorld Fixture;
    UAlphaCombatComponent* Player = Fixture.SpawnCombat(1);
    UAlphaCombatComponent* Ally = Fixture.SpawnCombat(1);
    if (!TestNotNull(TEXT("Player combat component exists"), Player)
        || !TestNotNull(TEXT("Ally combat component exists"), Ally))
    {
        return false;
    }
    Player->ReceiveHit(80.0f, 50.0f, Ally->GetOwner());
    TestEqual(TEXT("Ally cannot damage player health"), Player->GetHealth(), Player->GetMaxHealth());
    TestEqual(TEXT("Ally cannot damage player poise"), Player->GetPoise(), Player->GetMaxPoise());
    Player->ReceiveHit(80.0f, 50.0f, Player->GetOwner());
    TestEqual(TEXT("Self damage is rejected"), Player->GetHealth(), Player->GetMaxHealth());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaCombatDeathAndResetTest,
    "AlphaGame.Combat.Authority.DeathAndReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaCombatDeathAndResetTest::RunTest(const FString& Parameters)
{
    AlphaCombatTests::FCombatWorld Fixture;
    UAlphaCombatComponent* Player = Fixture.SpawnCombat(1);
    UAlphaCombatComponent* Enemy = Fixture.SpawnCombat(2);
    if (!TestNotNull(TEXT("Player combat component exists"), Player)
        || !TestNotNull(TEXT("Enemy combat component exists"), Enemy))
    {
        return false;
    }
    Player->ReceiveHit(10000.0f, 15.0f, Enemy->GetOwner());
    TestEqual(TEXT("Fatal damage clamps health to zero"), Player->GetHealth(), 0.0f);
    TestFalse(TEXT("Fatal damage marks actor dead"), Player->IsAlive());
    const float PoiseAtDeath = Player->GetPoise();
    Player->ReceiveHit(20.0f, 20.0f, Enemy->GetOwner());
    TestEqual(TEXT("Dead actor does not receive more health damage"), Player->GetHealth(), 0.0f);
    TestEqual(TEXT("Dead actor does not receive more poise damage"), Player->GetPoise(), PoiseAtDeath);
    Player->ResetVitals();
    TestTrue(TEXT("Reset restores alive state"), Player->IsAlive());
    TestEqual(TEXT("Reset restores health"), Player->GetHealth(), Player->GetMaxHealth());
    TestEqual(TEXT("Reset restores poise"), Player->GetPoise(), Player->GetMaxPoise());
    Player->ReceiveHit(10.0f, 10.0f, Enemy->GetOwner());
    TestEqual(TEXT("Revived actor can receive damage again"), Player->GetHealth(), Player->GetMaxHealth() - 10.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaCombatNegativeDamageTest,
    "AlphaGame.Combat.Authority.NegativeDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaCombatNegativeDamageTest::RunTest(const FString& Parameters)
{
    AlphaCombatTests::FCombatWorld Fixture;
    UAlphaCombatComponent* Player = Fixture.SpawnCombat(1);
    UAlphaCombatComponent* Enemy = Fixture.SpawnCombat(2);
    if (!TestNotNull(TEXT("Player combat component exists"), Player)
        || !TestNotNull(TEXT("Enemy combat component exists"), Enemy))
    {
        return false;
    }
    Player->ReceiveHit(25.0f, 15.0f, Enemy->GetOwner());
    Player->ReceiveHit(-1000.0f, -1000.0f, Enemy->GetOwner());
    TestEqual(TEXT("Negative damage cannot heal"), Player->GetHealth(), 75.0f);
    TestEqual(TEXT("Negative poise damage cannot restore poise"), Player->GetPoise(), 45.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaCombatInvulnerabilityTest,
    "AlphaGame.Combat.Authority.Invulnerability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaCombatInvulnerabilityTest::RunTest(const FString& Parameters)
{
    AlphaCombatTests::FCombatWorld Fixture;
    UAlphaCombatComponent* Player = Fixture.SpawnCombat(1);
    UAlphaCombatComponent* Enemy = Fixture.SpawnCombat(2);
    if (!TestNotNull(TEXT("Player exists"), Player) || !TestNotNull(TEXT("Enemy exists"), Enemy)) return false;
    Player->SetInvulnerableFor(0.1f);
    TestTrue(TEXT("Defensive window is active"), Player->IsInvulnerable());
    Player->ReceiveHit(1000.0f, 1000.0f, Enemy->GetOwner());
    TestEqual(TEXT("Defensive window rejects health damage"), Player->GetHealth(), Player->GetMaxHealth());
    TestEqual(TEXT("Defensive window rejects poise damage"), Player->GetPoise(), Player->GetMaxPoise());
    TestEqual(TEXT("Rejected hit cannot cancel rescue via damage revision"), Player->GetDamageRevision(), 0);
    Fixture.Advance(0.2f);
    TestFalse(TEXT("Defensive window expires with world time"), Player->IsInvulnerable());
    Player->SetInvulnerableFor(0.1f);
    Player->ResetVitals();
    TestFalse(TEXT("Round reset clears old defensive window"), Player->IsInvulnerable());
    Player->ReceiveHit(10.0f, 5.0f, Enemy->GetOwner());
    TestEqual(TEXT("Damage is accepted after reset clears invulnerability"), Player->GetHealth(), 90.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaCombatReviveTest,
    "AlphaGame.Combat.Authority.Revive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaCombatReviveTest::RunTest(const FString& Parameters)
{
    AlphaCombatTests::FCombatWorld Fixture;
    UAlphaCombatComponent* Player = Fixture.SpawnCombat(1);
    UAlphaCombatComponent* Enemy = Fixture.SpawnCombat(2);
    if (!TestNotNull(TEXT("Player exists"), Player) || !TestNotNull(TEXT("Enemy exists"), Enemy)) return false;
    TestFalse(TEXT("Revive cannot refill an already living character"), Player->Revive());
    Player->ReceiveHit(1000.0f, 20.0f, Enemy->GetOwner());
    TestFalse(TEXT("Dead character cannot act"), Player->CanAct());
    TestTrue(TEXT("Authority can revive a dead character"), Player->Revive());
    TestEqual(TEXT("Revive restores forty percent health"), Player->GetHealth(), 40.0f);
    TestEqual(TEXT("Revive restores full poise"), Player->GetPoise(), Player->GetMaxPoise());
    TestTrue(TEXT("Revive clears old stagger"), Player->CanAct());
    TestTrue(TEXT("Revive gives a defensive grace period"), Player->IsInvulnerable());
    Player->ReceiveHit(1000.0f, 20.0f, Enemy->GetOwner());
    TestEqual(TEXT("Revive grace period prevents immediate re-down"), Player->GetHealth(), 40.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaCombatActionLockTest,
    "AlphaGame.Combat.Authority.PoiseBreakAndActionLock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaCombatActionLockTest::RunTest(const FString& Parameters)
{
    AlphaCombatTests::FCombatWorld Fixture;
    UAlphaCombatComponent* Player = Fixture.SpawnCombat(1);
    UAlphaCombatComponent* Enemy = Fixture.SpawnCombat(2);
    if (!TestNotNull(TEXT("Player exists"), Player) || !TestNotNull(TEXT("Enemy exists"), Enemy)) return false;
    Player->ReceiveHit(0.0f, 100.0f, Enemy->GetOwner());
    TestTrue(TEXT("Poise break leaves the actor alive"), Player->IsAlive());
    TestFalse(TEXT("Poise break prevents actions"), Player->CanAct());
    TestEqual(TEXT("Poise break revision records the event"), Player->GetPoiseBreakRevision(), 1);
    Player->ResetVitals();
    TestTrue(TEXT("Round reset clears stagger"), Player->CanAct());
    Player->SetActionLockFor(0.1f);
    TestFalse(TEXT("Skill recovery prevents another action"), Player->CanAct());
    Fixture.Advance(0.2f);
    TestTrue(TEXT("Skill recovery expires without needing reset"), Player->CanAct());
    Player->SetActionLockFor(0.1f);
    Player->ResetVitals();
    TestTrue(TEXT("Reset clears skill recovery"), Player->CanAct());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaCombatProfilesAndRevisionTest,
    "AlphaGame.Combat.Authority.ProfilesAndDamageRevision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaCombatProfilesAndRevisionTest::RunTest(const FString& Parameters)
{
    AlphaCombatTests::FCombatWorld Fixture;
    UAlphaCombatComponent* Player = Fixture.SpawnCombat(1);
    UAlphaCombatComponent* Enemy = Fixture.SpawnCombat(2);
    if (!TestNotNull(TEXT("Player exists"), Player) || !TestNotNull(TEXT("Enemy exists"), Enemy)) return false;
    Player->ConfigureArchetype(0);
    TestEqual(TEXT("Shen Yan has the durable profile"), Player->GetMaxHealth(), 120.0f);
    Player->ConfigureArchetype(1);
    TestEqual(TEXT("A Ling has the agile profile"), Player->GetMaxHealth(), 90.0f);
    Player->ConfigureArchetype(2);
    TestEqual(TEXT("Wen Deng has the support profile"), Player->GetMaxHealth(), 100.0f);
    Player->ConfigureArchetype(99);
    TestEqual(TEXT("Unknown class cannot modify vitals"), Player->GetMaxHealth(), 100.0f);
    Player->ReceiveHit(10.0f, 0.0f, Enemy->GetOwner());
    const int32 Revision = Player->GetDamageRevision();
    TestEqual(TEXT("Accepted health damage advances rescue cancellation revision"), Revision, 1);
    Player->ReceiveHit(0.0f, 5.0f, Enemy->GetOwner());
    TestEqual(TEXT("Poise-only effect does not masquerade as health damage"), Player->GetDamageRevision(), Revision);
    Player->ResetVitals();
    TestEqual(TEXT("Reset preserves revision to avoid stale rescue snapshots"), Player->GetDamageRevision(), Revision);
    Player->ConfigureVitals(900.0f, 150.0f, 0.2f, 0.0f);
    TestEqual(TEXT("Boss profile configures scaled maximum health"), Player->GetMaxHealth(), 900.0f);
    TestEqual(TEXT("Boss profile refills health"), Player->GetHealth(), 900.0f);
    TestEqual(TEXT("Boss profile configures maximum poise"), Player->GetMaxPoise(), 150.0f);
    Player->ReceiveHit(10.0f, 5.0f, Enemy->GetOwner());
    TestTrue(TEXT("Boss profile resists normal hit stun"), Player->CanAct());
    Player->ReceiveHit(10.0f, 200.0f, Enemy->GetOwner());
    TestFalse(TEXT("Boss remains vulnerable to a real poise break"), Player->CanAct());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
