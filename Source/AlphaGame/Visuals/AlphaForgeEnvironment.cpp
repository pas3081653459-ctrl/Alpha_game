#include "Visuals/AlphaForgeEnvironment.h"

#include "Visuals/AlphaArtLibrary.h"
#include "Rules/LevelLayoutRules.h"
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
    // Illuminate the camera-facing (-Y) facade rather than only its back.
    Key->SetRelativeRotation(FRotator(-52, 140, 0));
    Key->SetIntensity(10.f);
    Key->ForwardShadingPriority = 1;
    Key->SetLightColor(FLinearColor(1.f, .91f, .8f));
    Key->SetCastShadows(true);

    UDirectionalLightComponent* Fill = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("CoolFill"));
    Fill->SetupAttachment(RootComponent);
    Fill->SetMobility(EComponentMobility::Movable);
    Fill->SetRelativeRotation(FRotator(-30, -35, 0));
    Fill->SetIntensity(4.f);
    Fill->ForwardShadingPriority = 0;
    Fill->SetLightColor(FLinearColor(.72f, .83f, 1.f));
    Fill->SetCastShadows(false);

    USkyLightComponent* Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("AmbientSky"));
    Sky->SetupAttachment(RootComponent);
    Sky->SetMobility(EComponentMobility::Movable);
    Sky->SetIntensity(1.f);
    Sky->SetLightColor(FLinearColor(.8f, .88f, 1.f));
    Sky->SetCastShadows(false);
    Sky->SetLowerHemisphereColor(FLinearColor(.22f, .27f, .34f));

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
    Settings.AutoExposureBias = 1.f;
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
    AddLamp(TEXT("FurnaceGlow"), FVector(1750, 830, 460), FLinearColor(1.f, .25f, .045f), 2200.f, 850.f);
    AddLamp(TEXT("LeftLantern"), FVector(-1120, 280, 300), FLinearColor(1.f, .48f, .1f), 600.f, 460.f);
    AddLamp(TEXT("RightLantern"), FVector(1840, 340, 300), FLinearColor(1.f, .48f, .1f), 600.f, 460.f);
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
    float Metallic, float Roughness, float Glow, int32 SurfaceStyle, float TextureStrength)
{
    UInstancedStaticMeshComponent* Batch = NewObject<UInstancedStaticMeshComponent>(this);
    AddInstanceComponent(Batch);
    Batch->SetupAttachment(RootComponent);
    Batch->SetMobility(EComponentMobility::Movable);
    Batch->SetStaticMesh(AlphaArt::Mesh(Shape));
    // Empty Entry has no guaranteed sky/reflection environment. Highly metallic
    // scenery becomes nearly black without IBL; keep a diffuse color foundation.
    Batch->SetMaterial(0, AlphaArt::Material(this, Tint, FMath::Min(Metallic, .35f), Roughness, Glow, .45f, SurfaceStyle, TextureStrength));
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
    // RouteGeometry supplies the actual non-rectangular floor and low outline.
    // This actor only supplies landmarks, sky/light and collision-free dressing.
    UInstancedStaticMeshComponent* Iron = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.25f, .33f, .41f), .25f, .65f);
    UInstancedStaticMeshComponent* Ceramic = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.7f, .96f, .95f), .08f, .6f, 0, 2, .7f);
    UInstancedStaticMeshComponent* Brass = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.84f, .58f, .25f), .3f, .5f);
    UInstancedStaticMeshComponent* Paper = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.97f, .93f, .8f), .02f, .85f, 0, 1, .5f);
    UInstancedStaticMeshComponent* Banner = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(1.f, .74f, .68f), .02f, .85f, 0, 3, .7f);
    UInstancedStaticMeshComponent* Amber = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(1.f, .43f, .12f), .02f, .6f, 1.1f);
    UInstancedStaticMeshComponent* Cyan = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.2f, .83f, .9f), .02f, .65f, .35f);
    UInstancedStaticMeshComponent* IronPipe = MakeBatch(EAlphaPrimitive::Cylinder, FLinearColor(.24f, .32f, .4f), .3f, .6f);
    UInstancedStaticMeshComponent* BrassPipe = MakeBatch(EAlphaPrimitive::Cylinder, FLinearColor(.78f, .49f, .19f), .3f, .6f);
    UInstancedStaticMeshComponent* Hearth = MakeBatch(EAlphaPrimitive::Cylinder, FLinearColor(.84f, .13f, .025f), .04f, .8f, 1.f);
    UInstancedStaticMeshComponent* Ember = MakeBatch(EAlphaPrimitive::Sphere, FLinearColor(1.f, .5f, .12f), 0, .8f, 1.4f);
    UInstancedStaticMeshComponent* DistantIron = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.3f, .4f, .47f), .1f, .9f);
    for (UInstancedStaticMeshComponent* Batch : {Amber, Cyan, Hearth, Ember}) Batch->SetCastShadow(false);
    const auto Block = [](UInstancedStaticMeshComponent* Batch, const FVector& Position,
        const FVector& Dimensions, const FRotator& Rotation = FRotator::ZeroRotator)
    {
        Add(Batch, Position, Dimensions / 100.f, Rotation);
    };

    // Entry: a small records desk and one banner distinguish the NPC briefing
    // area from both battle rooms. No full-width wall hides the guide.
    Block(Iron, FVector(-1080, 460, 70), FVector(280, 110, 140));
    Block(Brass, FVector(-1080, 460, 145), FVector(300, 125, 12));
    for (int32 Book = 0; Book < 3; ++Book)
        Block(Paper, FVector(-1160 + Book * 75.f, 440, 162 + Book % 2 * 7.f), FVector(58, 65, 15));
    Block(Iron, FVector(-1240, 510, 210), FVector(25, 25, 420));
    Block(Banner, FVector(-1190, 510, 310), FVector(110, 10, 165));
    Block(Amber, FVector(-960, 455, 215), FVector(30, 30, 65));

    // Landmark A: one massive forge hammer above the first wide hall. The
    // machinery is entirely behind Y=580 and never inside the playable floor.
    for (float X : {-350.f, 350.f})
    {
        Block(Iron, FVector(X, 770, 370), FVector(75, 100, 740));
        Block(Ceramic, FVector(X, 710, 280), FVector(90, 24, 420));
        Block(Brass, FVector(X, 690, 80), FVector(115, 28, 20));
    }
    Bar(Iron, FVector(-420, 770, 760), FVector(420, 770, 760), 110, 85);
    Pipe(BrassPipe, FVector(0, 760, 740), FVector(0, 760, 470), 26);
    Block(Iron, FVector(0, 750, 420), FVector(280, 180, 130));
    Block(Brass, FVector(0, 650, 420), FVector(240, 20, 65));
    Block(Hearth, FVector(0, 770, 175), FVector(250, 170, 260));
    Block(Amber, FVector(0, 675, 190), FVector(135, 12, 60));
    Block(Ceramic, FVector(-450, 850, 125), FVector(250, 170, 250));
    Pipe(IronPipe, FVector(-460, 920, 250), FVector(-460, 920, 600), 40);

    // The connecting loading corridor is visibly offset toward the camera.
    // Its pipes and warning stripes sit just OUTSIDE its rear Y=120 edge.
    Pipe(IronPipe, FVector(650, 230, 60), FVector(1110, 230, 60), 26);
    Pipe(BrassPipe, FVector(650, 235, 125), FVector(1110, 235, 125), 9);
    for (float X : {680.f, 1070.f})
    {
        Block(Iron, FVector(X, 245, 45), FVector(55, 70, 90));
        Block(Amber, FVector(X, 205, 80), FVector(28, 8, 20));
    }
    // Individual floor chevrons guide the bend; they do not fill the gap.
    for (float X : {700.f, 1050.f})
    {
        const FVector A(X-25,-110,4+AlphaLayoutRules::SurfaceHeight(1,{X-25,0}));
        const FVector B(X+15,-80,4+AlphaLayoutRules::SurfaceHeight(1,{X+15,0}));
        const FVector C(X-25,-50,A.Z);
        Bar(Brass,A,B,4,3);
        Bar(Brass,B,C,4,3);
    }

    // Landmark B: the furnace belongs to the distinct second encounter/Boss
    // room at X=1750, rather than duplicating the first-room backdrop.
    const FVector Core(1750, 1000, 500);
    Add(IronPipe, Core + FVector(0, 70, 0), FVector(6.5f, 6.5f, 1.6f), FRotator(0, 0, 90));
    Add(Hearth, Core, FVector(5.3f, 5.3f, .16f), FRotator(0, 0, 90));
    Add(Ember, Core + FVector(0, -20, 0), FVector(3.4f, .45f, 3.4f));
    Ring(Brass, Core + FVector(0, -42, 0), 295.f, 30, 20.f, 25.f);
    Ring(Iron, Core + FVector(0, -65, 0), 228.f, 22, 12.f, 18.f);
    Ring(Amber, Core + FVector(0, -70, 0), 205.f, 22, 5.f, 7.f);
    for (int32 Index = -2; Index <= 2; ++Index)
    {
        const float X = Index * 72.f;
        const float Height = FMath::Sqrt(FMath::Max(0.f, 220.f * 220.f - X * X));
        Bar(Iron, Core + FVector(X, -86, -Height), Core + FVector(X, -86, Height), 16, 14);
    }
    Block(Iron, FVector(1750, 980, 100), FVector(800, 250, 160));
    Block(Ceramic, FVector(1750, 835, 120), FVector(560, 28, 85));
    for (float X : {1260.f, 2240.f})
    {
        Block(Iron, FVector(X, 840, 425), FVector(75, 95, 850));
        Block(Ceramic, FVector(X, 780, 330), FVector(90, 25, 480));
        Block(Brass, FVector(X, 760, 570), FVector(110, 30, 20));
        Pipe(BrassPipe, FVector(X + 55, 900, 180), FVector(X + 55, 900, 800), 15);
    }
    Bar(Iron, FVector(1180, 870, 900), FVector(2320, 870, 900), 100, 70);
    Pipe(IronPipe, FVector(1410, 1100, 450), FVector(1230, 1100, 450), 40);
    Pipe(IronPipe, FVector(1230, 1100, 450), FVector(1230, 1100, 1100), 40);

    // A thin brass octagon decorates the boss floor without adding a second
    // floor mesh or implying a larger walkable footprint.
    for (int32 Segment = 0; Segment < 8; ++Segment)
    {
        const float A = Segment * PI / 4.f;
        const float B = (Segment + 1) * PI / 4.f;
        Bar(Brass, FVector(1800 + FMath::Cos(A) * 440, FMath::Sin(A) * 440, 3),
            FVector(1800 + FMath::Cos(B) * 440, FMath::Sin(B) * 440, 3), 5, 2);
    }
    Block(Paper, FVector(2470, 330, 85), FVector(105, 80, 155));
    Pipe(BrassPipe, FVector(2440, 410, 110), FVector(2440, 820, 110), 18);
    Block(Cyan, FVector(2440, 810, 110), FVector(65, 20, 45));

    // Two varied, lower-contrast distant silhouettes replace the repeated
    // tower/roof wall. Empty sky separates the hammer and furnace landmarks.
    Block(DistantIron, FVector(-450, 1480, 400), FVector(220, 210, 800));
    Block(DistantIron, FVector(2260, 1570, 570), FVector(280, 240, 1140));
    for (int32 Window = 0; Window < 3; ++Window)
        Block(Cyan, FVector(2260 + (Window - 1) * 60, 1445, 920), FVector(14, 7, 45));
}
