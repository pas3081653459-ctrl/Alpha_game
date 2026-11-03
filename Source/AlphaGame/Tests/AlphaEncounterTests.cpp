#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/Boss/AlphaForgeBoss.h"
#include "Combat/AlphaCombatComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaForgeAttackFootprintTest,
    "AlphaGame.Encounter.ForgeBoss.AttackFootprints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaForgeAttackFootprintTest::RunTest(const FString& Parameters)
{
    const FVector Origin(100, 50, 8);
    const FVector Forward(1, 0, 0);
    const auto Inside = [Origin, Forward](EAlphaForgeAttack Attack, const FVector& Offset)
    {
        return AAlphaForgeBoss::IsPointInAttackZone(Attack, Origin, Forward, Origin + Offset);
    };
    TestTrue(TEXT("Sweep hits a grounded target in front"), Inside(EAlphaForgeAttack::Sweep, FVector(250, 0, 80)));
    TestFalse(TEXT("Sweep leaves a safe space behind"), Inside(EAlphaForgeAttack::Sweep, FVector(-150, 0, 80)));
    TestFalse(TEXT("Sweep leaves its outside arc safe"), Inside(EAlphaForgeAttack::Sweep, FVector(50, 250, 80)));
    TestFalse(TEXT("Sweep cannot reach past its displayed radius"), Inside(EAlphaForgeAttack::Sweep, FVector(331, 0, 80)));
    TestTrue(TEXT("Fire line hits in its rectangle"), Inside(EAlphaForgeAttack::FireLine, FVector(900, 80, 80)));
    TestFalse(TEXT("Fire line can be avoided by depth movement"), Inside(EAlphaForgeAttack::FireLine, FVector(900, 100, 80)));
    TestFalse(TEXT("Fire line cannot hit behind the boss"), Inside(EAlphaForgeAttack::FireLine, FVector(-1, 0, 80)));
    TestFalse(TEXT("Fire line stops at its telegraphed end"), Inside(EAlphaForgeAttack::FireLine, FVector(1001, 0, 80)));
    TestTrue(TEXT("Hammer hits inside its locked circle"), Inside(EAlphaForgeAttack::Hammer, FVector(150, 150, 80)));
    TestFalse(TEXT("Hammer misses outside its locked circle"), Inside(EAlphaForgeAttack::Hammer, FVector(200, 200, 80)));
    TestFalse(TEXT("Ground attack can be jumped above its finite height"), Inside(EAlphaForgeAttack::Hammer, FVector(0, 0, 200)));
    TestFalse(TEXT("No attack never deals zone damage"), Inside(EAlphaForgeAttack::None, FVector(0, 0, 80)));
    TestTrue(TEXT("Direction rotates the line footprint"), AAlphaForgeBoss::IsPointInAttackZone(
        EAlphaForgeAttack::FireLine, Origin, FVector(0, 1, 0), Origin + FVector(20, 700, 80)));
    TestFalse(TEXT("A rotated line does not retain the old axis"), AAlphaForgeBoss::IsPointInAttackZone(
        EAlphaForgeAttack::FireLine, Origin, FVector(0, 1, 0), Origin + FVector(700, 20, 80)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaForgeScalingPhaseTest,
    "AlphaGame.Encounter.ForgeBoss.ScalingPhaseAndNoHeal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaForgeScalingPhaseTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Settings;
    Settings.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
        .CreateAISystem(false).RequiresHitProxies(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
        true, ERHIFeatureLevel::Num, &Settings);
    if (!TestNotNull(TEXT("Authority test world created"), World)) return false;
    AAlphaForgeBoss* Boss = World->SpawnActor<AAlphaForgeBoss>();
    if (!TestNotNull(TEXT("Boss spawned"), Boss))
    {
        World->DestroyWorld(false);
        return false;
    }
    Boss->ConfigureForPlayers(3);
    UAlphaCombatComponent* Combat = Boss->GetCombatComponent();
    TestEqual(TEXT("Three-player boss has scaled maximum health"), Combat->GetMaxHealth(), 1200.f);
    TestEqual(TEXT("Boss begins in phase one"), Boss->GetBossPhase(), 1);
    Combat->ReceiveHit(650.f, 0.f, nullptr);
    Boss->Tick(.016f);
    TestEqual(TEXT("Crossing half health activates phase two"), Boss->GetBossPhase(), 2);
    Boss->ConfigureForPlayers(1);
    TestEqual(TEXT("A second player-count update cannot heal or rescale a live boss"), Combat->GetHealth(), 550.f);
    TestEqual(TEXT("Live maximum health remains fixed after a disconnect"), Combat->GetMaxHealth(), 1200.f);
    Combat->ReceiveHit(10000.f, 0.f, nullptr);
    TestFalse(TEXT("Boss can actually be defeated through authority damage"), Combat->IsAlive());
    World->DestroyWorld(false);
    return true;
}

#endif
