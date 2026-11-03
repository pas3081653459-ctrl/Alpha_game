#include "Characters/AlphaCompanion.h"

#include "AIController.h"
#include "Abilities/AlphaAbilityComponent.h"
#include "Characters/Boss/AlphaForgeBoss.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Combat/AlphaCombatComponent.h"
#include "Coop/AlphaReviveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Rules/CompanionRules.h"
#include "Rules/LevelLayoutRules.h"
#include "World/AlphaGameState.h"

namespace AlphaCompanionRoute
{
int32 Chapter(const UWorld* World)
{
    const AAlphaGameState* State = World ? World->GetGameState<AAlphaGameState>() : nullptr;
    return State ? State->ChapterIndex : 0;
}

AlphaLayoutRules::Point Local(const UWorld* World, const FVector& Location)
{
    const FVector Origin = AAlphaGameState::GetChapterOrigin(Chapter(World));
    return {Location.X - Origin.X, Location.Y - Origin.Y};
}

bool Contains(const UWorld* World, const FVector& Location)
{
    return AlphaLayoutRules::IsWalkable(Chapter(World), Local(World, Location));
}

bool Clear(const UWorld* World, const FVector& From, const FVector& To, double Margin = 45.)
{
    return AlphaLayoutRules::SegmentClear(Chapter(World), Local(World, From), Local(World, To), Margin);
}
}

AAlphaCompanion::AAlphaCompanion()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    Tags.AddUnique(TEXT("Companion"));
    CombatComponent->TeamId = 1;
    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AAlphaCompanion::BeginPlay()
{
    Super::BeginPlay();
    SetActorTickEnabled(HasAuthority());
    DesiredDestination = GetActorLocation();
    if (HasAuthority()) CombatComponent->OnHit.AddDynamic(this, &AAlphaCompanion::HandleCompanionHit);
}

void AAlphaCompanion::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        CombatComponent->OnHit.RemoveDynamic(this, &AAlphaCompanion::HandleCompanionHit);
        GetReviveComponent()->EndRevive();
    }
    Super::EndPlay(EndPlayReason);
}

void AAlphaCompanion::HandleDeath()
{
    Super::HandleDeath();
    if (HasAuthority())
    {
        DesiredMove = FVector::ZeroVector;
        SetAction(EAlphaCompanionAction::Idle);
    }
}

double AAlphaCompanion::ServerTime() const
{
    const AAlphaGameState* State = GetWorld() ? GetWorld()->GetGameState<AAlphaGameState>() : nullptr;
    return State ? State->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
}

void AAlphaCompanion::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority()) return;
    DecisionAccumulator += DeltaSeconds;
    if (DecisionAccumulator >= 0.1f)
    {
        DecisionAccumulator = 0.0f;
        Decide();
    }
    // Feeding CharacterMovement every frame avoids a 10 Hz input pulse slowing the pawn down.
    if (IsCoopParticipant() && !IsDefeated() && CombatComponent->CanAct() && !DesiredMove.IsNearlyZero())
    {
        AddMovementInput(DesiredMove);
    }
}

void AAlphaCompanion::SetAction(EAlphaCompanionAction NewAction)
{
    if (BotAction == NewAction) return;
    BotAction = NewAction;
    ForceNetUpdate();
}

FString AAlphaCompanion::GetBehaviorLabel() const
{
    switch (BotAction)
    {
    case EAlphaCompanionAction::Follow: return TEXT("Follow");
    case EAlphaCompanionAction::Attack: return TEXT("Attack");
    case EAlphaCompanionAction::Revive: return TEXT("Revive");
    case EAlphaCompanionAction::Evade: return TEXT("Evade");
    default: return TEXT("Idle");
    }
}

AAlphaCharacter* AAlphaCompanion::FindHumanAnchor() const
{
    AAlphaCharacter* Living = nullptr;
    AAlphaCharacter* Downed = nullptr;
    float LivingDistance = TNumericLimits<float>::Max();
    float DownedDistance = TNumericLimits<float>::Max();
    for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
    {
        AAlphaCharacter* Candidate = *It;
        if (!Candidate->IsPlayerControlled() || !Candidate->IsCoopParticipant()
            || !AlphaCompanionRoute::Contains(GetWorld(), Candidate->GetActorLocation())) continue;
        const float Distance = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
        if (Candidate->GetCombatComponent()->IsAlive() && !Candidate->IsDefeated())
        {
            if (Distance < LivingDistance) { Living = Candidate; LivingDistance = Distance; }
        }
        else if (Distance < DownedDistance) { Downed = Candidate; DownedDistance = Distance; }
    }
    return Living ? Living : Downed;
}

AAlphaCharacter* AAlphaCompanion::FindDownedAlly() const
{
    if (ServerTime() < RescueRetryAt) return nullptr;
    AAlphaCharacter* Closest = nullptr;
    float BestDistance = TNumericLimits<float>::Max();
    for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
    {
        AAlphaCharacter* Candidate = *It;
        if (Candidate == this || !Candidate->IsCoopParticipant() || !Candidate->IsDefeated()
            || Candidate->GetCombatComponent()->IsAlive() || IsDangerous(Candidate->GetActorLocation())
            || !AlphaCompanionRoute::Contains(GetWorld(), Candidate->GetActorLocation())) continue;
        AAlphaCharacter* Rescuer = Candidate->GetReviveComponent()->GetCurrentRescuer();
        if (IsValid(Rescuer) && Rescuer != this) continue;
        const float Distance = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
        if (Distance < BestDistance) { Closest = Candidate; BestDistance = Distance; }
    }
    return Closest;
}

AAlphaCharacter* AAlphaCompanion::FindEnemy(const AAlphaCharacter* Human) const
{
    if (!Human) return nullptr;
    AAlphaCharacter* Closest = nullptr;
    float BestDistance = TNumericLimits<float>::Max();
    for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
    {
        AAlphaCharacter* Candidate = *It;
        if (Candidate == this || Candidate->GetCombatComponent()->TeamId != 2
            || !Candidate->GetCombatComponent()->IsAlive() || Candidate->IsDefeated()
            || !AlphaCompanionRoute::Contains(GetWorld(), Candidate->GetActorLocation())) continue;
        // Stay with the human instead of dragging a solo player toward the opposite end of the arena.
        if (FVector::DistSquared2D(Human->GetActorLocation(), Candidate->GetActorLocation()) > FMath::Square(1100.0f)) continue;
        const float Distance = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
        if (Distance < BestDistance) { Closest = Candidate; BestDistance = Distance; }
    }
    return Closest;
}

void AAlphaCompanion::GatherBossThreats()
{
    Threats.Reset();
    for (TActorIterator<AAlphaForgeBoss> It(GetWorld()); It; ++It)
    {
        AAlphaForgeBoss* Boss = *It;
        if (Boss->GetCombatComponent()->IsAlive() && Boss->GetAttackState() == EAlphaForgeAttackState::Windup) Threats.Add(Boss);
    }
}

bool AAlphaCompanion::IsDangerous(const FVector& Location, bool bMargin) const
{
    const FVector Offsets[] = {FVector::ZeroVector, FVector(45, 0, 0), FVector(-45, 0, 0), FVector(0, 45, 0), FVector(0, -45, 0)};
    for (const TWeakObjectPtr<AAlphaForgeBoss>& WeakBoss : Threats)
    {
        const AAlphaForgeBoss* Boss = WeakBoss.Get();
        if (!Boss || Boss->GetAttackState() != EAlphaForgeAttackState::Windup) continue;
        for (int32 Index = 0; Index < (bMargin ? UE_ARRAY_COUNT(Offsets) : 1); ++Index)
        {
            if (AAlphaForgeBoss::IsPointInAttackZone(Boss->GetAttackType(), Boss->GetTelegraphOrigin(),
                Boss->GetTelegraphDirection(), Location + Offsets[Index])) return true;
        }
    }
    return false;
}

bool AAlphaCompanion::HasDirectPath(const FVector& Destination) const
{
    if (!AlphaCompanionRoute::Clear(GetWorld(), GetActorLocation(), Destination)) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AlphaCompanionPath), false, this);
    return !GetWorld()->SweepTestByChannel(GetActorLocation(), Destination, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeSphere(32.0f), Query);
}

bool AAlphaCompanion::ChooseEvadeDestination(FVector& Destination) const
{
    const FVector Origin = GetActorLocation();
    const int32 Chapter = AlphaCompanionRoute::Chapter(GetWorld());
    if (!IsDangerous(Origin)) { Destination = Origin; return true; }
    bool bFound = false;
    float BestDistance = TNumericLimits<float>::Max();
    const float Radii[] = {180.0f, 300.0f, 450.0f, 650.0f};
    for (float Radius : Radii)
    {
        for (int32 Index = 0; Index < 16; ++Index)
        {
            const float Angle = 2.0f * PI * Index / 16.0f;
            FVector Candidate = Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Radius;
            // Reject off-route escape points rather than folding them across a
            // missing bridge corner or onto the far side of a boundary wall.
            if (!AlphaLayoutRules::IsWalkable(Chapter, AlphaCompanionRoute::Local(GetWorld(), Candidate), 45.)) continue;
            if (IsDangerous(Candidate) || !HasDirectPath(Candidate)) continue;
            const float Distance = FVector::DistSquared2D(Origin, Candidate);
            if (Distance < BestDistance)
            {
                Destination = Candidate;
                BestDistance = Distance;
                bFound = true;
            }
        }
        if (bFound) break; // Prefer the nearest safe ring; never run through a boundary to a remote point.
    }
    return bFound;
}

FVector AAlphaCompanion::AddPersonalSpace(const FVector& Direction) const
{
    FVector Steering = Direction;
    for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
    {
        const AAlphaCharacter* Ally = *It;
        if (Ally == this || !Ally->IsCoopParticipant() || !Ally->GetCombatComponent()->IsAlive()) continue;
        FVector Away = GetActorLocation() - Ally->GetActorLocation();
        Away.Z = 0.0f;
        const float Distance = Away.Size();
        if (Distance >= 125.0f) continue;
        if (Distance < 1.0f) Away = FVector(0, GetArchetype() % 2 == 0 ? -1.0f : 1.0f, 0);
        Steering += Away.GetSafeNormal() * (1.0f - FMath::Clamp(Distance / 125.0f, 0.0f, 1.0f)) * 2.0f;
    }
    return Steering.GetSafeNormal2D();
}

void AAlphaCompanion::MoveToward(const FVector& Destination, float AcceptanceRadius)
{
    const int32 Chapter = AlphaCompanionRoute::Chapter(GetWorld());
    const FVector ArenaOrigin = AAlphaGameState::GetChapterOrigin(Chapter);
    const AlphaLayoutRules::Point From = AlphaCompanionRoute::Local(GetWorld(), GetActorLocation());
    const AlphaLayoutRules::Point Goal = AlphaLayoutRules::Project(Chapter, AlphaCompanionRoute::Local(GetWorld(), Destination));
    DesiredDestination = ArenaOrigin + FVector(Goal.X, Goal.Y, GetActorLocation().Z - ArenaOrigin.Z);
    const FVector Delta = DesiredDestination - GetActorLocation();
    if (Delta.SizeSquared2D() <= FMath::Square(AcceptanceRadius)) { StopMoving(); return; }
    const AlphaLayoutRules::Point Waypoint = AlphaLayoutRules::NextWaypoint(Chapter, From, Goal);
    const FVector RouteDelta(Waypoint.X - From.X, Waypoint.Y - From.Y, 0.);
    if (RouteDelta.SizeSquared2D() < .25) { StopMoving(); return; }
    const FVector RouteDirection = RouteDelta.GetSafeNormal2D();
    DesiredMove = BotAction == EAlphaCompanionAction::Evade ? RouteDirection : AddPersonalSpace(RouteDirection);
    // Personal-space separation may push sideways at a narrow bridge. Keep its
    // next short step inside the same collision layout, or use the route line.
    const FVector Probe = GetActorLocation() + DesiredMove * 75.f;
    if (!AlphaCompanionRoute::Clear(GetWorld(), GetActorLocation(), Probe)) DesiredMove = RouteDirection;
    FaceMoveIntent(DesiredMove);
}

void AAlphaCompanion::StopMoving()
{
    DesiredMove = FVector::ZeroVector;
    ConsumeMovementInputVector();
    if (GetCharacterMovement()->IsMovingOnGround()) GetCharacterMovement()->StopMovementImmediately();
    else
    {
        // A decision to stand still must not reset falling velocity and accidentally suspend a bot in the air.
        GetCharacterMovement()->Velocity.X = 0.0f;
        GetCharacterMovement()->Velocity.Y = 0.0f;
    }
}

void AAlphaCompanion::Follow(AAlphaCharacter* Human)
{
    if (!Human) { StopMoving(); return; }
    const float DepthOffset = static_cast<float>(GetArchetype() - 1) * 140.0f;
    const FVector Formation = Human->GetActorLocation() - Human->GetActorForwardVector() * (180.0f + GetArchetype() * 35.0f)
        + Human->GetActorRightVector() * DepthOffset;
    if (IsDangerous(Formation)) { SetAction(EAlphaCompanionAction::Evade); StopMoving(); return; }
    MoveToward(Formation, 65.0f);
}

bool AAlphaCompanion::TrySkill(AAlphaCharacter* Enemy)
{
    if (!Enemy || !CombatComponent->CanAct()) return false;
    if (!AlphaCompanionRoute::Clear(GetWorld(), GetActorLocation(), Enemy->GetActorLocation(), 0.)) return false;
    const FVector Delta = Enemy->GetActorLocation() - GetActorLocation();
    FaceMoveIntent(Delta);
    const float Distance = Delta.Size();
    // A full-energy ultimate has priority. Spend at most one I per three
    // successful U casts, then reserve the last ten energy for reaching O.
    const int32 Priority[] = {2, 1, 0};
    for (int32 Slot : Priority)
    {
        if (Slot == 1 && (BasicSkillsSinceSpend < 3 || GetAbilityComponent()->GetEnergy() >= 90.f)) continue;
        if (!GetAbilityComponent()->CanUseSkill(Slot)) continue;
        // A dash into an active floor tell is never a valid defensive response.
        if (GetArchetype() == 1 && Slot == 0 && !Threats.IsEmpty()) continue;
        if (GetArchetype() == 1 && Slot == 0 && !AlphaCompanionRoute::Clear(GetWorld(), GetActorLocation(),
            GetActorLocation() + GetActorForwardVector() * 420.f)) continue;
        if (!AlphaCompanionRules::ShouldUseSkill(GetArchetype(), Slot, Distance, true,
            GetAbilityComponent()->GetSkillRemaining(Slot), CombatComponent->CanAct(), GetAbilityComponent()->GetEnergy())) continue;
        if (!AlphaCompanionRules::InSkillFootprint(GetArchetype(), Slot,
            FVector::DotProduct(Delta, GetActorForwardVector()), FVector::DotProduct(Delta, GetActorRightVector()), Delta.Z)) continue;
        GetAbilityComponent()->TryUseSkill(Slot);
        if (GetAbilityComponent()->GetSkillRemaining(Slot) <= 0.f) continue;
        if (Slot == 0) ++BasicSkillsSinceSpend;
        else BasicSkillsSinceSpend = 0;
        return true;
    }
    return false;
}

void AAlphaCompanion::Attack(AAlphaCharacter* Enemy)
{
    if (!Enemy) { StopMoving(); return; }
    const FVector Delta = Enemy->GetActorLocation() - GetActorLocation();
    FaceMoveIntent(Delta);
    if (TrySkill(Enemy)) { StopMoving(); return; }
    if (AlphaCompanionRules::InMeleeRange(FVector::DotProduct(Delta, GetActorForwardVector()),
        FVector::DotProduct(Delta, GetActorRightVector()), Delta.Z)
        && AlphaCompanionRoute::Clear(GetWorld(), GetActorLocation(), Enemy->GetActorLocation(), 0.))
    {
        StopMoving();
        if (ServerTime() >= NextAttackAt)
        {
            // The same single attack input advances the shared five-stage
            // combo; recovery/input buffering remain combat-component rules.
            CombatComponent->TryAttack(false);
            NextAttackAt = ServerTime() + .3;
        }
    }
    else
    {
        const float ForwardOffsets[] = {135.0f, 85.0f, 85.0f};
        const float DepthOffsets[] = {0.0f, -60.0f, 60.0f};
        const int32 Hero = FMath::Clamp(GetArchetype(), 0, 2);
        // Distinct in-range slots leave room for another bot and a human at the same enemy.
        const FVector Approach = Enemy->GetActorLocation() - GetActorForwardVector() * ForwardOffsets[Hero]
            + GetActorRightVector() * DepthOffsets[Hero];
        if (IsDangerous(Approach)) { SetAction(EAlphaCompanionAction::Evade); StopMoving(); return; }
        MoveToward(Approach, 25.0f);
    }
}

void AAlphaCompanion::Rescue(AAlphaCharacter* Ally)
{
    if (!Ally) { GetReviveComponent()->EndRevive(); StopMoving(); return; }
    const float Distance = FVector::Dist(GetActorLocation(), Ally->GetActorLocation());
    FCollisionQueryParams Sight(SCENE_QUERY_STAT(AlphaCompanionRescueApproach), false, this);
    Sight.AddIgnoredActor(Ally);
    const bool bClearRescue = AlphaCompanionRoute::Clear(GetWorld(), GetActorLocation(), Ally->GetActorLocation(), 0.)
        && !GetWorld()->LineTraceTestByChannel(GetActorLocation(), Ally->GetActorLocation(), ECC_Visibility, Sight);
    if (Distance <= 195.0f && bClearRescue)
    {
        StopMoving();
        GetReviveComponent()->BeginRevive();
        AAlphaCharacter* ActualTarget = GetReviveComponent()->GetReviveTarget();
        if (!ActualTarget || IsDangerous(ActualTarget->GetActorLocation()))
        {
            // Usually an occluded or newly claimed ally. Do not hold the interaction latch indefinitely.
            GetReviveComponent()->EndRevive();
            RescueRetryAt = ServerTime() + 0.6;
        }
        return;
    }
    // A downed ally may be close in Euclidean distance but around a bridge
    // corner. Route toward their floor rather than repeatedly attempting a
    // blocked rescue from the other side of the boundary.
    if (IsDangerous(Ally->GetActorLocation())) { SetAction(EAlphaCompanionAction::Evade); StopMoving(); return; }
    MoveToward(Ally->GetActorLocation(), bClearRescue ? 150.f : 25.f);
}

void AAlphaCompanion::Decide()
{
    DesiredMove = FVector::ZeroVector;
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    if (!IsCoopParticipant() || IsDefeated() || !CombatComponent->IsAlive() || !State
        || State->RoundState == EAlphaRoundState::Defeat)
    {
        GetReviveComponent()->EndRevive();
        StopMoving();
        SetAction(EAlphaCompanionAction::Idle);
        return;
    }
    if (State->RoundState == EAlphaRoundState::Victory)
    {
        // The chapter now continues back to its guide. A last AI victory must
        // not leave the only human downed, unable to confirm the story debrief.
        if (!CombatComponent->CanAct()) { StopMoving(); return; }
        if (AAlphaCharacter* Downed = FindDownedAlly())
        {
            SetAction(EAlphaCompanionAction::Revive);
            Rescue(Downed);
        }
        else
        {
            GetReviveComponent()->EndRevive();
            SetAction(EAlphaCompanionAction::Follow);
            Follow(FindHumanAnchor());
        }
        return;
    }
    GatherBossThreats();
    AAlphaCharacter* Human = FindHumanAnchor();
    AAlphaCharacter* Downed = FindDownedAlly();
    AAlphaCharacter* Enemy = FindEnemy(Human);
    const bool bActive = State->RoundState == EAlphaRoundState::Fighting || State->RoundState == EAlphaRoundState::Intermission;
    const bool bInDanger = IsDangerous(GetActorLocation());
    if (bActive && bInDanger)
    {
        for (const TWeakObjectPtr<AAlphaForgeBoss>& Boss : Threats)
        {
            if (Boss.IsValid()) EvadeUntil = FMath::Max(EvadeUntil, Boss->GetAttackResolveTime() + 0.05);
        }
    }
    const bool bSafeAdjacentRescue = Downed && FVector::Dist(GetActorLocation(), Downed->GetActorLocation()) <= 195.0f;
    if (bActive && !bInDanger && !Threats.IsEmpty() && ServerTime() < EvadeUntil
        && CombatComponent->CanAct() && Human && !bSafeAdjacentRescue)
    {
        // Once clear of a tell, hold the nearest safe point rather than walking straight back into it.
        GetReviveComponent()->EndRevive();
        SetAction(EAlphaCompanionAction::Evade);
        StopMoving();
        return;
    }
    AlphaCompanionRules::Decision Decision;
    Decision.CanAct = CombatComponent->CanAct() && Human != nullptr;
    Decision.RunActive = bActive;
    Decision.InDanger = bInDanger;
    Decision.HasDownedAlly = Downed != nullptr;
    Decision.HasEnemy = Enemy != nullptr;
    Decision.HasHuman = Human != nullptr;
    const AlphaCompanionRules::Action Action = AlphaCompanionRules::ChooseAction(Decision);
    if (Action != AlphaCompanionRules::Action::Revive) GetReviveComponent()->EndRevive();
    switch (Action)
    {
    case AlphaCompanionRules::Action::Evade:
    {
        SetAction(EAlphaCompanionAction::Evade);
        FVector Destination;
        if (ChooseEvadeDestination(Destination)) MoveToward(Destination, 20.0f);
        else StopMoving(); // Ordinary collision/damage still applies if no reachable candidate exists.
        break;
    }
    case AlphaCompanionRules::Action::Revive: SetAction(EAlphaCompanionAction::Revive); Rescue(Downed); break;
    case AlphaCompanionRules::Action::Attack: SetAction(EAlphaCompanionAction::Attack); Attack(Enemy); break;
    case AlphaCompanionRules::Action::Follow: SetAction(EAlphaCompanionAction::Follow); Follow(Human); break;
    default: SetAction(EAlphaCompanionAction::Idle); StopMoving(); break;
    }
}

void AAlphaCompanion::HandleCompanionHit(float ActualDamage, AActor* InstigatorActor)
{
    if (!HasAuthority()) return;
    RescueRetryAt = ServerTime() + 1.2;
    GetReviveComponent()->EndRevive();
    DesiredMove = FVector::ZeroVector;
    DecisionAccumulator = 0.1f;
}

void AAlphaCompanion::ResetCompanionBrain()
{
    if (!HasAuthority()) return;
    GetReviveComponent()->EndRevive();
    StopMoving();
    DesiredDestination = GetActorLocation();
    Threats.Reset();
    DecisionAccumulator = 0.1f;
    RescueRetryAt = 0.0;
    NextAttackAt = 0.0;
    EvadeUntil = 0.0;
    BasicSkillsSinceSpend = 0;
    SetAction(EAlphaCompanionAction::Idle);
}

void AAlphaCompanion::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AAlphaCompanion, BotAction);
}
