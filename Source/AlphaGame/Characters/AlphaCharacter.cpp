#include "Characters/AlphaCharacter.h"

#include "Combat/AlphaCombatComponent.h"
#include "Coop/AlphaReviveComponent.h"
#include "Abilities/AlphaAbilityComponent.h"
#include "Visuals/AlphaCharacterVisualComponent.h"
#include "Visuals/AlphaCombatEffect.h"
#include "Visuals/AlphaCombatAudio.h"
#include "Visuals/AlphaCombatCamera.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AAlphaCharacter::AAlphaCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(60.0f);
    SetMinNetUpdateFrequency(20.0f);

    GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
    GetCharacterMovement()->MaxWalkSpeed = 480.0f;
    GetCharacterMovement()->JumpZVelocity = 680.0f;
    GetCharacterMovement()->AirControl = 0.35f;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    bUseControllerRotationYaw = false;
    Tags.AddUnique(TEXT("Player"));

    CombatComponent = CreateDefaultSubobject<UAlphaCombatComponent>(TEXT("Combat"));
    CombatComponent->TeamId = 1;
    ReviveComponent = CreateDefaultSubobject<UAlphaReviveComponent>(TEXT("Revive"));
    AbilityComponent = CreateDefaultSubobject<UAlphaAbilityComponent>(TEXT("Abilities"));
    VisualComponent = CreateDefaultSubobject<UAlphaCharacterVisualComponent>(TEXT("CharacterVisuals"));

    PrototypeBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeBody"));
    PrototypeBody->SetupAttachment(GetCapsuleComponent());
    PrototypeBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrototypeBody->SetGenerateOverlapEvents(false);
    PrototypeBody->SetCanEverAffectNavigation(false);
    PrototypeBody->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.5f));
    PrototypeWeapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeWeapon"));
    PrototypeWeapon->SetupAttachment(GetCapsuleComponent());
    PrototypeWeapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrototypeWeapon->SetGenerateOverlapEvents(false);
    PrototypeWeapon->SetCanEverAffectNavigation(false);
    PrototypeWeapon->SetRelativeLocation(FVector(60.0f, -25.0f, 10.0f));
    PrototypeWeapon->SetRelativeScale3D(FVector(0.85f, 0.12f, 0.12f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (BodyMesh.Succeeded())
    {
        PrototypeBody->SetStaticMesh(BodyMesh.Object);
        PrototypeWeapon->SetStaticMesh(BodyMesh.Object);
    }
    if (BasicMaterial.Succeeded())
    {
        PrototypeBody->SetMaterial(0, BasicMaterial.Object);
        PrototypeWeapon->SetMaterial(0, BasicMaterial.Object);
    }
}

bool AAlphaCharacter::IsCoopParticipant() const
{
    return CombatComponent && CombatComponent->TeamId == 1 && GetController()
        && (IsPlayerControlled() || ActorHasTag(TEXT("Companion")));
}

void AAlphaCharacter::BeginPlay()
{
    Super::BeginPlay();
    CombatComponent->OnDeath.AddDynamic(this, &AAlphaCharacter::HandleDeath);
    CombatComponent->OnAttack.AddDynamic(this, &AAlphaCharacter::ShowAttack);
    CombatComponent->OnAttackCancelled.AddDynamic(this, &AAlphaCharacter::CancelAttackVisuals);
    CombatComponent->OnPoiseBroken.AddDynamic(this, &AAlphaCharacter::ShowPoiseBreak);
    CombatComponent->OnHit.AddDynamic(this, &AAlphaCharacter::ShowHit);
    CombatComponent->OnComboAttack.AddDynamic(this, &AAlphaCharacter::ShowCombo);
    CombatComponent->OnComboFinished.AddDynamic(this, &AAlphaCharacter::HandleComboFinished);
    CombatComponent->OnAirComboChanged.AddDynamic(this, &AAlphaCharacter::HandleAirComboChanged);
    BodyMaterial = PrototypeBody->CreateAndSetMaterialInstanceDynamic(0);
    WeaponMaterial = PrototypeWeapon->CreateAndSetMaterialInstanceDynamic(0);
    VisualComponent->InitializeAnchors(PrototypeBody, PrototypeWeapon);
    // Hide only the original rendering; their child visual anchors stay visible and retain hit poses.
    PrototypeBody->SetVisibility(false, false);
    PrototypeWeapon->SetVisibility(false, false);
    if (HasAuthority() && CombatComponent->TeamId == 1) CombatComponent->ConfigureArchetype(Archetype);
    ApplyArchetypeAppearance();
    LastRequestedFacing = GetActorForwardVector().GetSafeNormal2D();
    RestoreWeaponPose();
    ApplyDefeatedState();
}

void AAlphaCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(WeaponFeedbackTimer);
    GetWorldTimerManager().ClearTimer(PoiseFeedbackTimer);
    GetWorldTimerManager().ClearTimer(AirComboHoldTimer);
    bAirComboHeld = false;
    ApplyAirComboHold();
    bSprinting = bPredictedSprinting = false;
    RefreshSprintSpeed();
    Super::EndPlay(EndPlayReason);
}

void AAlphaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AAlphaCharacter::MoveForward);
    PlayerInputComponent->BindAxis(TEXT("MoveDepth"), this, &AAlphaCharacter::MoveDepth);
    PlayerInputComponent->BindAction(TEXT("DirectionD"), IE_Pressed, this, &AAlphaCharacter::DirectionDPressed);
    PlayerInputComponent->BindAction(TEXT("DirectionD"), IE_Released, this, &AAlphaCharacter::DirectionDReleased);
    PlayerInputComponent->BindAction(TEXT("DirectionA"), IE_Pressed, this, &AAlphaCharacter::DirectionAPressed);
    PlayerInputComponent->BindAction(TEXT("DirectionA"), IE_Released, this, &AAlphaCharacter::DirectionAReleased);
    PlayerInputComponent->BindAction(TEXT("DirectionW"), IE_Pressed, this, &AAlphaCharacter::DirectionWPressed);
    PlayerInputComponent->BindAction(TEXT("DirectionW"), IE_Released, this, &AAlphaCharacter::DirectionWReleased);
    PlayerInputComponent->BindAction(TEXT("DirectionS"), IE_Pressed, this, &AAlphaCharacter::DirectionSPressed);
    PlayerInputComponent->BindAction(TEXT("DirectionS"), IE_Released, this, &AAlphaCharacter::DirectionSReleased);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &AAlphaCharacter::BeginJump);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
    PlayerInputComponent->BindAction(TEXT("LightAttack"), IE_Pressed, this, &AAlphaCharacter::LightAttack);
    PlayerInputComponent->BindAction(TEXT("HeavyAttack"), IE_Pressed, this, &AAlphaCharacter::HeavyAttack);
    PlayerInputComponent->BindAction(TEXT("Dodge"), IE_Pressed, this, &AAlphaCharacter::Dodge);
    PlayerInputComponent->BindAction(TEXT("Revive"), IE_Pressed, this, &AAlphaCharacter::BeginRevive);
    PlayerInputComponent->BindAction(TEXT("Revive"), IE_Released, this, &AAlphaCharacter::EndRevive);
    PlayerInputComponent->BindAction(TEXT("Skill1"), IE_Pressed, this, &AAlphaCharacter::Skill1);
    PlayerInputComponent->BindAction(TEXT("Skill2"), IE_Pressed, this, &AAlphaCharacter::Skill2);
    PlayerInputComponent->BindAction(TEXT("Skill3"), IE_Pressed, this, &AAlphaCharacter::Skill3);
}

void AAlphaCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (IsPlayerControlled())
    {
        if (IsSprinting() && (IsMenuBlockingActions() || bDefeated || !CombatComponent->CanAct()
            || !GetCharacterMovement()->IsMovingOnGround())) StopSprint();
        if (IsLocallyControlled() && !IsMenuBlockingActions() && !bDefeated && CombatComponent->CanAct()) UpdateMoveFacing();
    }
}

void AAlphaCharacter::MoveForward(float Value)
{
    MoveInputIntent.X = FMath::IsFinite(Value) ? FMath::Clamp(Value, -1.0f, 1.0f) : 0.0f;
    if (IsMenuBlockingActions() || bDefeated || !CombatComponent->CanAct()) return;
    AddMovementInput(FVector::ForwardVector, MoveInputIntent.X);
}

void AAlphaCharacter::MoveDepth(float Value)
{
    MoveInputIntent.Y = FMath::IsFinite(Value) ? FMath::Clamp(Value, -1.0f, 1.0f) : 0.0f;
    if (!IsMenuBlockingActions() && !bDefeated && CombatComponent->CanAct())
    {
        AddMovementInput(FVector::RightVector, MoveInputIntent.Y);
    }
}

void AAlphaCharacter::FaceAlongLane(float Direction)
{
    FaceMoveIntent(FVector(Direction, 0.0f, 0.0f));
}

void AAlphaCharacter::FaceMoveIntent(const FVector& XYIntent)
{
    if (XYIntent.ContainsNaN() || CombatComponent->IsAttackInProgress()
        || (GetWorld() && GetWorld()->GetTimeSeconds() < VisualFacingLockedUntil)) return;
    const FVector Direction = XYIntent.GetSafeNormal2D();
    if (Direction.IsNearlyZero()) return;
    const FRotator DesiredRotation(0.0f, Direction.Rotation().Yaw, 0.0f);
    if (!GetActorRotation().Equals(DesiredRotation, 0.1f))
    {
        SetActorRotation(DesiredRotation);
        if (HasAuthority()) ForceNetUpdate();
    }
}

void AAlphaCharacter::ServerSetFacing_Implementation(FVector XYIntent)
{
    if (XYIntent.ContainsNaN() || FMath::Abs(XYIntent.Z) > 0.01f
        || !FMath::IsNearlyEqual(XYIntent.SizeSquared2D(), 1.0, 0.03)
        || bDefeated || !CombatComponent->CanAct()) return;
    FaceMoveIntent(XYIntent);
}

void AAlphaCharacter::UpdateMoveFacing()
{
    const FVector Intent = MoveInputIntent.GetSafeNormal2D();
    if (Intent.IsNearlyZero()) return; // Preserve the last useful facing after releasing movement.
    FaceMoveIntent(Intent);
    const double Now = GetWorld()->GetTimeSeconds();
    if (!HasAuthority() && (!Intent.Equals(LastRequestedFacing, 0.01f) || Now - LastFacingRequestTime >= 0.15))
    {
        ServerSetFacing(Intent);
        LastRequestedFacing = Intent;
        LastFacingRequestTime = Now;
    }
}

bool AAlphaCharacter::IsSprinting() const
{
    return !HasAuthority() && IsLocallyControlled() ? bPredictedSprinting : bSprinting;
}

void AAlphaCharacter::RefreshSprintSpeed()
{
    if (CombatComponent->TeamId != 1) return; // Enemy/Boss movement profiles stay under their own AI.
    GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * (IsSprinting() ? static_cast<float>(AlphaPlayerControl::SprintMultiplier) : 1.0f);
}

void AAlphaCharacter::ResetTapHistory(bool bAuthorityState)
{
    AlphaPlayerControl::TapState* States = bAuthorityState ? AuthorityTapStates : LocalTapStates;
    for (int32 Index = 0; Index < 4; ++Index) States[Index] = AlphaPlayerControl::TapState{};
}

void AAlphaCharacter::AcknowledgeSprintState()
{
    SprintRevision = SprintRevision == MAX_int32 ? 0 : SprintRevision + 1;
    ForceNetUpdate();
}

void AAlphaCharacter::SetSprintState(bool bEnabled, int32 KeyIndex)
{
    if (HasAuthority())
    {
        bSprinting = bEnabled;
        SprintKey = bEnabled ? KeyIndex : -1;
    }
    else
    {
        bPredictedSprinting = bEnabled;
        PredictedSprintKey = bEnabled ? KeyIndex : -1;
    }
    RefreshSprintSpeed();
}

void AAlphaCharacter::StopSprint()
{
    SetSprintState(false, -1);
    ResetTapHistory(HasAuthority());
    if (HasAuthority()) AcknowledgeSprintState();
    else if (HasLocalNetOwner() && IsLocallyControlled()) ServerStopSprint();
}

void AAlphaCharacter::ServerStopSprint_Implementation()
{
    StopSprint();
}

void AAlphaCharacter::OnRep_SprintState()
{
    if (IsLocallyControlled())
    {
        // bSprinting remains pure authority state, so a rejection can correct prediction even when it stayed false.
        const bool bStillHeld = SprintKey >= 0 && SprintKey < 4 && LocalTapStates[SprintKey].Held;
        bPredictedSprinting = bSprinting && bStillHeld && !IsMenuBlockingActions() && !bDefeated;
        PredictedSprintKey = bPredictedSprinting ? SprintKey : -1;
    }
    RefreshSprintSpeed();
}

void AAlphaCharacter::ProcessDirectionKey(int32 KeyIndex, bool bPressed, bool bAuthorityState)
{
    if (KeyIndex < 0 || KeyIndex >= 4 || !GetWorld()) return;
    AlphaPlayerControl::TapState* States = bAuthorityState ? AuthorityTapStates : LocalTapStates;
    const bool bDoubleTap = AlphaPlayerControl::RegisterTap(States[KeyIndex], bPressed, GetWorld()->GetTimeSeconds());
    const int32 ActiveKey = bAuthorityState ? SprintKey : PredictedSprintKey;
    if (!bPressed && ActiveKey == KeyIndex) SetSprintState(false, -1);
    const FVector HeldIntent((States[0].Held ? 1.0f : 0.0f) - (States[1].Held ? 1.0f : 0.0f),
        (States[2].Held ? 1.0f : 0.0f) - (States[3].Held ? 1.0f : 0.0f), 0.0f);
    if (bPressed && !IsMenuBlockingActions() && !bDefeated && CombatComponent->CanAct()) FaceMoveIntent(HeldIntent);
    if (bDoubleTap && bPressed && !HeldIntent.IsNearlyZero() && !IsMenuBlockingActions()
        && !bDefeated && CombatComponent->CanAct() && GetCharacterMovement()->IsMovingOnGround()) SetSprintState(true, KeyIndex);
    if (bAuthorityState) AcknowledgeSprintState();
}

void AAlphaCharacter::HandleDirectionKey(int32 KeyIndex, bool bPressed)
{
    if (bPressed && (IsMenuBlockingActions() || bDefeated)) return;
    ProcessDirectionKey(KeyIndex, bPressed, HasAuthority());
    if (!HasAuthority() && HasLocalNetOwner()) ServerDirectionKey(KeyIndex, bPressed);
}

void AAlphaCharacter::ServerDirectionKey_Implementation(int32 KeyIndex, bool bPressed)
{
    if (!IsPlayerControlled()) return;
    ProcessDirectionKey(KeyIndex, bPressed, true);
}

void AAlphaCharacter::DirectionDPressed() { HandleDirectionKey(0, true); }
void AAlphaCharacter::DirectionDReleased() { HandleDirectionKey(0, false); }
void AAlphaCharacter::DirectionAPressed() { HandleDirectionKey(1, true); }
void AAlphaCharacter::DirectionAReleased() { HandleDirectionKey(1, false); }
void AAlphaCharacter::DirectionWPressed() { HandleDirectionKey(2, true); }
void AAlphaCharacter::DirectionWReleased() { HandleDirectionKey(2, false); }
void AAlphaCharacter::DirectionSPressed() { HandleDirectionKey(3, true); }
void AAlphaCharacter::DirectionSReleased() { HandleDirectionKey(3, false); }

void AAlphaCharacter::BeginJump()
{
    if (!IsMenuBlockingActions() && !bDefeated && CombatComponent->CanAct())
    {
        StopSprint();
        ReviveComponent->EndRevive();
        if (CanJump())
        {
            CancelAirComboForAction();
            Jump();
        }
    }
}

void AAlphaCharacter::LightAttack()
{
    if (!IsMenuBlockingActions() && !bDefeated)
    {
        StopSprint();
        ReviveComponent->EndRevive();
        SyncAttackFacing();
        CombatComponent->TryAttack(false);
    }
}

void AAlphaCharacter::HeavyAttack()
{
    if (!IsMenuBlockingActions() && !bDefeated)
    {
        StopSprint();
        ReviveComponent->EndRevive();
        SyncAttackFacing();
        CombatComponent->TryAttack(true);
    }
}

void AAlphaCharacter::SyncAttackFacing()
{
    // Direction key actions run before axis updates; use their held state when available.
    const AlphaPlayerControl::TapState* States = HasAuthority() ? AuthorityTapStates : LocalTapStates;
    const FVector Held((States[0].Held ? 1. : 0.) - (States[1].Held ? 1. : 0.),
        (States[2].Held ? 1. : 0.) - (States[3].Held ? 1. : 0.), 0.);
    if (!Held.IsNearlyZero()) FaceMoveIntent(Held);
    if (!HasAuthority())
    {
        const FVector Facing = GetActorForwardVector().GetSafeNormal2D();
        ServerSetFacing(Facing);
        LastRequestedFacing = Facing;
        LastFacingRequestTime = GetWorld()->GetTimeSeconds();
    }
}

void AAlphaCharacter::Dodge()
{
    if (!IsMenuBlockingActions() && !bDefeated)
    {
        StopSprint();
        SyncAttackFacing();
        ServerDodge();
    }
}

void AAlphaCharacter::BeginRevive()
{
    if (!IsMenuBlockingActions() && !bDefeated && CombatComponent->CanAct())
    {
        StopSprint();
        CancelAirComboForAction();
        ReviveComponent->BeginRevive();
    }
}

void AAlphaCharacter::EndRevive()
{
    ReviveComponent->EndRevive();
}

void AAlphaCharacter::Skill1()
{
    if (IsMenuBlockingActions()) return;
    StopSprint();
    ReviveComponent->EndRevive();
    SyncAttackFacing();
    AbilityComponent->TryUseSkill(0);
}

void AAlphaCharacter::Skill2()
{
    if (IsMenuBlockingActions()) return;
    StopSprint();
    ReviveComponent->EndRevive();
    SyncAttackFacing();
    AbilityComponent->TryUseSkill(1);
}

void AAlphaCharacter::Skill3()
{
    if (IsMenuBlockingActions()) return;
    StopSprint();
    ReviveComponent->EndRevive();
    SyncAttackFacing();
    AbilityComponent->TryUseSkill(2);
}

bool AAlphaCharacter::IsMenuBlockingActions() const
{
    const APlayerController* Player = Cast<APlayerController>(GetController());
    return Player && Player->IsMoveInputIgnored();
}

void AAlphaCharacter::ServerDodge_Implementation()
{
    if (bDefeated || !CombatComponent->CanAct()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastDodgeTime < DodgeCooldown) return;
    LastDodgeTime = Now;
    ReviveComponent->CancelRevive();
    CombatComponent->SetInvulnerableFor(0.18f);
    CombatComponent->SetActionLockFor(0.35f);

    // Rolling follows facing, including when momentum still points another way.
    const FVector Direction = GetActorForwardVector().GetSafeNormal2D();
    LaunchCharacter(Direction * DodgeSpeed, true, false);
    MulticastRoll(Direction);
}

void AAlphaCharacter::MulticastRoll_Implementation(FVector Facing)
{
    SetActorRotation(Facing.GetSafeNormal2D().Rotation());
    VisualComponent->PlayRoll();
    AlphaAudio::Play(GetWorld(), GetActorLocation(), EAlphaSoundCue::Roll, .6f);
}

void AAlphaCharacter::HandleDeath()
{
    if (!HasAuthority() || bDefeated) return;
    bDefeated = true;
    ReviveComponent->RegisterDowned();
    ApplyDefeatedState();
    ForceNetUpdate();
}

void AAlphaCharacter::OnRep_Defeated()
{
    ApplyDefeatedState();
}

void AAlphaCharacter::ApplyDefeatedState()
{
    GetWorldTimerManager().ClearTimer(PoiseFeedbackTimer);
    GetWorldTimerManager().ClearTimer(WeaponFeedbackTimer);
    if (bDefeated)
    {
        StopSprint();
        EndAirComboHold();
        StopJumping();
        GetCharacterMovement()->StopMovementImmediately();
        // Keep gravity active for an aerial knockout; MOVE_None would leave a
        // downed teammate floating above the rescue range indefinitely.
        GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    }
    else
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
    RestoreBodyAppearance();
    RestoreWeaponPose();
    VisualComponent->SetDefeated(bDefeated);
}

void AAlphaCharacter::ShowAttack(bool bHeavy)
{
    if (bDefeated) return;
    SetActorRotation(CombatComponent->GetAttackFacing().Rotation());
    VisualFacingLockedUntil = GetWorld()->GetTimeSeconds() + (bHeavy ? .8 : .85);
    ReviveComponent->CancelRevive();
    // Modular weapon animation is cosmetic; the combat component still owns all attack timing.
    VisualComponent->PlayAttack(bHeavy);
    AlphaAudio::Play(GetWorld(), GetActorLocation(), EAlphaSoundCue::Slash, .55f);
}

void AAlphaCharacter::CancelAttackVisuals()
{
    VisualFacingLockedUntil = 0.0;
    VisualComponent->CancelAttackPresentation();
}

void AAlphaCharacter::ShowCombo(int32 Stage, bool bAirborne)
{
    if (bDefeated) return;
    if (HasAuthority() || IsLocallyControlled()) StopSprint();
    ReviveComponent->CancelRevive();
    SetActorRotation(CombatComponent->GetAttackFacing().Rotation());
    VisualFacingLockedUntil = GetWorld()->GetTimeSeconds()
        + AlphaPlayerControl::GetComboAttack(Stage, bAirborne).Duration;
    VisualComponent->PlayCombo(Stage, bAirborne);
    AlphaAudio::Play(GetWorld(), GetActorLocation(), EAlphaSoundCue::Slash, Stage == 5 ? .85f : .6f);
}

void AAlphaCharacter::HandleComboFinished()
{
    VisualFacingLockedUntil = 0.0;
    EndAirComboHold();
}

void AAlphaCharacter::HandleAirComboChanged(bool bHolding)
{
    if (bHolding) BeginAirComboHold();
    else EndAirComboHold();
}

void AAlphaCharacter::BeginAirComboHold()
{
    if (bDefeated || !CombatComponent->IsAlive()) return;
    if (bAirComboHeld && bSavedGravity) return;
    bAirComboHeld = true;
    ApplyAirComboHold();
    // An independent safety deadline never replaces the combat component's
    // per-flight latch. A fifth hit or cancellation cannot renew that latch.
    GetWorldTimerManager().SetTimer(AirComboHoldTimer, this, &AAlphaCharacter::AirComboHoldExpired,
        static_cast<float>(AlphaPlayerControl::AirComboMaxSeconds), false);
    if (HasAuthority()) ForceNetUpdate();
}

void AAlphaCharacter::EndAirComboHold()
{
    bAirComboHeld = false;
    GetWorldTimerManager().ClearTimer(AirComboHoldTimer);
    ApplyAirComboHold();
    if (HasAuthority()) ForceNetUpdate();
}

void AAlphaCharacter::ApplyAirComboHold()
{
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (bAirComboHeld && !bDefeated)
    {
        if (!bSavedGravity)
        {
            SavedGravityScale = Movement->GravityScale;
            bSavedGravity = true;
        }
        Movement->GravityScale = 0.f;
        Movement->Velocity.Z = 0.;
        Movement->SetMovementMode(MOVE_Falling);
    }
    else if (bSavedGravity)
    {
        Movement->GravityScale = SavedGravityScale;
        bSavedGravity = false;
    }
}

void AAlphaCharacter::OnRep_AirComboHeld()
{
    ApplyAirComboHold();
}

void AAlphaCharacter::AirComboHoldExpired()
{
    if (HasAuthority()) CombatComponent->CancelCurrentAttack();
    EndAirComboHold();
}

void AAlphaCharacter::CancelAirComboForAction()
{
    if (HasAuthority()) CombatComponent->CancelCurrentAttack();
    else if (HasLocalNetOwner()) ServerCancelAirCombo();
    EndAirComboHold();
}

void AAlphaCharacter::ServerCancelAirCombo_Implementation()
{
    CombatComponent->CancelCurrentAttack();
}

void AAlphaCharacter::ShowPoiseBreak()
{
    if (bDefeated) return;
    VisualComponent->PlayPoiseBreak();
    AlphaAudio::Play(GetWorld(), GetActorLocation(), EAlphaSoundCue::Poise, .85f);
    AAlphaCombatEffect::Spawn(GetWorld(), GetActorLocation(), GetActorForwardVector(), EAlphaEffectType::Poise, TeamColor);
    // A visual stagger cue, not a movement stun or network time dilation.
    if (BodyMaterial) BodyMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.85f, 0.15f));
    PrototypeBody->SetRelativeRotation(FRotator(0.0f, 0.0f, 18.0f));
    GetWorldTimerManager().SetTimer(PoiseFeedbackTimer, this, &AAlphaCharacter::RestoreBodyAppearance, 0.3f, false);
}

void AAlphaCharacter::ShowHit(float ActualDamage, AActor* InstigatorActor)
{
    if (ActualDamage <= 0.0f) return;
    if (HasAuthority() || IsLocallyControlled()) StopSprint();
    VisualComponent->PlayHit(ActualDamage);
    AlphaAudio::Play(GetWorld(), GetActorLocation(), EAlphaSoundCue::Hit, .7f);
    AAlphaCombatCamera::KickAt(GetWorld(), GetActorLocation(), .22f);
    if (IsValid(InstigatorActor))
        if (UAlphaCharacterVisualComponent* AttackerVisual = InstigatorActor->FindComponentByClass<UAlphaCharacterVisualComponent>())
            AttackerVisual->PausePoseForImpact(.04f);
    AAlphaCombatEffect::Spawn(GetWorld(), GetActorLocation(), GetActorForwardVector(), EAlphaEffectType::Hit, FLinearColor(1.0f, 0.65f, 0.3f));
}

void AAlphaCharacter::RestoreWeaponPose()
{
    PrototypeWeapon->SetRelativeLocation(FVector(32.0f, -30.0f, 20.0f));
    PrototypeWeapon->SetRelativeRotation(FRotator::ZeroRotator);
    PrototypeWeapon->SetRelativeScale3D(FVector::OneVector);
    PrototypeWeapon->SetVisibility(false, false);
    if (WeaponMaterial) WeaponMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.65f, 0.7f, 0.75f));
}

void AAlphaCharacter::RestoreBodyAppearance()
{
    PrototypeBody->SetRelativeRotation(bDefeated ? FRotator(0.0f, 0.0f, 75.0f) : FRotator::ZeroRotator);
    if (BodyMaterial)
    {
        BodyMaterial->SetVectorParameterValue(TEXT("Color"), bDefeated ? FLinearColor(0.15f, 0.15f, 0.15f) : TeamColor);
    }
}

void AAlphaCharacter::ResetForRound()
{
    if (!HasAuthority()) return;
    StopSprint();
    EndAirComboHold();
    ReviveComponent->ResetForRound();
    AbilityComponent->ResetForRound();
    CombatComponent->ResetVitals();
    bDefeated = false;
    LastDodgeTime = -1000.0;
    LastFacingRequestTime = -1000.0;
    ApplyDefeatedState();
    ForceNetUpdate();
}

bool AAlphaCharacter::RecoverFromRevive()
{
    if (!HasAuthority() || !bDefeated || !CombatComponent->IsAlive()) return false;
    bDefeated = false;
    ApplyDefeatedState();
    ForceNetUpdate();
    return true;
}

void AAlphaCharacter::SetArchetype(int32 NewArchetype)
{
    if (!HasAuthority() || CombatComponent->TeamId != 1 || NewArchetype < 0 || NewArchetype > 2) return;
    StopSprint();
    EndAirComboHold();
    Archetype = NewArchetype;
    ReviveComponent->ResetForRound();
    AbilityComponent->ResetForRound();
    CombatComponent->ConfigureArchetype(Archetype);
    bDefeated = false;
    ApplyArchetypeAppearance();
    ApplyDefeatedState();
    ForceNetUpdate();
}

void AAlphaCharacter::OnRep_Archetype()
{
    ApplyArchetypeAppearance();
}

void AAlphaCharacter::ApplyArchetypeAppearance()
{
    if (CombatComponent->TeamId != 1)
    {
        TeamColor = FLinearColor(0.85f, 0.12f, 0.04f);
    }
    else if (Archetype == 1)
    {
        TeamColor = FLinearColor(0.02f, 0.7f, 0.55f);
        PrototypeBody->SetRelativeScale3D(FVector(0.45f, 0.45f, 1.4f));
        BaseWalkSpeed = 560.0f;
    }
    else if (Archetype == 2)
    {
        TeamColor = FLinearColor(0.95f, 0.7f, 0.08f);
        PrototypeBody->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.65f));
        BaseWalkSpeed = 440.0f;
    }
    else
    {
        TeamColor = FLinearColor(0.65f, 0.06f, 0.12f);
        PrototypeBody->SetRelativeScale3D(FVector(0.65f, 0.6f, 1.5f));
        BaseWalkSpeed = 480.0f;
    }
    RefreshSprintSpeed();
    VisualComponent->RefreshAppearance(Archetype, CombatComponent->TeamId != 1, ActorHasTag(TEXT("Boss")));
    RestoreBodyAppearance();
    RestoreWeaponPose();
}

void AAlphaCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AAlphaCharacter, bDefeated);
    DOREPLIFETIME(AAlphaCharacter, Archetype);
    DOREPLIFETIME(AAlphaCharacter, bSprinting);
    DOREPLIFETIME(AAlphaCharacter, SprintKey);
    DOREPLIFETIME(AAlphaCharacter, SprintRevision);
    DOREPLIFETIME(AAlphaCharacter, bAirComboHeld);
}
