#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AlphaCombatEffect.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

UENUM()
enum class EAlphaEffectType : uint8
{
    Slash,
    HeavySlash,
    Dash,
    Launch,
    Pull,
    Pulse,
    Coop,
    Hit,
    Poise,
    Revive,
    Combo,
    Sprint,
    Ultimate,
    GroundImpact
};

/** Bounded, local-only mesh effects. No gameplay, collision, replication or transparent overdraw. */
UCLASS(NotBlueprintable, Transient)
class ALPHAGAME_API AAlphaCombatEffect : public AActor
{
    GENERATED_BODY()

public:
    AAlphaCombatEffect();
    virtual void Tick(float DeltaSeconds) override;

    /** Returns null on a dedicated server, distant camera, exhausted budget or missing mesh. */
    static AAlphaCombatEffect* Spawn(UWorld* World, const FVector& Location, const FVector& Forward,
        EAlphaEffectType Type, FLinearColor Color, bool bEmpowered = false);
    static AAlphaCombatEffect* SpawnCombo(UWorld* World, const FVector& Location, const FVector& Forward,
        int32 Stage, bool bAirborne, FLinearColor Color);

    static AAlphaCombatEffect* SpawnSkill(UWorld* World, const FVector& Location, const FVector& Forward,
        int32 Archetype, int32 Slot, FLinearColor Color);

    /** Cosmetic only: caller supplies a confirmed surface contact and gameplay footprint. */
    static AAlphaCombatEffect* SpawnGroundImpact(UWorld* World, const FVector& Location,
        const FVector& Normal, const FVector& Forward, float Radius, FLinearColor Color);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    struct FPiece
    {
        FVector Start = FVector::ZeroVector;
        FVector Drift = FVector::ZeroVector;
        FVector Scale = FVector::OneVector;
        FQuat Rotation = FQuat::Identity;
        float RadialStart = 1.0f;
        float RadialEnd = 1.0f;
        float DelayFraction = 0.0f;
        float Gravity = 0.0f;
    };

    static AAlphaCombatEffect* SpawnInternal(UWorld* World, const FVector& Location, const FVector& Forward,
        EAlphaEffectType Type, FLinearColor Color, bool bEmpowered, int32 Stage, bool bAirborne);
    void Configure(EAlphaEffectType InType, FLinearColor Color, bool bEmpowered, int32 Stage, bool bAirborne);
    void ConfigureCombo(int32 Stage, bool bAirborne);
    void ConfigureGroundImpact(float Radius);
    void ConfigureSkill(int32 Archetype, int32 Slot);
    void AddBeam(const FVector& Start, const FVector& End, float Thickness,
        const FVector& Drift = FVector::ZeroVector, float RadialStart = 1.0f, float RadialEnd = 1.0f);
    void AddRing(int32 Segments, float Radius, float Height, float Thickness, float StartScale, float EndScale);
    void AddSparks(int32 Count, float Radius, float Travel, float Height, int32 Seed);
    void UpdatePieces(float Progress);
    int32 Priority() const;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UInstancedStaticMeshComponent> Instances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UInstancedStaticMeshComponent> CoreInstances;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> EffectMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> CoreMaterial;

    TArray<FPiece> Pieces;
    EAlphaEffectType EffectType = EAlphaEffectType::Hit;
    float Age = 0.0f;
    float Duration = 0.3f;
    float PeakGlow = 1.0f;
    FLinearColor BaseTint = FLinearColor::White;
    static constexpr int32 MaxInstances = 24;
    static constexpr int32 MaxEffectsPerWorld = 24;
};
