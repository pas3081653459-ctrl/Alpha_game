#include "World/AlphaRouteGeometry.h"

#include "Rules/LevelLayoutRules.h"
#include "Visuals/AlphaArtLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AAlphaRouteGeometry::AAlphaRouteGeometry()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(false);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RouteRoot"));
}

void AAlphaRouteGeometry::BeginPlay()
{
    Super::BeginPlay();
    BuildRoute();
}

void AAlphaRouteGeometry::AddCollisionBox(const FVector& Center, const FVector& Extent)
{
    UBoxComponent* Box = NewObject<UBoxComponent>(this);
    AddInstanceComponent(Box);
    Box->SetMobility(EComponentMobility::Movable);
    Box->SetupAttachment(RootComponent);
    Box->SetRelativeLocation(Center);
    Box->SetBoxExtent(Extent);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->SetCollisionObjectType(ECC_WorldStatic);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->SetGenerateOverlapEvents(false);
    Box->SetCanEverAffectNavigation(false);
    Box->SetHiddenInGame(true);
    Box->RegisterComponent();
    CollisionPieces.Add(Box);
}

UInstancedStaticMeshComponent* AAlphaRouteGeometry::MakeBatch(FLinearColor Tint, int32 Style, float Strength)
{
    if (GetNetMode() == NM_DedicatedServer) return nullptr;
    UInstancedStaticMeshComponent* Batch = NewObject<UInstancedStaticMeshComponent>(this);
    AddInstanceComponent(Batch);
    Batch->SetMobility(EComponentMobility::Movable);
    Batch->SetupAttachment(RootComponent);
    Batch->SetStaticMesh(AlphaArt::Mesh(EAlphaPrimitive::Cube));
    Batch->SetMaterial(0, AlphaArt::Material(this,Tint,.08f,.85f,0.f,.45f,Style,Strength));
    Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Batch->SetCanEverAffectNavigation(false);
    Batch->SetGenerateOverlapEvents(false);
    Batch->SetCastShadow(false);
    Batch->RegisterComponent();
    SurfaceBatches.Add(Batch);
    return Batch;
}

void AAlphaRouteGeometry::BuildRoute()
{
    if (!AlphaLayoutRules::ValidChapter(ChapterIndex)) return;
    // Opaque, lightly tinted atlas surfaces remain readable without a sky capture.
    UInstancedStaticMeshComponent* Floor = MakeBatch(FLinearColor(.95f,.96f,1.f),ChapterIndex==0?0:1,.65f);
    UInstancedStaticMeshComponent* Rim = MakeBatch(ChapterIndex==0?FLinearColor(.63f,.39f,.18f):FLinearColor(.2f,.55f,.58f),ChapterIndex==0?0:2,.2f);
    for (const AlphaLayoutRules::Rect& Rect:AlphaLayoutRules::FloorTiles(ChapterIndex))
    {
        const FVector Center((Rect.MinX+Rect.MaxX)*.5,(Rect.MinY+Rect.MaxY)*.5,-50);
        const FVector Extent((Rect.MaxX-Rect.MinX)*.5,(Rect.MaxY-Rect.MinY)*.5,50);
        AddCollisionBox(Center,Extent);
        if (Floor) Floor->AddInstance(FTransform(FRotator::ZeroRotator,Center,Extent*2./100.));
        ++FloorPieceCount;
    }
    for (const AlphaLayoutRules::Segment& Edge:AlphaLayoutRules::BoundarySegments(ChapterIndex))
    {
        const FVector Start(Edge.A.X,Edge.A.Y,0),End(Edge.B.X,Edge.B.Y,0);
        const FVector Mid=(Start+End)*.5;
        const FVector Delta=End-Start;
        FVector Outside(-Delta.Y,Delta.X,0);
        Outside.Normalize();
        if (AlphaLayoutRules::Contains(ChapterIndex,{Mid.X+Outside.X*.1,Mid.Y+Outside.Y*.1})) Outside*=-1.;
        const bool bVertical=FMath::Abs(Delta.X)<.01;
        AddCollisionBox(Mid+Outside*10.+FVector(0,0,500),
            bVertical?FVector(10,Delta.Size()*.5,500):FVector(Delta.Size()*.5,10,500));
        if (Rim)
        {
            Rim->AddInstance(FTransform(Delta.Rotation(),Mid+FVector(0,0,20),FVector(Delta.Size()/100.,.14,.4)));
            const int32 Posts=FMath::Max(1,FMath::CeilToInt(Delta.Size()/220.));
            for (int32 I=0;I<=Posts;++I)
                Rim->AddInstance(FTransform(FRotator::ZeroRotator,FMath::Lerp(Start,End,static_cast<double>(I)/Posts)+FVector(0,0,36),FVector(.16,.16,.72)));
        }
        ++BoundaryPieceCount;
    }
    if (ChapterIndex==0)
    {
        // A real 30cm step-up cargo dais. It has its own matching collision and
        // visible mesh, while the continuous ground below remains in the union.
        const FVector Center(200,385,15),Extent(250,135,15);
        AddCollisionBox(Center,Extent);
        if (Floor) Floor->AddInstance(FTransform(FRotator::ZeroRotator,Center,Extent*2./100.));
    }
}
