#include "Visuals/AlphaCombatEffect.h"

#include "Visuals/AlphaArtLibrary.h"
#include "Rules/PresentationRules.h"
#include "Rules/PlayerControlRules.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
TArray<TWeakObjectPtr<AAlphaCombatEffect>> ActiveEffects;

int32 EffectPriority(EAlphaEffectType Type)
{
    if (Type == EAlphaEffectType::Sprint) return 0;
    if (Type == EAlphaEffectType::Revive) return 4;
    if (Type == EAlphaEffectType::Hit || Type == EAlphaEffectType::Poise) return 3;
    if (Type == EAlphaEffectType::Coop || Type == EAlphaEffectType::Ultimate) return 2;
    return 1;
}
}

AAlphaCombatEffect::AAlphaCombatEffect()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = false;
    SetCanBeDamaged(false);
    SetActorEnableCollision(false);
    Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("EffectInstances"));
    SetRootComponent(Instances);
    Instances->SetMobility(EComponentMobility::Movable);
    Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Instances->SetGenerateOverlapEvents(false);
    Instances->SetCanEverAffectNavigation(false);
    Instances->SetCastShadow(false);
    Instances->SetReceivesDecals(false);
}

AAlphaCombatEffect* AAlphaCombatEffect::Spawn(UWorld* World, const FVector& Location,
    const FVector& Forward, EAlphaEffectType Type, FLinearColor Color, bool bEmpowered)
{
    return SpawnInternal(World, Location, Forward, Type, Color, bEmpowered, 0, false);
}

AAlphaCombatEffect* AAlphaCombatEffect::SpawnCombo(UWorld* World, const FVector& Location,
    const FVector& Forward, int32 Stage, bool bAirborne, FLinearColor Color)
{
    if (Stage < 1 || Stage > 5) return nullptr;
    return SpawnInternal(World, Location, Forward, EAlphaEffectType::Combo, Color, false, Stage, bAirborne);
}

AAlphaCombatEffect* AAlphaCombatEffect::SpawnInternal(UWorld* World, const FVector& Location,
    const FVector& Forward, EAlphaEffectType Type, FLinearColor Color, bool bEmpowered, int32 Stage, bool bAirborne)
{
    if (!World || World->GetNetMode() == NM_DedicatedServer || Location.ContainsNaN() || Forward.ContainsNaN()) return nullptr;
    if (const APlayerController* Player = World->GetFirstPlayerController())
    {
        if (Player->PlayerCameraManager && FVector::DistSquared(Location,
            Player->PlayerCameraManager->GetCameraLocation()) > FMath::Square(10000.0f)) return nullptr;
    }
    ActiveEffects.RemoveAll([](const TWeakObjectPtr<AAlphaCombatEffect>& Effect)
    {
        return !Effect.IsValid() || Effect->IsActorBeingDestroyed();
    });
    int32 InWorld = 0;
    AAlphaCombatEffect* Candidate = nullptr;
    for (const TWeakObjectPtr<AAlphaCombatEffect>& Entry : ActiveEffects)
    {
        AAlphaCombatEffect* Effect = Entry.Get();
        if (!Effect || Effect->GetWorld() != World) continue;
        ++InWorld;
        if (!Candidate || Effect->Priority() < Candidate->Priority()
            || (Effect->Priority() == Candidate->Priority() && Effect->Age > Candidate->Age)) Candidate = Effect;
    }
    if (InWorld >= MaxEffectsPerWorld)
    {
        // Preserve hit, stagger and revival readability when cosmetic attack trails are busy.
        if (!Candidate || EffectPriority(Type) < Candidate->Priority()) return nullptr;
        if (!Candidate->Destroy()) return nullptr;
    }
    FVector Facing = Forward.GetSafeNormal2D();
    if (Facing.IsNearlyZero()) Facing = FVector::ForwardVector;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;
    AAlphaCombatEffect* Effect = World->SpawnActor<AAlphaCombatEffect>(Location, Facing.Rotation(), Params);
    if (!Effect) return nullptr;
    Effect->Configure(Type, Color, bEmpowered, Stage, bAirborne);
    if (Effect->Pieces.IsEmpty())
    {
        Effect->Destroy();
        return nullptr;
    }
    ActiveEffects.Add(Effect);
    return Effect;
}

int32 AAlphaCombatEffect::Priority() const
{
    return EffectPriority(EffectType);
}

void AAlphaCombatEffect::Configure(EAlphaEffectType InType, FLinearColor Color, bool bEmpowered,
    int32 Stage, bool bAirborne)
{
    EffectType = InType;
    Pieces.Reserve(MaxInstances);
    Instances->SetStaticMesh(AlphaArt::Mesh(EAlphaPrimitive::Cube));
    if (!Instances->GetStaticMesh()) return;
    BaseTint = bEmpowered ? FMath::Lerp(Color, FLinearColor(0.85f, 0.95f, 1.0f), 0.4f) : Color;
    // Limited emissive intensity avoids drowning out the boss's red/yellow attack telegraphs.
    PeakGlow = bEmpowered ? 1.8f : 1.1f;
    EffectMaterial = AlphaArt::Material(this, BaseTint, 0.0f, 0.45f, PeakGlow);
    if (EffectMaterial) Instances->SetMaterial(0, EffectMaterial);

    switch (EffectType)
    {
    case EAlphaEffectType::Combo:
        ConfigureCombo(Stage, bAirborne);
        break;
    case EAlphaEffectType::Sprint:
        Duration = .26f;
        // These are foot-level wind/dust streaks, not a defensive or damaging volume.
        for (int32 Index = 0; Index < 6; ++Index)
        {
            const float Side = (Index % 2 ? 1.0f : -1.0f) * (16.0f + Index * 2.0f);
            const float Height = 3.0f + (Index % 3) * 5.0f;
            AddBeam(FVector(-12, Side, Height), FVector(-48 - Index * 7, Side, Height),
                2.0f, FVector(-35, Side * .3f, 5.0f));
        }
        break;
    case EAlphaEffectType::Ultimate:
        Duration = .72f;
        // Shared conservative footprint: Shen's rear radius is 180 and A Ling's
        // radial ultimate is 300. Keep the ring at 150 and every forward tip < 280.
        AddRing(14, 150.0f, -55.0f, 5.0f, .25f, 1.0f);
        for (int32 Index = 0; Index < 8; ++Index)
        {
            const float AngleA = FMath::Lerp(-1.1f, 1.1f, static_cast<float>(Index) / 8.0f);
            const float AngleB = FMath::Lerp(-1.1f, 1.1f, static_cast<float>(Index + 1) / 8.0f);
            AddBeam(FVector(145 + FMath::Cos(AngleA) * 125, -10, FMath::Sin(AngleA) * 65),
                FVector(145 + FMath::Cos(AngleB) * 125, -10, FMath::Sin(AngleB) * 65), 7.0f);
        }
        AddBeam(FVector(75, -25, -20), FVector(95, -25, 40), 4.0f);
        AddBeam(FVector(125, 25, -20), FVector(145, 25, 40), 4.0f);
        break;
    case EAlphaEffectType::Slash:
    case EAlphaEffectType::HeavySlash:
    {
        const bool bHeavy = EffectType == EAlphaEffectType::HeavySlash;
        Duration = bHeavy ? 0.38f : 0.24f;
        // Shared with ordinary heavy attacks: outer tip stays below their
        // minimum 240-unit sweep reach, even though the cleave skill is longer.
        const float Radius = bHeavy ? 155.0f : 95.0f;
        const float Center = bHeavy ? 80.0f : 95.0f;
        const int32 Segments = 16;
        for (int32 Index = 0; Index < Segments; ++Index)
        {
            const float A = FMath::Lerp(-1.1f, 1.1f, static_cast<float>(Index) / Segments);
            const float B = FMath::Lerp(-1.1f, 1.1f, static_cast<float>(Index + 1) / Segments);
            const float HeightScale = bHeavy ? .35f : .6f;
            const FVector Start(Center + FMath::Cos(A) * Radius, -8.0f, FMath::Sin(A) * Radius * HeightScale);
            const FVector End(Center + FMath::Cos(B) * Radius, -8.0f, FMath::Sin(B) * Radius * HeightScale);
            AddBeam(Start, End, bHeavy ? 7.0f : 4.0f, FVector(bHeavy ? 0.0f : 15.0f, 0, -6.0f));
        }
        AddSparks(8, 35.0f, bHeavy ? 110.0f : 65.0f, 0, 31);
        break;
    }
    case EAlphaEffectType::Dash:
        Duration = .32f;
        for (int32 Index = 0; Index < 8; ++Index)
        {
            const float Side = (Index % 2 ? 1.0f : -1.0f) * (18.0f + Index * 3.0f);
            const float Height = -40.0f + Index * 12.0f;
            AddBeam(FVector(-70, Side, Height), FVector(140 - Index * 8, Side, Height),
                3.0f, FVector(45, 0, 0));
        }
        AddSparks(8, 30.0f, 95.0f, 0, 53);
        break;
    case EAlphaEffectType::Launch:
        Duration = .45f;
        AddRing(16, 200.0f, -72.0f, 5.0f, .4f, 1.0f);
        AddSparks(8, 75.0f, 140.0f, 25.0f, 71);
        break;
    case EAlphaEffectType::Pull:
        Duration = .7f;
        AddRing(20, 410.0f, -72.0f, 4.0f, 1.0f, .23f);
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float Angle = Index * HALF_PI + .3f;
            const FVector Outer(FMath::Cos(Angle) * 320.0f, FMath::Sin(Angle) * 320.0f, -60.0f);
            AddBeam(Outer, Outer + FVector(0, 0, 28), 6.0f, -Outer * .75f);
        }
        break;
    case EAlphaEffectType::Pulse:
        Duration = .65f;
        AddRing(20, 300.0f, -72.0f, 5.0f, .12f, 1.0f);
        AddSparks(4, 45.0f, 95.0f, -25.0f, 89);
        break;
    case EAlphaEffectType::Coop:
        Duration = .5f;
        AddRing(16, 125.0f, 0.0f, 5.0f, .15f, 1.0f);
        AddSparks(8, 25.0f, 90.0f, 0.0f, 107);
        break;
    case EAlphaEffectType::Hit:
        Duration = .22f;
        AddSparks(12, 8.0f, 75.0f, 0.0f, 127);
        break;
    case EAlphaEffectType::Poise:
        Duration = .48f;
        AddRing(16, 95.0f, -35.0f, 5.0f, .4f, 1.0f);
        AddSparks(8, 15.0f, 120.0f, 0.0f, 149);
        break;
    case EAlphaEffectType::Revive:
        Duration = .9f;
        AddRing(16, 85.0f, -75.0f, 5.0f, .65f, 1.0f);
        for (int32 Index = 0; Index < 8; ++Index)
        {
            const float Angle = Index * PI / 4.0f;
            const FVector Start(FMath::Cos(Angle) * 60.0f, FMath::Sin(Angle) * 60.0f, -60.0f);
            AddBeam(Start, Start + FVector(0, 0, 16), 5.0f, FVector(0, 0, 150));
        }
        break;
    }
    SetLifeSpan(Duration + .2f);
    UpdatePieces(0.0f);
}

void AAlphaCombatEffect::ConfigureCombo(int32 Stage, bool bAirborne)
{
    const AlphaPlayerControl::ComboAttackData Data = AlphaPlayerControl::GetComboAttack(Stage, bAirborne);
    Duration = static_cast<float>(FMath::Max(.08, Data.Duration - Data.Windup));
    const float Reach = static_cast<float>(Data.Reach);
    const float Width = static_cast<float>(Data.HalfWidth);
    const float Height = static_cast<float>(Data.HalfHeight);
    const float Z = static_cast<float>(Data.VerticalOffset);
    if (Stage == 4)
    {
        // A narrow thrust rather than a wide slash; all pieces stay inside the
        // shared forward box and remain still in local space after release.
        AddBeam(FVector(12, -8, Z), FVector(Reach - 10, -8, Z), 5.0f);
        AddBeam(FVector(28, 8, Z - 10), FVector(Reach - 30, 8, Z - 10), 2.5f);
        AddBeam(FVector(Reach - 45, -18, Z - 12), FVector(Reach - 12, -8, Z), 3.0f);
        AddBeam(FVector(Reach - 45, 2, Z + 12), FVector(Reach - 12, -8, Z), 3.0f);
        return;
    }
    if (Stage == 5)
    {
        // Ground/air finishers both strike a finite box. This local down-pulse
        // follows that box instead of projecting a full circle around the pawn.
        const float Radius = FMath::Min(Width * .7f, Reach * .3f);
        const float Center = Reach * .5f;
        const float Bottom = Z - Height * .7f;
        for (int32 Index = 0; Index < 16; ++Index)
        {
            const float A = Index * 2.0f * PI / 16.0f;
            const float B = (Index + 1) * 2.0f * PI / 16.0f;
            AddBeam(FVector(Center + FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, Bottom),
                FVector(Center + FMath::Cos(B) * Radius, FMath::Sin(B) * Radius, Bottom), 4.0f);
        }
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float X = Center - 24.0f + Index * 16.0f;
            AddBeam(FVector(X, -8, Z + Height * .65f), FVector(X + 8, -8, Bottom + 8), 4.0f);
        }
        return;
    }
    const int32 Segments = 18;
    for (int32 Index = 0; Index < Segments; ++Index)
    {
        const float Sign = Stage == 2 ? -1.0f : 1.0f;
        const float A = Sign * FMath::Lerp(-1.15f, 1.15f, static_cast<float>(Index) / Segments);
        const float B = Sign * FMath::Lerp(-1.15f, 1.15f, static_cast<float>(Index + 1) / Segments);
        FVector Start, End;
        if (Stage == 3)
        {
            Start = FVector(Reach * .4f + FMath::Cos(A) * Reach * .35f, -8, Z + FMath::Sin(A) * Height * .72f);
            End = FVector(Reach * .4f + FMath::Cos(B) * Reach * .35f, -8, Z + FMath::Sin(B) * Height * .72f);
        }
        else
        {
            Start = FVector(Reach * .45f + FMath::Cos(A) * Reach * .43f, FMath::Sin(A) * Width * .8f, Z);
            End = FVector(Reach * .45f + FMath::Cos(B) * Reach * .43f, FMath::Sin(B) * Width * .8f, Z);
        }
        AddBeam(Start, End, Stage == 3 ? 4.0f : 3.5f);
        // A moving trail reads the reverse cut without changing hit detection.
        if (!Pieces.IsEmpty()) Pieces.Last().DelayFraction = static_cast<float>(Index) / Segments * .3f;
    }
}

void AAlphaCombatEffect::AddBeam(const FVector& Start, const FVector& End, float Thickness,
    const FVector& Drift, float RadialStart, float RadialEnd)
{
    if (Pieces.Num() >= MaxInstances) return;
    FPiece Piece;
    Piece.Start = (Start + End) * .5f;
    Piece.Drift = Drift;
    const float Length = static_cast<float>(FVector::Distance(Start, End));
    Piece.Scale = FVector(FMath::Max(1.0f, Length) / 100.0f, Thickness / 100.0f, Thickness / 100.0f);
    Piece.Rotation = (End - Start).GetSafeNormal().Rotation().Quaternion();
    Piece.RadialStart = RadialStart;
    Piece.RadialEnd = RadialEnd;
    Pieces.Add(Piece);
    Instances->AddInstance(FTransform(Piece.Rotation, Piece.Start, Piece.Scale));
}

void AAlphaCombatEffect::AddRing(int32 Segments, float Radius, float Height, float Thickness,
    float StartScale, float EndScale)
{
    for (int32 Index = 0; Index < Segments; ++Index)
    {
        const float A = Index * 2.0f * PI / Segments;
        const float B = (Index + 1) * 2.0f * PI / Segments;
        AddBeam(FVector(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, Height),
            FVector(FMath::Cos(B) * Radius, FMath::Sin(B) * Radius, Height), Thickness,
            FVector::ZeroVector, StartScale, EndScale);
    }
}

void AAlphaCombatEffect::AddSparks(int32 Count, float Radius, float Travel, float Height, int32 Seed)
{
    FRandomStream Random(Seed);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const float Angle = Random.FRandRange(-PI, PI);
        const FVector Direction(FMath::Cos(Angle), Random.FRandRange(-.25f, .25f), FMath::Sin(Angle));
        const FVector Start = Direction * Radius + FVector(0, -15.0f, Height);
        AddBeam(Start, Start + Direction * Random.FRandRange(12.0f, 28.0f), Random.FRandRange(2.0f, 4.0f),
            Direction * Travel + FVector(0, 0, 25.0f));
    }
}

void AAlphaCombatEffect::UpdatePieces(float Progress)
{
    const float Ease = 1.0f - FMath::Square(1.0f - Progress);
    const float Dissolve = static_cast<float>(AlphaPresentation::EffectEnvelope(Age, Duration));
    for (int32 Index = 0; Index < Pieces.Num(); ++Index)
    {
        const FPiece& Piece = Pieces[Index];
        const bool bVisible = Progress >= Piece.DelayFraction;
        const float LocalProgress = FMath::Clamp((Progress - Piece.DelayFraction) / (1.0f - Piece.DelayFraction), 0.0f, 1.0f);
        const float LocalEase = 1.0f - FMath::Square(1.0f - LocalProgress);
        const float LocalDissolve = Piece.DelayFraction > 0.0f
            ? static_cast<float>(AlphaPresentation::EffectEnvelope(LocalProgress, 1.0)) : Dissolve;
        const float RadialScale = FMath::Lerp(Piece.RadialStart, Piece.RadialEnd, LocalEase);
        FVector Position = Piece.Start;
        Position.X *= RadialScale;
        Position.Y *= RadialScale;
        Position += Piece.Drift * LocalEase;
        FVector Scale = Piece.Scale * (bVisible ? FMath::Max(.005f, LocalDissolve) : 0.0f);
        if (Piece.RadialStart != Piece.RadialEnd) Scale.X *= RadialScale;
        Instances->UpdateInstanceTransform(Index, FTransform(Piece.Rotation, Position, Scale), false,
            Index == Pieces.Num() - 1, true);
    }
    if (EffectMaterial)
    {
        EffectMaterial->SetScalarParameterValue(TEXT("Glow"), PeakGlow * Dissolve);
        const FLinearColor FadedTint = BaseTint * (.35f + .65f * Dissolve);
        EffectMaterial->SetVectorParameterValue(TEXT("Tint"), FadedTint);
        EffectMaterial->SetVectorParameterValue(TEXT("Color"), FadedTint);
    }
}

void AAlphaCombatEffect::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Age += FMath::Max(0.0f, DeltaSeconds);
    if (Age >= Duration)
    {
        Destroy();
        return;
    }
    UpdatePieces(FMath::Clamp(Age / Duration, 0.0f, 1.0f));
}

void AAlphaCombatEffect::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ActiveEffects.RemoveAll([this](const TWeakObjectPtr<AAlphaCombatEffect>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == this;
    });
    Super::EndPlay(EndPlayReason);
}
