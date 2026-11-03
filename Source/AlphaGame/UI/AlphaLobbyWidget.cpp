#include "UI/AlphaLobbyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Characters/AlphaCharacter.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "World/AlphaGameMode.h"
#include "World/AlphaGameState.h"
#include "World/AlphaPlayerController.h"

UButton* UAlphaLobbyWidget::AddButton(UVerticalBox* Panel, const FString& Label)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
    Text->SetText(FText::FromString(Label));
    Text->SetColorAndOpacity(FSlateColor(FLinearColor(.88f, .83f, .68f)));
    Text->SetMargin(FMargin(12, 7));
    Button->AddChild(Text);
    Button->SetBackgroundColor(FLinearColor(.075f, .14f, .16f, 1.f));
    if (UVerticalBoxSlot* Slot = Panel->AddChildToVerticalBox(Button)) Slot->SetPadding(FMargin(0, 3));
    return Button;
}

void UAlphaLobbyWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget) return;
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(440.f);
    UCanvasPanelSlot* Position = Canvas->AddChildToCanvas(Size);
    Position->SetAnchors(FAnchors(.5f, .5f));
    Position->SetAlignment(FVector2D(.5f, .5f));
    Position->SetAutoSize(true);
    UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
    Border->SetBrushColor(FLinearColor(.025f, .04f, .055f, .97f));
    Border->SetPadding(FMargin(20));
    Size->AddChild(Border);
    UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
    Border->AddChild(Panel);
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(FText::FromString(TEXT("ASH TIDE | Three-player LAN")));
    Title->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, .7f, .25f)));
    Panel->AddChildToVerticalBox(Title);
    StatusText = WidgetTree->ConstructWidget<UTextBlock>();
    StatusText->SetAutoWrapText(true);
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(.62f, .75f, .77f)));
    Panel->AddChildToVerticalBox(StatusText);
    AddButton(Panel, TEXT("Create LAN room (resets current arena)"))->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::Host);
    AddressBox = WidgetTree->ConstructWidget<UEditableTextBox>();
    AddressBox->SetHintText(FText::FromString(TEXT("Host IPv4, e.g. 192.168.1.10:7777")));
    Panel->AddChildToVerticalBox(AddressBox);
    AddButton(Panel, TEXT("Join address"))->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::Join);
    AddButton(Panel, TEXT("Hero 1 | Shen Yan - heavy blade"))->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::HeroBlade);
    AddButton(Panel, TEXT("Hero 2 | A Ling - agile weaver"))->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::HeroWeaver);
    AddButton(Panel, TEXT("Hero 3 | Wen Deng - lantern control"))->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::HeroLantern);
    CompanionButton = AddButton(Panel, TEXT("AI teammates: auto-fill to three"));
    CompanionButton->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::ToggleCompanions);
    StartButton = AddButton(Panel, TEXT("Start run (host / solo)"));
    StartButton->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::Start);
    RestartButton = AddButton(Panel, TEXT("Reset to preparation (host / solo)"));
    RestartButton->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::Restart);
    AddButton(Panel, TEXT("Close | Tab to reopen"))->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::Close);
}

void UAlphaLobbyWidget::PrepareForDisplay()
{
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    bObservedPreparation = State && State->RoundState == EAlphaRoundState::Waiting;
}

void UAlphaLobbyWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    if (!StatusText) return;
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    const AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetOwningPlayerPawn());
    const bool bCanHostControl = GetWorld()->GetAuthGameMode<AAlphaGameMode>() != nullptr;
    const bool bWaiting = State && State->RoundState == EAlphaRoundState::Waiting;
    StartButton->SetIsEnabled(bCanHostControl && bWaiting);
    RestartButton->SetIsEnabled(bCanHostControl);
    CompanionButton->SetIsEnabled(bCanHostControl && bWaiting);
    if (UTextBlock* Caption = Cast<UTextBlock>(CompanionButton->GetContent()))
        Caption->SetText(FText::FromString(State && State->bCompanionFillEnabled
            ? TEXT("AI teammates: ON (click for pure solo)") : TEXT("AI teammates: OFF (click to fill party)")));
    StatusText->SetText(FText::FromString(FString::Printf(
        TEXT("%s | Hero %d | %d human + %d AI\nChoose heroes, then host starts. Hold E to revive."),
        GetWorld()->GetNetMode() == NM_Client ? TEXT("Client") : TEXT("Host / solo"),
        Character ? Character->GetArchetype() + 1 : 0, State ? State->HumanPlayerCount : 0, State ? State->CompanionCount : 0)));
    if (bWaiting) bObservedPreparation = true;
    if (bObservedPreparation && State && State->RoundState != EAlphaRoundState::Waiting)
    {
        bObservedPreparation = false;
        Close();
    }
}

void UAlphaLobbyWidget::Host()
{
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->HostGame();
}
void UAlphaLobbyWidget::Join()
{
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->JoinGame(AddressBox->GetText().ToString().TrimStartAndEnd());
}
void UAlphaLobbyWidget::Start()
{
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->StartRun();
}
void UAlphaLobbyWidget::Restart()
{
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->RestartRound();
}
void UAlphaLobbyWidget::HeroBlade()
{
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->SelectHero(0);
}

void UAlphaLobbyWidget::ToggleCompanions()
{
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer()))
        if (State) PC->SetBotFill(State->bCompanionFillEnabled ? 0 : 1);
}
void UAlphaLobbyWidget::HeroWeaver()
{
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->SelectHero(1);
}
void UAlphaLobbyWidget::HeroLantern()
{
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->SelectHero(2);
}
void UAlphaLobbyWidget::Close()
{
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->SetLobbyVisible(false);
}
