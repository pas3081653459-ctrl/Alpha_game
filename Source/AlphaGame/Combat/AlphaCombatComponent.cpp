#include "Combat/AlphaCombatComponent.h"
#include "Rules/CombatRules.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

UAlphaCombatComponent::UAlphaCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
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

void UAlphaCombatComponent::ExecuteAttack(bool bHeavy)
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !Owner->HasAuthority() || !World || !CanAct()) return;
    const double Now = CombatTime();
    if (!AlphaRules::CooldownReady(Now, RecoveryUntil))
    {
        // At most one buffered attack; early windup spam cannot queue an entire combo.
        if (!bAttackPending && RecoveryUntil - Now <= FMath::Max(0.01f, InputBufferWindow))
        {
            bBufferedAttack = true;
            bBufferedHeavy = bHeavy;
            BufferedAttackExpires = Now + FMath::Max(0.01f, InputBufferWindow) + 0.01;
        }
        return;
    }
    CancelPendingAttack();
    const float Windup = FMath::Max(0.01f, bHeavy ? HeavyWindup : LightWindup);
    const float TotalDuration = FMath::Max(Windup + 0.05f, bHeavy ? HeavyCooldown : LightCooldown);
    RecoveryUntil = Now + TotalDuration;
    bPendingHeavy = bHeavy;
    bAttackPending = true;
    MulticastAttack(bHeavy);
    World->GetTimerManager().SetTimer(WindupTimer, this, &UAlphaCombatComponent::ReleaseAttack, Windup, false);
    World->GetTimerManager().SetTimer(RecoveryTimer, this, &UAlphaCombatComponent::FinishRecovery, TotalDuration, false);
    Owner->ForceNetUpdate();
}

void UAlphaCombatComponent::FinishRecovery()
{
    const bool bShouldContinue = bBufferedAttack && CombatTime() <= BufferedAttackExpires && CanAct();
    const bool bHeavy = bBufferedHeavy;
    bBufferedAttack = false;
    RecoveryUntil = 0.0;
    if (bShouldContinue) ExecuteAttack(bHeavy);
}

void UAlphaCombatComponent::CancelPendingAttack()
{
    const bool bCancelledWindup = bAttackPending;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(WindupTimer);
        World->GetTimerManager().ClearTimer(RecoveryTimer);
    }
    bAttackPending = false;
    bBufferedAttack = false;
    BufferedAttackExpires = 0.0;
    RecoveryUntil = 0.0;
    if (bCancelledWindup && !bEndingPlay && GetOwner() && GetOwner()->HasAuthority())
    {
        MulticastAttackCancelled();
    }
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
    const FVector Start = Owner->GetActorLocation();
    const FVector End = Start + Owner->GetActorForwardVector() * FMath::Max(1.0f, AttackReach);
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AlphaMelee), false, Owner);
    TArray<FHitResult> Hits;
    World->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects,
        FCollisionShape::MakeSphere(FMath::Max(1.0f, AttackRadius)), Query);
    TSet<AActor*> HitActors;
    const float Damage = FMath::Max(0.0f, bHeavy ? HeavyDamage : LightDamage);
    const float PoiseDamage = FMath::Max(0.0f, bHeavy ? HeavyPoiseDamage : LightPoiseDamage);
    const APawn* AttackingPawn = Cast<APawn>(Owner);
    for (const FHitResult& Hit : Hits)
    {
        AActor* Target = Hit.GetActor();
        if (!IsValid(Target) || Target == Owner || HitActors.Contains(Target) || IsFriendly(Target)) continue;
        FCollisionQueryParams VisibilityQuery(SCENE_QUERY_STAT(AlphaMeleeVisibility), false, Owner);
        VisibilityQuery.AddIgnoredActor(Target);
        if (World->LineTraceTestByChannel(Start, Target->GetActorLocation(), ECC_Visibility, VisibilityQuery)) continue;
        HitActors.Add(Target);
        if (UAlphaCombatComponent* TargetCombat = Target->FindComponentByClass<UAlphaCombatComponent>())
        {
            const int32 BeforeDamage = TargetCombat->GetDamageRevision();
            const int32 BeforeBreak = TargetCombat->GetPoiseBreakRevision();
            TargetCombat->ReceiveHit(Damage, PoiseDamage, Owner);
            if (TargetCombat->GetDamageRevision() != BeforeDamage)
            {
                NotifyHitConfirmed(Target, TargetCombat->GetPoiseBreakRevision() != BeforeBreak);
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

void UAlphaCombatComponent::MulticastAttack_Implementation(bool bHeavy) { OnAttack.Broadcast(bHeavy); }
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
