#include "Characters/Boss/AlphaForgeBoss.h"

#include "AIController.h"
#include "CollisionQueryParams.h"
#include "Combat/AlphaCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Rules/CombatRules.h"
#include "Rules/LevelLayoutRules.h"
#include "Visuals/AlphaArtLibrary.h"
#include "Visuals/AlphaCharacterVisualComponent.h"
#include "World/AlphaGameState.h"

namespace AlphaForgeRoute
{
int32 Chapter(const UWorld* World)
{
    const AAlphaGameState* State = World ? World->GetGameState<AAlphaGameState>() : nullptr;
    return State ? State->ChapterIndex : 0;
}

AlphaLayoutRules::Point Local(int32 ChapterIndex, const FVector& Location)
{
    const FVector Origin = AAlphaGameState::GetChapterOrigin(ChapterIndex);
    return {Location.X - Origin.X, Location.Y - Origin.Y};
}

bool ClearAttack(const AActor* Source, const FVector& Origin, const AActor* Target)
{
    if (!Source || !Target || !Source->GetWorld() || Source->GetWorld() != Target->GetWorld()) return false;
    UWorld* World = Source->GetWorld();
    const int32 ChapterIndex = Chapter(World);
    if (!AlphaLayoutRules::SegmentClear(ChapterIndex, Local(ChapterIndex, Origin),
        Local(ChapterIndex, Target->GetActorLocation()), 0.)) return false;
    FCollisionQueryParams Sight(SCENE_QUERY_STAT(AlphaForgeAttackSight), false, Source);
    Sight.AddIgnoredActor(Target);
    return !World->LineTraceTestByChannel(Origin, Target->GetActorLocation(), ECC_Visibility, Sight);
}

AlphaLayoutRules::Point Next(int32 ChapterIndex, AlphaLayoutRules::Point From, AlphaLayoutRules::Point Goal)
{
    // The boss capsule has a 65cm radius, larger than ordinary party capsules. Keep
    // an 80cm clearance even when the shared party route offers a tighter cut.
    constexpr double Margin = 80.;
    From = AlphaLayoutRules::Project(ChapterIndex, From, Margin);
    Goal = AlphaLayoutRules::Project(ChapterIndex, Goal, Margin);
    if (AlphaLayoutRules::SegmentClear(ChapterIndex, From, Goal, Margin)) return Goal;
    const AlphaLayoutRules::Point Candidate = AlphaLayoutRules::NextWaypoint(ChapterIndex, From, Goal);
    if (AlphaLayoutRules::SegmentClear(ChapterIndex, From, Candidate, Margin)
        && AlphaLayoutRules::DistanceSquared(From, Candidate) > 1.) return Candidate;
    const AlphaLayoutRules::Point LaneFrom = AlphaLayoutRules::Project(ChapterIndex,
        {From.X, AlphaLayoutRules::RouteCenterY(ChapterIndex, From.X)}, Margin);
    if (AlphaLayoutRules::DistanceSquared(From, LaneFrom) > 1.
        && AlphaLayoutRules::SegmentClear(ChapterIndex, From, LaneFrom, Margin)) return LaneFrom;
    const AlphaLayoutRules::Point LaneGoal = AlphaLayoutRules::Project(ChapterIndex,
        {Goal.X, AlphaLayoutRules::RouteCenterY(ChapterIndex, Goal.X)}, Margin);
    const AlphaLayoutRules::Point LaneNext = AlphaLayoutRules::NextWaypoint(ChapterIndex, LaneFrom, LaneGoal);
    return AlphaLayoutRules::SegmentClear(ChapterIndex, From, LaneNext, Margin) ? LaneNext : From;
}
}

AAlphaForgeBoss::AAlphaForgeBoss()
{
    PrimaryActorTick.bCanEverTick = true;
    bAlwaysRelevant = true;
    Tags.Remove(TEXT("Player"));
    Tags.AddUnique(TEXT("Enemy"));
    Tags.AddUnique(TEXT("Boss"));
    CombatComponent->TeamId = 2;
    GetCapsuleComponent()->InitCapsuleSize(65.f, 120.f);
    PrototypeBody->SetRelativeScale3D(FVector(1.3f, 1.1f, 2.3f));
    PrototypeWeapon->SetRelativeScale3D(FVector(1.6f, .45f, .45f));
    GetCharacterMovement()->MaxWalkSpeed = 180.f;
    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    TelegraphMarkers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TelegraphMarkers"));
    TelegraphMarkers->SetupAttachment(GetCapsuleComponent());
    TelegraphMarkers->SetMobility(EComponentMobility::Movable);
    TelegraphMarkers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TelegraphMarkers->SetGenerateOverlapEvents(false);
    TelegraphMarkers->SetCastShadow(false);
    TelegraphMarkers->SetCanEverAffectNavigation(false);
    // Constructor references keep the engine assets reachable by the cooker.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MarkerMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarkerMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (MarkerMesh.Succeeded()) TelegraphMarkers->SetStaticMesh(MarkerMesh.Object);
    if (MarkerMaterial.Succeeded()) TelegraphMarkers->SetMaterial(0, MarkerMaterial.Object);
}

void AAlphaForgeBoss::BeginPlay()
{
    Super::BeginPlay();
    CombatComponent->OnPoiseBroken.AddDynamic(this, &AAlphaForgeBoss::HandlePoiseBreak);
    TelegraphMaterial = AlphaArt::EffectMaterial(this, FLinearColor(1.f, .12f, .025f), .8f);
    if (TelegraphMaterial) TelegraphMarkers->SetMaterial(0, TelegraphMaterial);
    // The modular visual component builds an oversized furnace hammer; gameplay capsule is unchanged.
}

void AAlphaForgeBoss::ConfigureForPlayers(int32 PlayerCount)
{
    if (!HasAuthority() || bConfigured) return;
    bConfigured = true;
    CombatComponent->ConfigureVitals(static_cast<float>(AlphaRules::BossHealth(PlayerCount)), 250.f, .2f, 0.f);
    AttackCooldownUntil = ServerTime() + 2.f;
    ForceNetUpdate();
}

void AAlphaForgeBoss::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AAlphaForgeBoss, BossPhase);
    DOREPLIFETIME(AAlphaForgeBoss, AttackType);
    DOREPLIFETIME(AAlphaForgeBoss, AttackState);
    DOREPLIFETIME(AAlphaForgeBoss, AttackServerStartTime);
    DOREPLIFETIME(AAlphaForgeBoss, AttackResolveTime);
    DOREPLIFETIME(AAlphaForgeBoss, AttackCooldownUntil);
    DOREPLIFETIME(AAlphaForgeBoss, TelegraphOrigin);
    DOREPLIFETIME(AAlphaForgeBoss, TelegraphDirection);
}

double AAlphaForgeBoss::ServerTime() const
{
    if (const AGameStateBase* State = GetWorld()->GetGameState()) return State->GetServerWorldTimeSeconds();
    return GetWorld()->GetTimeSeconds();
}

AAlphaCharacter* AAlphaForgeBoss::FindClosestLivingPlayer() const
{
    AAlphaCharacter* Closest = nullptr;
    const int32 ChapterIndex = AlphaForgeRoute::Chapter(GetWorld());
    float BestDistance = TNumericLimits<float>::Max();
    for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
    {
        AAlphaCharacter* Candidate = *It;
        if (!Candidate->IsCoopParticipant() || !Candidate->GetCombatComponent()->IsAlive()) continue;
        if (!AlphaLayoutRules::IsWalkable(ChapterIndex,
            AlphaForgeRoute::Local(ChapterIndex, Candidate->GetActorLocation()))) continue;
        const float Distance = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            Closest = Candidate;
        }
    }
    return Closest;
}

void AAlphaForgeBoss::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateTelegraph();
    if (!HasAuthority() || !CombatComponent->IsAlive() || IsDefeated()) return;
    if (BossPhase == 1 && CombatComponent->GetHealth() <= CombatComponent->GetMaxHealth() * .5f)
    {
        BossPhase = 2;
        GetCharacterMovement()->MaxWalkSpeed = 230.f;
        ForceNetUpdate();
    }
    // Short component hit reactions and poise breaks also gate this independent AI.
    if (!CombatComponent->CanAct())
    {
        GetCharacterMovement()->StopMovementImmediately();
        return;
    }
    const double Now = ServerTime();
    if (AttackState == EAlphaForgeAttackState::Windup)
    {
        if (Now >= AttackResolveTime) ResolveAttack();
        return;
    }
    if (Now < AttackCooldownUntil) return;
    AttackState = EAlphaForgeAttackState::Idle;
    AAlphaCharacter* Target = FindClosestLivingPlayer();
    if (!Target) return;
    FVector Delta = Target->GetActorLocation() - GetActorLocation();
    Delta.Z = 0.f;
    FaceMoveIntent(Delta);
    if (Delta.SizeSquared() > FMath::Square(650.f) || !AlphaForgeRoute::ClearAttack(this, GetActorLocation(), Target))
    {
        RouteRefreshCountdown -= DeltaSeconds;
        if (RouteRefreshCountdown <= 0.f || RoutedTarget.Get() != Target)
        {
            RouteRefreshCountdown = .15f;
            RoutedTarget = Target;
            const int32 ChapterIndex = AlphaForgeRoute::Chapter(GetWorld());
            const AlphaLayoutRules::Point Waypoint = AlphaForgeRoute::Next(ChapterIndex,
                AlphaForgeRoute::Local(ChapterIndex, GetActorLocation()), AlphaForgeRoute::Local(ChapterIndex, Target->GetActorLocation()));
            RouteWaypoint = AAlphaGameState::GetChapterOrigin(ChapterIndex) + FVector(Waypoint.X, Waypoint.Y, 0.);
        }
        const FVector RouteDelta = RouteWaypoint - GetActorLocation();
        if (RouteDelta.SizeSquared2D() > .25)
        {
            FaceMoveIntent(RouteDelta);
            AddMovementInput(RouteDelta.GetSafeNormal2D());
        }
    }
    else
    {
        BeginAttack(Target);
    }
}

void AAlphaForgeBoss::BeginAttack(AAlphaCharacter* Target)
{
    if (!HasAuthority() || !Target || !CombatComponent->CanAct()
        || !AlphaForgeRoute::ClearAttack(this, GetActorLocation(), Target)) return;
    GetCharacterMovement()->StopMovementImmediately();
    FVector Direction = Target->GetActorLocation() - GetActorLocation();
    Direction.Z = 0.f;
    if (!Direction.Normalize()) Direction = FVector::ForwardVector;
    AttackType = static_cast<EAlphaForgeAttack>(1 + AttackSequence % 3);
    ++AttackSequence;
    // A sweep always has a reachable target; distant turns become a line attack.
    if (AttackType == EAlphaForgeAttack::Sweep && FVector::DistSquared2D(GetActorLocation(), Target->GetActorLocation()) > FMath::Square(330.f))
    {
        AttackType = EAlphaForgeAttack::FireLine;
    }
    TelegraphDirection = Direction;
    TelegraphOrigin = AttackType == EAlphaForgeAttack::Hammer ? Target->GetActorLocation() : GetActorLocation();
    const int32 ChapterIndex = AlphaForgeRoute::Chapter(GetWorld());
    TelegraphOrigin.Z = AAlphaGameState::GetChapterOrigin(ChapterIndex).Z
        + AlphaLayoutRules::SurfaceHeight(ChapterIndex, AlphaForgeRoute::Local(ChapterIndex, TelegraphOrigin)) + 8.f;
    const float BaseWindup = AttackType == EAlphaForgeAttack::Hammer ? 1.35f : (AttackType == EAlphaForgeAttack::FireLine ? 1.05f : .8f);
    AttackServerStartTime = ServerTime();
    AttackResolveTime = AttackServerStartTime + BaseWindup * 1.1f * (BossPhase == 2 ? .75f : 1.f);
    AttackState = EAlphaForgeAttackState::Windup;
    ForceNetUpdate();
}

bool AAlphaForgeBoss::IsPointInAttackZone(EAlphaForgeAttack Attack, const FVector& Origin, const FVector& Direction, const FVector& Point)
{
    const auto ToPoint = [](const FVector& Value)
    {
        return AlphaRules::Point3D{Value.X, Value.Y, Value.Z};
    };
    // Native tests exercise this exact engine-independent authority footprint.
    return AlphaRules::IsPointInBossZone(static_cast<AlphaRules::BossAttackType>(Attack),
        ToPoint(Origin), ToPoint(Direction), ToPoint(Point));
}

void AAlphaForgeBoss::ResolveAttack()
{
    if (!HasAuthority() || !CombatComponent->CanAct()) return;
    const float Damage = AttackType == EAlphaForgeAttack::Hammer ? 20.f : (AttackType == EAlphaForgeAttack::FireLine ? 14.f : 12.f);
    for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
    {
        AAlphaCharacter* Target = *It;
        if (!Target->IsCoopParticipant() || !Target->GetCombatComponent()->IsAlive()) continue;
        if (IsPointInAttackZone(AttackType, TelegraphOrigin, TelegraphDirection, Target->GetActorLocation())
            && AlphaForgeRoute::ClearAttack(this, FVector(TelegraphOrigin) + FVector(0, 0, 50), Target))
        {
            // ReceiveHit enforces team, life and dodge invulnerability on authority.
            Target->GetCombatComponent()->ReceiveHit(Damage, 25.f, this);
        }
    }
    AttackState = EAlphaForgeAttackState::Recovery;
    AttackCooldownUntil = ServerTime() + (BossPhase == 2 ? 1.15f : 1.6f);
    ForceNetUpdate();
}

void AAlphaForgeBoss::HandlePoiseBreak()
{
    if (!HasAuthority() || !CombatComponent->IsAlive()) return;
    // Only a real poise break cancels the tell. Normal hits cannot constantly
    // restart it. High poise and .2s component stun prevent permanent locking.
    AttackType = EAlphaForgeAttack::None;
    AttackState = EAlphaForgeAttackState::Recovery;
    AttackCooldownUntil = ServerTime() + .7f;
    GetCharacterMovement()->StopMovementImmediately();
    ForceNetUpdate();
}

void AAlphaForgeBoss::HandleDeath()
{
    Super::HandleDeath();
    if (!HasAuthority()) return;
    AttackState = EAlphaForgeAttackState::Defeated;
    AttackType = EAlphaForgeAttack::None;
    // Keep the defeated boss until the GameMode confirms victory/reset.
    ForceNetUpdate();
}

void AAlphaForgeBoss::AddTelegraphSegment(const FVector& A, const FVector& B, float Width)
{
    FVector Start = A;
    FVector End = B;
    Start.Z = End.Z = 10.f;
    const FVector Delta = End - Start;
    if (Delta.SizeSquared() < .01f) return;
    const FTransform Transform(Delta.Rotation(), (Start + End) * .5f,
        FVector(Delta.Size() / 100.f, Width / 100.f, .03f));
    // Instances are local presentation only; the synchronized tell is world-space.
    TelegraphMarkers->AddInstance(Transform, true);
}

void AAlphaForgeBoss::UpdateTelegraph()
{
    if (GetNetMode() == NM_DedicatedServer) return;
    const bool bWindingUp = AttackState == EAlphaForgeAttackState::Windup;
    const double TellDuration = FMath::Max(.01, AttackResolveTime - AttackServerStartTime);
    const float TellProgress = FMath::Clamp(static_cast<float>((ServerTime() - AttackServerStartTime) / TellDuration), 0.f, 1.f);
    if (UAlphaCharacterVisualComponent* Visuals = GetVisualComponent())
    {
        // Read the existing replicated attack state; never invent another impact time or attack footprint.
        Visuals->SetBossTell(static_cast<int32>(AttackType), TellProgress, bWindingUp,
            AttackState == EAlphaForgeAttackState::Recovery);
    }
    if (AttackState != EAlphaForgeAttackState::Windup)
    {
        if (TelegraphMarkers->GetInstanceCount() > 0) TelegraphMarkers->ClearInstances();
        return;
    }
    TelegraphMarkers->ClearInstances();
    const FVector Origin = TelegraphOrigin;
    const FVector Forward = FVector(TelegraphDirection).GetSafeNormal2D();
    const FVector Side(-Forward.Y, Forward.X, 0.f);
    const double Duration = FMath::Max(.01, AttackResolveTime - AttackServerStartTime);
    const float Progress = FMath::Clamp(static_cast<float>((ServerTime() - AttackServerStartTime) / Duration), 0.f, 1.f);
    if (TelegraphMaterial)
    {
        const FLinearColor Danger = Progress > .7f
            ? FLinearColor(1.f, .85f, .02f) : FLinearColor(1.f, .12f, .025f);
        TelegraphMaterial->SetVectorParameterValue(TEXT("Tint"), Danger);
        TelegraphMaterial->SetVectorParameterValue(TEXT("Color"), Danger);
        TelegraphMaterial->SetScalarParameterValue(TEXT("Glow"), .8f + Progress * .4f);
    }
    const auto Circle = [this, Origin](float Radius, int32 Segments, float Width)
    {
        if (Radius < .1f) return;
        FVector Previous = Origin + FVector(Radius, 0, 0);
        for (int32 Index = 1; Index <= Segments; ++Index)
        {
            const float Angle = 2.f * PI * Index / Segments;
            const FVector Next = Origin + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0);
            AddTelegraphSegment(Previous, Next, Width);
            Previous = Next;
        }
    };
    if (AttackType == EAlphaForgeAttack::Hammer)
    {
        Circle(240.f, 32, 5.f);
        Circle(240.f * Progress, 16, 3.f); // 48 instances maximum.
    }
    else if (AttackType == EAlphaForgeAttack::FireLine)
    {
        const FVector End = Origin + Forward * 1000.f;
        AddTelegraphSegment(Origin + Side * 85.f, End + Side * 85.f);
        AddTelegraphSegment(Origin - Side * 85.f, End - Side * 85.f);
        AddTelegraphSegment(Origin + Side * 85.f, Origin - Side * 85.f);
        AddTelegraphSegment(End + Side * 85.f, End - Side * 85.f);
        AddTelegraphSegment(Origin, Origin + Forward * (1000.f * Progress), 3.f);
    }
    else if (AttackType == EAlphaForgeAttack::Sweep)
    {
        FVector Previous = Origin + Forward.RotateAngleAxis(-65.f, FVector::UpVector) * 330.f;
        AddTelegraphSegment(Origin, Previous);
        for (int32 Index = 1; Index <= 24; ++Index)
        {
            const FVector Next = Origin + Forward.RotateAngleAxis(-65.f + Index * (130.f / 24.f), FVector::UpVector) * 330.f;
            AddTelegraphSegment(Previous, Next);
            Previous = Next;
        }
        AddTelegraphSegment(Origin, Previous);
        AddTelegraphSegment(Origin, Origin + Forward.RotateAngleAxis(-65.f + Progress * 130.f, FVector::UpVector) * 330.f, 3.f);
    }
}
