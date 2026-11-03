#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AlphaEnemy.h"
#include "Combat/AlphaCombatComponent.h"
#include "Components/BoxComponent.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "World/AlphaGameState.h"

namespace AlphaEnemyTacticsTests
{
struct FWorldFixture
{
    UWorld* World = nullptr;
    AAlphaCharacter* Human = nullptr;
    AAlphaEnemy* Enemy = nullptr;

    FWorldFixture(int32 Role, FVector HumanPosition = FVector(400,0,96))
    {
        UWorld::InitializationValues Settings;
        Settings.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
            .CreateAISystem(false).RequiresHitProxies(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Settings);
        if (!World) return;
        AAlphaGameState* State = World->SpawnActor<AAlphaGameState>();
        World->SetGameState(State);
        APlayerController* PC = World->SpawnActor<APlayerController>();
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Human = World->SpawnActor<AAlphaCharacter>(AAlphaCharacter::StaticClass(),HumanPosition,FRotator::ZeroRotator,Params);
        if (!Human || !PC) return;
        PC->Possess(Human);
        Human->DispatchBeginPlay();
        Human->GetCharacterMovement()->SetComponentTickEnabled(false);
        Human->SetActorTickEnabled(false);
        Enemy = SpawnEnemy(Role,FVector(0,0,96));
    }
    ~FWorldFixture() { if (World) World->DestroyWorld(false); }
    AAlphaEnemy* SpawnEnemy(int32 Role, FVector Position)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AAlphaEnemy* Result = World->SpawnActor<AAlphaEnemy>(AAlphaEnemy::StaticClass(),Position,FRotator::ZeroRotator,Params);
        if (!Result) return nullptr;
        Result->DispatchBeginPlay();
        Result->ConfigureRole(Role);
        Result->GetCharacterMovement()->SetComponentTickEnabled(false);
        Result->SetActorTickEnabled(false);
        return Result;
    }
    bool Ready() const { return World && Human && Enemy && Human->GetController(); }
};

enum class EScenario { Shot, Interrupt, Jump, Depth, Wall, Melee, ShieldExposure };

class FAdvanceScenario : public IAutomationLatentCommand
{
public:
    FAdvanceScenario(FAutomationTestBase* InTest,TSharedPtr<FWorldFixture> InFixture,EScenario InScenario)
        : Test(InTest),Fixture(InFixture),Scenario(InScenario),InitialHealth(InFixture->Human->GetCombatComponent()->GetHealth()),
          BeganAt(FPlatformTime::Seconds()) {}
    virtual bool Update() override
    {
        if (LastFrame == GFrameCounter) return false;
        LastFrame = GFrameCounter;
        if (FPlatformTime::Seconds()-BeganAt > 20.)
        {
            Test->AddError(TEXT("Enemy tactics scenario did not finish within the real frame deadline."));
            return true;
        }
        // Use real engine frames for component timers. The enemy's actor tick
        // is disabled so the one explicit AI decision below cannot run twice.
        Fixture->World->Tick(LEVELTICK_All,.02f);
        Elapsed += .02f;
        if (!bChanged && Elapsed >= .2f && Scenario != EScenario::Melee && Scenario != EScenario::ShieldExposure)
        {
            bChanged = true;
            if (Scenario == EScenario::Interrupt)
                Fixture->Enemy->GetCombatComponent()->ReceiveHit(1.f,0.f,Fixture->Human);
            if (Scenario == EScenario::Jump) Fixture->Human->SetActorLocation(FVector(400,0,220));
            if (Scenario == EScenario::Depth) Fixture->Human->SetActorLocation(FVector(400,90,96));
            if (Scenario == EScenario::Wall)
            {
                AActor* Wall = Fixture->World->SpawnActor<AActor>();
                if (!Test->TestNotNull(TEXT("Blocking wall actor exists"),Wall)) return true;
                UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
                Wall->AddInstanceComponent(Box);
                Wall->SetRootComponent(Box);
                Box->SetBoxExtent(FVector(8,180,180));
                Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                Box->SetCollisionObjectType(ECC_WorldStatic);
                Box->SetCollisionResponseToAllChannels(ECR_Block);
                Box->RegisterComponent();
                Wall->SetActorLocation(FVector(200,0,100));
            }
        }
        Fixture->Enemy->Tick(.02f);
        if (!bCheckedWindup && Elapsed >= .16f && Scenario != EScenario::ShieldExposure)
        {
            bCheckedWindup = true;
            Test->TestEqual(TEXT("No damage before the telegraphed release"),Fixture->Human->GetCombatComponent()->GetHealth(),InitialHealth);
            Test->TestTrue(TEXT("An enemy does not cancel its own attack windup"),Fixture->Enemy->GetTellState()==EAlphaEnemyTellState::Windup);
        }
        if (Scenario == EScenario::ShieldExposure)
        {
            if (!bCheckedWindup && Elapsed >= .8f)
            {
                bCheckedWindup = true;
                Test->TestFalse(TEXT("Shield stays exposed after ordinary stagger has recovered"),Fixture->Enemy->IsGuardActive());
            }
            if (Elapsed < 2.1f) return false;
            Test->TestTrue(TEXT("Shield recovers after its full exposure window"),Fixture->Enemy->IsGuardActive());
            return true;
        }
        if (Elapsed < (Scenario==EScenario::Melee ? .4f : 1.1f)) return false;
        const bool bHit = Scenario==EScenario::Shot || Scenario==EScenario::Melee;
        Test->TestEqual(TEXT("Only an accepted unobstructed release causes health damage"),
            Fixture->Human->GetCombatComponent()->GetHealth(),InitialHealth-(bHit?(Scenario==EScenario::Melee?8.f:10.f):0.f));
        if (Scenario != EScenario::Melee)
            Test->TestEqual(TEXT("A cancelled windup never emits a shot; a dodge still permits one release"),
                Fixture->Enemy->GetShotRevision(),Scenario==EScenario::Interrupt?0:1);
        return true;
    }
private:
    FAutomationTestBase* Test;
    TSharedPtr<FWorldFixture> Fixture;
    EScenario Scenario;
    float InitialHealth;
    double BeganAt;
    uint64 LastFrame = MAX_uint64;
    float Elapsed = 0.f;
    bool bChanged = false;
    bool bCheckedWindup = false;
};

bool QueueScenario(FAutomationTestBase* Test,EScenario Scenario)
{
    TSharedPtr<FWorldFixture> Fixture = MakeShared<FWorldFixture>(Scenario==EScenario::Melee?0:2,
        Scenario==EScenario::Melee?FVector(120,0,96):FVector(400,0,96));
    if (!Test->TestTrue(TEXT("Enemy authority fixture exists"),Fixture->Ready())) return false;
    Fixture->Enemy->Tick(.01f);
    Test->TestTrue(TEXT("A valid target begins telegraphed windup"),Fixture->Enemy->GetTellState()==EAlphaEnemyTellState::Windup);
    ADD_LATENT_AUTOMATION_COMMAND(FAdvanceScenario(Test,Fixture,Scenario));
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaShieldDefenseTest,"AlphaGame.EnemyTactics.Shield.FrontFlankAndConfiguration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAlphaShieldDefenseTest::RunTest(const FString& Parameters)
{
    AlphaEnemyTacticsTests::FWorldFixture Fixture(1,FVector(100,0,96));
    if (!TestTrue(TEXT("Shield fixture exists"),Fixture.Ready())) return false;
    UAlphaCombatComponent* Combat = Fixture.Enemy->GetCombatComponent();
    TestTrue(TEXT("Configured shield role is observable"),Fixture.Enemy->GetEnemyRole()==EAlphaEnemyRole::Shield);
    TestEqual(TEXT("Shield remains a modest health upgrade"),Combat->GetHealth(),75.f);
    Combat->ReceiveHit(20.f,5.f,Fixture.Human);
    TestEqual(TEXT("Real ReceiveHit uses frontal mitigation hook"),Combat->GetHealth(),70.f);
    TestEqual(TEXT("Blocking never reduces poise damage"),Combat->GetPoise(),40.f);
    Fixture.Enemy->ConfigureRole(1);
    Fixture.Human->SetActorLocation(FVector(0,100,96));
    Combat->ReceiveHit(20.f,0.f,Fixture.Human);
    TestEqual(TEXT("Side attack bypasses shield"),Combat->GetHealth(),55.f);
    Fixture.Enemy->ConfigureRole(1);
    Fixture.Human->SetActorLocation(FVector(-100,0,96));
    Combat->ReceiveHit(20.f,0.f,Fixture.Human);
    TestEqual(TEXT("Rear attack bypasses shield"),Combat->GetHealth(),55.f);
    Fixture.Enemy->ConfigureRole(2);
    TestEqual(TEXT("Gunner is fragile"),Combat->GetHealth(),42.f);
    Fixture.Enemy->ConfigureRole(-1);
    TestTrue(TEXT("Invalid role cannot replace the configured gunner"),Fixture.Enemy->GetEnemyRole()==EAlphaEnemyRole::Gunner);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaShieldExposureTest,"AlphaGame.EnemyTactics.Shield.PoiseBreakExposure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAlphaShieldExposureTest::RunTest(const FString& Parameters)
{
    using namespace AlphaEnemyTacticsTests;
    TSharedPtr<FWorldFixture> Fixture = MakeShared<FWorldFixture>(1,FVector(100,0,96));
    if (!TestTrue(TEXT("Shield fixture exists"),Fixture->Ready())) return false;
    Fixture->Enemy->GetCombatComponent()->ReceiveHit(20.f,45.f,Fixture->Human);
    TestFalse(TEXT("Poise break immediately opens the shield"),Fixture->Enemy->IsGuardActive());
    Fixture->Human->SetActorLocation(FVector(1500,0,96)); // Avoid starting another melee while observing recovery.
    ADD_LATENT_AUTOMATION_COMMAND(FAdvanceScenario(this,Fixture,EScenario::ShieldExposure));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaGunnerReleaseTest,"AlphaGame.EnemyTactics.Gunner.DelayedSingleRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAlphaGunnerReleaseTest::RunTest(const FString& Parameters)
{ return AlphaEnemyTacticsTests::QueueScenario(this,AlphaEnemyTacticsTests::EScenario::Shot); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaGunnerInterruptTest,"AlphaGame.EnemyTactics.Gunner.DamageCancelsWindup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAlphaGunnerInterruptTest::RunTest(const FString& Parameters)
{ return AlphaEnemyTacticsTests::QueueScenario(this,AlphaEnemyTacticsTests::EScenario::Interrupt); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaGunnerAvoidanceTest,"AlphaGame.EnemyTactics.Gunner.JumpDepthAndWallAvoidance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAlphaGunnerAvoidanceTest::RunTest(const FString& Parameters)
{
    using namespace AlphaEnemyTacticsTests;
    return QueueScenario(this,EScenario::Jump) && QueueScenario(this,EScenario::Depth) && QueueScenario(this,EScenario::Wall);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaMeleeTacticalWindupTest,"AlphaGame.EnemyTactics.Melee.OwnWindupDoesNotCancel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAlphaMeleeTacticalWindupTest::RunTest(const FString& Parameters)
{ return AlphaEnemyTacticsTests::QueueScenario(this,AlphaEnemyTacticsTests::EScenario::Melee); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaEnemyAdmissionTest,"AlphaGame.EnemyTactics.Authority.TwoAttackSlotsPerTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAlphaEnemyAdmissionTest::RunTest(const FString& Parameters)
{
    using namespace AlphaEnemyTacticsTests;
    FWorldFixture Fixture(0,FVector(100,0,96));
    if (!TestTrue(TEXT("Admission fixture exists"),Fixture.Ready())) return false;
    Fixture.Human->SetActorLocation(FVector(0,0,96));
    Fixture.Enemy->SetActorLocation(FVector(100,0,96));
    AAlphaEnemy* Second = Fixture.SpawnEnemy(0,FVector(-100,0,96));
    AAlphaEnemy* Third = Fixture.SpawnEnemy(0,FVector(0,100,96));
    if (!TestNotNull(TEXT("Second enemy exists"),Second) || !TestNotNull(TEXT("Third enemy exists"),Third)) return false;
    Fixture.Enemy->Tick(.01f); Second->Tick(.01f); Third->Tick(.01f);
    TestTrue(TEXT("First attacker has a slot"),Fixture.Enemy->GetTellState()==EAlphaEnemyTellState::Windup);
    TestTrue(TEXT("Second attacker has a slot"),Second->GetTellState()==EAlphaEnemyTellState::Windup);
    TestTrue(TEXT("Third attacker waits"),Third->GetTellState()==EAlphaEnemyTellState::Idle);
    Fixture.Enemy->GetCombatComponent()->ReceiveHit(10000.f,0.f,Fixture.Human);
    Third->Tick(.01f);
    TestTrue(TEXT("An attacker death releases its admission slot"),Third->GetTellState()==EAlphaEnemyTellState::Windup);
    return true;
}

#endif
