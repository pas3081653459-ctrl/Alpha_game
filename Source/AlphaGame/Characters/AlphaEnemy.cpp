#include "Characters/AlphaEnemy.h"

#include "AIController.h"
#include "Combat/AlphaCombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "CollisionQueryParams.h"
#include "Net/UnrealNetwork.h"
#include "Rules/EnemyTacticsRules.h"
#include "Visuals/AlphaCharacterVisualComponent.h"
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
    if (HasAuthority())
    {
        CombatComponent->OnPoiseBroken.AddDynamic(this, &AAlphaEnemy::HandleShieldBroken);
        ConfigureRole(static_cast<int32>(EnemyRole));
    }
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
    if (!HasAuthority() || IsDefeated() || !CombatComponent->IsAlive()) return;
    UpdateTacticalAttack();
    if (TellState != EAlphaEnemyTellState::Idle || !CombatComponent->CanAct()) return;

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
    const bool bClear = AlphaLayoutRules::SegmentClear(Chapter, {LocalFrom.X, LocalFrom.Y}, {LocalTo.X, LocalTo.Y}, 0.)
        && HasLineOfSightTo(Target.Get());
    const bool bInReach = EnemyRole == EAlphaEnemyRole::Gunner
        ? AlphaEnemyRules::ShouldShoot(Delta.Size2D(), Delta.Z, bClear)
        : FMath::Abs(FVector::DotProduct(Delta, GetActorForwardVector())) <= AttackDistance
            && FMath::Abs(FVector::DotProduct(Delta, GetActorRightVector())) <= AttackDepthTolerance
            && FMath::Abs(Delta.Z) <= 100.0f && bClear;
    if (bInReach)
    {
        GetCharacterMovement()->StopMovementImmediately();
        if (HasAttackSlot(Target.Get())) BeginTacticalAttack();
        return;
    }
    RouteRefreshCountdown -= DeltaSeconds;
    if (RouteRefreshCountdown <= 0.f || RoutedTarget != Target)
    {
        RouteRefreshCountdown = .15f;
        RoutedTarget = Target;
        AlphaLayoutRules::Point Destination{LocalTo.X,LocalTo.Y};
        if (EnemyRole == EAlphaEnemyRole::Gunner && bClear && Delta.SizeSquared2D() < FMath::Square(250.f))
        {
            FVector Away = -Delta.GetSafeNormal2D();
            if (Away.IsNearlyZero()) Away = -GetActorForwardVector();
            Destination = AlphaLayoutRules::Project(Chapter,
                {LocalFrom.X + Away.X * 350., LocalFrom.Y + Away.Y * 350.});
        }
        const AlphaLayoutRules::Point Waypoint = AlphaLayoutRules::NextWaypoint(Chapter,
            {LocalFrom.X, LocalFrom.Y}, Destination);
        RouteWaypoint = ChapterOrigin + FVector(Waypoint.X, Waypoint.Y, 0.);
    }
    const FVector RouteDelta = RouteWaypoint - GetActorLocation();
    if (RouteDelta.SizeSquared2D() > .25)
    {
        FaceMoveIntent(RouteDelta);
        AddMovementInput(RouteDelta.GetSafeNormal2D());
    }
}

void AAlphaEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AAlphaEnemy, EnemyRole);
    DOREPLIFETIME(AAlphaEnemy, TellState);
    DOREPLIFETIME(AAlphaEnemy, TellOrigin);
    DOREPLIFETIME(AAlphaEnemy, TellDirection);
    DOREPLIFETIME(AAlphaEnemy, TellStartedAt);
    DOREPLIFETIME(AAlphaEnemy, TellResolveAt);
    DOREPLIFETIME(AAlphaEnemy, ShieldOpenUntil);
    DOREPLIFETIME(AAlphaEnemy, ShotRevision);
}

double AAlphaEnemy::ServerTime() const
{
    if (const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>()) return State->GetServerWorldTimeSeconds();
    return GetWorld()->GetTimeSeconds();
}

void AAlphaEnemy::ConfigureRole(int32 RoleIndex)
{
    if (!HasAuthority() || !AlphaEnemyRules::ValidRole(RoleIndex)) return;
    CombatComponent->CancelCurrentAttack();
    CombatComponent->ConfigureEnemyProfile();
    EnemyRole = static_cast<EAlphaEnemyRole>(RoleIndex);
    if (EnemyRole == EAlphaEnemyRole::Shield) CombatComponent->ConfigureVitals(75.f,45.f);
    else if (EnemyRole == EAlphaEnemyRole::Gunner) CombatComponent->ConfigureVitals(42.f,22.f);
    GetCharacterMovement()->MaxWalkSpeed = EnemyRole == EAlphaEnemyRole::Shield ? 170.f : (EnemyRole == EAlphaEnemyRole::Gunner ? 190.f : 210.f);
    TellState = EAlphaEnemyTellState::Idle;
    ShieldOpenUntil = 0.;
    AttackTarget.Reset();
    OnRep_Role();
    ForceNetUpdate();
}

void AAlphaEnemy::OnRep_Role()
{
    GetVisualComponent()->RefreshAppearance(GetArchetype(),true,false);
}

float AAlphaEnemy::GetTellProgress() const
{
    return static_cast<float>(AlphaEnemyRules::TellProgress(ServerTime(),TellStartedAt,TellResolveAt));
}

bool AAlphaEnemy::IsGuardActive() const
{
    return EnemyRole == EAlphaEnemyRole::Shield && CombatComponent->IsAlive() && CombatComponent->CanAct()
        && !IsDefeated() && TellState == EAlphaEnemyTellState::Idle && ServerTime() >= ShieldOpenUntil;
}

void AAlphaEnemy::ModifyIncomingHit(float& Damage, float& PoiseDamage, AActor* Source)
{
    // Full poise damage remains: repeated frontal attacks break the guard,
    // while a flank or aerial cross-up receives normal health damage.
    (void)PoiseDamage;
    if (!HasAuthority() || !IsValid(Source)) return;
    const FVector Offset = Source->GetActorLocation() - GetActorLocation();
    const double Distance = Offset.Size2D();
    const double Dot = FVector::DotProduct(GetActorForwardVector(),Offset.GetSafeNormal2D());
    if (AlphaEnemyRules::ShieldBlocks(IsGuardActive(),Dot,Distance,Offset.Z))
        Damage = static_cast<float>(AlphaEnemyRules::GuardedDamage(Damage,true));
}

void AAlphaEnemy::HandleShieldBroken()
{
    if (!HasAuthority() || EnemyRole != EAlphaEnemyRole::Shield) return;
    ShieldOpenUntil = ServerTime() + AlphaEnemyRules::ShieldOpenSeconds;
    EnterRecovery(.65f);
    ForceNetUpdate();
}

bool AAlphaEnemy::HasLineOfSightTo(AAlphaCharacter* Candidate) const
{
    if (!IsValid(Candidate)) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AlphaEnemySight),false,this);
    Query.AddIgnoredActor(Candidate);
    return !GetWorld()->LineTraceTestByChannel(GetActorLocation(),Candidate->GetActorLocation(),ECC_Visibility,Query);
}

bool AAlphaEnemy::HasAttackSlot(AAlphaCharacter* Candidate) const
{
    int32 Active = 0;
    for (TActorIterator<AAlphaEnemy> It(GetWorld()); It; ++It)
    {
        if (*It != this && It->GetCombatComponent()->IsAlive() && !It->IsActorBeingDestroyed()
            && It->TellState != EAlphaEnemyTellState::Idle && It->AttackTarget.Get() == Candidate) ++Active;
    }
    return AlphaEnemyRules::HasAttackSlot(Active);
}

void AAlphaEnemy::BeginTacticalAttack()
{
    if (!Target.IsValid() || !CombatComponent->CanAct() || !HasAttackSlot(Target.Get())) return;
    TellStartedAt = ServerTime();
    TellResolveAt = TellStartedAt + (EnemyRole == EAlphaEnemyRole::Gunner ? AlphaEnemyRules::GunnerWindup : .30);
    AttackDamageRevision = CombatComponent->GetDamageRevision();
    AttackTarget = Target;
    AttackBodyOrigin = GetActorLocation();
    TellDirection = GetActorForwardVector().GetSafeNormal2D();
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    const int32 Chapter = State ? State->ChapterIndex : 0;
    const FVector Local = GetActorLocation() - AAlphaGameState::GetChapterOrigin(Chapter);
    TellOrigin = FVector(GetActorLocation().X,GetActorLocation().Y,AlphaLayoutRules::SurfaceHeight(Chapter,{Local.X,Local.Y}) + 8.);
    if (EnemyRole != EAlphaEnemyRole::Gunner)
    {
        CombatComponent->TryAttack(false);
        if (!CombatComponent->IsAttackInProgress()) { AttackTarget.Reset(); return; }
    }
    TellState = EAlphaEnemyTellState::Windup;
    TacticalRecoveryUntil = TellResolveAt + (EnemyRole == EAlphaEnemyRole::Gunner ? AlphaEnemyRules::GunnerRecovery : .55);
    ForceNetUpdate();
}

void AAlphaEnemy::EnterRecovery(float Duration)
{
    TellState = EAlphaEnemyTellState::Recovery;
    TacticalRecoveryUntil = ServerTime() + Duration;
    GetCharacterMovement()->StopMovementImmediately();
    ForceNetUpdate();
}

void AAlphaEnemy::UpdateTacticalAttack()
{
    if (TellState == EAlphaEnemyTellState::Idle) return;
    if (TellState == EAlphaEnemyTellState::Windup)
    {
        if (!CombatComponent->CanAct() || CombatComponent->GetDamageRevision() != AttackDamageRevision
            || FVector::DistSquared2D(AttackBodyOrigin,GetActorLocation()) > FMath::Square(90.f))
        {
            CombatComponent->CancelCurrentAttack();
            EnterRecovery(.45f);
            return;
        }
        if (ServerTime() >= TellResolveAt)
        {
            if (EnemyRole == EAlphaEnemyRole::Gunner) ResolveGunnerShot();
            TellState = EAlphaEnemyTellState::Recovery;
            ForceNetUpdate();
        }
    }
    if (TellState == EAlphaEnemyTellState::Recovery && ServerTime() >= TacticalRecoveryUntil)
    {
        TellState = EAlphaEnemyTellState::Idle;
        AttackTarget.Reset();
        ForceNetUpdate();
    }
}

void AAlphaEnemy::ResolveGunnerShot()
{
    // One confirmed release, even if everybody dodged; cancellation never increments it.
    ShotRevision = ShotRevision == MAX_int32 ? 0 : ShotRevision + 1;
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    const int32 Chapter = State ? State->ChapterIndex : 0;
    const FVector ChapterOrigin = AAlphaGameState::GetChapterOrigin(Chapter);
    const FVector Right = FVector::CrossProduct(FVector::UpVector,TellDirection);
    for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
    {
        AAlphaCharacter* Victim = *It;
        if (!Victim->IsCoopParticipant() || !Victim->GetCombatComponent()->IsAlive()) continue;
        const FVector Feet = Victim->GetActorLocation() - FVector(0,0,Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        const FVector Offset = Feet - (TellOrigin - FVector(0,0,8));
        if (!AlphaEnemyRules::InGunnerLine(FVector::DotProduct(Offset,TellDirection),FVector::DotProduct(Offset,Right),Offset.Z)) continue;
        const FVector LocalTo = Feet - ChapterOrigin;
        const FVector LocalFrom = TellOrigin - ChapterOrigin;
        if (!AlphaLayoutRules::SegmentClear(Chapter,{LocalFrom.X,LocalFrom.Y},{LocalTo.X,LocalTo.Y},0.)) continue;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(AlphaGunnerHit),false,this);
        Query.AddIgnoredActor(Victim);
        if (GetWorld()->LineTraceTestByChannel(TellOrigin + FVector(0,0,35),Victim->GetActorLocation(),ECC_Visibility,Query)) continue;
        Victim->GetCombatComponent()->ReceiveHit(static_cast<float>(AlphaEnemyRules::GunnerDamage),8.f,this);
    }
}

void AAlphaEnemy::HandleDeath()
{
    Super::HandleDeath();
    if (HasAuthority())
    {
        TellState = EAlphaEnemyTellState::Idle;
        AttackTarget.Reset();
        SetLifeSpan(2.0f);
    }
}
