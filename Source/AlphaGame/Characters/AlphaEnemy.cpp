#include "Characters/AlphaEnemy.h"

#include "AIController.h"
#include "Combat/AlphaCombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

AAlphaEnemy::AAlphaEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    Tags.Remove(TEXT("Player"));
    Tags.AddUnique(TEXT("Enemy"));
    CombatComponent->TeamId = 2;
    GetCharacterMovement()->MaxWalkSpeed = 250.0f;
    PrototypeBody->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.25f));
    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AAlphaEnemy::FindTarget()
{
    Target.Reset();
    float NearestDistanceSquared = TNumericLimits<float>::Max();
    for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
    {
        AAlphaCharacter* Candidate = *It;
        if (!Candidate->IsCoopParticipant() || Candidate->IsDefeated() || !Candidate->GetCombatComponent()->IsAlive()) continue;
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

    const FVector Delta = Target->GetActorLocation() - GetActorLocation();
    FaceAlongLane(Delta.X);
    const bool bInReach = FMath::Abs(Delta.X) <= AttackDistance
        && FMath::Abs(Delta.Y) <= AttackDepthTolerance
        && FMath::Abs(Delta.Z) <= 100.0f;
    if (bInReach)
    {
        CombatComponent->TryAttack(false);
    }
    else
    {
        AddMovementInput(FVector(Delta.X, Delta.Y, 0.0f).GetSafeNormal());
    }
}

void AAlphaEnemy::HandleDeath()
{
    Super::HandleDeath();
    if (HasAuthority()) SetLifeSpan(2.0f);
}
