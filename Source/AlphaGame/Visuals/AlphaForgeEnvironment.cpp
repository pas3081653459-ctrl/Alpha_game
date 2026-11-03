#include "Visuals/AlphaForgeEnvironment.h"

#include "Visuals/AlphaArtLibrary.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AAlphaForgeEnvironment::AAlphaForgeEnvironment()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(false);
    SetActorEnableCollision(false);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ForgeRoot"));

    UDirectionalLightComponent* Key = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("WarmKey"));
    Key->SetupAttachment(RootComponent);
    Key->SetMobility(EComponentMobility::Movable);
    Key->SetRelativeRotation(FRotator(-52, -35, 0));
    Key->SetIntensity(7.f);
    Key->SetLightColor(FLinearColor(1.f, .91f, .8f));
    Key->SetCastShadows(true);

    UDirectionalLightComponent* Fill = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("CoolFill"));
    Fill->SetupAttachment(RootComponent);
    Fill->SetMobility(EComponentMobility::Movable);
    Fill->SetRelativeRotation(FRotator(-30, 140, 0));
    Fill->SetIntensity(2.f);
    Fill->SetLightColor(FLinearColor(.36f, .58f, 1.f));
    Fill->SetCastShadows(false);

    USkyLightComponent* Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("AmbientSky"));
    Sky->SetupAttachment(RootComponent);
    Sky->SetMobility(EComponentMobility::Movable);
    Sky->SetIntensity(.35f);
    Sky->SetLightColor(FLinearColor(.45f, .6f, .68f));
    Sky->SetCastShadows(false);
    Sky->SetLowerHemisphereColor(FLinearColor(.055f, .08f, .09f));

    UExponentialHeightFogComponent* Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("WorkshopHaze"));
    Fog->SetupAttachment(RootComponent);
    Fog->SetRelativeLocation(FVector(0, 0, -150));
    Fog->SetFogDensity(.003f);
    Fog->SetFogHeightFalloff(.18f);
    Fog->SetFogInscatteringColor(FLinearColor(.045f, .075f, .085f));
    Fog->SetStartDistance(1100.f);

    UPostProcessComponent* Grade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("WorkshopGrade"));
    Grade->SetupAttachment(RootComponent);
    Grade->bUnbound = true;
    FPostProcessSettings& Settings = Grade->Settings;
    Settings.bOverride_AutoExposureMethod = true;
    Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Settings.AutoExposureApplyPhysicalCameraExposure = false;
    Settings.bOverride_AutoExposureBias = true;
    Settings.AutoExposureBias = -1.f;
    Settings.bOverride_BloomIntensity = true;
    Settings.BloomIntensity = .18f;
    Settings.bOverride_BloomThreshold = true;
    Settings.BloomThreshold = 2.f;
    Settings.bOverride_VignetteIntensity = true;
    Settings.VignetteIntensity = .08f;
    Settings.bOverride_MotionBlurAmount = true;
    Settings.MotionBlurAmount = 0.f;
    Settings.bOverride_AmbientOcclusionIntensity = true;
    Settings.AmbientOcclusionIntensity = .35f;

    // Three local lights maximum; none cast dynamic shadows or require Lumen.
    const auto AddLamp = [this](const TCHAR* Name, FVector Position, FLinearColor Color, float Intensity, float Radius)
    {
        UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(Name);
        Light->SetupAttachment(RootComponent);
        Light->SetRelativeLocation(Position);
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetLightColor(Color);
        Light->SetIntensity(Intensity);
        Light->SetAttenuationRadius(Radius);
        Light->SetCastShadows(false);
    };
    AddLamp(TEXT("FurnaceGlow"), FVector(0, 660, 420), FLinearColor(1.f, .25f, .045f), 2200.f, 850.f);
    AddLamp(TEXT("LeftLantern"), FVector(-1120, 280, 300), FLinearColor(1.f, .48f, .1f), 600.f, 460.f);
    AddLamp(TEXT("RightLantern"), FVector(1120, 280, 300), FLinearColor(1.f, .48f, .1f), 600.f, 460.f);
}

void AAlphaForgeEnvironment::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() != NM_DedicatedServer)
    {
        BuildWorkshop();
        if (USkyLightComponent* Sky = FindComponentByClass<USkyLightComponent>()) Sky->RecaptureSky();
    }
}

UInstancedStaticMeshComponent* AAlphaForgeEnvironment::MakeBatch(EAlphaPrimitive Shape, const FLinearColor& Tint,
    float Metallic, float Roughness, float Glow)
{
    UInstancedStaticMeshComponent* Batch = NewObject<UInstancedStaticMeshComponent>(this);
    AddInstanceComponent(Batch);
    Batch->SetupAttachment(RootComponent);
    Batch->SetMobility(EComponentMobility::Movable);
    Batch->SetStaticMesh(AlphaArt::Mesh(Shape));
    Batch->SetMaterial(0, AlphaArt::Material(this, Tint, Metallic, Roughness, Glow));
    Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Batch->SetGenerateOverlapEvents(false);
    Batch->SetCanEverAffectNavigation(false);
    Batch->SetCastShadow(true);
    Batch->RegisterComponent();
    Batches.Add(Batch);
    return Batch;
}

void AAlphaForgeEnvironment::Add(UInstancedStaticMeshComponent* Batch, const FVector& Position,
    const FVector& Scale, const FRotator& Rotation)
{
    if (Batch) Batch->AddInstance(FTransform(Rotation, Position, Scale));
}

void AAlphaForgeEnvironment::Bar(UInstancedStaticMeshComponent* Batch, const FVector& Start,
    const FVector& End, float Width, float Height)
{
    const FVector Delta = End - Start;
    if (Delta.IsNearlyZero()) return;
    Add(Batch, (Start + End) * .5f, FVector(Delta.Size() / 100.f, Width / 100.f, Height / 100.f), Delta.Rotation());
}

void AAlphaForgeEnvironment::Pipe(UInstancedStaticMeshComponent* Batch, const FVector& Start,
    const FVector& End, float Radius)
{
    const FVector Delta = End - Start;
    if (Delta.IsNearlyZero()) return;
    const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Delta.GetSafeNormal());
    Add(Batch, (Start + End) * .5f, FVector(Radius / 50.f, Radius / 50.f, Delta.Size() / 100.f), Rotation.Rotator());
}

void AAlphaForgeEnvironment::Ring(UInstancedStaticMeshComponent* Batch, const FVector& Center,
    float Radius, int32 Segments, float Width, float Depth)
{
    FVector Previous = Center + FVector(Radius, 0, 0);
    for (int32 Index = 1; Index <= Segments; ++Index)
    {
        const float Angle = 2.f * PI * Index / Segments;
        const FVector Next = Center + FVector(FMath::Cos(Angle) * Radius, 0, FMath::Sin(Angle) * Radius);
        Bar(Batch, Previous, Next, Depth, Width);
        Previous = Next;
    }
}

void AAlphaForgeEnvironment::BuildWorkshop()
{
    // 13 mesh/material batches, 416 instances, fixed seed, no Tick or gameplay collision.
    UInstancedStaticMeshComponent* Iron = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.035f, .055f, .065f), .8f, .5f);
    UInstancedStaticMeshComponent* DarkFloor = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.105f, .13f, .135f), .4f, .8f);
    UInstancedStaticMeshComponent* LightFloor = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.16f, .185f, .18f), .25f, .8f);
    UInstancedStaticMeshComponent* Ceramic = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.055f, .27f, .27f), .12f, .3f);
    UInstancedStaticMeshComponent* Brass = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.5f, .29f, .09f), .8f, .35f);
    UInstancedStaticMeshComponent* Amber = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(1.f, .23f, .025f), .05f, .6f, 3.f);
    UInstancedStaticMeshComponent* Cyan = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.08f, .45f, .48f), .05f, .5f, .7f);
    UInstancedStaticMeshComponent* IronPipe = MakeBatch(EAlphaPrimitive::Cylinder, FLinearColor(.055f, .075f, .08f), .85f, .45f);
    UInstancedStaticMeshComponent* BrassPipe = MakeBatch(EAlphaPrimitive::Cylinder, FLinearColor(.45f, .25f, .065f), .8f, .35f);
    UInstancedStaticMeshComponent* Hearth = MakeBatch(EAlphaPrimitive::Cylinder, FLinearColor(.55f, .045f, .008f), .1f, .8f, 2.f);
    UInstancedStaticMeshComponent* Ember = MakeBatch(EAlphaPrimitive::Sphere, FLinearColor(1.f, .3f, .04f), 0.f, .7f, 3.f);
    UInstancedStaticMeshComponent* Roof = MakeBatch(EAlphaPrimitive::Cone, FLinearColor(.035f, .14f, .145f), .3f, .7f);
    UInstancedStaticMeshComponent* DistantIron = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.065f, .105f, .12f), .3f, .9f);
    Amber->SetCastShadow(false);
    Cyan->SetCastShadow(false);
    Hearth->SetCastShadow(false);
    Ember->SetCastShadow(false);
    FRandomStream Random(308165);

    // Readable, moderately bright combat floor; visual surface tops at Z=1.
    Add(Iron, FVector(0, 0, -18), FVector(28.2f, 8.2f, .35f));
    for (int32 X = 0; X < 14; ++X)
    {
        for (int32 Y = 0; Y < 4; ++Y)
        {
            Add((X + Y) % 3 == 0 ? LightFloor : DarkFloor,
                FVector(-1300 + X * 200, -300 + Y * 200, -2), FVector(1.97f, 1.97f, .06f));
        }
    }
    for (int32 X = -13; X <= 13; ++X)
    {
        for (int32 Side : {-1, 1})
        {
            Add(Brass, FVector(X * 100, Side * 360, 2), FVector(.52f, .025f, .025f));
        }
    }
    for (int32 Side : {-1, 1})
    {
        Add(Iron, FVector(0, Side * 425, -8), FVector(28.5f, .5f, .3f));
        Add(Brass, FVector(0, Side * 405, 1), FVector(28.f, .025f, .035f));
    }
    // Low foreground framing stays below players' feet; tall details at corners.
    for (int32 Side : {-1, 1})
    {
        Add(Iron, FVector(Side * 1380, -470, 15), FVector(.4f, .7f, .5f));
        Add(Brass, FVector(Side * 1380, -470, 42), FVector(.5f, .78f, .06f));
    }

    // Layer 1: huge recessed furnace, segmented brass rings and protective cage.
    const FVector Core(0, 910, 480);
    Add(IronPipe, Core + FVector(0, 70, 0), FVector(6.5f, 6.5f, 1.6f), FRotator(0, 0, 90));
    Add(Hearth, Core, FVector(5.3f, 5.3f, .16f), FRotator(0, 0, 90));
    Add(Ember, Core + FVector(0, -20, 0), FVector(3.4f, .45f, 3.4f));
    Ring(Brass, Core + FVector(0, -42, 0), 295.f, 32, 20.f, 25.f);
    Ring(Iron, Core + FVector(0, -65, 0), 228.f, 24, 12.f, 18.f);
    Ring(Amber, Core + FVector(0, -70, 0), 205.f, 24, 5.f, 7.f);
    for (int32 Index = -2; Index <= 2; ++Index)
    {
        const float X = Index * 72.f;
        const float HalfHeight = FMath::Sqrt(FMath::Max(0.f, 220.f * 220.f - X * X));
        Bar(Iron, Core + FVector(X, -86, -HalfHeight), Core + FVector(X, -86, HalfHeight), 16.f, 14.f);
    }
    for (int32 Index = 0; Index < 12; ++Index)
    {
        const float Angle = Index * 30.f;
        const FVector Position = Core + FVector(FMath::Cos(FMath::DegreesToRadians(Angle)) * 326.f, -35, FMath::Sin(FMath::DegreesToRadians(Angle)) * 326.f);
        Add(Brass, Position, FVector(.32f, .35f, .24f), FRotator(-Angle, 0, 0));
    }
    Add(Iron, FVector(0, 900, 115), FVector(8, 3, 1.5f));
    Add(Ceramic, FVector(0, 725, 105), FVector(5.6f, .25f, .7f));
    for (int32 Index = -3; Index <= 3; ++Index)
    {
        Add(Amber, FVector(Index * 70, 704, 108), FVector(.28f, .06f, .1f));
    }

    // Layer 2: structural bays, ceramic cladding, conduits and hanging lanterns.
    for (int32 Side : {-1, 1})
    {
        for (int32 Bay = 0; Bay < 3; ++Bay)
        {
            const float X = Side * (600.f + Bay * 360.f);
            Add(Iron, FVector(X, 700, 440), FVector(.65f, .8f, 8.8f));
            Add(Ceramic, FVector(X, 645, 335), FVector(.8f, .25f, 4.1f));
            Add(Brass, FVector(X, 625, 125), FVector(.95f, .35f, .14f));
            Add(Brass, FVector(X, 625, 550), FVector(.95f, .35f, .14f));
            Add(Iron, FVector(X, 700, 900), FVector(1.1f, 1.15f, .3f));
            Bar(Iron, FVector(X - 180, 700, 920), FVector(X + 180, 700, 920), 65, 55);
            Pipe(BrassPipe, FVector(X + Side * 65, 760, 140), FVector(X + Side * 65, 760, 800), 16);
            for (int32 Clamp = 0; Clamp < 3; ++Clamp)
            {
                Add(Iron, FVector(X + Side * 65, 760, 230 + Clamp * 230), FVector(.65f, .5f, .12f));
            }
        }
        Bar(Iron, FVector(Side * 480, 880, 840), FVector(Side * 1400, 880, 840), 100, 70);
        Pipe(IronPipe, FVector(Side * 380, 1000, 430), FVector(Side * 560, 1000, 430), 45);
        Pipe(IronPipe, FVector(Side * 560, 1000, 430), FVector(Side * 560, 1000, 1080), 45);
        Pipe(BrassPipe, FVector(Side * 350, 1150, 1030), FVector(Side * 1420, 1150, 1030), 22);

        const FVector Lantern(Side * 1120, 480, 335);
        Pipe(BrassPipe, Lantern + FVector(0, 0, 80), Lantern + FVector(0, 0, 360), 4);
        Add(Ember, Lantern, FVector(.75f, .75f, 1.1f));
        Add(IronPipe, Lantern + FVector(0, 0, 60), FVector(.9f, .9f, .12f));
        Add(IronPipe, Lantern - FVector(0, 0, 60), FVector(.9f, .9f, .12f));
        for (int32 Strip = 0; Strip < 4; ++Strip)
        {
            const float Angle = Strip * PI * .5f;
            const FVector Offset(FMath::Cos(Angle) * 35, FMath::Sin(Angle) * 35, 0);
            Pipe(BrassPipe, Lantern + Offset - FVector(0, 0, 55), Lantern + Offset + FVector(0, 0, 55), 3);
        }
    }

    // Layer 3: distant towers and cog silhouettes, deliberately lower contrast.
    Add(DistantIron, FVector(0, 1800, 580), FVector(45, .5f, 13));
    for (int32 Index = -4; Index <= 4; ++Index)
    {
        const float X = Index * 430.f;
        const float Height = Random.FRandRange(700.f, 1200.f);
        Add(DistantIron, FVector(X, 1550, Height * .5f), FVector(2.3f, 2.3f, Height / 100.f));
        Add(Roof, FVector(X, 1550, Height + 100), FVector(3.8f, 3.8f, 2.f));
        for (int32 Window = 0; Window < 3; ++Window)
        {
            Add(Cyan, FVector(X + (Window - 1) * 55, 1428, Height - 100), FVector(.12f, .04f, .4f));
        }
    }
    for (int32 Side : {-1, 1})
    {
        const FVector GearCenter(Side * 1030, 1240, 660);
        Ring(Iron, GearCenter, 155, 16, 26, 22);
        for (int32 Tooth = 0; Tooth < 12; ++Tooth)
        {
            const float Angle = Tooth * 30.f;
            const FVector Radial(FMath::Cos(FMath::DegreesToRadians(Angle)), 0, FMath::Sin(FMath::DegreesToRadians(Angle)));
            Bar(Iron, GearCenter + Radial * 135, GearCenter + Radial * 187, 25, 32);
        }
        Pipe(BrassPipe, GearCenter + FVector(0, -20, 0), GearCenter + FVector(0, 25, 0), 35);
    }
}
