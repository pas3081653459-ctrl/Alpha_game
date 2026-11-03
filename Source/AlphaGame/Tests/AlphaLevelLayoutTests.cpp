#if WITH_DEV_AUTOMATION_TESTS

#include "World/AlphaRouteGeometry.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaRouteCollisionTest,
    "AlphaGame.LevelLayout.Collision.ConnectedRouteAndRealNegativeSpace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaRouteCollisionTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Settings;
    Settings.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
        .CreateAISystem(false).RequiresHitProxies(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,
        true,ERHIFeatureLevel::Num,&Settings);
    if (!TestNotNull(TEXT("Route test world exists"),World)) return false;
    struct FWorldScope {UWorld* World;~FWorldScope(){World->DestroyWorld(false);}} Scope{World};
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    const auto Trace=[World,&Objects](const FVector& Start,const FVector& End,FHitResult& Hit)
    {
        return World->LineTraceSingleByObjectType(Hit,Start,End,Objects,FCollisionQueryParams(SCENE_QUERY_STAT(AlphaRouteTest),false));
    };
    for (int32 Chapter=0;Chapter<3;++Chapter)
    {
        const FVector Origin(Chapter*6000.,0,0);
        const FTransform Transform(FRotator::ZeroRotator,Origin);
        AAlphaRouteGeometry* Route=World->SpawnActorDeferred<AAlphaRouteGeometry>(AAlphaRouteGeometry::StaticClass(),Transform);
        if (!TestNotNull(TEXT("Route actor exists"),Route)) return false;
        Route->ChapterIndex=Chapter;
        Route->FinishSpawning(Transform);
        Route->DispatchBeginPlay();
        TestTrue(TEXT("Collision floor has multiple real pieces"),Route->GetFloorPieceCount()>1);
        TestTrue(TEXT("Outer contour has multiple blocking pieces"),Route->GetBoundaryPieceCount()>4);
        FHitResult Hit;
        TestTrue(TEXT("NPC landing floor exists"),Trace(Origin+FVector(-1000,0,200),Origin+FVector(-1000,0,-200),Hit));
        TestTrue(TEXT("Narrow connector has actual floor"),Trace(Origin+FVector(900,0,200),Origin+FVector(900,0,-200),Hit));
        TestFalse(TEXT("Water beside bridge has no hidden rectangular floor"),Trace(Origin+FVector(900,450,200),Origin+FVector(900,450,-200),Hit));
        TestFalse(TEXT("Connector joins have no accidental internal wall"),Trace(Origin+FVector(550,0,95),Origin+FVector(1300,0,95),Hit));
        TestTrue(TEXT("Bridge sides have blocking collision"),Trace(Origin+FVector(900,0,95),Origin+FVector(900,500,95),Hit));
        if (Chapter==0)
        {
            TestTrue(TEXT("Cargo dais has real collision"),Trace(Origin+FVector(200,385,200),Origin+FVector(200,385,-200),Hit));
            TestTrue(TEXT("Cargo dais collision matches raised visible surface"),FMath::IsNearlyEqual(Hit.ImpactPoint.Z,30.,.1));
        }
        if (Chapter==2)
            TestTrue(TEXT("Canal rear garden is truly walkable"),Trace(Origin+FVector(1900,500,200),Origin+FVector(1900,500,-200),Hit));
    }
    return true;
}

#endif
