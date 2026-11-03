#include "Combat/AlphaCombatComponent.h"
#include "Rules/CombatRules.h"
#include "Rules/PlayerControlRules.h"
#include "World/AlphaBreakableProp.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

UAlphaCombatComponent::UAlphaCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.05f;
    SetIsReplicatedByDefault(true);
}

void UAlphaCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner() && GetOwner()->HasAuthority()) ResetVitals();
}

void UAlphaCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    CancelPendingAttack();
    Super::EndPlay(EndPlayReason);
}

bool UAlphaCombatComponent::IsOwnerAirborne() const
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    return Character && Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsFalling();
}

void UAlphaCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!GetOwner() || !GetOwner()->HasAuthority() || TeamId != 1) return;
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (Character && Character->GetCharacterMovement()->IsMovingOnGround())
    {
        bAirHoldUsedForCurrentJump = false;
        if (bAirComboActive) CancelPendingAttack();
    }
}

void UAlphaCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UAlphaCombatComponent, TeamId);
    DOREPLIFETIME(UAlphaCombatComponent, MaxHealth);
    DOREPLIFETIME(UAlphaCombatComponent, MaxPoise);
    DOREPLIFETIME(UAlphaCombatComponent, Health);
    DOREPLIFETIME(UAlphaCombatComponent, Poise);
    DOREPLIFETIME(UAlphaCombatComponent, RecoveryUntil);
    DOREPLIFETIME(UAlphaCombatComponent, StaggerUntil);
    DOREPLIFETIME(UAlphaCombatComponent, InvulnerableUntil);
    DOREPLIFETIME(UAlphaCombatComponent, DamageRevision);
    DOREPLIFETIME(UAlphaCombatComponent, PoiseBreakRevision);
    DOREPLIFETIME(UAlphaCombatComponent, ComboStage);
    DOREPLIFETIME(UAlphaCombatComponent, bAirComboActive);
}

double UAlphaCombatComponent::CombatTime() const
{
    const UWorld* World = GetWorld();
    if (!World) return 0.0;
    if (const AGameStateBase* State = World->GetGameState()) return State->GetServerWorldTimeSeconds();
    return World->GetTimeSeconds();
}

bool UAlphaCombatComponent::CanAct() const
{
    return IsAlive() && AlphaRules::CooldownReady(CombatTime(), StaggerUntil);
}

bool UAlphaCombatComponent::IsAttackInProgress() const
{
    return IsAlive() && CombatTime() < RecoveryUntil;
}

bool UAlphaCombatComponent::IsInvulnerable() const
{
    return IsAlive() && CombatTime() < InvulnerableUntil;
}

void UAlphaCombatComponent::SetInvulnerableFor(float Duration)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsAlive() || !FMath::IsFinite(Duration)) return;
    InvulnerableUntil = FMath::Max(InvulnerableUntil, CombatTime() + FMath::Clamp(Duration, 0.0f, 5.0f));
    GetOwner()->ForceNetUpdate();
}

void UAlphaCombatComponent::SetActionLockFor(float Duration)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsAlive() || !FMath::IsFinite(Duration) || Duration <= 0.0f) return;
    CancelPendingAttack();
    StaggerUntil = FMath::Max(StaggerUntil, CombatTime() + FMath::Clamp(Duration, 0.0f, 5.0f));
    GetOwner()->ForceNetUpdate();
}

void UAlphaCombatComponent::ConfigureVitals(float NewMaxHealth, float NewMaxPoise,
    float InPoiseBreakStunDuration, float InNormalHitStunDuration)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !FMath::IsFinite(NewMaxHealth)
        || !FMath::IsFinite(NewMaxPoise) || !FMath::IsFinite(InPoiseBreakStunDuration)
        || !FMath::IsFinite(InNormalHitStunDuration)) return;
    MaxHealth = FMath::Clamp(NewMaxHealth, 1.0f, 1000000.0f);
    MaxPoise = FMath::Clamp(NewMaxPoise, 1.0f, 1000000.0f);
    PoiseBreakStunDuration = FMath::Clamp(InPoiseBreakStunDuration, 0.01f, 5.0f);
    NormalHitStunDuration = FMath::Clamp(InNormalHitStunDuration, 0.0f, 1.0f);
    ResetVitals();
}

void UAlphaCombatComponent::CancelCurrentAttack()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    const double PreviousRecovery = RecoveryUntil;
    CancelPendingAttack();
    // Interaction cancellation must not become a way to bypass attack recovery.
    RecoveryUntil = PreviousRecovery;
    if (UWorld* World = GetWorld())
    {
        const float Remaining = static_cast<float>(RecoveryUntil - CombatTime());
        if (Remaining > 0.0f)
        {
            World->GetTimerManager().SetTimer(RecoveryTimer, this, &UAlphaCombatComponent::FinishRecovery, Remaining, false);
        }
    }
    GetOwner()->ForceNetUpdate();
}

void UAlphaCombatComponent::TryAttack(bool bHeavy)
{
    AActor* Owner = GetOwner();
    if (!Owner || !CanAct()) return;
    if (Owner->HasAuthority()) ExecuteAttack(bHeavy);
    else if (Owner->HasLocalNetOwner()) ServerTryAttack(bHeavy);
}

void UAlphaCombatComponent::ServerTryAttack_Implementation(bool bHeavy)
{
    ExecuteAttack(bHeavy);
}

float UAlphaCombatComponent::GetComboWindup() const
{
    return static_cast<float>(AlphaPlayerControl::GetComboAttack(FMath::Max(1, ComboStage), bPendingAirborne).Windup);
}

float UAlphaCombatComponent::GetComboDuration() const
{
    return static_cast<float>(AlphaPlayerControl::GetComboAttack(FMath::Max(1, ComboStage), bPendingAirborne).Duration);
}

void UAlphaCombatComponent::ExecuteAttack(bool bHeavy)
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !Owner->HasAuthority() || !World || !CanAct()) return;
    const double Now = CombatTime();
    const bool bUseCombo = TeamId == 1;
    // Legacy heavy requests from companion bots join the same five-stage player chain.
    if (bUseCombo) bHeavy = false;
    if (!AlphaRules::CooldownReady(Now, RecoveryUntil))
    {
        const double BufferWindow = bUseCombo ? AlphaPlayerControl::ComboInputBufferSeconds : FMath::Max(0.01f, InputBufferWindow);
        if (RecoveryUntil - Now <= BufferWindow && (bUseCombo || !bAttackPending))
        {
            bBufferedAttack = true;
            bBufferedHeavy = bHeavy;
            BufferedAttackExpires = RecoveryUntil + 0.05;
            if (bUseCombo)
            {
                World->GetTimerManager().SetTimer(ComboTimeoutTimer, this, &UAlphaCombatComponent::ExpireCombo,
                    static_cast<float>(AlphaPlayerControl::ComboTimeoutSeconds), false);
            }
        }
        return;
    }

    // Starting the next stage must not use CancelPendingAttack, which intentionally ends the chain.
    World->GetTimerManager().ClearTimer(WindupTimer);
    World->GetTimerManager().ClearTimer(RecoveryTimer);
    bBufferedAttack = false;
    BufferedAttackExpires = 0.0;
    bPendingCombo = bUseCombo;
    bPendingAirborne = bUseCombo && IsOwnerAirborne();
    PendingFacing = Owner->GetActorForwardVector().GetSafeNormal2D();
    if (PendingFacing.IsNearlyZero()) PendingFacing = FVector::ForwardVector;

    float Windup = FMath::Max(0.01f, bHeavy ? HeavyWindup : LightWindup);
    float TotalDuration = FMath::Max(Windup + 0.05f, bHeavy ? HeavyCooldown : LightCooldown);
    if (bUseCombo)
    {
        // A jump/landing starts its own chain instead of inheriting a ground finisher.
        if (ComboStage > 0 && bLastComboAirborne != bPendingAirborne) FinishCombo();
        ComboStage = AlphaPlayerControl::NextComboStage(ComboStage);
        bLastComboAirborne = bPendingAirborne;
        const AlphaPlayerControl::ComboAttackData Data = AlphaPlayerControl::GetComboAttack(ComboStage, bPendingAirborne);
        Windup = static_cast<float>(Data.Windup);
        TotalDuration = static_cast<float>(Data.Duration);
        if (bPendingAirborne && !bAirComboActive && !bAirHoldUsedForCurrentJump)
        {
            bAirHoldUsedForCurrentJump = true;
            SetAirComboActive(true);
        }
        World->GetTimerManager().SetTimer(ComboTimeoutTimer, this, &UAlphaCombatComponent::ExpireCombo,
            static_cast<float>(AlphaPlayerControl::ComboTimeoutSeconds), false);
    }
    RecoveryUntil = Now + TotalDuration;
    bPendingHeavy = bHeavy;
    bAttackPending = true;
    if (bUseCombo) MulticastComboAttack(ComboStage, bPendingAirborne, PendingFacing);
    else MulticastAttack(bHeavy, PendingFacing);
    World->GetTimerManager().SetTimer(WindupTimer, this, &UAlphaCombatComponent::ReleaseAttack, Windup, false);
    World->GetTimerManager().SetTimer(RecoveryTimer, this, &UAlphaCombatComponent::FinishRecovery, TotalDuration, false);
    Owner->ForceNetUpdate();
}

void UAlphaCombatComponent::FinishRecovery()
{
    const bool bShouldContinue = bBufferedAttack && CombatTime() <= BufferedAttackExpires && CanAct();
    const bool bHeavy = bBufferedHeavy;
    const bool bFinishedFifth = bPendingCombo && ComboStage == 5;
    bBufferedAttack = false;
    bPendingCombo = false;
    RecoveryUntil = 0.0;
    if (bFinishedFifth) FinishCombo();
    if (bShouldContinue) ExecuteAttack(bHeavy);
}

void UAlphaCombatComponent::CancelPendingAttack()
{
    const bool bCancelledAttack = bAttackPending || ComboStage > 0 || RecoveryUntil > CombatTime();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(WindupTimer);
        World->GetTimerManager().ClearTimer(RecoveryTimer);
    }
    bAttackPending = false;
    bBufferedAttack = false;
    BufferedAttackExpires = 0.0;
    RecoveryUntil = 0.0;
    bPendingCombo = false;
    FinishCombo();
    if (bCancelledAttack && !bEndingPlay && GetOwner() && GetOwner()->HasAuthority())
    {
        MulticastAttackCancelled();
    }
}

void UAlphaCombatComponent::SetAirComboActive(bool bActive)
{
    if (bAirComboActive == bActive) return;
    bAirComboActive = bActive;
    if (UWorld* World = GetWorld())
    {
        if (bActive)
        {
            World->GetTimerManager().SetTimer(AirComboLimitTimer, this,
                &UAlphaCombatComponent::ExpireAirCombo, static_cast<float>(AlphaPlayerControl::AirComboMaxSeconds), false);
        }
        else World->GetTimerManager().ClearTimer(AirComboLimitTimer);
    }
    if (bEndingPlay) OnAirComboChanged.Broadcast(bActive);
    else if (GetOwner() && GetOwner()->HasAuthority()) MulticastAirComboChanged(bActive);
    if (GetOwner()) GetOwner()->ForceNetUpdate();
}

void UAlphaCombatComponent::FinishCombo()
{
    const bool bHadCombo = ComboStage > 0 || bAirComboActive;
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(ComboTimeoutTimer);
    SetAirComboActive(false);
    ComboStage = 0;
    if (bHadCombo)
    {
        if (bEndingPlay) OnComboFinished.Broadcast();
        else if (GetOwner() && GetOwner()->HasAuthority()) MulticastComboFinished();
    }
}

void UAlphaCombatComponent::ExpireCombo()
{
    // A timeout is not a queued attack. It only clears the chain and returns gravity.
    CancelPendingAttack();
}

void UAlphaCombatComponent::ExpireAirCombo()
{
    // This lock persists until a genuine landing, so clicking again cannot restart hovering.
    bAirHoldUsedForCurrentJump = true;
    CancelPendingAttack();
}

void UAlphaCombatComponent::ReleaseAttack()
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !Owner->HasAuthority() || !World || !bAttackPending || !CanAct())
    {
        CancelPendingAttack();
        return;
    }
    bAttackPending = false;
    const bool bHeavy = bPendingHeavy;
    // A single sweep at release is this graybox's hit window. The server owns all attack transforms.
    const AlphaPlayerControl::ComboAttackData Data = AlphaPlayerControl::GetComboAttack(ComboStage, bPendingAirborne);
    const float Reach = bPendingCombo ? static_cast<float>(Data.Reach) * (AttackReach / 175.0f) : FMath::Max(1.0f, AttackReach);
    const FVector Start = Owner->GetActorLocation() + FVector(0.0f, 0.0f, bPendingCombo ? Data.VerticalOffset : 0.0);
    // The facing vector is frozen when the attack begins, even if movement turns the owner during windup.
    const FVector End = Start + PendingFacing * Reach;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AlphaMelee), false, Owner);
    TArray<FHitResult> Hits;
    const FCollisionShape Shape = bPendingCombo
        ? FCollisionShape::MakeBox(FVector(35.0f, Data.HalfWidth, Data.HalfHeight))
        : FCollisionShape::MakeBox(FVector(35.0f, FMath::Max(1.0f, AttackRadius), 60.0f));
    World->SweepMultiByObjectType(Hits, Start, End, PendingFacing.Rotation().Quaternion(), Objects, Shape, Query);
    if (bPendingCombo)
    {
        AAlphaBreakableProp::AffectAttack(World, Owner, Start, End, static_cast<float>(Data.HalfWidth));
    }
    const int32 MeleeHitCount = Hits.Num();
    FVector ImpactOrigin = FVector::ZeroVector;
    bool bHasGroundImpact = false;
    if (bPendingCombo && ComboStage == 5)
    {
        const FVector Probe = Owner->GetActorLocation() + PendingFacing * 110.f + FVector(0, 0, 30.f);
        FHitResult FloorHit;
        FCollisionObjectQueryParams FloorObjects;
        FloorObjects.AddObjectTypesToQuery(ECC_WorldStatic);
        if (World->LineTraceSingleByObjectType(FloorHit, Probe,
            Probe - FVector(0, 0, AlphaPlayerControl::GroundImpactTraceDepth(bPendingAirborne)), FloorObjects, Query)
            && FloorHit.ImpactNormal.Z >= .65f)
        {
            const float Radius = static_cast<float>(AlphaPlayerControl::GroundImpactRadius(bPendingAirborne));
            ImpactOrigin = FloorHit.ImpactPoint + FloorHit.ImpactNormal * 8.f;
            FCollisionQueryParams ImpactSight(SCENE_QUERY_STAT(AlphaImpactSight), false, Owner);
            // Do not project a shockwave through a wall merely because floor exists beyond it.
            bHasGroundImpact = !World->LineTraceTestByChannel(Owner->GetActorLocation(), ImpactOrigin, ECC_Visibility, ImpactSight);
            if (bHasGroundImpact)
            {
                TArray<FHitResult> ImpactHits;
                World->SweepMultiByObjectType(ImpactHits, ImpactOrigin, ImpactOrigin + FVector(0, 0, .1f),
                    FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius), Query);
                Hits.Append(ImpactHits);
                AAlphaBreakableProp::AffectAttack(World, Owner, ImpactOrigin, ImpactOrigin, Radius);
                MulticastGroundImpact(FloorHit.ImpactPoint, FloorHit.ImpactNormal, Radius);
            }
        }
    }
    // Melee and shockwave share one damage set: a finisher can never double-hit a target.
    TSet<AActor*> HitActors;
    const float Damage = bPendingCombo ? LightDamage * static_cast<float>(Data.DamageScale)
        : FMath::Max(0.0f, bHeavy ? HeavyDamage : LightDamage);
    const float PoiseDamage = bPendingCombo ? LightPoiseDamage * static_cast<float>(Data.PoiseScale)
        : FMath::Max(0.0f, bHeavy ? HeavyPoiseDamage : LightPoiseDamage);
    const APawn* AttackingPawn = Cast<APawn>(Owner);
    for (int32 HitIndex = 0; HitIndex < Hits.Num(); ++HitIndex)
    {
        const FHitResult& Hit = Hits[HitIndex];
        AActor* Target = Hit.GetActor();
        if (!IsValid(Target) || Target == Owner || HitActors.Contains(Target) || IsFriendly(Target)) continue;
        if (TeamId == 2)
        {
            const ACharacter* AttackerCharacter = Cast<ACharacter>(Owner);
            const ACharacter* TargetCharacter = Cast<ACharacter>(Target);
            if (AttackerCharacter && TargetCharacter)
            {
                const double AttackerFeet = Owner->GetActorLocation().Z - AttackerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                const double TargetFeet = Target->GetActorLocation().Z - TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                // Check feet height rather than IsFalling: the very first airborne frame is still hittable.
                if (FMath::Abs(TargetFeet - AttackerFeet) > 120.0) continue;
            }
        }
        FCollisionQueryParams VisibilityQuery(SCENE_QUERY_STAT(AlphaMeleeVisibility), false, Owner);
        VisibilityQuery.AddIgnoredActor(Target);
        const FVector HitOrigin = HitIndex < MeleeHitCount ? Start : ImpactOrigin;
        if (World->LineTraceTestByChannel(HitOrigin, Target->GetActorLocation(), ECC_Visibility, VisibilityQuery)) continue;
        HitActors.Add(Target);
        if (UAlphaCombatComponent* TargetCombat = Target->FindComponentByClass<UAlphaCombatComponent>())
        {
            const int32 BeforeDamage = TargetCombat->GetDamageRevision();
            const int32 BeforeBreak = TargetCombat->GetPoiseBreakRevision();
            TargetCombat->ReceiveHit(Damage, PoiseDamage, Owner);
            if (TargetCombat->GetDamageRevision() != BeforeDamage)
            {
                NotifyHitConfirmed(Target, TargetCombat->GetPoiseBreakRevision() != BeforeBreak);
                const float Lift = bPendingCombo ? static_cast<float>(AlphaPlayerControl::ComboLiftSpeed(ComboStage, bPendingAirborne)) : 0.f;
                if (Lift > 0.f && TargetCombat->IsAlive() && !Target->ActorHasTag(TEXT("Boss")))
                {
                    if (ACharacter* Victim = Cast<ACharacter>(Target))
                        Victim->LaunchCharacter(PendingFacing * 90.f + FVector(0, 0, Lift), true, true);
                }
            }
        }
        else
        {
            UGameplayStatics::ApplyDamage(Target, Damage,
                AttackingPawn ? AttackingPawn->GetController() : nullptr, Owner, nullptr);
        }
    }
}

void UAlphaCombatComponent::NotifyHitConfirmed(AActor* Target, bool bPoiseBroken)
{
    if (GetOwner() && GetOwner()->HasAuthority() && IsValid(Target)) OnHitConfirmed.Broadcast(Target, bPoiseBroken);
}

bool UAlphaCombatComponent::IsFriendly(const AActor* OtherActor) const
{
    if (!OtherActor) return false;
    const UAlphaCombatComponent* OtherCombat = OtherActor->FindComponentByClass<UAlphaCombatComponent>();
    if (OtherCombat && TeamId >= 0 && OtherCombat->TeamId == TeamId) return true;
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    const APawn* OtherPawn = Cast<APawn>(OtherActor);
    return OwnerPawn && OtherPawn && OwnerPawn->IsPlayerControlled() && OtherPawn->IsPlayerControlled();
}

void UAlphaCombatComponent::ReceiveHit(float Damage, float PoiseDamage, AActor* InstigatorActor)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority()) return;
    if (AAlphaCharacter* Character = Cast<AAlphaCharacter>(Owner))
        Character->ModifyIncomingHit(Damage, PoiseDamage, InstigatorActor);
    const AlphaRules::DamageResult Result = AlphaRules::ResolveDamage(Health, Poise, MaxHealth, MaxPoise,
        Damage, PoiseDamage, InstigatorActor == Owner || IsFriendly(InstigatorActor), IsInvulnerable());
    if (!Result.Accepted) return;
    CancelPendingAttack();
    Health = static_cast<float>(Result.Health);
    Poise = static_cast<float>(Result.Poise);
    const float ActualDamage = static_cast<float>(Result.ActualDamage);
    if (ActualDamage > 0.0f)
    {
        DamageRevision = DamageRevision == MAX_int32 ? 0 : DamageRevision + 1;
        if (NormalHitStunDuration > 0.0f)
        {
            StaggerUntil = FMath::Max(StaggerUntil, CombatTime() + NormalHitStunDuration);
        }
    }
    if (Result.PoiseBroken)
    {
        PoiseBreakRevision = PoiseBreakRevision == MAX_int32 ? 0 : PoiseBreakRevision + 1;
        StaggerUntil = FMath::Max(StaggerUntil, CombatTime() + FMath::Max(0.01f, PoiseBreakStunDuration));
        MulticastPoiseBroken();
    }
    MulticastHit(ActualDamage, InstigatorActor);
    OnVitalsChanged.Broadcast();
    if (!IsAlive()) OnDeath.Broadcast();
    Owner->ForceNetUpdate();
}

void UAlphaCombatComponent::ResetVitals()
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority()) return;
    CancelPendingAttack();
    MaxHealth = FMath::Max(1.0f, MaxHealth);
    MaxPoise = FMath::Max(1.0f, MaxPoise);
    Health = MaxHealth;
    Poise = MaxPoise;
    StaggerUntil = 0.0;
    InvulnerableUntil = 0.0;
    bAirHoldUsedForCurrentJump = false;
    // Revisions are monotonic across resets, so rescue snapshots cannot alias a new round.
    OnVitalsChanged.Broadcast();
    Owner->ForceNetUpdate();
}

bool UAlphaCombatComponent::Revive(float HealthFraction)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || IsAlive() || !FMath::IsFinite(HealthFraction)) return false;
    CancelPendingAttack();
    Health = FMath::Max(1.0f, MaxHealth * FMath::Clamp(HealthFraction, 0.01f, 1.0f));
    Poise = MaxPoise;
    StaggerUntil = 0.0;
    bAirHoldUsedForCurrentJump = false;
    InvulnerableUntil = CombatTime() + 1.0;
    OnVitalsChanged.Broadcast();
    Owner->ForceNetUpdate();
    return true;
}

void UAlphaCombatComponent::ConfigureArchetype(int32 Archetype)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Archetype < 0 || Archetype > 2) return;
    // Server-selected fixed profiles: Shen Yan / A Ling / Wen Deng.
    MaxHealth = Archetype == 0 ? 120.0f : (Archetype == 1 ? 90.0f : 100.0f);
    MaxPoise = Archetype == 0 ? 80.0f : (Archetype == 1 ? 45.0f : 60.0f);
    LightDamage = Archetype == 0 ? 18.0f : (Archetype == 1 ? 12.0f : 14.0f);
    HeavyDamage = Archetype == 0 ? 36.0f : (Archetype == 1 ? 25.0f : 28.0f);
    LightPoiseDamage = Archetype == 0 ? 14.0f : (Archetype == 1 ? 8.0f : 12.0f);
    HeavyPoiseDamage = Archetype == 0 ? 42.0f : (Archetype == 1 ? 25.0f : 34.0f);
    LightWindup = Archetype == 0 ? 0.15f : (Archetype == 1 ? 0.08f : 0.12f);
    HeavyWindup = Archetype == 0 ? 0.36f : (Archetype == 1 ? 0.23f : 0.30f);
    LightCooldown = Archetype == 0 ? 0.42f : (Archetype == 1 ? 0.28f : 0.36f);
    HeavyCooldown = Archetype == 0 ? 0.85f : (Archetype == 1 ? 0.62f : 0.78f);
    AttackReach = Archetype == 2 ? 205.0f : 175.0f;
    ResetVitals();
}

void UAlphaCombatComponent::ConfigureEnemyProfile()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || TeamId != 2) return;
    MaxHealth = 55.0f;
    MaxPoise = 35.0f;
    LightDamage = 8.0f;
    LightPoiseDamage = 6.0f;
    LightCooldown = 0.85f;
    LightWindup = 0.30f;
    ResetVitals();
}

void UAlphaCombatComponent::MulticastAttack_Implementation(bool bHeavy, FVector Facing)
{
    PendingFacing = Facing.GetSafeNormal2D();
    OnAttack.Broadcast(bHeavy);
}
void UAlphaCombatComponent::MulticastComboAttack_Implementation(int32 Stage, bool bAirborne, FVector Facing)
{
    PendingFacing = Facing.GetSafeNormal2D();
    ComboStage = Stage;
    bPendingAirborne = bAirborne;
    OnComboAttack.Broadcast(Stage, bAirborne);
}
void UAlphaCombatComponent::MulticastComboFinished_Implementation()
{
    ComboStage = 0;
    OnComboFinished.Broadcast();
}
void UAlphaCombatComponent::MulticastAirComboChanged_Implementation(bool bHolding)
{
    bAirComboActive = bHolding;
    OnAirComboChanged.Broadcast(bHolding);
}
void UAlphaCombatComponent::MulticastAttackCancelled_Implementation() { OnAttackCancelled.Broadcast(); }
void UAlphaCombatComponent::MulticastPoiseBroken_Implementation() { OnPoiseBroken.Broadcast(); }
void UAlphaCombatComponent::MulticastHit_Implementation(float ActualDamage, AActor* InstigatorActor)
{
    OnHit.Broadcast(ActualDamage, InstigatorActor);
}

void UAlphaCombatComponent::OnRep_Health(float PreviousHealth)
{
    OnVitalsChanged.Broadcast();
    if (PreviousHealth > 0.0f && !IsAlive()) OnDeath.Broadcast();
}

void UAlphaCombatComponent::OnRep_Poise() { OnVitalsChanged.Broadcast(); }

void UAlphaCombatComponent::MulticastGroundImpact_Implementation(FVector Location, FVector Normal, float Radius)
{
    OnGroundImpact.Broadcast(Location, Normal, Radius);
}
