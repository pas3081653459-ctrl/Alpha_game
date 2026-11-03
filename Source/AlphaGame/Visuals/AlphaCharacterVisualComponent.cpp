#include "Visuals/AlphaCharacterVisualComponent.h"

#include "Visuals/AlphaArtLibrary.h"
#include "Visuals/AlphaCombatCamera.h"
#include "Combat/AlphaCombatComponent.h"
#include "Abilities/AlphaAbilityComponent.h"
#include "Visuals/AlphaCombatEffect.h"
#include "Visuals/AlphaCombatAudio.h"
#include "Characters/AlphaCharacter.h"
#include "Characters/AlphaEnemy.h"
#include "Rules/PlayerControlRules.h"
#include "Rules/LevelLayoutRules.h"
#include "World/AlphaGameState.h"
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
    if (AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetOwner()))
    {
        Character->GetCombatComponent()->OnGroundImpact.AddDynamic(this, &UAlphaCharacterVisualComponent::ShowGroundImpact);
        Character->GetAbilityComponent()->OnSkillActivated.AddDynamic(this, &UAlphaCharacterVisualComponent::PlaySkill);
    }
    if (GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer)
    {
        GetWorld()->GetTimerManager().SetTimer(TickBudgetTimer, this,
            &UAlphaCharacterVisualComponent::UpdateTickBudget, 0.5f, true);
    }
}

void UAlphaCharacterVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetOwner()))
    {
        Character->GetCombatComponent()->OnGroundImpact.RemoveDynamic(this, &UAlphaCharacterVisualComponent::ShowGroundImpact);
        Character->GetAbilityComponent()->OnSkillActivated.RemoveDynamic(this, &UAlphaCharacterVisualComponent::PlaySkill);
    }
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
    if (Material) Material->SetScalarParameterValue(TEXT("AnimeShade"), 1.f);
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
    EnemyTell = nullptr;
    EnemyTellMaterial = nullptr;
    ShieldFace = nullptr;
    ShieldMaterial = nullptr;
    bEnemyWindingUp = false;
    bShieldGuarding = bLastShieldGuarding = false;
    bObservedEnemyShotRevision = false;
    EnemyShotVisibleUntil = -1000.;
    ClothMaterial = MetalMaterial = AccentMaterial = FaceMaterial = GlowMaterial = nullptr;
}

void UAlphaCharacterVisualComponent::RefreshAppearance(int32 Hero, bool bEnemy, bool bBoss)
{
    HeroType = FMath::Clamp(Hero, 0, 2);
    bEnemyVisual = bEnemy;
    bBossVisual = bBoss;
    const AAlphaEnemy* Enemy = Cast<AAlphaEnemy>(GetOwner());
    EnemyRole = Enemy && bEnemyVisual && !bBossVisual ? static_cast<int32>(Enemy->GetEnemyRole()) : 0;
    if (!BodyRoot || !WeaponRoot) return;
    ClearParts();
    BodyRoot->SetRelativeScale3D(FVector(bBossVisual ? 1.35f : 1.0f));
    WeaponRoot->SetRelativeScale3D(FVector(bBossVisual ? 1.35f : 1.0f));
    const FLinearColor Accent = bEnemyVisual ? (EnemyRole == 2 ? FLinearColor(.08f, .48f, .88f)
        : (EnemyRole == 1 ? FLinearColor(.85f, .32f, .07f) : FLinearColor(0.85f, 0.22f, 0.035f)))
        : (HeroType == 0 ? FLinearColor(0.6f, 0.025f, 0.055f)
        : (HeroType == 1 ? FLinearColor(0.025f, 0.5f, 0.42f) : FLinearColor(0.75f, 0.52f, 0.13f)));
    ClothMaterial = MakeMaterial(bEnemyVisual ? Accent * .45f : Accent * 0.7f, 0.02f, 0.78f);
    MetalMaterial = MakeMaterial(FLinearColor(.23f, .3f, .38f), .25f, .55f);
    AccentMaterial = MakeMaterial(Accent, bEnemyVisual ? 0.65f : 0.35f, 0.42f);
    FaceMaterial = MakeMaterial(bEnemyVisual ? FLinearColor(0.74f, 0.67f, 0.49f) : FLinearColor(0.7f, 0.55f, 0.37f), 0.08f, 0.7f);
    GlowMaterial = MakeMaterial(bEnemyVisual ? FLinearColor(1.0f, 0.19f, 0.02f) : Accent * 1.6f, 0.2f, 0.3f, bBossVisual ? 4.0f : 2.2f);
    // Keep skin and energy clean; only garments receive restrained woven detail.
    for (UMaterialInstanceDynamic* Garment : {ClothMaterial.Get(), AccentMaterial.Get()})
    {
        if (!Garment) continue;
        Garment->SetScalarParameterValue(TEXT("SurfaceStyle"), 3.f);
        Garment->SetScalarParameterValue(TEXT("TextureStrength"), .18f);
    }
    BuildSilhouette();
    BuildWeapon();
    if (Enemy && !bBossVisual) BuildEnemyTell();
    if (!bEnemyVisual) BuildAnimeDetails();
    SkillStartedAt = -1000.0;
    AttackStartedAt = -1000.0;
    HitUntil = PoiseUntil = -1000.0;
    bAttackCueSpawned = true;
    SetDefeated(bDefeatedVisual);
    UpdateTickBudget();
}

void UAlphaCharacterVisualComponent::BuildSilhouette()
{
    // Ten-part humanoid core; details are batched separately rather than one component per hair strand.
    const bool bArmored = bBossVisual || (bEnemyVisual ? EnemyRole == 1 : HeroType == 0);
    AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, 0, 12), FVector(bArmored ? 42 : 33, 38, 53), ClothMaterial);
    AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, 0, -22), FVector(29, 32, 18), MetalMaterial);
    AddPart(EAlphaPrimitive::Sphere, BodyRoot, FVector(0, 0, 63), FVector(29, 28, 32), bEnemyVisual ? MetalMaterial : FaceMaterial);
    AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(13, 0, 64), FVector(6, 23, 23), FaceMaterial);

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

    if (bBossVisual)
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
    else if (bEnemyVisual && EnemyRole == 1)
    {
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, -36, 36), FVector(38, 28, 29), MetalMaterial, FRotator(0, 0, -12));
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, 36, 36), FVector(38, 28, 29), MetalMaterial, FRotator(0, 0, 12));
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(23, 0, 15), FVector(9, 35, 40), AccentMaterial);
        ShieldMaterial = MakeMaterial(FLinearColor(.8f, .36f, .09f), .15f, .6f);
        ShieldFace = AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(34, -20, 0), FVector(13, 65, 85), ShieldMaterial);
        AddPart(EAlphaPrimitive::Cube, ShieldFace, FVector(55, 0, 0), FVector(8, 12, 74), MetalMaterial);
    }
    else if (bEnemyVisual && EnemyRole == 2)
    {
        AddPart(EAlphaPrimitive::Cylinder, BodyRoot, FVector(-30, 0, 15), FVector(30, 30, 60), AccentMaterial);
        AddPart(EAlphaPrimitive::Sphere, BodyRoot, FVector(-31, -17, 14), FVector(21, 8, 28), GlowMaterial);
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, -30, 34), FVector(23, 19, 16), MetalMaterial);
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, 30, 34), FVector(23, 19, 16), MetalMaterial);
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(18, 0, 66), FVector(4, 24, 7), GlowMaterial);
    }
    else if (bEnemyVisual)
    {
        AddPart(EAlphaPrimitive::Cube, LeftArm, FVector(0, 0, -20), FVector(17, 19, 12), AccentMaterial);
        AddPart(EAlphaPrimitive::Cube, RightArm, FVector(0, 0, -20), FVector(17, 19, 12), AccentMaterial);
    }
    else if (HeroType == 0)
    {
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, -34, 34), FVector(34, 25, 23), MetalMaterial, FRotator(0, 0, -12));
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, 32, 34), FVector(26, 20, 20), AccentMaterial);
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(22, 0, 14), FVector(7, 31, 37), MetalMaterial);
        ClothAccent = AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(-17, -15, -27), FVector(8, 22, 59), AccentMaterial, FRotator(-12, 0, -8));
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(-17, 15, -27), FVector(8, 22, 59), AccentMaterial, FRotator(-12, 0, 8));
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(6, 0, 75), FVector(22, 31, 5), AccentMaterial);
    }
    else if (HeroType == 1)
    {
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(0, 32, 32), FVector(25, 20, 25), MetalMaterial);
        ClothAccent = AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(-22, -3, 37), FVector(39, 10, 7), AccentMaterial, FRotator(-12, 0, -8));
        AddPart(EAlphaPrimitive::Cube, BodyRoot, FVector(-23, 0, 15), FVector(20, 28, 33), MetalMaterial);
        AddPart(EAlphaPrimitive::Cylinder, BodyRoot, FVector(-35, -14, 17), FVector(20, 20, 8), AccentMaterial, FRotator(0, 0, 90));
        AddPart(EAlphaPrimitive::Sphere, BodyRoot, FVector(-35, -20, 17), FVector(11, 5, 11), GlowMaterial);
        AddPart(EAlphaPrimitive::Sphere, BodyRoot, FVector(-16, 0, 78), FVector(14, 14, 22), MetalMaterial, FRotator(-25, 0, 0));
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

void UAlphaCharacterVisualComponent::BuildAnimeDetails()
{
    // One instanced batch for the hair silhouette, brows and belt.
    // Retains the low component budget while adding large readable anime shapes.
    UInstancedStaticMeshComponent* Details = NewObject<UInstancedStaticMeshComponent>(GetOwner());
    GetOwner()->AddInstanceComponent(Details);
    Details->SetupAttachment(BodyRoot);
    Details->SetMobility(EComponentMobility::Movable);
    Details->SetStaticMesh(AlphaArt::Mesh(EAlphaPrimitive::Cube));
    Details->SetMaterial(0, MetalMaterial);
    Details->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Details->SetGenerateOverlapEvents(false);
    Details->SetCanEverAffectNavigation(false);
    Details->SetCastShadow(false);
    Details->SetReceivesDecals(false);
    Details->RegisterComponent();
    Parts.Add(Details);
    Details->AddInstance(FTransform(FRotator::ZeroRotator, FVector(-3, 0, 76), FVector(.3f, .3f, .13f)));
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const float Across = (Index - 2) * 6.f;
        Details->AddInstance(FTransform(FRotator(-25.f - Index * 5.f, 0, Across * 1.5f),
            FVector(10, Across, 78 + FMath::Abs(Index - 2) * 2), FVector(.12f, .07f, .26f)));
    }
    // Side locks and graphic eyebrow lines remain visible without emissive materials.
    for (float Side : {-1.f, 1.f})
    {
        Details->AddInstance(FTransform(FRotator(-15, 0, Side * 10), FVector(-4, Side * 14, 65), FVector(.14f, .07f, HeroType == 2 ? .35f : .22f)));
        Details->AddInstance(FTransform(FRotator(0, 0, Side * -12), FVector(17, Side * 6, 68), FVector(.025f, .085f, .025f)));
    }
    if (HeroType == 1)
        Details->AddInstance(FTransform(FRotator(-55, 0, 0), FVector(-22, 0, 72), FVector(.13f, .14f, .45f)));
    Details->AddInstance(FTransform(FRotator::ZeroRotator, FVector(1, 0, -16), FVector(.34f, .35f, .05f)));
}

void UAlphaCharacterVisualComponent::PlaySkill(int32 Slot)
{
    if (bDefeatedVisual) return;
    CancelAttackPresentation();
    SkillSlot = FMath::Clamp(Slot, 0, 2);
    SkillStartedAt = VisualTime();
    if (SkillSlot == 2) AAlphaCombatCamera::KickAt(GetWorld(), GetOwner()->GetActorLocation(), .65f);
    UpdateTickBudget();
}

void UAlphaCharacterVisualComponent::ShowGroundImpact(FVector Location, FVector Normal, float Radius)
{
    if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) return;
    const AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetOwner());
    const FVector Facing = Character ? Character->GetCombatComponent()->GetAttackFacing() : GetOwner()->GetActorForwardVector();
    AAlphaCombatEffect::SpawnGroundImpact(GetWorld(), Location, Normal, Facing, Radius,
        BaseTints.IsValidIndex(2) ? BaseTints[2] : FLinearColor::White);
    AlphaAudio::Play(GetWorld(), Location, EAlphaSoundCue::Poise, .65f);
    AAlphaCombatCamera::KickAt(GetWorld(), Location, .8f);
}

void UAlphaCharacterVisualComponent::PausePoseForImpact(float Seconds)
{
    if (!GetWorld() || !FMath::IsFinite(Seconds) || bDefeatedVisual) return;
    PosePausedUntil = FMath::Max(PosePausedUntil, VisualTime() + FMath::Clamp(Seconds, 0.f, .1f));
    UpdateTickBudget();
}

void UAlphaCharacterVisualComponent::BuildEnemyTell()
{
    if (EnemyRole != 2 || !GetOwner() || GetWorld()->GetNetMode() == NM_DedicatedServer) return;
    EnemyTell = NewObject<UInstancedStaticMeshComponent>(GetOwner());
    GetOwner()->AddInstanceComponent(EnemyTell);
    EnemyTell->SetupAttachment(GetOwner()->GetRootComponent());
    EnemyTell->SetMobility(EComponentMobility::Movable);
    EnemyTell->SetStaticMesh(AlphaArt::Mesh(EAlphaPrimitive::Cube));
    EnemyTellMaterial = AlphaArt::EffectMaterial(this, FLinearColor(1.f, .3f, .035f), .35f);
    EnemyTell->SetMaterial(0, EnemyTellMaterial);
    EnemyTell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EnemyTell->SetGenerateOverlapEvents(false);
    EnemyTell->SetCanEverAffectNavigation(false);
    EnemyTell->SetCastShadow(false);
    EnemyTell->SetReceivesDecals(false);
    EnemyTell->RegisterComponent();
    for (int32 Index = 0; Index < 8; ++Index)
        EnemyTell->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector));
    EnemyTell->SetVisibility(false);
    Parts.Add(EnemyTell);
}

void UAlphaCharacterVisualComponent::UpdateEnemyPresentation()
{
    const AAlphaEnemy* Enemy = Cast<AAlphaEnemy>(GetOwner());
    if (!Enemy || bBossVisual) return;
    bShieldGuarding = EnemyRole == 1 && Enemy->IsGuardActive();
    const int32 ShotRevision = Enemy->GetShotRevision();
    if (bObservedEnemyShotRevision && ShotRevision != LastEnemyShotRevision && EnemyRole == 2)
    {
        EnemyShotVisibleUntil = VisualTime() + .10;
        const FVector Muzzle = WeaponRoot ? WeaponRoot->GetComponentLocation() + WeaponRoot->GetForwardVector()*125.
            : GetOwner()->GetActorLocation();
        AAlphaCombatEffect::Spawn(GetWorld(), Muzzle, Enemy->GetTellDirection(), EAlphaEffectType::Hit, FLinearColor(1.f,.55f,.08f));
        AlphaAudio::Play(GetWorld(), Muzzle, EAlphaSoundCue::Skill, .55f);
    }
    LastEnemyShotRevision = ShotRevision;
    bObservedEnemyShotRevision = true;
    const bool bWindup = EnemyRole == 2 && Enemy->GetTellState() == EAlphaEnemyTellState::Windup;
    bEnemyWindingUp = bWindup;
    EnemyTellProgress = bWindup ? FMath::Clamp(Enemy->GetTellProgress(), 0.f, 1.f) : 0.f;
    if (!EnemyTell) return;
    const bool bShot = VisualTime() < EnemyShotVisibleUntil;
    EnemyTell->SetVisibility((bWindup || bShot) && !bDefeatedVisual);
    if (!bWindup && !bShot) return;
    FVector Start = Enemy->GetTellOrigin();
    FVector Forward = Enemy->GetTellDirection().GetSafeNormal2D();
    if (Forward.IsNearlyZero()) Forward = FVector::ForwardVector;
    const FVector Side(-Forward.Y, Forward.X, 0);
    float Length = 760.f;
    constexpr float HalfWidth = 48.f;
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    const int32 Chapter = State ? State->ChapterIndex : 0;
    const FVector ChapterOrigin = AAlphaGameState::GetChapterOrigin(Chapter);
    // Damage rejects paths across missing floor; stop the visible lane there too.
    for (float Distance = 16.f; Distance <= 760.f; Distance += 16.f)
    {
        const FVector Sample = Start + Forward*Distance - ChapterOrigin;
        if (!AlphaLayoutRules::IsWalkable(Chapter,{Sample.X,Sample.Y})) { Length=FMath::Max(1.f,Distance-16.f); break; }
        Start.Z = FMath::Max(Start.Z, ChapterOrigin.Z + AlphaLayoutRules::SurfaceHeight(Chapter,{Sample.X,Sample.Y}) + 8.);
    }
    const float Progress = bShot ? 1.f : EnemyTellProgress;
    const auto Beam = [this](int32 Index, const FVector& A, const FVector& B, float Width)
    {
        const FVector Delta = B - A;
        EnemyTell->UpdateInstanceTransform(Index,
            FTransform(Delta.Rotation(), (A+B)*.5, FVector(Delta.Size()/100., Width/100., .025)),
            true, Index == 7, true);
    };
    Beam(0, Start-Side*HalfWidth, Start+Forward*Length-Side*HalfWidth, 4.f);
    Beam(1, Start+Side*HalfWidth, Start+Forward*Length+Side*HalfWidth, 4.f);
    Beam(2, Start-Side*HalfWidth, Start+Side*HalfWidth, 4.f);
    Beam(3, Start+Forward*Length-Side*HalfWidth, Start+Forward*Length+Side*HalfWidth, 4.f);
    Beam(4, Start, Start+Forward*FMath::Max(1.f,Length*Progress), bShot ? 8.f : 3.f);
    const FVector Tip=Start+Forward*(Length*.7f);
    Beam(5, Tip-Forward*30-Side*18, Tip, 4.f);
    Beam(6, Tip-Forward*30+Side*18, Tip, 4.f);
    Beam(7, Start-Side*12, Start+Side*12, 7.f);
    if (EnemyTellMaterial)
    {
        const FLinearColor Color=bShot ? FLinearColor(1.f,.85f,.45f)
            : FMath::Lerp(FLinearColor(1.f,.28f,.035f),FLinearColor(1.f,.72f,.12f),EnemyTellProgress);
        EnemyTellMaterial->SetVectorParameterValue(TEXT("Tint"),Color);
        EnemyTellMaterial->SetVectorParameterValue(TEXT("Color"),Color);
        EnemyTellMaterial->SetScalarParameterValue(TEXT("Glow"),bShot ? 1.f : .25f+EnemyTellProgress*.4f);
    }
}

void UAlphaCharacterVisualComponent::BuildWeapon()
{
    if (bBossVisual)
    {
        AddPart(EAlphaPrimitive::Cylinder, WeaponRoot, FVector(45, 0, 0), FVector(9, 9, 115), MetalMaterial, FRotator(90, 0, 0));
        AddPart(EAlphaPrimitive::Cube, WeaponRoot, FVector(95, 0, 0), FVector(40, 66, 54), AccentMaterial);
    }
    else if (bEnemyVisual && EnemyRole == 2)
    {
        AddPart(EAlphaPrimitive::Cylinder, WeaponRoot, FVector(48, 0, 0), FVector(9, 9, 125), MetalMaterial, FRotator(90, 0, 0));
        AddPart(EAlphaPrimitive::Cube, WeaponRoot, FVector(112, 0, 0), FVector(23, 19, 19), AccentMaterial);
        AddPart(EAlphaPrimitive::Sphere, WeaponRoot, FVector(125, 0, 0), FVector(9, 13, 13), GlowMaterial);
        AddPart(EAlphaPrimitive::Cube, WeaponRoot, FVector(-8, 0, -8), FVector(27, 15, 22), ClothMaterial);
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
    RollStartedAt = -1000.0;
    SkillStartedAt = -1000.0;
    bDefeatedVisual = bNewDefeated;
    AttackStartedAt = -1000.0;
    HitUntil = PoiseUntil = -1000.0;
    bAttackCueSpawned = true;
    bBossWindup = false;
    PosePausedUntil = -1000.;
    if (EnemyTell) EnemyTell->SetVisibility(false);
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
    ComboStage = 0;
    bHeavyAttack = bHeavy;
    AttackWindup = bEnemyVisual ? (bHeavy ? 0.32f : 0.12f)
        : (HeroType == 0 ? (bHeavy ? 0.36f : 0.15f)
        : (HeroType == 1 ? (bHeavy ? 0.23f : 0.08f) : (bHeavy ? 0.30f : 0.12f)));
    AttackDuration = AttackWindup + (bHeavy ? 0.24f : 0.16f);
    if (bEnemyVisual && !bBossVisual)
    {
        AttackWindup = .30f;
        AttackDuration = .85f;
    }
    AttackStartedAt = VisualTime();
    bAttackCueSpawned = false;
    UpdateTickBudget();
}

void UAlphaCharacterVisualComponent::PlayCombo(int32 Stage, bool bAirborne)
{
    if (bDefeatedVisual || !WeaponRoot) return;
    if (AttackCue.IsValid()) AttackCue->Destroy();
    ComboStage = FMath::Clamp(Stage, 1, 5);
    bComboAirborne = bAirborne;
    const AlphaPlayerControl::ComboAttackData Data = AlphaPlayerControl::GetComboAttack(ComboStage, bAirborne);
    AttackWindup = static_cast<float>(Data.Windup);
    AttackDuration = static_cast<float>(Data.Duration);
    AttackStartedAt = VisualTime();
    AttackFacing = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
    bAttackCueSpawned = false;
    UpdateTickBudget();
}

void UAlphaCharacterVisualComponent::PlayRoll()
{
    if (bDefeatedVisual || !BodyRoot) return;
    CancelAttackPresentation();
    RollStartedAt = VisualTime();
    UpdateTickBudget();
}

void UAlphaCharacterVisualComponent::UpdateComboPose(float AttackTime)
{
    const bool bPreparing = AttackTime < AttackWindup;
    const float Phase = FMath::Clamp(bPreparing ? AttackTime / AttackWindup
        : (AttackTime - AttackWindup) / (AttackDuration - AttackWindup), 0.f, 1.f);
    const float Ease = FMath::InterpEaseOut(0.f, 1.f, Phase, 2.f);
    FRotator Pose = FRotator::ZeroRotator;
    FVector Offset = FVector::ZeroVector;
    switch (ComboStage)
    {
    case 1: Pose.Yaw = bPreparing ? -70.f * Phase : FMath::Lerp(-70.f, 75.f, Ease); break;
    case 2: Pose = FRotator(-10.f, bPreparing ? 75.f * Phase : FMath::Lerp(75.f, -80.f, Ease), 0); break;
    case 3: Pose.Pitch = bPreparing ? 55.f * Phase : FMath::Lerp(55.f, -90.f, Ease); break;
    case 4:
        Offset.X = bPreparing ? -25.f * Phase : FMath::Sin(Phase * PI) * 65.f;
        Pose.Pitch = -5.f;
        break;
    case 5:
        Pose.Pitch = bPreparing ? -130.f * Phase : FMath::Lerp(-130.f, 80.f, Ease);
        Offset.Z = bPreparing ? 45.f * Phase : FMath::Lerp(45.f, -30.f, Ease);
        break;
    default: break;
    }
    if (bComboAirborne)
    {
        // Downward-facing aerial variants preserve the server's downward hit volume.
        switch (ComboStage)
        {
        case 1: Pose = FRotator(bPreparing ? -65.f * Phase : FMath::Lerp(-65.f, 50.f, Ease), -30.f, -15.f); break;
        case 2: Pose = FRotator(30.f, bPreparing ? 100.f * Phase : FMath::Lerp(100.f, -110.f, Ease), 25.f); break;
        case 3: Pose = FRotator(bPreparing ? 80.f * Phase : FMath::Lerp(80.f, -100.f, Ease), 20.f, -35.f); break;
        case 4: Pose = FRotator(48.f, -10.f, 0); Offset = FVector(bPreparing ? -20.f : 55.f * FMath::Sin(Phase * PI), 0, -35.f); break;
        case 5: Pose = FRotator(bPreparing ? -155.f * Phase : FMath::Lerp(-155.f, 85.f, Ease), 0, 0); Offset.Z = bPreparing ? 65.f * Phase : FMath::Lerp(65.f, -65.f, Ease); break;
        default: break;
        }
    }
    WeaponRoot->SetRelativeRotation(Pose);
    WeaponRoot->SetRelativeLocation(Offset);
    const float Strike = bPreparing ? -Phase : FMath::Sin(Phase * PI);
    BodyRoot->SetRelativeRotation(FRotator((bComboAirborne ? 12.f : -7.f) * Strike, Pose.Yaw * .15f, bComboAirborne ? Pose.Roll * .35f : 0.f));
    if (LeftArm) LeftArm->SetRelativeRotation(FRotator(-25.f - 35.f * Strike, 0, -20.f));
    if (RightArm) RightArm->SetRelativeRotation(FRotator(Pose.Pitch * .65f, Pose.Yaw * .4f, 0));
    if (bComboAirborne)
    {
        const float Tuck = ComboStage == 5 ? 60.f : (ComboStage == 3 ? 48.f : 35.f);
        if (LeftLeg) LeftLeg->SetRelativeRotation(FRotator(Tuck, 0, -10.f));
        if (RightLeg) RightLeg->SetRelativeRotation(FRotator(ComboStage == 4 ? -45.f : -15.f, 0, 10.f));
    }
    if (!bPreparing && !bAttackCueSpawned)
    {
        AttackCue = AAlphaCombatEffect::SpawnCombo(GetWorld(), GetOwner()->GetActorLocation(), AttackFacing,
            ComboStage, bComboAirborne, BaseTints.IsValidIndex(2) ? BaseTints[2] : FLinearColor::White);
        bAttackCueSpawned = true;
    }
}

void UAlphaCharacterVisualComponent::PlayHit(float Damage)
{
    if (Damage <= 0.0f || !BodyRoot) return;
    CancelAttackPresentation();
    HitUntil = VisualTime() + 0.16;
    ApplyMaterialState(1.0f, 0.0f);
    PausePoseForImpact(.04f);
    UpdateTickBudget();
}

void UAlphaCharacterVisualComponent::PlayPoiseBreak()
{
    if (!BodyRoot) return;
    CancelAttackPresentation();
    PoiseUntil = VisualTime() + 0.4;
    ApplyMaterialState(0.0f, 1.0f);
    PausePoseForImpact(.07f);
    UpdateTickBudget();
}

void UAlphaCharacterVisualComponent::CancelAttackPresentation()
{
    if (AttackCue.IsValid()) AttackCue->Destroy();
    AttackCue.Reset();
    ComboStage = 0;
    bComboAirborne = false;
    SkillStartedAt = -1000.0;
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
    const bool bNear = DistanceSquared < FMath::Square(8500.0f);
    SetComponentTickEnabled(bNear && !bDefeatedVisual);
    // Route framing can pull the camera to 5200cm plus height/side offsets.
    // Keep visible actors and danger tells updating across that envelope.
    SetComponentTickInterval(DistanceSquared > FMath::Square(7000.0f) ? 0.066f : 0.0f);
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
    // Danger tells remain current even while a cosmetic limb pose is held.
    UpdateEnemyPresentation();
    if (Now < PosePausedUntil) return;
    const AAlphaCharacter* Hero = Cast<AAlphaCharacter>(GetOwner());
    const bool bRunning = Hero && Hero->IsSprinting() && Hero->GetCharacterMovement()->IsMovingOnGround();
    const float Speed = FMath::Clamp(GetOwner()->GetVelocity().Size2D() / 500.0f, 0.0f, 1.0f);
    GaitPhase += DeltaTime * Speed * 11.0f * (bRunning ? 1.8f : 1.f);
    if (bRunning && Speed > .1f && Now - LastSprintCueAt >= .1)
    {
        const FVector Feet = GetOwner()->GetActorLocation() - FVector(0, 0, 82);
        AAlphaCombatEffect::Spawn(GetWorld(), Feet, GetOwner()->GetActorForwardVector(),
            EAlphaEffectType::Sprint, FLinearColor(.25f, .7f, .8f));
        LastSprintCueAt = Now;
    }
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
    BodyRoot->SetRelativeRotation(FRotator(bRunning ? -13.f : 0.f, 0, 0));
    const float RollAge = static_cast<float>(Now - RollStartedAt);
    if (RollAge >= 0.f && RollAge < .35f)
    {
        const float Progress = RollAge / .35f;
        BodyRoot->SetRelativeRotation(FRotator(-360.f * Progress, 0, 0));
        BodyRoot->SetRelativeLocation(FVector(0, 0, -15.f * FMath::Sin(Progress * PI)));
        if (LeftLeg) LeftLeg->SetRelativeRotation(FRotator(55, 0, 0));
        if (RightLeg) RightLeg->SetRelativeRotation(FRotator(55, 0, 0));
    }
    if (ClothAccent)
    {
        const float Flutter = FMath::Sin(GaitPhase - 0.8f) * Speed * 8.0f;
        const FRotator Rotation = HeroType == 2 ? FRotator(Flutter * 0.2f, 0, 0) : FRotator(-12.0f + Flutter, 0, -8);
        ClothAccent->SetRelativeRotation(Rotation);
    }
    if (LanternRing) LanternRing->AddLocalRotation(FRotator(DeltaTime * 16.0f, 0, 0));

    const float AttackTime = static_cast<float>(Now - AttackStartedAt);
    const float SkillAge = static_cast<float>(Now - SkillStartedAt);
    const float SkillDuration = SkillSlot == 2 ? .7f : .35f;
    if (SkillAge >= 0.f && SkillAge < SkillDuration && RollAge >= .35f)
    {
        const float Pulse = FMath::Sin(SkillAge / SkillDuration * PI);
        const float Pitch = HeroType == 2 ? -85.f : (HeroType == 1 ? 35.f : -40.f);
        WeaponRoot->SetRelativeRotation(FRotator(Pitch * Pulse, (HeroType == 1 ? -75.f : 20.f) * Pulse, 0));
        WeaponRoot->SetRelativeLocation(FVector(25.f * Pulse, 0, (SkillSlot == 2 ? 50.f : 15.f) * Pulse));
        BodyRoot->SetRelativeRotation(FRotator(-12.f * Pulse, 0, 0));
        if (LeftArm) LeftArm->SetRelativeRotation(FRotator(-100.f * Pulse, 0, -25.f * Pulse));
        if (RightArm) RightArm->SetRelativeRotation(FRotator(Pitch * Pulse, 0, 20.f * Pulse));
    }
    else if (bBossWindup)
    {
        const float Raise = FMath::InterpEaseInOut(0.0f, 1.0f, BossWindupProgress, 2.0f);
        WeaponRoot->SetRelativeRotation(FRotator(-95.0f * Raise, BossAttackType == 1 ? -50.0f * Raise : 0.0f, 0));
        WeaponRoot->SetRelativeLocation(FVector(0, 0, Raise * 35.0f));
    }
    else if (bEnemyWindingUp)
    {
        WeaponRoot->SetRelativeRotation(FRotator(-8.f * EnemyTellProgress, 0, 0));
        WeaponRoot->SetRelativeLocation(FVector(-12.f * EnemyTellProgress, 0, 10));
        if (RightArm) RightArm->SetRelativeRotation(FRotator(-65, 0, 0));
    }
    else if (ComboStage > 0 && AttackTime >= 0.f && AttackTime < AttackDuration)
    {
        UpdateComboPose(AttackTime);
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
                if (bEnemyVisual)
                    AAlphaCombatEffect::SpawnCombo(GetWorld(), GetOwner()->GetActorLocation(), GetOwner()->GetActorForwardVector(),
                        1, false, FLinearColor(1.f, .25f, .04f));
                else
                    AAlphaCombatEffect::Spawn(GetWorld(), GetOwner()->GetActorLocation(), GetOwner()->GetActorForwardVector(),
                        bHeavyAttack ? EAlphaEffectType::HeavySlash : EAlphaEffectType::Slash, BaseTints[2]);
            }
            bAttackCueSpawned = true;
        }
    }
    else
    {
        WeaponRoot->SetRelativeRotation(FRotator(0, 0, Wave * Speed * 3.0f));
        WeaponRoot->SetRelativeLocation(FVector::ZeroVector);
        if (bShieldGuarding && LeftArm) LeftArm->SetRelativeRotation(FRotator(-35, 0, -15));
    }
    ApplyMaterialState(HitFlash, PoiseFlash);
    if (ShieldFace)
    {
        ShieldFace->SetRelativeLocation(bShieldGuarding ? FVector(34,-20,0) : FVector(5,-38,-12));
        ShieldFace->SetRelativeRotation(bShieldGuarding ? FRotator::ZeroRotator : FRotator(0,65,0));
        if (ShieldMaterial)
        {
            const FLinearColor GuardTint = FMath::Lerp(bShieldGuarding ? FLinearColor(.95f,.5f,.12f)
                : FLinearColor(.25f,.18f,.12f), FLinearColor::White, HitFlash * .8f);
            ShieldMaterial->SetVectorParameterValue(TEXT("Tint"),GuardTint);
            ShieldMaterial->SetVectorParameterValue(TEXT("Color"),GuardTint);
            ShieldMaterial->SetScalarParameterValue(TEXT("Glow"),bShieldGuarding ? .18f : 0.f);
        }
    }
}
