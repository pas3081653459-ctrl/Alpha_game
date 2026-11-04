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

void AAlphaRouteGeometry::AddCollisionBox(const FVector& Center, const FVector& Extent, const FRotator& Rotation)
{
    UBoxComponent* Box = NewObject<UBoxComponent>(this);
    AddInstanceComponent(Box);
    Box->SetMobility(EComponentMobility::Movable);
    Box->SetupAttachment(RootComponent);
    Box->SetRelativeLocation(Center);
    Box->SetRelativeRotation(Rotation);
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
    for (const AlphaLayoutRules::TerrainTile& Tile:AlphaLayoutRules::TerrainTiles(ChapterIndex))
    {
        const AlphaLayoutRules::Rect& Rect=Tile.Bounds;
        const double Run=Rect.MaxX-Rect.MinX,Rise=Tile.EndZ-Tile.StartZ;
        const double Pitch=FMath::Atan2(Rise,Run);
        const FRotator Rotation(FMath::RadiansToDegrees(Pitch),0,0);
        const FVector Normal(-FMath::Sin(Pitch),0,FMath::Cos(Pitch));
        const FVector Top((Rect.MinX+Rect.MaxX)*.5,(Rect.MinY+Rect.MaxY)*.5,(Tile.StartZ+Tile.EndZ)*.5);
        const FVector Center=Top-Normal*50.;
        const FVector Extent(FMath::Sqrt(Run*Run+Rise*Rise)*.5,(Rect.MaxY-Rect.MinY)*.5,50);
        AddCollisionBox(Center,Extent,Rotation);
        if (Floor) Floor->AddInstance(FTransform(Rotation,Center,Extent*2./100.));
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
            // Segment side trims at slope changes too; a long level rail would
            // otherwise disappear inside a raised deck or float over its ramp.
            const int32 Posts=FMath::Max(1,FMath::CeilToInt(Delta.Size()/100.));
            for (int32 I=0;I<Posts;++I)
            {
                FVector A=FMath::Lerp(Start,End,static_cast<double>(I)/Posts);
                FVector B=FMath::Lerp(Start,End,static_cast<double>(I+1)/Posts);
                A.Z=AlphaLayoutRules::SurfaceHeight(ChapterIndex,{A.X,A.Y})+20.;
                B.Z=AlphaLayoutRules::SurfaceHeight(ChapterIndex,{B.X,B.Y})+20.;
                const FVector Rail=B-A;
                Rim->AddInstance(FTransform(Rail.Rotation(),(A+B)*.5,FVector(Rail.Size()/100.,.14,.4)));
                // Tall retaining faces make each elevation readable from the
                // game camera while staying outside the playable footprint.
                const double BaseZ=-100.,TopZ=(A.Z+B.Z)*.5-20.;
                Rim->AddInstance(FTransform(Delta.Rotation(),(A+B)*.5+Outside*7.+FVector(0,0,(BaseZ-TopZ)*.5-20.),
                    FVector(Delta.Size()/Posts/100.,.12,(TopZ-BaseZ)/100.)));
                if (I%2==0) Rim->AddInstance(FTransform(FRotator::ZeroRotator,A+FVector(0,0,16),FVector(.16,.16,.72)));
            }
        }
        ++BoundaryPieceCount;
    }
}
