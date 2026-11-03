#include "Coop/AlphaReviveComponent.h"

#include "Characters/AlphaCharacter.h"
#include "Combat/AlphaCombatComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Rules/CombatRules.h"
#include "World/AlphaGameState.h"

UAlphaReviveComponent::UAlphaReviveComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.05f;
}

void UAlphaReviveComponent::BeginPlay()
{
    Super::BeginPlay();
    if (AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetOwner()))
    {
        Character->GetCombatComponent()->OnHit.AddDynamic(this, &UAlphaReviveComponent::HandleOwnerHit);
    }
    SetComponentTickEnabled(GetOwner() && GetOwner()->HasAuthority());
}

void UAlphaReviveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CancelRevive();
    CancelIncomingRevive();
    Super::EndPlay(EndPlayReason);
}

void UAlphaReviveComponent::BeginRevive()
{
    if (!GetOwner()) return;
    if (GetOwner()->HasAuthority()) SetReviveHeld(true);
    else if (GetOwner()->HasLocalNetOwner()) ServerSetReviveHeld(true);
}

void UAlphaReviveComponent::EndRevive()
{
    if (!GetOwner()) return;
    if (GetOwner()->HasAuthority()) SetReviveHeld(false);
    else if (GetOwner()->HasLocalNetOwner()) ServerSetReviveHeld(false);
}

void UAlphaReviveComponent::ServerSetReviveHeld_Implementation(bool bHeld)
{
    SetReviveHeld(bHeld);
}

void UAlphaReviveComponent::SetReviveHeld(bool bHeld)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (!bHeld)
    {
        CancelRevive();
        return;
    }
    if (!CanRescue(Cast<AAlphaCharacter>(GetOwner())) || !IsRoundActive()) return;
    bReviveHeld = true;
    // Claim immediately; another rescuer in the same frame will see this claim.
    if (!ReviveTarget)
    {
        if (AAlphaCharacter* Candidate = FindNearestEligibleTarget())
        {
            AAlphaCharacter* Rescuer = Cast<AAlphaCharacter>(GetOwner());
            // Starting a rescue also cancels a prior melee windup/recovery instead of attacking while helping.
            Rescuer->GetCombatComponent()->CancelCurrentAttack();
            ReviveTarget = Candidate;
            Candidate->GetReviveComponent()->CurrentRescuer = Rescuer;
            RescuerDamageRevision = Rescuer->GetCombatComponent()->GetDamageRevision();
            TargetDamageRevision = Candidate->GetCombatComponent()->GetDamageRevision();
            GetOwner()->ForceNetUpdate();
            Candidate->ForceNetUpdate();
        }
    }
}

bool UAlphaReviveComponent::IsRoundActive() const
{
    const AAlphaGameState* State = GetWorld() ? GetWorld()->GetGameState<AAlphaGameState>() : nullptr;
    // Standalone automation worlds can exercise the component without installing a game mode.
    return !State || State->RoundState == EAlphaRoundState::Fighting || State->RoundState == EAlphaRoundState::Intermission;
}

bool UAlphaReviveComponent::CanRescue(AAlphaCharacter* Character) const
{
    return IsValid(Character) && Character->IsCoopParticipant() && !Character->IsDefeated()
        && Character->GetCombatComponent()->CanAct();
}

bool UAlphaReviveComponent::HasClearLineOfSight(const AAlphaCharacter* Candidate) const
{
    if (!Candidate || !GetOwner() || !GetWorld()) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AlphaReviveVisibility), false, GetOwner());
    Query.AddIgnoredActor(Candidate);
    return !GetWorld()->LineTraceTestByChannel(GetOwner()->GetActorLocation(), Candidate->GetActorLocation(), ECC_Visibility, Query);
}

bool UAlphaReviveComponent::IsEligibleTarget(AAlphaCharacter* Candidate) const
{
    const AAlphaCharacter* Rescuer = Cast<AAlphaCharacter>(GetOwner());
    if (!IsValid(Candidate) || !Rescuer || Candidate == Rescuer || !Candidate->IsCoopParticipant()
        || !Candidate->IsDefeated() || Candidate->GetCombatComponent()->IsAlive()
        || Candidate->GetCombatComponent()->TeamId != Rescuer->GetCombatComponent()->TeamId) return false;
    const AAlphaCharacter* ClaimedBy = Candidate->GetReviveComponent()->CurrentRescuer;
    if (IsValid(ClaimedBy) && ClaimedBy != Rescuer) return false;
    return FVector::DistSquared(Rescuer->GetActorLocation(), Candidate->GetActorLocation()) <= FMath::Square(ReviveRange)
        && HasClearLineOfSight(Candidate);
}

AAlphaCharacter* UAlphaReviveComponent::FindNearestEligibleTarget() const
{
    AAlphaCharacter* Nearest = nullptr;
    float NearestDistance = TNumericLimits<float>::Max();
    for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
    {
        AAlphaCharacter* Candidate = *It;
        if (!IsEligibleTarget(Candidate)) continue;
        const float Distance = FVector::DistSquared(GetOwner()->GetActorLocation(), Candidate->GetActorLocation());
        if (Distance < NearestDistance)
        {
            Nearest = Candidate;
            NearestDistance = Distance;
        }
    }
    return Nearest;
}

void UAlphaReviveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bReviveHeld) return;
    AAlphaCharacter* Rescuer = Cast<AAlphaCharacter>(GetOwner());
    if (!CanRescue(Rescuer) || !IsRoundActive())
    {
        CancelRevive();
        return;
    }
    if (!ReviveTarget)
    {
        SetReviveHeld(true);
        if (!ReviveTarget) return;
    }
    if (!IsValid(ReviveTarget))
    {
        CancelRevive();
        return;
    }
    AlphaRules::ReviveConditions Conditions;
    Conditions.HelperAlive = CanRescue(Rescuer);
    Conditions.TargetDown = ReviveTarget->IsCoopParticipant() && ReviveTarget->IsDefeated()
        && !ReviveTarget->GetCombatComponent()->IsAlive()
        && ReviveTarget->GetCombatComponent()->TeamId == Rescuer->GetCombatComponent()->TeamId;
    Conditions.InRange = FVector::DistSquared(Rescuer->GetActorLocation(), ReviveTarget->GetActorLocation()) <= FMath::Square(ReviveRange);
    Conditions.HasLineOfSight = HasClearLineOfSight(ReviveTarget);
    Conditions.HoldingInteract = bReviveHeld;
    Conditions.DamageUnchanged = Rescuer->GetCombatComponent()->GetDamageRevision() == RescuerDamageRevision
        && ReviveTarget->GetCombatComponent()->GetDamageRevision() == TargetDamageRevision;
    Conditions.OwnsClaim = ReviveTarget->GetReviveComponent()->CurrentRescuer == Rescuer;
    const float RequiredDuration = ReviveTarget->GetReviveComponent()->GetRequiredReviveDuration();
    const AlphaRules::ReviveStep Step = AlphaRules::AdvanceRevive(ElapsedTime, DeltaTime, RequiredDuration, Conditions);
    if (Step.Cancelled)
    {
        CancelRevive();
        return;
    }
    ElapsedTime = static_cast<float>(Step.Elapsed);
    ReviveProgress = FMath::Clamp(ElapsedTime / RequiredDuration, 0.0f, 1.0f);
    if (Step.Completed)
    {
        AAlphaCharacter* CompletedTarget = ReviveTarget;
        CancelRevive();
        if (CompletedTarget->GetCombatComponent()->Revive(0.4f)) CompletedTarget->RecoverFromRevive();
    }
}

void UAlphaReviveComponent::CancelRevive()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (IsValid(ReviveTarget))
    {
        UAlphaReviveComponent* TargetRevive = ReviveTarget->GetReviveComponent();
        if (TargetRevive->CurrentRescuer == GetOwner())
        {
            TargetRevive->CurrentRescuer = nullptr;
            ReviveTarget->ForceNetUpdate();
        }
    }
    ReviveTarget = nullptr;
    ReviveProgress = 0.0f;
    ElapsedTime = 0.0f;
    bReviveHeld = false;
    GetOwner()->ForceNetUpdate();
}

void UAlphaReviveComponent::CancelIncomingRevive()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (IsValid(CurrentRescuer)) CurrentRescuer->GetReviveComponent()->CancelRevive();
    CurrentRescuer = nullptr;
}

void UAlphaReviveComponent::HandleOwnerHit(float ActualDamage, AActor* InstigatorActor)
{
    // Combat broadcasts only accepted hits; poise-only hits must also interrupt interaction.
    CancelRevive();
    CancelIncomingRevive();
}

void UAlphaReviveComponent::RegisterDowned()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    CancelRevive();
    CancelIncomingRevive();
    DownCount = FMath::Min(DownCount + 1, 5);
}

float UAlphaReviveComponent::GetRequiredReviveDuration() const
{
    return static_cast<float>(AlphaRules::ReviveDuration(DownCount));
}

void UAlphaReviveComponent::ResetForRound()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    CancelRevive();
    CancelIncomingRevive();
    DownCount = 0;
}

void UAlphaReviveComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UAlphaReviveComponent, ReviveTarget);
    DOREPLIFETIME(UAlphaReviveComponent, CurrentRescuer);
    DOREPLIFETIME(UAlphaReviveComponent, ReviveProgress);
    DOREPLIFETIME(UAlphaReviveComponent, DownCount);
}
