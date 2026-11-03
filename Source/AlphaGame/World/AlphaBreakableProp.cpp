#include "World/AlphaBreakableProp.h"

#include "Combat/AlphaCombatComponent.h"
#include "Visuals/AlphaArtLibrary.h"
#include "Visuals/AlphaCombatEffect.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AAlphaBreakableProp::AAlphaBreakableProp()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true; // Only eight props in the arena; late joins receive persistent state.
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("PropRoot"));
    for (int32 Index = 0; Index < 7; ++Index)
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(
            *FString::Printf(TEXT("CratePart%d"), Index));
        Part->SetupAttachment(RootComponent);
        Part->SetStaticMesh(AlphaArt::Mesh(EAlphaPrimitive::Cube));
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false);
        if (Index < 5)
        {
            Part->SetRelativeLocation(FVector((Index - 2) * 12.f, 0, 0));
            Part->SetRelativeScale3D(FVector(.112f, .58f, .64f));
        }
        else
        {
            Part->SetRelativeLocation(FVector(0, 0, (Index == 5 ? -1.f : 1.f) * 21.f));
            Part->SetRelativeScale3D(FVector(.65f, .64f, .065f));
        }
        Parts.Add(Part);
    }
}

void AAlphaBreakableProp::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() != NM_DedicatedServer)
    {
        for (int32 Index = 0; Index < Parts.Num(); ++Index)
        {
            Parts[Index]->SetMaterial(0, AlphaArt::Material(this,
                Index < 5 ? FLinearColor(.48f, .19f, .065f) : FLinearColor(.09f, .16f, .23f),
                Index < 5 ? 0.f : .5f, .85f));
        }
    }
    PresentState(false); // Loading an already broken prop does not replay an old explosion.
    bPresentationReady = true;
}

void AAlphaBreakableProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AAlphaBreakableProp, bBroken);
}

void AAlphaBreakableProp::AffectAttack(UWorld* World, AActor* DamageSource, const FVector& Start,
    const FVector& End, float Radius)
{
    if (!World || !IsValid(DamageSource) || DamageSource->GetWorld() != World || !DamageSource->HasAuthority()
        || Start.ContainsNaN() || End.ContainsNaN() || !FMath::IsFinite(Radius) || Radius <= 0.f) return;
    const UAlphaCombatComponent* Combat = DamageSource->FindComponentByClass<UAlphaCombatComponent>();
    if (!Combat || Combat->TeamId != 1 || !Combat->IsAlive()) return;
    const float SafeRadius = FMath::Min(Radius, 600.f);
    for (TActorIterator<AAlphaBreakableProp> It(World); It; ++It)
    {
        AAlphaBreakableProp* Prop = *It;
        if (Prop->bBroken || !Prop->HasAuthority()) continue;
        const FVector Center = Prop->GetActorLocation();
        const FVector Closest = FMath::ClosestPointOnSegment(Center, Start, End);
        if (FVector::DistSquared(Closest, Center) > FMath::Square(SafeRadius + 44.f)) continue;
        FCollisionQueryParams Sight(SCENE_QUERY_STAT(AlphaPropImpact), false, DamageSource);
        Sight.AddIgnoredActor(Prop);
        if (World->LineTraceTestByChannel(Start, Center, ECC_Visibility, Sight)) continue;
        Prop->bBroken = true;
        Prop->PresentState(true);
        Prop->ForceNetUpdate();
    }
}

void AAlphaBreakableProp::OnRep_Broken()
{
    PresentState(bPresentationReady && bBroken);
}

void AAlphaBreakableProp::PresentState(bool bBurst)
{
    for (UStaticMeshComponent* Part : Parts) if (Part) Part->SetVisibility(!bBroken);
    if (bBurst && GetNetMode() != NM_DedicatedServer)
    {
        AAlphaCombatEffect::Spawn(GetWorld(), GetActorLocation(), FVector::ForwardVector,
            EAlphaEffectType::Poise, FLinearColor(1.f, .55f, .15f));
    }
}

void AAlphaBreakableProp::ResetForRound()
{
    if (!HasAuthority()) return;
    bBroken = false;
    PresentState(false);
    ForceNetUpdate();
}
