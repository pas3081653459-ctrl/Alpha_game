#include "Characters/AlphaCharacter.h"

#include "Combat/AlphaCombatComponent.h"
#include "Coop/AlphaReviveComponent.h"
#include "Abilities/AlphaAbilityComponent.h"
#include "Visuals/AlphaCharacterVisualComponent.h"
#include "Visuals/AlphaCombatEffect.h"
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
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);
    NetUpdateFrequency = 60.0f;
    MinNetUpdateFrequency = 20.0f;

    GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
    GetCharacterMovement()->MaxWalkSpeed = 480.0f;
    GetCharacterMovement()->JumpZVelocity = 600.0f;
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

void AAlphaCharacter::BeginPlay()
{
    Super::BeginPlay();
    CombatComponent->OnDeath.AddDynamic(this, &AAlphaCharacter::HandleDeath);
    CombatComponent->OnAttack.AddDynamic(this, &AAlphaCharacter::ShowAttack);
    CombatComponent->OnAttackCancelled.AddDynamic(this, &AAlphaCharacter::CancelAttackVisuals);
    CombatComponent->OnPoiseBroken.AddDynamic(this, &AAlphaCharacter::ShowPoiseBreak);
    CombatComponent->OnHit.AddDynamic(this, &AAlphaCharacter::ShowHit);
    BodyMaterial = PrototypeBody->CreateAndSetMaterialInstanceDynamic(0);
    WeaponMaterial = PrototypeWeapon->CreateAndSetMaterialInstanceDynamic(0);
    VisualComponent->InitializeAnchors(PrototypeBody, PrototypeWeapon);
    // Hide only the original rendering; their child visual anchors stay visible and retain hit poses.
    PrototypeBody->SetVisibility(false, false);
    PrototypeWeapon->SetVisibility(false, false);
    if (HasAuthority() && CombatComponent->TeamId == 1) CombatComponent->ConfigureArchetype(Archetype);
    ApplyArchetypeAppearance();
    bLastRequestedFacingRight = GetActorForwardVector().X >= 0.0f;
    RestoreWeaponPose();
    ApplyDefeatedState();
}

void AAlphaCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(WeaponFeedbackTimer);
    GetWorldTimerManager().ClearTimer(PoiseFeedbackTimer);
    Super::EndPlay(EndPlayReason);
}

void AAlphaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AAlphaCharacter::MoveForward);
    PlayerInputComponent->BindAxis(TEXT("MoveDepth"), this, &AAlphaCharacter::MoveDepth);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &AAlphaCharacter::BeginJump);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
    PlayerInputComponent->BindAction(TEXT("LightAttack"), IE_Pressed, this, &AAlphaCharacter::LightAttack);
    PlayerInputComponent->BindAction(TEXT("HeavyAttack"), IE_Pressed, this, &AAlphaCharacter::HeavyAttack);
    PlayerInputComponent->BindAction(TEXT("Dodge"), IE_Pressed, this, &AAlphaCharacter::Dodge);
    PlayerInputComponent->BindAction(TEXT("Revive"), IE_Pressed, this, &AAlphaCharacter::BeginRevive);
    PlayerInputComponent->BindAction(TEXT("Revive"), IE_Released, this, &AAlphaCharacter::EndRevive);
    PlayerInputComponent->BindAction(TEXT("Skill1"), IE_Pressed, this, &AAlphaCharacter::Skill1);
    PlayerInputComponent->BindAction(TEXT("Skill2"), IE_Pressed, this, &AAlphaCharacter::Skill2);
}

void AAlphaCharacter::MoveForward(float Value)
{
    if (IsMenuBlockingActions() || bDefeated || !CombatComponent->CanAct()) return;
    AddMovementInput(FVector::ForwardVector, Value);
    if (!FMath::IsNearlyZero(Value))
    {
        FaceAlongLane(Value);
        const bool bFaceRight = Value > 0.0f;
        const double Now = GetWorld()->GetTimeSeconds();
        // A server-side round teleport can change yaw without changing the client's cached input sign.
        // Refresh while moving, at a bounded rate, rather than relying only on sign changes.
        if (!HasAuthority() && (bFaceRight != bLastRequestedFacingRight || Now - LastFacingRequestTime >= 0.15))
        {
            ServerSetFacing(bFaceRight);
            LastFacingRequestTime = Now;
        }
        bLastRequestedFacingRight = bFaceRight;
    }
}

void AAlphaCharacter::MoveDepth(float Value)
{
    if (!IsMenuBlockingActions() && !bDefeated && CombatComponent->CanAct())
    {
        AddMovementInput(FVector::RightVector, Value);
    }
}

void AAlphaCharacter::FaceAlongLane(float Direction)
{
    if (!FMath::IsNearlyZero(Direction))
    {
        const FRotator DesiredRotation(0.0f, Direction > 0.0f ? 0.0f : 180.0f, 0.0f);
        if (!GetActorRotation().Equals(DesiredRotation, 0.1f))
        {
            SetActorRotation(DesiredRotation);
            // Owning clients predict the turn; other clients receive authoritative movement rotation.
            if (HasAuthority()) ForceNetUpdate();
        }
    }
}

void AAlphaCharacter::ServerSetFacing_Implementation(bool bFaceRight)
{
    if (!bDefeated && CombatComponent->IsAlive())
    {
        FaceAlongLane(bFaceRight ? 1.0f : -1.0f);
    }
}

void AAlphaCharacter::BeginJump()
{
    if (!IsMenuBlockingActions() && !bDefeated && CombatComponent->CanAct())
    {
        ReviveComponent->EndRevive();
        Jump();
    }
}

void AAlphaCharacter::LightAttack()
{
    if (!IsMenuBlockingActions() && !bDefeated)
    {
        ReviveComponent->EndRevive();
        SyncAttackFacing();
        CombatComponent->TryAttack(false);
    }
}

void AAlphaCharacter::HeavyAttack()
{
    if (!IsMenuBlockingActions() && !bDefeated)
    {
        ReviveComponent->EndRevive();
        SyncAttackFacing();
        CombatComponent->TryAttack(true);
    }
}

void AAlphaCharacter::SyncAttackFacing()
{
    if (!HasAuthority())
    {
        // Both facing and the combat component's attack RPC are reliable on this actor's channel.
        // Submit facing first so an immediate post-reset attack uses the displayed direction.
        const bool bFaceRight = GetActorForwardVector().X >= 0.0f;
        ServerSetFacing(bFaceRight);
        bLastRequestedFacingRight = bFaceRight;
        LastFacingRequestTime = GetWorld()->GetTimeSeconds();
    }
}

void AAlphaCharacter::Dodge()
{
    if (!IsMenuBlockingActions() && !bDefeated) ServerDodge();
}

void AAlphaCharacter::BeginRevive()
{
    if (!IsMenuBlockingActions() && !bDefeated && CombatComponent->CanAct()) ReviveComponent->BeginRevive();
}

void AAlphaCharacter::EndRevive()
{
    ReviveComponent->EndRevive();
}

void AAlphaCharacter::Skill1()
{
    if (IsMenuBlockingActions()) return;
    ReviveComponent->EndRevive();
    SyncAttackFacing();
    AbilityComponent->TryUseSkill(0);
}

void AAlphaCharacter::Skill2()
{
    if (IsMenuBlockingActions()) return;
    ReviveComponent->EndRevive();
    SyncAttackFacing();
    AbilityComponent->TryUseSkill(1);
}

bool AAlphaCharacter::IsMenuBlockingActions() const
{
    const APlayerController* Player = Cast<APlayerController>(GetController());
    return Player && Player->IsMoveInputIgnored();
}

void AAlphaCharacter::ServerDodge_Implementation()
{
    if (bDefeated || !CombatComponent->CanAct() || !GetCharacterMovement()->IsMovingOnGround()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastDodgeTime < DodgeCooldown) return;
    LastDodgeTime = Now;
    ReviveComponent->CancelRevive();
    CombatComponent->SetInvulnerableFor(0.18f);
    CombatComponent->SetActionLockFor(0.18f);

    // Derive movement from authoritative velocity; clients cannot supply an arbitrary launch vector.
    FVector Direction = GetVelocity().GetSafeNormal2D();
    if (Direction.IsNearlyZero()) Direction = GetActorForwardVector();
    LaunchCharacter(Direction * DodgeSpeed, true, false);
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
        StopJumping();
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
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
    ReviveComponent->CancelRevive();
    // Modular weapon animation is cosmetic; the combat component still owns all attack timing.
    VisualComponent->PlayAttack(bHeavy);
}

void AAlphaCharacter::CancelAttackVisuals()
{
    VisualComponent->CancelAttackPresentation();
}

void AAlphaCharacter::ShowPoiseBreak()
{
    if (bDefeated) return;
    VisualComponent->PlayPoiseBreak();
    AAlphaCombatEffect::Spawn(GetWorld(), GetActorLocation(), GetActorForwardVector(), EAlphaEffectType::Poise, TeamColor);
    // A visual stagger cue, not a movement stun or network time dilation.
    if (BodyMaterial) BodyMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.85f, 0.15f));
    PrototypeBody->SetRelativeRotation(FRotator(0.0f, 0.0f, 18.0f));
    GetWorldTimerManager().SetTimer(PoiseFeedbackTimer, this, &AAlphaCharacter::RestoreBodyAppearance, 0.3f, false);
}

void AAlphaCharacter::ShowHit(float ActualDamage, AActor* InstigatorActor)
{
    if (ActualDamage <= 0.0f) return;
    VisualComponent->PlayHit(ActualDamage);
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
        GetCharacterMovement()->MaxWalkSpeed = 560.0f;
    }
    else if (Archetype == 2)
    {
        TeamColor = FLinearColor(0.95f, 0.7f, 0.08f);
        PrototypeBody->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.65f));
        GetCharacterMovement()->MaxWalkSpeed = 440.0f;
    }
    else
    {
        TeamColor = FLinearColor(0.65f, 0.06f, 0.12f);
        PrototypeBody->SetRelativeScale3D(FVector(0.65f, 0.6f, 1.5f));
        GetCharacterMovement()->MaxWalkSpeed = 480.0f;
    }
    VisualComponent->RefreshAppearance(Archetype, CombatComponent->TeamId != 1, ActorHasTag(TEXT("Boss")));
    RestoreBodyAppearance();
    RestoreWeaponPose();
}

void AAlphaCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AAlphaCharacter, bDefeated);
    DOREPLIFETIME(AAlphaCharacter, Archetype);
}
