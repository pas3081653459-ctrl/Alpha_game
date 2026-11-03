#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AlphaCharacter.h"
#include "Combat/AlphaCombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaDirectionSprintTest,
    "AlphaGame.Movement.Authority.FacingAndDoubleTapSprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaDirectionSprintTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Settings;
    Settings.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
        .CreateAISystem(false).RequiresHitProxies(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
        true, ERHIFeatureLevel::Num, &Settings);
    if (!TestNotNull(TEXT("World exists"), World)) return false;
    struct FScope { UWorld* World; ~FScope() { World->DestroyWorld(false); } } Scope{World};
    AAlphaCharacter* Character = World->SpawnActor<AAlphaCharacter>();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Character exists"), Character) || !TestNotNull(TEXT("Controller exists"), Controller)) return false;
    Controller->Possess(Character);
    Character->DispatchBeginPlay();
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    // Isolate authority input validation from floor collision; real movement is
    // covered by the manual UE input/physics checklist, not this fixture.
    Movement->SetComponentTickEnabled(false);
    Movement->SetMovementMode(MOVE_Walking);
    Character->FaceMoveIntent(FVector(0, 1, 0));
    TestTrue(TEXT("W/S depth input can face along Y"), Character->GetActorForwardVector().Equals(FVector(0, 1, 0), .001));
    Character->FaceMoveIntent(FVector(1, 1, 0));
    TestTrue(TEXT("Diagonal facing is normalized"), Character->GetActorForwardVector().Equals(FVector(1, 1, 0).GetSafeNormal(), .001));
    UFunction* KeyIntent = Character->FindFunction(TEXT("ServerDirectionKey"));
    if (!TestNotNull(TEXT("Authority input endpoint exists"), KeyIntent)) return false;
    const auto Key = [Character, KeyIntent](int32 Index, bool Pressed)
    {
        struct FKeyIntent { int32 KeyIndex; bool bPressed; } Params{Index, Pressed};
        Character->ProcessEvent(KeyIntent, &Params);
    };
    Key(0, true);
    Key(0, false);
    World->Tick(LEVELTICK_All, .1f);
    Key(0, true);
    TestTrue(TEXT("A released double tap starts sprint"), Character->IsSprinting());
    TestEqual(TEXT("Sprint is exactly 2.5 times the class base speed"), Movement->MaxWalkSpeed, Character->GetBaseWalkSpeed() * 2.5f);
    Key(0, true);
    TestEqual(TEXT("Held-key repeats never multiply speed again"), Movement->MaxWalkSpeed, Character->GetBaseWalkSpeed() * 2.5f);
    Key(0, false);
    TestFalse(TEXT("Releasing the sprint key stops sprint"), Character->IsSprinting());
    TestEqual(TEXT("Release restores class walking speed"), Movement->MaxWalkSpeed, Character->GetBaseWalkSpeed());
    Character->SetArchetype(1);
    TestEqual(TEXT("Class changes retain their own base movement speed"), Movement->MaxWalkSpeed, 560.f);
    Character->ResetForRound();
    TestFalse(TEXT("Reset clears sprint"), Character->IsSprinting());
    return true;
}

#endif
