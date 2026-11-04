#include "World/AlphaPlayerController.h"
#include "World/AlphaGameMode.h"
#include "World/AlphaGameInstance.h"
#include "Characters/AlphaCharacter.h"
#include "Combat/AlphaCombatComponent.h"
#include "World/AlphaGameState.h"
#include "Coop/AlphaReviveComponent.h"
#include "UI/AlphaLobbyWidget.h"
#include "UI/AlphaDialogueWidget.h"
#include "World/AlphaStoryNPC.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Visuals/AlphaCombatCamera.h"
#include "Visuals/AlphaCombatAudio.h"
#include "Components/InputComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AAlphaPlayerController::AAlphaPlayerController()
{
    bAutoManageActiveCameraTarget = false;
}

void AAlphaPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
    FActorSpawnParameters CameraParameters;
    CameraParameters.Owner = this;
    if (AAlphaCombatCamera* Camera = GetWorld()->SpawnActor<AAlphaCombatCamera>(
        AAlphaCombatCamera::StaticClass(), FVector(0, -2800, 1200), FRotator::ZeroRotator, CameraParameters))
    {
        SetViewTarget(Camera);
    }
    SetLobbyVisible(true);
    ApplySavedPresentationSettings();
    GetWorldTimerManager().SetTimer(PreferredHeroTimer, this, &AAlphaPlayerController::ApplyPreferredHeroWhenReady, .25f, true);
}

void AAlphaPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(PreferredHeroTimer);
    Super::EndPlay(EndPlayReason);
}

void AAlphaPlayerController::ApplySavedPresentationSettings()
{
    if (!IsLocalController()) return;
    if (const UAlphaGameInstance* Instance = GetWorld()->GetGameInstance<UAlphaGameInstance>())
    {
        AlphaAudio::SetVolume(Instance->GetAudioVolume());
        CameraShakeStrength(Instance->GetCameraShake());
    }
}

void AAlphaPlayerController::SetPresentationSettings(float Volume, float Shake)
{
    if (!IsLocalController()) return;
    if (UAlphaGameInstance* Instance = GetWorld()->GetGameInstance<UAlphaGameInstance>())
        if (Instance->SetPresentationSettings(Volume,Shake)) ApplySavedPresentationSettings();
}

void AAlphaPlayerController::ApplyPreferredHeroWhenReady()
{
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetPawn());
    if (!State || !Character) return;
    GetWorldTimerManager().ClearTimer(PreferredHeroTimer);
    if (State->RoundState != EAlphaRoundState::Waiting || bSubmittedHeroSelection) return;
    if (const UAlphaGameInstance* Instance = GetWorld()->GetGameInstance<UAlphaGameInstance>())
        if (Character->GetArchetype() != Instance->GetPreferredHero()) SelectHero(Instance->GetPreferredHero());
}

bool AAlphaPlayerController::ReplayChapter(int32 Chapter)
{
    if (!IsLocalController()) return false;
    AAlphaGameMode* Mode = GetWorld()->GetAuthGameMode<AAlphaGameMode>();
    if (!Mode || !Mode->ReplayChapter(Chapter)) return false;
    FinishStoryDialogue(false);
    SetLobbyVisible(false);
    return true;
}

void AAlphaPlayerController::ClientRecordChapterComplete_Implementation(int32 Chapter)
{
    if (!IsLocalController()) return;
    if (UAlphaGameInstance* Instance = GetWorld()->GetGameInstance<UAlphaGameInstance>())
        if (Instance->RecordChapterComplete(Chapter)) Instance->SetPreferredChapter(FMath::Min(Chapter+1,2));
}

void AAlphaPlayerController::ClientRememberChapter_Implementation(int32 Chapter)
{
    if (!IsLocalController()) return;
    if (UAlphaGameInstance* Instance = GetWorld()->GetGameInstance<UAlphaGameInstance>())
        if (Chapter <= Instance->GetHighestUnlockedChapter()) Instance->SetPreferredChapter(Chapter);
}

void AAlphaPlayerController::CameraShakeStrength(float Strength)
{
    if (AAlphaCombatCamera* Camera = Cast<AAlphaCombatCamera>(GetViewTarget())) Camera->SetShakeStrength(Strength);
}

void AAlphaPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction(TEXT("ToggleMenu"), IE_Pressed, this, &AAlphaPlayerController::ToggleLobby);
    InputComponent->BindAction(TEXT("InteractStory"), IE_Pressed, this, &AAlphaPlayerController::InteractStory);
}

void AAlphaPlayerController::ToggleLobby()
{
    if (DialogueWidget && DialogueWidget->IsVisible()) FinishStoryDialogue(false);
    SetLobbyVisible(!LobbyWidget || !LobbyWidget->IsVisible());
}

void AAlphaPlayerController::SetLobbyVisible(bool bVisible)
{
    if (!IsLocalController()) return;
    if (bVisible && DialogueWidget && DialogueWidget->IsVisible()) FinishStoryDialogue(false);
    if (!LobbyWidget && bVisible)
    {
        LobbyWidget = CreateWidget<UAlphaLobbyWidget>(this, UAlphaLobbyWidget::StaticClass());
        if (LobbyWidget) LobbyWidget->AddToViewport(10);
    }
    if (LobbyWidget)
    {
        if (bVisible) LobbyWidget->PrepareForDisplay();
        LobbyWidget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    UpdateUIInputMode();
}

void AAlphaPlayerController::UpdateUIInputMode()
{
    const bool bDialogue = DialogueWidget && DialogueWidget->IsVisible();
    const bool bVisible = bDialogue || (LobbyWidget && LobbyWidget->IsVisible());
    if (bVisible)
    {
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        if (bDialogue) Mode.SetWidgetToFocus(DialogueWidget->TakeWidget());
        else if (LobbyWidget) Mode.SetWidgetToFocus(LobbyWidget->TakeWidget());
        SetInputMode(Mode);
    }
    else SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = bVisible;
    ResetIgnoreMoveInput();
    ResetIgnoreLookInput();
    if (bVisible)
    {
        SetIgnoreMoveInput(true);
        SetIgnoreLookInput(true);
        if (AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetPawn()))
        {
            Character->StopSprint();
            Character->GetReviveComponent()->EndRevive();
            Character->GetCharacterMovement()->StopMovementImmediately();
        }
    }
}

void AAlphaPlayerController::ContinueChapter()
{
    if (AAlphaGameMode* Mode = GetWorld()->GetAuthGameMode<AAlphaGameMode>())
    {
        if (Mode->AdvanceChapter()) SetLobbyVisible(false);
        else ClientMessage(TEXT("Finish the current chapter conversation before continuing."));
    }
    else ClientMessage(TEXT("Only the host can continue to the next chapter."));
}

void AAlphaPlayerController::InteractStory()
{
    if (!IsLocalController() || (LobbyWidget && LobbyWidget->IsVisible())
        || (DialogueWidget && DialogueWidget->IsVisible())) return;
    ServerInteractStory();
}

void AAlphaPlayerController::ServerInteractStory_Implementation()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastStoryRequestAt < .35) return;
    LastStoryRequestAt = Now;
    AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetPawn());
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    if (!Character || !State || State->StoryStage == EAlphaStoryStage::Combat
        || State->RoundState == EAlphaRoundState::Defeat) return;
    for (TActorIterator<AAlphaStoryNPC> It(GetWorld()); It; ++It)
    {
        if (It->CanTalk(Character))
        {
            ServerStoryChapter = State->ChapterIndex;
            ServerStoryStage = static_cast<uint8>(State->StoryStage);
            ServerStoryRevision = State->StoryRevision;
            Character->GetCombatComponent()->SetActionLockFor(.15f);
            Character->GetCharacterMovement()->StopMovementImmediately();
            ClientOpenStoryDialogue(State->ChapterIndex, State->StoryStage == EAlphaStoryStage::Debrief
                || State->StoryStage == EAlphaStoryStage::Complete, State->StoryRevision);
            return;
        }
    }
    ClientMessage(TEXT("Move closer to the chapter NPC and leave a clear line of sight."));
}

void AAlphaPlayerController::ClientOpenStoryDialogue_Implementation(int32 Chapter, bool bDebrief, int32 Revision)
{
    if (!IsLocalController()) return;
    if (!DialogueWidget)
    {
        DialogueWidget = CreateWidget<UAlphaDialogueWidget>(this, UAlphaDialogueWidget::StaticClass());
        if (DialogueWidget) DialogueWidget->AddToViewport(20);
    }
    if (!DialogueWidget) return;
    VisibleStoryChapter = Chapter;
    if (LobbyWidget) LobbyWidget->SetVisibility(ESlateVisibility::Collapsed);
    DialogueWidget->OpenChapter(Chapter, bDebrief, Revision);
    DialogueWidget->SetVisibility(ESlateVisibility::Visible);
    UpdateUIInputMode();
    DialogueWidget->SetUserFocus(this);
}

void AAlphaPlayerController::FinishStoryDialogue(bool bAdvance)
{
    if (!IsLocalController() || VisibleStoryChapter < 0) return;
    ServerFinishStoryDialogue(VisibleStoryChapter, bAdvance);
    HideStoryDialogue();
}

void AAlphaPlayerController::HideStoryDialogue()
{
    VisibleStoryChapter = -1;
    if (DialogueWidget) DialogueWidget->SetVisibility(ESlateVisibility::Collapsed);
    UpdateUIInputMode();
}

void AAlphaPlayerController::ServerFinishStoryDialogue_Implementation(int32 Chapter, bool bAdvance)
{
    const int32 ExpectedChapter = ServerStoryChapter;
    ServerStoryChapter = -1; // Each authorized conversation can complete only once.
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    if (!bAdvance || !State || Chapter != ExpectedChapter || Chapter != State->ChapterIndex
        || State->StoryRevision != ServerStoryRevision
        || static_cast<uint8>(State->StoryStage) != ServerStoryStage) return;
    if (AAlphaGameMode* Mode = GetWorld()->GetAuthGameMode<AAlphaGameMode>())
        Mode->TalkToStoryNPC(Cast<AAlphaCharacter>(GetPawn()));
}

void AAlphaPlayerController::StartRun()
{
    if (AAlphaGameMode* Mode = GetWorld()->GetAuthGameMode<AAlphaGameMode>())
    {
        if (Mode->StartPrototypeRun()) SetLobbyVisible(false);
        else ClientMessage(TEXT("Talk to the chapter guide with F and confirm the mission before starting."));
    }
    else ClientMessage(TEXT("Only the host can start the run."));
}

void AAlphaPlayerController::SelectHero(int32 HeroIndex)
{
    if (IsLocalController())
    {
        bSubmittedHeroSelection = true;
        ServerSelectHero(HeroIndex);
    }
}

void AAlphaPlayerController::ServerSelectHero_Implementation(int32 HeroIndex)
{
    AAlphaGameMode* Mode = GetWorld()->GetAuthGameMode<AAlphaGameMode>();
    AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetPawn());
    if (!Mode || !Character || HeroIndex < 0 || HeroIndex > 2 || !Mode->IsSelectionAllowed())
    {
        ClientMessage(TEXT("Choose a hero only during preparation or after the run ends."));
        return;
    }
    Character->SetArchetype(HeroIndex);
    Mode->RefreshCompanions();
    ClientHeroSelectionAccepted(HeroIndex);
}

void AAlphaPlayerController::ClientHeroSelectionAccepted_Implementation(int32 Hero)
{
    if (!IsLocalController()) return;
    if (UAlphaGameInstance* Instance = GetWorld()->GetGameInstance<UAlphaGameInstance>()) Instance->SetPreferredHero(Hero);
}

void AAlphaPlayerController::SetBotFill(int32 Enabled)
{
    AAlphaGameMode* Mode = GetWorld()->GetAuthGameMode<AAlphaGameMode>();
    if (!Mode)
    {
        ClientMessage(TEXT("Only the host can change AI teammate fill."));
        return;
    }
    if ((Enabled != 0 && Enabled != 1) || !Mode->SetCompanionFillEnabled(Enabled == 1))
        ClientMessage(TEXT("AI fill accepts 0 or 1 and can only be changed during preparation."));
}

void AAlphaPlayerController::TestDownSelf()
{
    if (IsLocalController()) ServerTestDownSelf();
}

void AAlphaPlayerController::ServerTestDownSelf_Implementation()
{
#if !UE_BUILD_SHIPPING
    AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetPawn());
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    if (!Character || !State || (State->RoundState != EAlphaRoundState::Fighting
        && State->RoundState != EAlphaRoundState::Intermission))
    {
        ClientMessage(TEXT("Start a run before testing rescue."));
        return;
    }
    UAlphaCombatComponent* Combat = Character->GetCombatComponent();
    if (!Combat->IsAlive() || Combat->IsInvulnerable())
    {
        ClientMessage(TEXT("Wait until alive and no longer protected, then retry TestDownSelf."));
        return;
    }
    Combat->ReceiveHit(Combat->GetMaxHealth() + 1.f, 0.f, nullptr);
#else
    ClientMessage(TEXT("TestDownSelf is available in development builds only."));
#endif
}

void AAlphaPlayerController::HostGame()
{
    if (!IsLocalController()) return;
    if (UAlphaGameInstance* Instance = Cast<UAlphaGameInstance>(GetWorld()->GetGameInstance())) Instance->BeginConnectionAttempt();
    UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, TEXT("listen"));
}

bool AAlphaPlayerController::IsValidLANAddress(const FString& Address)
{
    // Only a literal IPv4 address with an optional numeric port is accepted.
    // Reject map options, command separators and arbitrary travel URLs.
    FString Host = Address;
    FString Port;
    if (Address.Split(TEXT(":"), &Host, &Port))
    {
        if (Port.IsEmpty() || Port.Len() > 5) return false;
        for (TCHAR Character : Port) if (Character < '0' || Character > '9') return false;
        const int32 PortValue = FCString::Atoi(*Port);
        if (PortValue < 1 || PortValue > 65535) return false;
    }
    TArray<FString> Octets;
    Host.ParseIntoArray(Octets, TEXT("."), false);
    if (Octets.Num() != 4) return false;
    for (const FString& Octet : Octets)
    {
        if (Octet.IsEmpty() || Octet.Len() > 3) return false;
        for (TCHAR Character : Octet) if (Character < '0' || Character > '9') return false;
        if (FCString::Atoi(*Octet) > 255) return false;
    }
    return true;
}

void AAlphaPlayerController::JoinGame(const FString& Address)
{
    if (!IsLocalController()) return;
    if (!IsValidLANAddress(Address))
    {
        ClientMessage(TEXT("JoinGame expects an IPv4 address, e.g. 192.168.1.10:7777"));
        return;
    }
    if (UAlphaGameInstance* Instance = Cast<UAlphaGameInstance>(GetWorld()->GetGameInstance())) Instance->BeginConnectionAttempt();
    ClientTravel(Address, TRAVEL_Absolute);
}

void AAlphaPlayerController::RestartRound()
{
    if (AAlphaGameMode* Mode = GetWorld()->GetAuthGameMode<AAlphaGameMode>())
    {
        Mode->RestartPrototypeRound();
        SetLobbyVisible(true);
    }
    else
    {
        ClientMessage(TEXT("Only the host can restart the round."));
    }
}
