#include "Visuals/AlphaCharacterVisualComponent.h"

#include "Visuals/AlphaArtLibrary.h"
#include "Visuals/AlphaCombatEffect.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

UAlphaCharacterVisualComponent::UAlphaCharacterVisualComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    // Deliberately not replicated: authority transforms and combat events are the source of truth.
}

void UAlphaCharacterVisualComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer)
    {
        GetWorld()->GetTimerManager().SetTimer(TickBudgetTimer, this,
            &UAlphaCharacterVisualComponent::UpdateTickBudget, 0.5f, true);
    }
}

void UAlphaCharacterVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(TickBudgetTimer);
    ClearParts();
    Super::EndPlay(EndPlayReason);
}

void UAlphaCharacterVisualComponent::InitializeAnchors(USceneComponent* BodyAnchor, USceneComponent* WeaponAnchor)
{
    if (!BodyAnchor || !WeaponAnchor || !GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) return;
    if (!BodyRoot)
    {
        BodyRoot = AddPivot(TEXT("CharacterArtBody"), BodyAnchor, FVector::ZeroVector);
        BodyRoot->SetAbsolute(false, false, true);
    }
    if (!WeaponRoot)
    {
        WeaponRoot = AddPivot(TEXT("CharacterArtWeapon"), WeaponAnchor, FVector::ZeroVector);
        WeaponRoot->SetAbsolute(false, false, true);
    }
    UpdateTickBudget();
}

USceneComponent* UAlphaCharacterVisualComponent::AddPivot(FName Name, USceneComponent* Parent, const FVector& Location)
{
    USceneComponent* Pivot = NewObject<USceneComponent>(GetOwner(),
        MakeUniqueObjectName(GetOwner(), USceneComponent::StaticClass(), Name));
    GetOwner()->AddInstanceComponent(Pivot);
    Pivot->SetupAttachment(Parent);
    Pivot->SetMobility(EComponentMobility::Movable);
    Pivot->SetRelativeLocation(Location);
    Pivot->RegisterComponent();
    if (Parent != GetOwner()->GetRootComponent() && Parent != nullptr
        && Name != FName(TEXT("CharacterArtBody")) && Name != FName(TEXT("CharacterArtWeapon"))) Pivots.Add(Pivot);
    return Pivot;
}

UMaterialInstanceDynamic* UAlphaCharacterVisualComponent::MakeMaterial(FLinearColor Tint, float Metallic, float Roughness, float Glow)
{
    UMaterialInstanceDynamic* Material = AlphaArt::Material(this, Tint, Metallic, Roughness, Glow);
    Materials.Add(Material);
    BaseTints.Add(Tint);
    BaseGlows.Add(Glow);
    return Material;
}

UStaticMeshComponent* UAlphaCharacterVisualComponent::AddPart(EAlphaPrimitive Shape, USceneComponent* Parent,
    const FVector& Location, const FVector& Dimensions, UMaterialInstanceDynamic* Material, const FRotator& Rotation)
{
    UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(GetOwner());
    GetOwner()->AddInstanceComponent(Part);
    Part->SetupAttachment(Parent);
    Part->SetMobility(EComponentMobility::Movable);
    Part->SetStaticMesh(AlphaArt::Mesh(Shape));
    Part->SetMaterial(0, Material);
    Part->SetRelativeLocation(Location);
    Part->SetRelativeRotation(Rotation);
    // Engine basic shapes are 100 cm across; dimensions are authored in local centimeters.
    Part->SetRelativeScale3D(Dimensions / 100.0f);
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetGenerateOverlapEvents(false);
    Part->SetCanEverAffectNavigation(false);
    Part->SetCastShadow(Material != GlowMaterial);
    Part->SetReceivesDecals(false);
    Part->RegisterComponent();
    Parts.Add(Part);
    return Part;
}

void UAlphaCharacterVisualComponent::ClearParts()
{
    for (UStaticMeshComponent* Part : Parts) if (IsValid(Part)) Part->DestroyComponent();
    for (USceneComponent* Pivot : Pivots) if (IsValid(Pivot)) Pivot->DestroyComponent();
    Parts.Reset();
    Pivots.Reset();
    Materials.Reset();
    BaseTints.Reset();
    BaseGlows.Reset();
    LastHitFlash = LastPoiseFlash = -1.0f;
    LeftLeg = RightLeg = LeftArm = RightArm = nullptr;
    ClothAccent = nullptr;
    LanternRing = nullptr;
    ClothMaterial = MetalMaterial = AccentMaterial = FaceMaterial = GlowMaterial = nullptr;
}

void UAlphaCharacterVisualComponent::RefreshAppearance(int32 Hero, bool bEnemy, bool bBoss)
{
    HeroType = FMath::Clamp(Hero, 0, 2);
    bEnemyVisual = bEnemy;
    bBossVisual = bBoss;
    if (!BodyRoot || !WeaponRoot) return;
    ClearParts();
    BodyRoot->SetRelativeScale3D(FVector(bBossVisual ? 1.35f : 1.0f));
    WeaponRoot->SetRelativeScale3D(FVector(bBossVisual ? 1.35f : 1.0f));
    const FLinearColor Accent = bEnemyVisual ? FLinearColor(0.85f, 0.22f, 0.035f)
        : (HeroType == 0 ? FLinearColor(0.6f, 0.025f, 0.055f)
        : (HeroType == 1 ? FLinearColor(0.025f, 0.5f, 0.42f) : FLinearColor(0.75f, 0.52f, 0.13f)));
    ClothMaterial = MakeMaterial(bEnemyVisual ? FLinearColor(0.095f, 0.07f, 0.06f) : Accent * 0.38f, 0.05f, 0.85f);
    MetalMaterial = MakeMaterial(FLinearColor(0.13f, 0.16f, 0.18f), 0.78f, 0.3f);
    AccentMaterial = MakeMaterial(Accent, bEnemyVisual ? 0.65f : 0.35f, 0.42f);
    FaceMaterial = MakeMaterial(bEnemyVisual ? FLinearColor(0.74f, 0.67f, 0.49f) : FLinearColor(0.7f, 0.55f, 0.37f), 0.08f, 0.7f);
    GlowMaterial = MakeMaterial(bEnemyVisual ? FLinearColor(1.0f, 0.19f, 0.02f) : Accent * 1.6f, 0.2f, 0.3f, bBossVisual ? 4.0f : 2.2f);
    BuildSilhouette();
    BuildWeapon();
    AttackStartedAt = -1000.0;
    HitUntil = PoiseUntil = -1000.0;
    bAttackCueSpawned = true;
    SetDefeated(bDefeatedVisual);
    UpdateTickBudget();
}

void UAlphaCharacterVisualComponent::BuildSilhouette()
{
    // Ten-part humanoid core. Shoulder/cloth/prop additions keep each silhouette at <= 20 mesh components.
    const bool bArmored = bEnemyVisual || HeroType == 0;
    AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, 0, 12), FVector(bArmored ? 42 : 33, 38, 53), ClothMaterial);
    AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, 0, -22), FVector(29, 32, 18), MetalMaterial);
    AddPart(EAlphaPrimitive::Sphere, BodyRoot, FVector(0, 0, 63), FVector(29, 28, 32), bEnemyVisual ? MetalMaterial : FaceMaterial);
    AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(13, 0, 64), FVector(6, 23, 23), bEnemyVisual ? FaceMaterial : MetalMaterial);

    LeftArm = AddPivot(TEXT("ArtLeftArm"), BodyRoot, FVector(0, -28, 34));
    RightArm = AddPivot(TEXT("ArtRightArm"), BodyRoot, FVector(0, 28, 34));
    AddPart(EAlphaPrimitive::Cube, LeftArm, FVector(0, 0, -22), FVector(15, 16, 43), bArmored ? MetalMaterial : AccentMaterial);
    AddPart(EAlphaPrimitive::Cube, RightArm, FVector(0, 0, -22), FVector(HeroType == 1 && !bEnemyVisual ? 21 : 15, 16, 43), MetalMaterial);
    LeftLeg = AddPivot(TEXT("ArtLeftLeg"), BodyRoot, FVector(0, -12, -29));
    RightLeg = AddPivot(TEXT("ArtRightLeg"), BodyRoot, FVector(0, 12, -29));
    USceneComponent* Legs[] = {LeftLeg.Get(), RightLeg.Get()};
    for (USceneComponent* Leg : Legs)
    {
        AddPart(EAlphaPrimitive::Cube, Leg, FVector(0, 0, -22), FVector(17, 18, 42), ClothMaterial);
        AddPart(EAlphaPrimitive::Cube, Leg, FVector(9, 0, -49), FVector(30, 21, 12), MetalMaterial);
    }

    if (bEnemyVisual)
    {
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, -34, 35), FVector(bBossVisual ? 42 : 27, 24, 23), MetalMaterial, FRotator(0, 0, -15));
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, 34, 35), FVector(bBossVisual ? 42 : 27, 24, 23), MetalMaterial, FRotator(0, 0, 15));
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(24, 0, 15), FVector(10, 32, 38), AccentMaterial);
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(-27, 0, 17), FVector(25, 39, 45), MetalMaterial);
        AddPart(EAlphaPrimitive::Cylinder, BodyRoot, FVector(-41, 0, 20), FVector(25, 25, 8), GlowMaterial, FRotator(90, 0, 0));
        AddPart(EAlphaPrimitive::Cylinder, BodyRoot, FVector(-29, -14, 49), FVector(11, 11, bBossVisual ? 46 : 27), MetalMaterial);
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(17, 0, 66), FVector(3, 20, 4), GlowMaterial);
        if (bBossVisual) AddPart(EAlphaPrimitive::Cylinder, BodyRoot, FVector(-29, 14, 49), FVector(11, 11, 46), MetalMaterial);
    }
    else if (HeroType == 0)
    {
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, -34, 34), FVector(34, 25, 23), MetalMaterial, FRotator(0, 0, -12));
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, 32, 34), FVector(26, 20, 20), AccentMaterial);
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(22, 0, 14), FVector(7, 31, 37), MetalMaterial);
        ClothAccent = AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(-17, -15, -27), FVector(8, 22, 59), AccentMaterial, FRotator(-12, 0, -8));
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(-17, 15, -27), FVector(8, 22, 59), AccentMaterial, FRotator(-12, 0, 8));
        AddPart(EAlphaPrimitive::Cone, BodyRoot, FVector(-5, 0, 79), FVector(29, 28, 16), MetalMaterial);
    }
    else if (HeroType == 1)
    {
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, 32, 32), FVector(25, 20, 25), MetalMaterial);
        ClothAccent = AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(-22, -3, 37), FVector(39, 10, 7), AccentMaterial, FRotator(-12, 0, -8));
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(-23, 0, 15), FVector(20, 28, 33), MetalMaterial);
        AddPart(EAlphaPrimitive::Cylinder, BodyRoot, FVector(-35, -14, 17), FVector(20, 20, 8), AccentMaterial, FRotator(0, 0, 90));
        AddPart(EAlphaPrimitive::Sphere, BodyRoot, FVector(-35, -20, 17), FVector(11, 5, 11), GlowMaterial);
        AddPart(EAlphaPrimitive::Cone, BodyRoot, FVector(-8, 0, 78), FVector(32, 29, 22), MetalMaterial, FRotator(-25, 0, 0));
    }
    else
    {
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, -30, 31), FVector(25, 20, 17), AccentMaterial);
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, 30, 31), FVector(25, 20, 17), AccentMaterial);
        ClothAccent = AddPart(EAlphaPrimitive::Cone, BodyRoot, FVector(-2, 0, -33), FVector(54, 47, 67), ClothMaterial);
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(18, 0, 22), FVector(7, 19, 22), AccentMaterial);
        AddPart(EAlphaPrimitive::Cylinder, BodyRoot, FVector(-21, -27, -4), FVector(19, 19, 31), MetalMaterial);
        AddPart(EAlphaPrimitive::Sphere, BodyRoot, FVector(-21, -27, -4), FVector(15, 15, 23), GlowMaterial);
        LanternRing = NewObject<UInstancedStaticMeshComponent>(GetOwner());
        GetOwner()->AddInstanceComponent(LanternRing);
        LanternRing->SetupAttachment(BodyRoot);
        LanternRing->SetMobility(EComponentMobility::Movable);
        LanternRing->SetStaticMesh(AlphaArt::Mesh(EAlphaPrimitive::Cube));
        LanternRing->SetMaterial(0, GlowMaterial);
        LanternRing->SetRelativeLocation(FVector(0, 20, 45));
        LanternRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        LanternRing->SetGenerateOverlapEvents(false);
        LanternRing->SetCanEverAffectNavigation(false);
        LanternRing->SetCastShadow(false);
        LanternRing->SetReceivesDecals(false);
        LanternRing->RegisterComponent();
        Parts.Add(LanternRing);
        for (int32 Index = 0; Index < 8; ++Index)
        {
            const float Angle = 2.0f * PI * Index / 8.0f;
            LanternRing->AddInstance(FTransform(FRotator(-FMath::RadiansToDegrees(Angle) - 90.0f, 0, 0),
                FVector(FMath::Cos(Angle) * 39.0f, 0, FMath::Sin(Angle) * 39.0f), FVector(0.23f, 0.025f, 0.045f)));
        }
    }
}

void UAlphaCharacterVisualComponent::BuildWeapon()
{
    if (bBossVisual)
    {
        AddPart(EAlphaPrimitive::Cylinder, WeaponRoot, FVector(45, 0, 0), FVector(9, 9, 115), MetalMaterial, FRotator(90, 0, 0));
        AddPart(EAlphaPrimitive::Cube, WeaponRoot, FVector(95, 0, 0), FVector(40, 66, 54), AccentMaterial);
    }
    else if (bEnemyVisual)
    {
        AddPart(EAlphaPrimitive::Cube, WeaponRoot, FVector(45, 0, 0), FVector(70, 8, 24), MetalMaterial);
        AddPart(EAlphaPrimitive::Cylinder, WeaponRoot, FVector(3, 0, 0), FVector(7, 7, 27), ClothMaterial, FRotator(90, 0, 0));
    }
    else if (HeroType == 0)
    {
        AddPart(EAlphaPrimitive::Cube, WeaponRoot, FVector(58, 0, 0), FVector(100, 5, 12), FaceMaterial, FRotator(0, 0, 4));
        AddPart(EAlphaPrimitive::Cube, WeaponRoot, FVector(4, 0, 0), FVector(6, 12, 28), AccentMaterial);
        AddPart(EAlphaPrimitive::Cylinder, WeaponRoot, FVector(-13, 0, 0), FVector(7, 7, 28), ClothMaterial, FRotator(90, 0, 0));
    }
    else if (HeroType == 1)
    {
        AddPart(EAlphaPrimitive::Cube, WeaponRoot, FVector(31, -10, 0), FVector(55, 5, 10), FaceMaterial, FRotator(8, 0, 0));
        AddPart(EAlphaPrimitive::Cube, WeaponRoot, FVector(23, 14, -10), FVector(48, 5, 10), FaceMaterial, FRotator(-12, 0, 0));
        AddPart(EAlphaPrimitive::Cube, WeaponRoot, FVector(0, -10, 0), FVector(9, 9, 18), AccentMaterial);
        AddPart(EAlphaPrimitive::Cube, WeaponRoot, FVector(-5, 14, -10), FVector(9, 9, 18), AccentMaterial);
    }
    else
    {
        AddPart(EAlphaPrimitive::Cylinder, WeaponRoot, FVector(5, 0, 4), FVector(6, 6, 145), MetalMaterial);
        AddPart(EAlphaPrimitive::Sphere, WeaponRoot, FVector(5, 0, 77), FVector(23, 23, 28), GlowMaterial);
    }
}

void UAlphaCharacterVisualComponent::SetDefeated(bool bNewDefeated)
{
    bDefeatedVisual = bNewDefeated;
    AttackStartedAt = -1000.0;
    HitUntil = PoiseUntil = -1000.0;
    bAttackCueSpawned = true;
    bBossWindup = false;
    if (WeaponRoot)
    {
        WeaponRoot->SetVisibility(!bDefeatedVisual, true);
        WeaponRoot->SetRelativeLocation(FVector::ZeroVector);
        WeaponRoot->SetRelativeRotation(FRotator::ZeroRotator);
    }
    if (BodyRoot)
    {
        BodyRoot->SetRelativeLocation(FVector::ZeroVector);
        BodyRoot->SetRelativeRotation(FRotator::ZeroRotator);
    }
    ApplyMaterialState(0.0f, 0.0f);
    UpdateTickBudget();
}

double UAlphaCharacterVisualComponent::VisualTime() const
{
    return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UAlphaCharacterVisualComponent::PlayAttack(bool bHeavy)
{
    if (bDefeatedVisual || !WeaponRoot) return;
    bHeavyAttack = bHeavy;
    AttackWindup = bEnemyVisual ? (bHeavy ? 0.32f : 0.12f)
        : (HeroType == 0 ? (bHeavy ? 0.36f : 0.15f)
        : (HeroType == 1 ? (bHeavy ? 0.23f : 0.08f) : (bHeavy ? 0.30f : 0.12f)));
    AttackDuration = AttackWindup + (bHeavy ? 0.24f : 0.16f);
    AttackStartedAt = VisualTime();
    bAttackCueSpawned = false;
    UpdateTickBudget();
}

void UAlphaCharacterVisualComponent::PlayHit(float Damage)
{
    if (Damage <= 0.0f || !BodyRoot) return;
    CancelAttackPresentation();
    HitUntil = VisualTime() + 0.16;
    ApplyMaterialState(1.0f, 0.0f);
    UpdateTickBudget();
}

void UAlphaCharacterVisualComponent::PlayPoiseBreak()
{
    if (!BodyRoot) return;
    CancelAttackPresentation();
    PoiseUntil = VisualTime() + 0.4;
    ApplyMaterialState(0.0f, 1.0f);
    UpdateTickBudget();
}

void UAlphaCharacterVisualComponent::CancelAttackPresentation()
{
    AttackStartedAt = -1000.0;
    bAttackCueSpawned = true;
    if (WeaponRoot && !bBossWindup)
    {
        WeaponRoot->SetRelativeLocation(FVector::ZeroVector);
        WeaponRoot->SetRelativeRotation(FRotator::ZeroRotator);
    }
}

void UAlphaCharacterVisualComponent::SetBossTell(int32 Attack, float Progress, bool bWindingUp, bool bRecovering)
{
    if (!bBossVisual || bDefeatedVisual) return;
    if (bBossWindup && !bWindingUp && bRecovering && Attack != 0)
    {
        PlayAttack(true);
        AttackStartedAt -= AttackWindup; // authoritative impact has already occurred; swing out of the held pose.
    }
    bBossWindup = bWindingUp;
    BossAttackType = Attack;
    BossWindupProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
}

void UAlphaCharacterVisualComponent::UpdateTickBudget()
{
    if (!GetWorld() || !GetOwner() || !BodyRoot || GetWorld()->GetNetMode() == NM_DedicatedServer)
    {
        SetComponentTickEnabled(false);
        return;
    }
    float DistanceSquared = 0.0f;
    if (const APlayerController* Player = GetWorld()->GetFirstPlayerController())
    {
        FVector ViewLocation;
        FRotator ViewRotation;
        Player->GetPlayerViewPoint(ViewLocation, ViewRotation);
        DistanceSquared = FVector::DistSquared(ViewLocation, GetOwner()->GetActorLocation());
    }
    // The low-rate timer wakes this component when a remote/off-screen character comes back into view.
    const bool bNear = DistanceSquared < FMath::Square(6000.0f);
    SetComponentTickEnabled(bNear && !bDefeatedVisual);
    // The normal co-op camera can sit ~4,100 cm away; keep the entire arena at full visual rate.
    SetComponentTickInterval(DistanceSquared > FMath::Square(4500.0f) ? 0.066f : 0.0f);
}

void UAlphaCharacterVisualComponent::ApplyMaterialState(float HitFlash, float PoiseFlash)
{
    if (FMath::IsNearlyEqual(HitFlash, LastHitFlash, 0.001f)
        && FMath::IsNearlyEqual(PoiseFlash, LastPoiseFlash, 0.001f)
        && bLastMaterialDefeated == bDefeatedVisual) return;
    LastHitFlash = HitFlash;
    LastPoiseFlash = PoiseFlash;
    bLastMaterialDefeated = bDefeatedVisual;
    for (int32 Index = 0; Index < Materials.Num(); ++Index)
    {
        UMaterialInstanceDynamic* Material = Materials[Index];
        if (!Material) continue;
        FLinearColor Tint = BaseTints[Index];
        if (bDefeatedVisual) Tint *= 0.28f;
        else
        {
            Tint = FMath::Lerp(Tint, FLinearColor(1.0f, 0.78f, 0.18f), PoiseFlash * 0.8f);
            Tint = FMath::Lerp(Tint, FLinearColor::White, HitFlash * 0.8f);
        }
        Material->SetVectorParameterValue(TEXT("Tint"), Tint);
        Material->SetVectorParameterValue(TEXT("Color"), Tint); // BasicShapeMaterial fallback.
        Material->SetScalarParameterValue(TEXT("Glow"), bDefeatedVisual ? 0.0f : BaseGlows[Index] + HitFlash * 1.5f + PoiseFlash);
    }
}

void UAlphaCharacterVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!BodyRoot || !WeaponRoot || !GetOwner() || bDefeatedVisual) return;
    const double Now = VisualTime();
    const float Speed = FMath::Clamp(GetOwner()->GetVelocity().Size2D() / 500.0f, 0.0f, 1.0f);
    GaitPhase += DeltaTime * Speed * 11.0f;
    const float Wave = FMath::Sin(GaitPhase);
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    const bool bAirborne = Character && Character->GetCharacterMovement()->IsFalling();
    const float LegAngle = bAirborne ? 18.0f : Wave * Speed * 27.0f;
    if (LeftLeg) LeftLeg->SetRelativeRotation(FRotator(LegAngle, 0, 0));
    if (RightLeg) RightLeg->SetRelativeRotation(FRotator(-LegAngle, 0, 0));
    if (LeftArm) LeftArm->SetRelativeRotation(FRotator(-LegAngle * 0.65f, 0, 0));
    if (RightArm) RightArm->SetRelativeRotation(FRotator(LegAngle * 0.65f, 0, 0));
    const float HitFlash = FMath::Clamp(static_cast<float>((HitUntil - Now) / 0.16), 0.0f, 1.0f);
    const float PoiseFlash = FMath::Clamp(static_cast<float>((PoiseUntil - Now) / 0.4), 0.0f, 1.0f);
    BodyRoot->SetRelativeLocation(FVector(-HitFlash * 6.0f, 0, FMath::Abs(Wave) * Speed * 2.5f));
    if (ClothAccent)
    {
        const float Flutter = FMath::Sin(GaitPhase - 0.8f) * Speed * 8.0f;
        const FRotator Rotation = HeroType == 2 ? FRotator(Flutter * 0.2f, 0, 0) : FRotator(-12.0f + Flutter, 0, -8);
        ClothAccent->SetRelativeRotation(Rotation);
    }
    if (LanternRing) LanternRing->AddLocalRotation(FRotator(DeltaTime * 16.0f, 0, 0));

    const float AttackTime = static_cast<float>(Now - AttackStartedAt);
    if (bBossWindup)
    {
        const float Raise = FMath::InterpEaseInOut(0.0f, 1.0f, BossWindupProgress, 2.0f);
        WeaponRoot->SetRelativeRotation(FRotator(-95.0f * Raise, BossAttackType == 1 ? -50.0f * Raise : 0.0f, 0));
        WeaponRoot->SetRelativeLocation(FVector(0, 0, Raise * 35.0f));
    }
    else if (AttackTime >= 0.0f && AttackTime < AttackDuration)
    {
        const bool bBeforeImpact = AttackTime < AttackWindup;
        const float Phase = bBeforeImpact ? AttackTime / AttackWindup : (AttackTime - AttackWindup) / (AttackDuration - AttackWindup);
        const float Pitch = bBeforeImpact ? FMath::Lerp(0.0f, -65.0f, Phase) : FMath::Lerp(-65.0f, 55.0f, FMath::InterpEaseOut(0.0f, 1.0f, Phase, 3.0f));
        WeaponRoot->SetRelativeRotation(FRotator(Pitch, bHeavyAttack ? 20.0f * Phase : -12.0f * Phase, 0));
        WeaponRoot->SetRelativeLocation(FVector(bBeforeImpact ? 0.0f : FMath::Sin(Phase * PI) * 25.0f, 0, 0));
        if (RightArm) RightArm->SetRelativeRotation(FRotator(Pitch * 0.65f, 0, 0));
        if (!bBeforeImpact && !bAttackCueSpawned)
        {
            // Boss range is defined by its replicated floor tell, never by an unrelated generic slash arc.
            if (!bBossVisual)
            {
                AAlphaCombatEffect::Spawn(GetWorld(), GetOwner()->GetActorLocation(), GetOwner()->GetActorForwardVector(),
                    bHeavyAttack ? EAlphaEffectType::HeavySlash : EAlphaEffectType::Slash,
                    bEnemyVisual ? FLinearColor(1.0f, 0.25f, 0.04f) : BaseTints[2]);
            }
            bAttackCueSpawned = true;
        }
    }
    else
    {
        WeaponRoot->SetRelativeRotation(FRotator(0, 0, Wave * Speed * 3.0f));
        WeaponRoot->SetRelativeLocation(FVector::ZeroVector);
    }
    ApplyMaterialState(HitFlash, PoiseFlash);
}
