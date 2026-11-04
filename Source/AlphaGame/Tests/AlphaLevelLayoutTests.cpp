#if WITH_DEV_AUTOMATION_TESTS

#include "World/AlphaRouteGeometry.h"
#include "Rules/LevelLayoutRules.h"
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
        TestTrue(TEXT("Narrow connector has actual floor"),Trace(Origin+FVector(900,0,500),Origin+FVector(900,0,-200),Hit));
        TestFalse(TEXT("Water beside bridge has no hidden rectangular floor"),Trace(Origin+FVector(900,450,200),Origin+FVector(900,450,-200),Hit));
        TestFalse(TEXT("Connector joins have no accidental internal wall"),Trace(Origin+FVector(550,0,400),Origin+FVector(1300,0,400),Hit));
        TestTrue(TEXT("Bridge sides have blocking collision"),Trace(Origin+FVector(900,0,400),Origin+FVector(900,500,400),Hit));
        for (double X : {-1000.,-200.,0.,400.,600.,750.,900.,1150.,1250.,1750.,2400.})
        {
            const double Y=AlphaLayoutRules::RouteCenterY(Chapter,X);
            const double Expected=AlphaLayoutRules::SurfaceHeight(Chapter,{X,Y});
            const bool bFloor=Trace(Origin+FVector(X,Y,Expected+200),Origin+FVector(X,Y,-200),Hit);
            TestTrue(TEXT("Slope/high platform has collision at each profile sample"),bFloor);
            if (bFloor) TestTrue(TEXT("Collision height matches authoritative spawn and visual profile"),
                FMath::IsNearlyEqual(Hit.ImpactPoint.Z,Expected,.2));
        }
        if (Chapter==2)
            TestTrue(TEXT("Canal rear garden is truly walkable"),Trace(Origin+FVector(1900,500,500),Origin+FVector(1900,500,-200),Hit));
    }
    return true;
}

#endif
