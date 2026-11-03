#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AlphaCompanion.h"
#include "Characters/Boss/AlphaForgeBoss.h"
#include "AIController.h"
#include "Abilities/AlphaAbilityComponent.h"
#include "Combat/AlphaCombatComponent.h"
#include "Coop/AlphaReviveComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "World/AlphaGameState.h"

namespace AlphaCompanionTests
{
/** Real authority actors/components, without an encounter game mode or a running physics simulation. */
class FCompanionWorld
{
public:
    FCompanionWorld()
    {
        UWorld::InitializationValues Settings;
        Settings.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
            .CreateAISystem(false).RequiresHitProxies(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
            true, ERHIFeatureLevel::Num, &Settings);
        if (!World) return;
        State = World->SpawnActor<AAlphaGameState>();
        World->SetGameState(State);
        DamageSource = World->SpawnActor<AActor>();
        if (!DamageSource) return;
        UAlphaCombatComponent* EnemyCombat = NewObject<UAlphaCombatComponent>(DamageSource);
        DamageSource->AddInstanceComponent(EnemyCombat);
        EnemyCombat->TeamId = 2;
        EnemyCombat->RegisterComponent();
        EnemyCombat->ResetVitals();
    }

    ~FCompanionWorld() { if (World) World->DestroyWorld(false); }
    bool IsReady() const { return World && State && DamageSource; }

    template<typename T>
    T* SpawnPawn(const FVector& Location)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<T>(T::StaticClass(), Location, FRotator::ZeroRotator, Params);
    }

    AAlphaCharacter* SpawnHuman(const FVector& Location)
    {
        AAlphaCharacter* Human = SpawnPawn<AAlphaCharacter>(Location);
        APlayerController* Controller = World ? World->SpawnActor<APlayerController>() : nullptr;
        if (!Human || !Controller) return nullptr;
        Controller->Possess(Human);
        Human->DispatchBeginPlay();
        return Human;
    }

    AAlphaCompanion* SpawnCompanion(const FVector& Location)
    {
        AAlphaCompanion* Bot = SpawnPawn<AAlphaCompanion>(Location);
        if (!Bot) return nullptr;
        // The production pawn also auto-possesses. A minimal test world may not initialize an AI system.
        if (!Bot->GetController())
        {
            AAIController* Controller = World->SpawnActor<AAIController>();
            if (!Controller) return nullptr;
            Controller->Possess(Bot);
        }
        Bot->DispatchBeginPlay();
        return Bot;
    }

    AAlphaCharacter* SpawnEnemy(const FVector& Location)
    {
        AAlphaCharacter* Enemy = SpawnPawn<AAlphaCharacter>(Location);
        if (!Enemy) return nullptr;
        Enemy->GetCombatComponent()->TeamId = 2;
        Enemy->DispatchBeginPlay();
        return Enemy;
    }

    void Down(AAlphaCharacter* Character) const
    {
        Character->GetCombatComponent()->ReceiveHit(100000.0f, 0.0f, DamageSource);
    }

    void CompleteRescue(AAlphaCharacter* Rescuer) const
    {
        Rescuer->GetReviveComponent()->TickComponent(3.0f, LEVELTICK_All, nullptr);
    }

    UWorld* World = nullptr;
    AAlphaGameState* State = nullptr;
    AActor* DamageSource = nullptr;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaCompanionWaitingAndTerminalTest,
    "AlphaGame.Companion.Authority.WaitingAndTerminal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaCompanionWaitingAndTerminalTest::RunTest(const FString& Parameters)
{
    AlphaCompanionTests::FCompanionWorld Fixture;
    if (!TestTrue(TEXT("Authority fixture initialized"), Fixture.IsReady())) return false;
    AAlphaCharacter* Human = Fixture.SpawnHuman(FVector(-600, 0, 100));
    AAlphaCompanion* Bot = Fixture.SpawnCompanion(FVector(0, 0, 100));
    AAlphaCharacter* Enemy = Fixture.SpawnEnemy(FVector(120, 0, 100));
    if (!TestNotNull(TEXT("Human spawned"), Human) || !TestNotNull(TEXT("Companion spawned"), Bot)
        || !TestNotNull(TEXT("Enemy spawned"), Enemy)) return false;
    Fixture.State->RoundState = EAlphaRoundState::Waiting;
    const float EnemyHealth = Enemy->GetCombatComponent()->GetHealth();
    Bot->Tick(0.11f);
    TestTrue(TEXT("Bot uses an AI controller and the ordinary friendly team"), Cast<AAIController>(Bot->GetController()) && Bot->IsCoopParticipant());
    TestTrue(TEXT("Waiting companion follows"), Bot->GetCompanionAction() == EAlphaCompanionAction::Follow);
    TestEqual(TEXT("Waiting does not damage enemies"), Enemy->GetCombatComponent()->GetHealth(), EnemyHealth);
    TestEqual(TEXT("Waiting does not consume skill cooldown"), Bot->GetAbilityComponent()->GetSkillRemaining(0), 0.0f);
    Fixture.State->RoundState = EAlphaRoundState::Victory;
    Bot->Tick(0.11f);
    TestTrue(TEXT("Terminal state stops decisions"), Bot->GetCompanionAction() == EAlphaCompanionAction::Idle);
    TestTrue(TEXT("Terminal state clears movement input"), Bot->GetPendingMovementInputVector().IsNearlyZero());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaCompanionRescuesDownedHumanTest,
    "AlphaGame.Companion.Authority.RescuesOnlyHumanDown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaCompanionRescuesDownedHumanTest::RunTest(const FString& Parameters)
{
    AlphaCompanionTests::FCompanionWorld Fixture;
    if (!TestTrue(TEXT("Authority fixture initialized"), Fixture.IsReady())) return false;
    AAlphaCharacter* Human = Fixture.SpawnHuman(FVector(120, 0, 100));
    AAlphaCompanion* Bot = Fixture.SpawnCompanion(FVector(0, 0, 100));
    if (!TestNotNull(TEXT("Human spawned"), Human) || !TestNotNull(TEXT("Companion spawned"), Bot)) return false;
    Fixture.State->RoundState = EAlphaRoundState::Fighting;
    Fixture.Down(Human);
    Bot->Tick(0.11f);
    TestTrue(TEXT("Only human being down does not disable a living companion"), Bot->GetCompanionAction() == EAlphaCompanionAction::Revive);
    TestTrue(TEXT("Bot claims human through shared rescue component"), Bot->GetReviveComponent()->GetReviveTarget() == Human);
    TestFalse(TEXT("A rescue claim does not instantly heal"), Human->GetCombatComponent()->IsAlive());
    Fixture.CompleteRescue(Bot);
    TestTrue(TEXT("Shared three-second rescue revives human"), Human->GetCombatComponent()->IsAlive() && !Human->IsDefeated());
    TestEqual(TEXT("Human receives standard forty-percent health"), Human->GetCombatComponent()->GetHealth(), Human->GetCombatComponent()->GetMaxHealth() * 0.4f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaCompanionClaimDamageRetryTest,
    "AlphaGame.Companion.Authority.ExclusiveClaimAndRetry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaCompanionClaimDamageRetryTest::RunTest(const FString& Parameters)
{
    AlphaCompanionTests::FCompanionWorld Fixture;
    if (!TestTrue(TEXT("Authority fixture initialized"), Fixture.IsReady())) return false;
    AAlphaCharacter* Human = Fixture.SpawnHuman(FVector(120, 0, 100));
    AAlphaCompanion* First = Fixture.SpawnCompanion(FVector(0, -90, 100));
    AAlphaCompanion* Second = Fixture.SpawnCompanion(FVector(0, 90, 100));
    if (!TestNotNull(TEXT("Human spawned"), Human) || !TestNotNull(TEXT("First companion spawned"), First)
        || !TestNotNull(TEXT("Second companion spawned"), Second)) return false;
    Fixture.State->RoundState = EAlphaRoundState::Fighting;
    Fixture.Down(Human);
    First->Tick(0.11f);
    Second->Tick(0.11f);
    TestTrue(TEXT("First rescuer retains exclusive claim"), Human->GetReviveComponent()->GetCurrentRescuer() == First);
    TestNull(TEXT("Second bot does not compete for claim"), Second->GetReviveComponent()->GetReviveTarget());
    First->GetCombatComponent()->ReceiveHit(10.0f, 0.0f, Fixture.DamageSource);
    First->Tick(0.11f);
    TestNull(TEXT("Damaged bot does not immediately restart rescue"), First->GetReviveComponent()->GetReviveTarget());
    Second->Tick(0.11f);
    TestTrue(TEXT("Another healthy bot can take released claim"), Human->GetReviveComponent()->GetCurrentRescuer() == Second);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaHumanRescuesCompanionTest,
    "AlphaGame.Companion.Authority.HumanRevivesBotAndAerialDown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaHumanRescuesCompanionTest::RunTest(const FString& Parameters)
{
    AlphaCompanionTests::FCompanionWorld Fixture;
    if (!TestTrue(TEXT("Authority fixture initialized"), Fixture.IsReady())) return false;
    AAlphaCharacter* Human = Fixture.SpawnHuman(FVector(-100, 0, 100));
    AAlphaCompanion* Bot = Fixture.SpawnCompanion(FVector(0, 0, 200));
    if (!TestNotNull(TEXT("Human spawned"), Human) || !TestNotNull(TEXT("Companion spawned"), Bot)) return false;
    Fixture.State->RoundState = EAlphaRoundState::Fighting;
    Bot->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    Fixture.Down(Bot);
    Bot->Tick(0.11f);
    TestTrue(TEXT("Bot remains downed without self revival"), Bot->IsDefeated() && !Bot->GetCombatComponent()->IsAlive());
    TestTrue(TEXT("Aerial down preserves falling instead of suspending a corpse"), Bot->GetCharacterMovement()->MovementMode == MOVE_Falling);
    Human->GetReviveComponent()->BeginRevive();
    TestTrue(TEXT("Human can claim a downed AI-controlled ally"), Human->GetReviveComponent()->GetReviveTarget() == Bot);
    Fixture.CompleteRescue(Human);
    TestTrue(TEXT("Human restores bot using ordinary rescue"), Bot->GetCombatComponent()->IsAlive() && !Bot->IsDefeated());
    TestTrue(TEXT("Bot movement restores after revival"), Bot->GetCharacterMovement()->MovementMode == MOVE_Walking);
    Bot->ResetCompanionBrain();
    TestTrue(TEXT("Brain reset clears action"), Bot->GetCompanionAction() == EAlphaCompanionAction::Idle);
    TestTrue(TEXT("Brain reset clears pending input"), Bot->GetPendingMovementInputVector().IsNearlyZero());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaCompanionSharedCombatTest,
    "AlphaGame.Companion.Authority.SharedSkillsDamageAndAlignment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaCompanionSharedCombatTest::RunTest(const FString& Parameters)
{
    AlphaCompanionTests::FCompanionWorld Fixture;
    if (!TestTrue(TEXT("Authority fixture initialized"), Fixture.IsReady())) return false;
    AAlphaCharacter* Human = Fixture.SpawnHuman(FVector(-500, 0, 100));
    AAlphaCompanion* Bot = Fixture.SpawnCompanion(FVector(0, 0, 100));
    AAlphaCharacter* Enemy = Fixture.SpawnEnemy(FVector(125, 0, 100));
    if (!TestNotNull(TEXT("Human spawned"), Human) || !TestNotNull(TEXT("Companion spawned"), Bot)
        || !TestNotNull(TEXT("Enemy spawned"), Enemy)) return false;
    Fixture.State->RoundState = EAlphaRoundState::Fighting;
    Bot->SetArchetype(2);
    const float EnemyHealth = Enemy->GetCombatComponent()->GetHealth();
    Bot->Tick(0.11f);
    TestTrue(TEXT("Companion enters attack behavior"), Bot->GetCompanionAction() == EAlphaCompanionAction::Attack);
    TestTrue(TEXT("Ordinary skill damages enemy"), Enemy->GetCombatComponent()->GetHealth() < EnemyHealth);
    TestTrue(TEXT("Bot consumes the normal ability cooldown"), Bot->GetAbilityComponent()->GetSkillRemaining(0) > 0.0f);
    const float BotHealth = Bot->GetCombatComponent()->GetHealth();
    Bot->GetCombatComponent()->ReceiveHit(20.0f, 0.0f, Fixture.DamageSource);
    TestEqual(TEXT("AI receives normal enemy damage"), Bot->GetCombatComponent()->GetHealth(), BotHealth - 20.0f);
    Bot->GetCombatComponent()->ReceiveHit(20.0f, 0.0f, Human);
    TestEqual(TEXT("Human friendly fire remains disabled"), Bot->GetCombatComponent()->GetHealth(), BotHealth - 20.0f);

    // Reset actors for a depth-misaligned blade skill; the bot must move, not waste a cooldown.
    Bot->ResetForRound();
    Bot->ResetCompanionBrain();
    Bot->SetArchetype(0);
    Enemy->SetActorLocation(FVector(30, 260, 100));
    Bot->Tick(0.11f);
    TestEqual(TEXT("Misaligned first skill remains ready"), Bot->GetAbilityComponent()->GetSkillRemaining(0), 0.0f);
    TestEqual(TEXT("Misaligned second skill remains ready"), Bot->GetAbilityComponent()->GetSkillRemaining(1), 0.0f);
    TestTrue(TEXT("Bot moves to align instead"), !Bot->GetPendingMovementInputVector().IsNearlyZero());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaCompanionBossEvadeTest,
    "AlphaGame.Companion.Authority.BossFootprintEvadeAndSafeStop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaCompanionBossEvadeTest::RunTest(const FString& Parameters)
{
    AlphaCompanionTests::FCompanionWorld Fixture;
    if (!TestTrue(TEXT("Authority fixture initialized"), Fixture.IsReady())) return false;
    AAlphaCharacter* Human = Fixture.SpawnHuman(FVector(-600, 150, 100));
    AAlphaCompanion* Bot = Fixture.SpawnCompanion(FVector(160, 0, 100));
    AAlphaForgeBoss* Boss = Fixture.SpawnPawn<AAlphaForgeBoss>(FVector(0, 0, 120));
    if (!TestNotNull(TEXT("Human spawned"), Human) || !TestNotNull(TEXT("Companion spawned"), Bot)
        || !TestNotNull(TEXT("Boss spawned"), Boss)) return false;
    Boss->DispatchBeginPlay();
    Fixture.State->RoundState = EAlphaRoundState::Fighting;
    Boss->Tick(0.11f);
    if (!TestTrue(TEXT("Boss starts a real windup"), Boss->GetAttackState() == EAlphaForgeAttackState::Windup)) return false;
    TestTrue(TEXT("Bot starts inside authority footprint"), AAlphaForgeBoss::IsPointInAttackZone(Boss->GetAttackType(),
        Boss->GetTelegraphOrigin(), Boss->GetTelegraphDirection(), Bot->GetActorLocation()));
    Bot->Tick(0.11f);
    TestTrue(TEXT("Danger takes priority over attack"), Bot->GetCompanionAction() == EAlphaCompanionAction::Evade);
    const FVector SafePoint = Bot->GetDesiredDestination();
    TestFalse(TEXT("Chosen destination is outside exact boss footprint"), AAlphaForgeBoss::IsPointInAttackZone(Boss->GetAttackType(),
        Boss->GetTelegraphOrigin(), Boss->GetTelegraphDirection(), SafePoint));
    TestTrue(TEXT("Destination stays inside walkable arena bounds"), FMath::Abs(SafePoint.X) <= 1250.0f && FMath::Abs(SafePoint.Y) <= 300.0f);
    // Test positioning simulates arrival. Production AI always walks via CharacterMovement and never teleports.
    Bot->SetActorLocation(SafePoint);
    Bot->Tick(0.11f);
    TestTrue(TEXT("Safe bot holds position until the tell resolves"), Bot->GetCompanionAction() == EAlphaCompanionAction::Evade);
    TestTrue(TEXT("Safe stop clears movement input"), Bot->GetPendingMovementInputVector().IsNearlyZero());
    TestFalse(TEXT("Evading grants no invulnerability"), Bot->GetCombatComponent()->IsInvulnerable());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
