#include "Abilities/AlphaAbilityComponent.h"
#include "World/AlphaBreakableProp.h"

#include "Characters/AlphaCharacter.h"
#include "Combat/AlphaCombatComponent.h"
#include "Coop/AlphaReviveComponent.h"
#include "Rules/CombatRules.h"
#include "Rules/CompanionRules.h"
#include "Rules/PlayerControlRules.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Visuals/AlphaCombatEffect.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

UAlphaAbilityComponent::UAlphaAbilityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UAlphaAbilityComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UAlphaCombatComponent* Combat = GetOwner()->FindComponentByClass<UAlphaCombatComponent>())
    {
        Combat->OnHitConfirmed.AddDynamic(this, &UAlphaAbilityComponent::HandleConfirmedHit);
    }
}

void UAlphaAbilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UAlphaCombatComponent* Combat = GetOwner()->FindComponentByClass<UAlphaCombatComponent>())
    {
        Combat->OnHitConfirmed.RemoveDynamic(this, &UAlphaAbilityComponent::HandleConfirmedHit);
    }
    Super::EndPlay(EndPlayReason);
}

void UAlphaAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UAlphaAbilityComponent, SkillOneReadyAt);
    DOREPLIFETIME(UAlphaAbilityComponent, SkillTwoReadyAt);
    DOREPLIFETIME(UAlphaAbilityComponent, UltimateReadyAt);
    DOREPLIFETIME(UAlphaAbilityComponent, Energy);
}

double UAlphaAbilityComponent::AbilityTime() const
{
    if (!GetWorld()) return 0.0;
    if (const AGameStateBase* State = GetWorld()->GetGameState()) return State->GetServerWorldTimeSeconds();
    return GetWorld()->GetTimeSeconds();
}

float UAlphaAbilityComponent::GetSkillRemaining(int32 Slot) const
{
    if (Slot < 0 || Slot > 2) return 0.0f;
    const double ReadyAt = Slot == 0 ? SkillOneReadyAt : (Slot == 1 ? SkillTwoReadyAt : UltimateReadyAt);
    return static_cast<float>(FMath::Max(0.0, ReadyAt - AbilityTime()));
}

bool UAlphaAbilityComponent::CanUseSkill(int32 Slot) const
{
    const AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetOwner());
    if (!Character || !GetWorld() || Slot < 0 || Slot > 2) return false;
    const UAlphaCombatComponent* Combat = Character->GetCombatComponent();
    if (!Combat || Combat->TeamId != 1 || Character->IsDefeated()) return false;
    const double ReadyAt = Slot == 0 ? SkillOneReadyAt : (Slot == 1 ? SkillTwoReadyAt : UltimateReadyAt);
    return AlphaPlayerControl::CanCast(Slot, Energy, Combat->CanAct(),
        AlphaRules::CooldownReady(AbilityTime(), ReadyAt));
}

void UAlphaAbilityComponent::TryUseSkill(int32 Slot)
{
    if (!GetOwner() || Slot < 0 || Slot > 2) return;
    if (GetOwner()->HasAuthority()) ExecuteSkill(Slot);
    else if (GetOwner()->HasLocalNetOwner()) ServerUseSkill(Slot);
}

void UAlphaAbilityComponent::ServerUseSkill_Implementation(int32 Slot)
{
    ExecuteSkill(Slot);
}

void UAlphaAbilityComponent::ExecuteSkill(int32 Slot)
{
    AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetOwner());
    if (!Character || !Character->HasAuthority() || !CanUseSkill(Slot)) return;
    UAlphaCombatComponent* Combat = Character->GetCombatComponent();
    const double Now = AbilityTime();
    double& ReadyAt = Slot == 0 ? SkillOneReadyAt : (Slot == 1 ? SkillTwoReadyAt : UltimateReadyAt);

    if (UAlphaReviveComponent* Revive = Character->GetReviveComponent()) Revive->CancelRevive();
    Character->StopSprint();
    const int32 Archetype = FMath::Clamp(Character->GetArchetype(), 0, 2);
    static const float Cooldowns[3][3] = {{3.0f, 8.0f, 20.0f}, {3.0f, 7.0f, 20.0f}, {3.0f, 9.0f, 20.0f}};
    ReadyAt = Now + Cooldowns[Archetype][Slot];
    Energy = static_cast<float>(AlphaPlayerControl::EnergyChange(Slot, Energy));
    // U alone builds energy. I and O have fixed costs; full energy never silently empowers U/I.
    // The action lock also cancels pending melee, buffered combos and air-combo hold.
    Combat->SetActionLockFor(Slot == 0 ? 0.35f : (Slot == 1 ? 0.55f : 0.8f));
    MulticastSkill(Archetype, Slot, Character->GetActorLocation(), Character->GetActorForwardVector());

    const float SkillRadius = static_cast<float>(AlphaCompanionRules::GetSkillRadius(Archetype, Slot));
    const float SkillReach = static_cast<float>(AlphaCompanionRules::GetSkillReach(Archetype, Slot));
    if (Slot == 2)
    {
        if (Archetype == 0) AffectEnemies(SkillRadius, SkillReach, 100.0f, 110.0f);
        else if (Archetype == 1) AffectEnemies(SkillRadius, SkillReach, 80.0f, 90.0f, 0.0f, 600.0f);
        else AffectEnemies(SkillRadius, SkillReach, 65.0f, 140.0f);
    }
    else if (Archetype == 0 && Slot == 0)
    {
        // Guard burst: no claim that a successful parry was detected.
        Combat->SetInvulnerableFor(0.35f);
        AffectEnemies(SkillRadius, SkillReach, 16.0f, 25.0f);
    }
    else if (Archetype == 0)
    {
        AffectEnemies(SkillRadius, SkillReach, 42.0f, 55.0f);
    }
    else if (Archetype == 1 && Slot == 0)
    {
        // Launch uses normal CharacterMovement collision; never teleport through a wall.
        Character->LaunchCharacter(Character->GetActorForwardVector() * 900.0f, true, false);
        Combat->SetInvulnerableFor(0.12f);
        AffectEnemies(SkillRadius, SkillReach, 20.0f, 20.0f);
    }
    else if (Archetype == 1)
    {
        AffectEnemies(SkillRadius, SkillReach, 24.0f, 26.0f, 0.0f, 450.0f);
    }
    else if (Slot == 0)
    {
        AffectEnemies(SkillRadius, SkillReach, 8.0f, 16.0f, 350.0f);
    }
    else
    {
        AffectEnemies(SkillRadius, SkillReach, 12.0f, 60.0f);
    }
    Character->ForceNetUpdate();
}

void UAlphaAbilityComponent::AffectEnemies(float Radius, float Reach, float Damage, float PoiseDamage,
    float PullSpeed, float LiftSpeed)
{
    AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetOwner());
    if (!Character || !Character->HasAuthority() || !GetWorld()) return;
    UAlphaCombatComponent* Combat = Character->GetCombatComponent();
    const FVector Start = Character->GetActorLocation();
    const FVector Facing = Character->GetActorForwardVector().GetSafeNormal2D();
    const FVector End = Start + (Facing.IsNearlyZero() ? FVector::ForwardVector : Facing) * Reach;
    AAlphaBreakableProp::AffectAttack(GetWorld(), Character, Start, End, Radius);
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AlphaSkill), false, Character);
    TArray<FHitResult> Hits;
    GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects,
        FCollisionShape::MakeSphere(Radius), Query);
    TSet<AActor*> Seen;
    for (const FHitResult& Hit : Hits)
    {
        ACharacter* Enemy = Cast<ACharacter>(Hit.GetActor());
        if (!IsValid(Enemy) || Enemy == Character || Seen.Contains(Enemy)) continue;
        Seen.Add(Enemy);
        UAlphaCombatComponent* Victim = Enemy->FindComponentByClass<UAlphaCombatComponent>();
        if (!Victim || Victim->TeamId == Combat->TeamId || !Victim->IsAlive()) continue;
        FCollisionQueryParams Sight(SCENE_QUERY_STAT(AlphaSkillSight), false, Character);
        Sight.AddIgnoredActor(Enemy);
        if (GetWorld()->LineTraceTestByChannel(Start, Enemy->GetActorLocation(), ECC_Visibility, Sight)) continue;
        const int32 Revision = Victim->GetDamageRevision();
        const int32 BreakRevision = Victim->GetPoiseBreakRevision();
        Victim->ReceiveHit(Damage, PoiseDamage, Character);
        if (Victim->GetDamageRevision() == Revision) continue;
        Combat->NotifyHitConfirmed(Enemy, Victim->GetPoiseBreakRevision() != BreakRevision);
        if (!Victim->IsAlive() || Enemy->ActorHasTag(TEXT("Boss"))) continue;
        if (PullSpeed > 0.0f)
        {
            const FVector Delta = Start - Enemy->GetActorLocation();
            if (Delta.SizeSquared2D() > FMath::Square(90.0f))
            {
                Enemy->LaunchCharacter(Delta.GetSafeNormal2D() * PullSpeed, true, false);
            }
        }
        else if (LiftSpeed > 0.0f)
        {
            Enemy->LaunchCharacter(Character->GetActorForwardVector() * 180.0f + FVector(0, 0, LiftSpeed), true, true);
        }
    }
}

void UAlphaAbilityComponent::HandleConfirmedHit(AActor* Target, bool bPoiseBroken)
{
    AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetOwner());
    if (!Character || !Character->HasAuthority() || !IsValid(Target)) return;
    UAlphaCombatComponent* Combat = Character->GetCombatComponent();
    if (!Combat || Combat->TeamId != 1 || !Combat->IsAlive()) return;
    const double Now = AbilityTime();
    UAlphaAbilityComponent* Partner = nullptr;
    double MostRecent = -1000.0;
    for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
    {
        AAlphaCharacter* Ally = *It;
        UAlphaAbilityComponent* Ability = Ally->FindComponentByClass<UAlphaAbilityComponent>();
        if (Ally == Character || !Ability || !Ally->GetCombatComponent()->IsAlive()
            || Ally->GetCombatComponent()->TeamId != Combat->TeamId) continue;
        if (Ability->LastHitTarget.Get() == Target && Ability->LastHitAt > MostRecent)
        {
            Partner = Ability;
            MostRecent = Ability->LastHitAt;
        }
    }
    const TWeakObjectPtr<AActor> Key(Target);
    const double* LastProc = LastCoopProcAt.Find(Key);
    if (Partner && AlphaRules::IsCoopFollowup(true, Now, MostRecent, LastProc ? *LastProc : -1000.0))
    {
        // Propagate target-specific proc time to every teammate, preventing alternating RPCs
        // from bypassing the shared target cooldown. This bonus never changes energy.
        for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
        {
            if (It->GetCombatComponent()->TeamId != Combat->TeamId) continue;
            if (UAlphaAbilityComponent* Ability = It->FindComponentByClass<UAlphaAbilityComponent>())
            {
                Ability->LastCoopProcAt.Add(Key, Now);
            }
        }
        if (UAlphaCombatComponent* Victim = Target->FindComponentByClass<UAlphaCombatComponent>())
        {
            if (Victim->IsAlive()) Victim->ReceiveHit(0.0f, 18.0f, Character);
        }
        MulticastCoopCue(Target->GetActorLocation());
    }
    LastHitTarget = Target;
    LastHitAt = Now;
    for (auto It = LastCoopProcAt.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || Now - It.Value() > 10.0) It.RemoveCurrent();
    }
}

void UAlphaAbilityComponent::ResetForRound()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    SkillOneReadyAt = SkillTwoReadyAt = UltimateReadyAt = 0.0;
    Energy = 0.0f;
    LastHitTarget.Reset();
    LastHitAt = -1000.0;
    LastCoopProcAt.Reset();
    GetOwner()->ForceNetUpdate();
}

void UAlphaAbilityComponent::MulticastSkill_Implementation(int32 Archetype, int32 Slot, FVector Origin, FVector Facing)
{
    OnSkillActivated.Broadcast(Slot);
    if (!GetOwner() || !GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) return;
    const FLinearColor Color = Archetype == 0 ? FLinearColor(.9f, .5f, .22f)
        : (Archetype == 1 ? FLinearColor(.12f, .75f, .8f) : FLinearColor(.9f, .7f, .25f));
    // Snapshot the authority's release transform; replicated movement may arrive later.
    AAlphaCombatEffect::SpawnSkill(GetWorld(), Origin, Facing, Archetype, Slot, Color);
}

void UAlphaAbilityComponent::MulticastCoopCue_Implementation(FVector Location)
{
    if (GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer)
    {
        AAlphaCombatEffect::Spawn(GetWorld(), Location, FVector::ForwardVector,
            EAlphaEffectType::Coop, FLinearColor(.35f, .9f, .75f));
    }
}
