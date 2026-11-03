#if WITH_DEV_AUTOMATION_TESTS

#include "World/AlphaBreakableProp.h"
#include "Characters/AlphaCharacter.h"
#include "Combat/AlphaCombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Visuals/AlphaArtLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaPropImpactTest,
    "AlphaGame.Environment.Props.RangeObstructionAndReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaPropImpactTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Settings;
    Settings.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
        .CreateAISystem(false).RequiresHitProxies(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
        true, ERHIFeatureLevel::Num, &Settings);
    if (!TestNotNull(TEXT("World exists"), World)) return false;
    AAlphaCharacter* Hero = World->SpawnActor<AAlphaCharacter>(FVector(0, 0, 100), FRotator::ZeroRotator);
    AAlphaBreakableProp* Prop = World->SpawnActor<AAlphaBreakableProp>(FVector(230, 0, 100), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Hero exists"), Hero) || !TestNotNull(TEXT("Prop exists"), Prop))
    {
        World->DestroyWorld(false);
        return false;
    }
    Hero->DispatchBeginPlay();
    Prop->DispatchBeginPlay();
    const FVector Start(0, 0, 100);
    AAlphaBreakableProp::AffectAttack(World, Hero, Start, FVector(80, 0, 100), 30);
    TestFalse(TEXT("Distant attack cannot break prop"), Prop->IsBroken());

    AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>(FVector(110, 0, 100), FRotator::ZeroRotator);
    if (TestNotNull(TEXT("Obstruction exists"), Wall))
    {
        UStaticMeshComponent* Mesh = Wall->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(AlphaArt::Mesh(EAlphaPrimitive::Cube));
        Mesh->SetWorldScale3D(FVector(.2f, 2.f, 2.f));
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        AAlphaBreakableProp::AffectAttack(World, Hero, Start, FVector(250, 0, 100), 50);
        TestFalse(TEXT("Attack cannot break scenery through wall"), Prop->IsBroken());
        Wall->Destroy();
    }
    AAlphaBreakableProp::AffectAttack(World, Hero, Start, FVector(250, 0, 100), 50);
    TestTrue(TEXT("In-range clear attack breaks prop"), Prop->IsBroken());
    AAlphaBreakableProp::AffectAttack(World, Hero, Start, FVector(250, 0, 100), 50);
    TestTrue(TEXT("Repeated hit preserves broken state"), Prop->IsBroken());
    Prop->ResetForRound();
    TestFalse(TEXT("Round reset restores prop"), Prop->IsBroken());
    Hero->GetCombatComponent()->TeamId = 2;
    AAlphaBreakableProp::AffectAttack(World, Hero, Start, FVector(250, 0, 100), 50);
    TestFalse(TEXT("Enemy attacks cannot consume player scenery targets"), Prop->IsBroken());
    Hero->GetCombatComponent()->TeamId = 1;
    AAlphaBreakableProp::AffectAttack(World, nullptr, Start, FVector(250, 0, 100), 50);
    TestFalse(TEXT("Missing source cannot break prop"), Prop->IsBroken());
    AAlphaBreakableProp::AffectAttack(World, Hero, Start, Start, -1);
    TestFalse(TEXT("Invalid radius cannot break prop"), Prop->IsBroken());
    World->DestroyWorld(false);
    return true;
}

#endif
