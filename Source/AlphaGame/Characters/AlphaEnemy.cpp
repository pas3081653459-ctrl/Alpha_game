#include "Characters/AlphaEnemy.h"

#include "AIController.h"
#include "Combat/AlphaCombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rules/LevelLayoutRules.h"
#include "World/AlphaGameState.h"

AAlphaEnemy::AAlphaEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    Tags.Remove(TEXT("Player"));
    Tags.AddUnique(TEXT("Enemy"));
    CombatComponent->TeamId = 2;
    GetCharacterMovement()->MaxWalkSpeed = 210.0f;
    PrototypeBody->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.25f));
    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AAlphaEnemy::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority()) CombatComponent->ConfigureEnemyProfile();
}

void AAlphaEnemy::FindTarget()
{
    Target.Reset();
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    const int32 Chapter = State ? State->ChapterIndex : 0;
    const FVector ChapterOrigin = AAlphaGameState::GetChapterOrigin(Chapter);
    float NearestDistanceSquared = TNumericLimits<float>::Max();
    for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
    {
        AAlphaCharacter* Candidate = *It;
        if (!Candidate->IsCoopParticipant() || Candidate->IsDefeated() || !Candidate->GetCombatComponent()->IsAlive()) continue;
        const FVector Local = Candidate->GetActorLocation() - ChapterOrigin;
        if (!AlphaLayoutRules::IsWalkable(Chapter, {Local.X, Local.Y})) continue;
        const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared < NearestDistanceSquared)
        {
            NearestDistanceSquared = DistanceSquared;
            Target = Candidate;
        }
    }
}

void AAlphaEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || IsDefeated() || !CombatComponent->CanAct()) return;

    RetargetCountdown -= DeltaSeconds;
    if (RetargetCountdown <= 0.0f || !Target.IsValid() || !Target->IsCoopParticipant() || !Target->GetCombatComponent()->IsAlive())
    {
        FindTarget();
        RetargetCountdown = 0.25f;
    }
    if (!Target.IsValid()) return;

    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    const int32 Chapter = State ? State->ChapterIndex : 0;
    const FVector ChapterOrigin = AAlphaGameState::GetChapterOrigin(Chapter);
    const FVector LocalFrom = GetActorLocation() - ChapterOrigin;
    const FVector LocalTo = Target->GetActorLocation() - ChapterOrigin;
    const FVector Delta = Target->GetActorLocation() - GetActorLocation();
    FaceMoveIntent(Delta);
    const bool bInReach = FMath::Abs(FVector::DotProduct(Delta, GetActorForwardVector())) <= AttackDistance
        && FMath::Abs(FVector::DotProduct(Delta, GetActorRightVector())) <= AttackDepthTolerance
        && FMath::Abs(Delta.Z) <= 100.0f
        && AlphaLayoutRules::SegmentClear(Chapter, {LocalFrom.X, LocalFrom.Y}, {LocalTo.X, LocalTo.Y}, 0.);
    if (bInReach)
    {
        GetCharacterMovement()->StopMovementImmediately();
        CombatComponent->TryAttack(false);
    }
    else
    {
        RouteRefreshCountdown -= DeltaSeconds;
        if (RouteRefreshCountdown <= 0.f || RoutedTarget != Target)
        {
            RouteRefreshCountdown = .15f;
            RoutedTarget = Target;
            const AlphaLayoutRules::Point Waypoint = AlphaLayoutRules::NextWaypoint(Chapter,
                {LocalFrom.X, LocalFrom.Y}, {LocalTo.X, LocalTo.Y});
            RouteWaypoint = ChapterOrigin + FVector(Waypoint.X, Waypoint.Y, 0.);
        }
        const FVector RouteDelta = RouteWaypoint - GetActorLocation();
        if (RouteDelta.SizeSquared2D() > .25)
        {
            const FVector RouteDirection = RouteDelta.GetSafeNormal2D();
            FaceMoveIntent(RouteDirection);
            AddMovementInput(RouteDirection);
        }
    }
}

void AAlphaEnemy::HandleDeath()
{
    Super::HandleDeath();
    if (HasAuthority()) SetLifeSpan(2.0f);
}
