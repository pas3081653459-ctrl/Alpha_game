#include "World/AlphaPlayerController.h"
#include "World/AlphaGameMode.h"
#include "World/AlphaGameInstance.h"
#include "Characters/AlphaCharacter.h"
#include "Combat/AlphaCombatComponent.h"
#include "World/AlphaGameState.h"
#include "Coop/AlphaReviveComponent.h"
#include "UI/AlphaLobbyWidget.h"
#include "Visuals/AlphaCombatCamera.h"
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
}

void AAlphaPlayerController::CameraShakeStrength(float Strength)
{
    if (AAlphaCombatCamera* Camera = Cast<AAlphaCombatCamera>(GetViewTarget())) Camera->SetShakeStrength(Strength);
}

void AAlphaPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction(TEXT("ToggleMenu"), IE_Pressed, this, &AAlphaPlayerController::ToggleLobby);
}

void AAlphaPlayerController::ToggleLobby()
{
    SetLobbyVisible(!LobbyWidget || !LobbyWidget->IsVisible());
}

void AAlphaPlayerController::SetLobbyVisible(bool bVisible)
{
    if (!IsLocalController()) return;
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
    if (bVisible)
    {
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
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
        }
    }
}

void AAlphaPlayerController::StartRun()
{
    if (AAlphaGameMode* Mode = GetWorld()->GetAuthGameMode<AAlphaGameMode>())
    {
        if (Mode->StartPrototypeRun()) SetLobbyVisible(false);
        else ClientMessage(TEXT("Prepare or restart the round before starting."));
    }
    else ClientMessage(TEXT("Only the host can start the run."));
}

void AAlphaPlayerController::SelectHero(int32 HeroIndex)
{
    if (IsLocalController()) ServerSelectHero(HeroIndex);
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
