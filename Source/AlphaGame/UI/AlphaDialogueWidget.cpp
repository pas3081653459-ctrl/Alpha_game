#include "UI/AlphaDialogueWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "World/AlphaGameState.h"
#include "World/AlphaPlayerController.h"
#include "World/AlphaStoryNPC.h"

void UAlphaDialogueWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(true);
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetMaxDesiredHeight(360.f);
    UCanvasPanelSlot* Position = Canvas->AddChildToCanvas(Size);
    Position->SetAnchors(FAnchors(.08f, .55f, .92f, .94f));
    Position->SetOffsets(FMargin(0));
    UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
    Border->SetBrushColor(FLinearColor(.025f, .045f, .09f, .98f));
    Border->SetPadding(FMargin(24.f, 16.f));
    Size->AddChild(Border);
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    Border->AddChild(Scroll);
    UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(Panel);
    const auto AddText = [this, Panel](FLinearColor Color, int32 FontSize)
    {
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = FontSize;
        Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(Color));
        Text->SetAutoWrapText(true);
        Panel->AddChildToVerticalBox(Text)->SetPadding(FMargin(0, 5));
        return Text;
    };
    SpeakerText = AddText(FLinearColor(1.f, .65f, .28f), 24);
    DialogueText = AddText(FLinearColor(.92f, .95f, 1.f), 20);
    PageText = AddText(FLinearColor(.6f, .74f, .88f), 14);
    UButton* Next = WidgetTree->ConstructWidget<UButton>();
    NextText = WidgetTree->ConstructWidget<UTextBlock>();
    NextText->SetMargin(FMargin(16, 8));
    Next->AddChild(NextText);
    Next->SetBackgroundColor(FLinearColor(.65f, .2f, .07f));
    Next->OnClicked.AddDynamic(this, &UAlphaDialogueWidget::NextPage);
    Panel->AddChildToVerticalBox(Next)->SetPadding(FMargin(0, 8));
    UButton* Cancel = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* CancelText = WidgetTree->ConstructWidget<UTextBlock>();
    CancelText->SetText(FText::FromString(TEXT("暂时离开（不推进剧情） · Esc")));
    CancelText->SetMargin(FMargin(12, 6));
    Cancel->AddChild(CancelText);
    Cancel->OnClicked.AddDynamic(this, &UAlphaDialogueWidget::Dismiss);
    Panel->AddChildToVerticalBox(Cancel);
}

void UAlphaDialogueWidget::OpenChapter(int32 Chapter, bool bDebrief, int32 Revision)
{
    ChapterIndex = FMath::Clamp(Chapter, 0, 2);
    PageIndex = 0;
    bDebriefDialogue = bDebrief;
    StoryRevision = Revision;
    OpenedAt = GetWorld()->GetTimeSeconds();
    UpdatePage();
}

void UAlphaDialogueWidget::UpdatePage()
{
    SpeakerText->SetText(AAlphaStoryNPC::GetSpeakerName(ChapterIndex));
    DialogueText->SetText(AAlphaStoryNPC::GetDialogue(ChapterIndex, PageIndex, bDebriefDialogue));
    const int32 Count = AAlphaStoryNPC::GetPageCount(ChapterIndex);
    PageText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d · Enter / 空格继续 · 对话不暂停联机世界"), PageIndex + 1, Count)));
    NextText->SetText(FText::FromString(PageIndex + 1 < Count ? TEXT("下一句  →") : TEXT("听完了 · 确认并继续")));
}

void UAlphaDialogueWidget::NextPage()
{
    if (++PageIndex < AAlphaStoryNPC::GetPageCount(ChapterIndex)) UpdatePage();
    else if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->FinishStoryDialogue(true);
}

void UAlphaDialogueWidget::Dismiss()
{
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->FinishStoryDialogue(false);
}

FReply UAlphaDialogueWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    if (KeyEvent.IsRepeat()) return FReply::Handled();
    if (KeyEvent.GetKey() == EKeys::Escape) { Dismiss(); return FReply::Handled(); }
    if (KeyEvent.GetKey() == EKeys::Enter || KeyEvent.GetKey() == EKeys::SpaceBar)
    {
        NextPage();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry, KeyEvent);
}

void UAlphaDialogueWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    // RPCs and GameState replication use different actor channels. Allow a
    // brief initial convergence window before closing an outdated view.
    if (IsVisible() && State && GetWorld()->GetTimeSeconds() - OpenedAt >= .5
        && (State->ChapterIndex != ChapterIndex || State->StoryRevision != StoryRevision
            || State->StoryStage == EAlphaStoryStage::Combat)) Dismiss();
}
