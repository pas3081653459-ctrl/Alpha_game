#include "UI/AlphaLobbyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Characters/AlphaCharacter.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "World/AlphaGameInstance.h"
#include "World/AlphaGameMode.h"
#include "World/AlphaGameState.h"
#include "World/AlphaPlayerController.h"

namespace AlphaLobbyStyle
{
const FLinearColor Ink(.018f, .035f, .075f, .97f);
const FLinearColor Panel(.035f, .065f, .12f, .98f);
const FLinearColor Accent(1.f, .32f, .1f);
const FLinearColor Text(.92f, .95f, 1.f);
const FLinearColor Muted(.62f, .73f, .85f);
}

UTextBlock* UAlphaLobbyWidget::AddText(UVerticalBox* Panel, const FString& Label, int32 FontSize, FLinearColor Color)
{
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
    Text->SetText(FText::FromString(Label));
    Text->SetAutoWrapText(true);
    Text->SetColorAndOpacity(FSlateColor(Color));
    FSlateFontInfo Font = Text->GetFont();
    Font.Size = FontSize;
    Text->SetFont(Font);
    if (UVerticalBoxSlot* Slot = Panel->AddChildToVerticalBox(Text)) Slot->SetPadding(FMargin(0, 0, 0, 10));
    return Text;
}

UButton* UAlphaLobbyWidget::AddButton(UVerticalBox* Panel, const FString& Label)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
    Text->SetText(FText::FromString(Label));
    Text->SetAutoWrapText(true);
    Text->SetColorAndOpacity(FSlateColor(AlphaLobbyStyle::Text));
    Text->SetMargin(FMargin(14, 10));
    FSlateFontInfo Font = Text->GetFont();
    Font.Size = 15;
    Text->SetFont(Font);
    Button->AddChild(Text);
    Button->SetBackgroundColor(FLinearColor(.075f, .13f, .22f));
    if (UVerticalBoxSlot* Slot = Panel->AddChildToVerticalBox(Button)) Slot->SetPadding(FMargin(0, 3, 0, 5));
    return Button;
}

void UAlphaLobbyWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget) return;
    Backdrop = WidgetTree->ConstructWidget<UBorder>();
    Backdrop->SetBrushColor(AlphaLobbyStyle::Ink);
    Backdrop->SetPadding(FMargin(32, 24));
    WidgetTree->RootWidget = Backdrop;
    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
    Backdrop->AddChild(Root);
    AddText(Root, TEXT("烬潮：归炉"), 32, AlphaLobbyStyle::Accent);
    AddText(Root, TEXT("ASH TIDE  /  失去记忆，不能免除责任。"), 13, AlphaLobbyStyle::Muted);
    StatusText = AddText(Root, TEXT("正在准备旅程……"), 13, AlphaLobbyStyle::Text);

    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
    UVerticalBoxSlot* ScrollSlot = Root->AddChildToVerticalBox(Scroll);
    ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    LayoutFrame = WidgetTree->ConstructWidget<USizeBox>();
    Scroll->AddChild(LayoutFrame);

    StoryCard = WidgetTree->ConstructWidget<UBorder>();
    StoryCard->SetBrushColor(AlphaLobbyStyle::Panel);
    StoryCard->SetPadding(FMargin(22));
    UVerticalBox* Story = WidgetTree->ConstructWidget<UVerticalBox>();
    StoryCard->AddChild(Story);
    AddText(Story, TEXT("此刻，重返归炉城"), 13, AlphaLobbyStyle::Accent);
    ChapterText = AddText(Story, TEXT("第一章 · 灰渡码头"), 25, AlphaLobbyStyle::Text);
    StoryText = AddText(Story, TEXT(""), 17, AlphaLobbyStyle::Text);
    ObjectiveText = AddText(Story, TEXT(""), 16, FLinearColor(1.f, .78f, .35f));
    ExploreButton = AddButton(Story, TEXT("进入场景 · 寻找 NPC [F]"));
    ExploreButton->SetBackgroundColor(FLinearColor(.7f, .19f, .055f));
    ExploreButton->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::Close);
    StartButton = AddButton(Story, TEXT("开始本章战斗"));
    StartButton->SetBackgroundColor(FLinearColor(.7f, .19f, .055f));
    StartButton->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::Start);
    ContinueButton = AddButton(Story, TEXT("前往下一章"));
    ContinueButton->SetBackgroundColor(FLinearColor(.7f, .19f, .055f));
    ContinueButton->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::Continue);
    AddButton(Story, TEXT("章节重玩 · 展开 / 收起"))->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::ToggleReplay);
    ReplayPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    Story->AddChildToVerticalBox(ReplayPanel);
    ReplayPanel->SetVisibility(ESlateVisibility::Collapsed);
    AddText(ReplayPanel, TEXT("完成章节后解锁下一章。房主可在准备或结算时带全队重玩，从本章开头重新接任务；战斗中不能切换。"), 13, AlphaLobbyStyle::Muted);
    ResumeButton = AddButton(ReplayPanel, TEXT("继续上次章节 · 从章节开头"));
    ResumeButton->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::ResumeSavedChapter);
    ReplayButtons.Add(AddButton(ReplayPanel, TEXT("第一章 · 灰渡码头")));
    ReplayButtons.Last()->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::ReplayDock);
    ReplayButtons.Add(AddButton(ReplayPanel, TEXT("第二章 · 百炉工坊")));
    ReplayButtons.Last()->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::ReplayForge);
    ReplayButtons.Add(AddButton(ReplayPanel, TEXT("第三章 · 悬灯水道")));
    ReplayButtons.Last()->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::ReplayCanal);
    AddText(Story, TEXT("一个人也能开始\n默认补齐 AI 队友；进入场景后，靠近固定 NPC 按 F 阅读故事。"), 14, AlphaLobbyStyle::Muted);
    AddText(Story, TEXT("WASD 移动 / 双击奔跑\nJ 五段连招 · K 跳跃 · L 翻滚\nU / I / O 技能 · 按住 E 救援\nF 交谈 · Tab 打开或关闭此面板"), 13, AlphaLobbyStyle::Muted);

    PartyCard = WidgetTree->ConstructWidget<UBorder>();
    PartyCard->SetBrushColor(AlphaLobbyStyle::Panel);
    PartyCard->SetPadding(FMargin(22));
    UVerticalBox* Party = WidgetTree->ConstructWidget<UVerticalBox>();
    PartyCard->AddChild(Party);
    AddText(Party, TEXT("选择同行者"), 23, AlphaLobbyStyle::Text);
    AddText(Party, TEXT("三种职业 · 1—3 人合作 · 选择在准备与结算时生效"), 13, AlphaLobbyStyle::Muted);
    HeroButtons.Add(AddButton(Party, TEXT("沈砚  /  长刀\n正面牵制与破势 · 短时防护、重斩与前方爆发")));
    HeroButtons.Last()->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::HeroBlade);
    HeroButtons.Add(AddButton(Party, TEXT("阿绫  /  机关双刃\n机动与空战 · 突进、挑空与范围追击")));
    HeroButtons.Last()->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::HeroWeaver);
    HeroButtons.Add(AddButton(Party, TEXT("闻灯  /  灯杖\n聚怪与控制 · 牵引、破势与灯域爆发")));
    HeroButtons.Last()->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::HeroLantern);
    CompanionButton = AddButton(Party, TEXT("AI 队友：开启 · 自动补齐三人"));
    CompanionButton->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::ToggleCompanions);

    AddButton(Party, TEXT("声音与镜头 · 展开 / 收起"))->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::ToggleSettings);
    SettingsPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    Party->AddChildToVerticalBox(SettingsPanel);
    SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
    AddText(SettingsPanel, TEXT("设置仅影响本机。拖动后点击应用；音量或镜头震动设为 0 即关闭。"), 13, AlphaLobbyStyle::Muted);
    VolumeText = AddText(SettingsPanel, TEXT("战斗音量"), 15, AlphaLobbyStyle::Text);
    VolumeSlider = WidgetTree->ConstructWidget<USlider>();
    VolumeSlider->SetMinValue(0.f);
    VolumeSlider->SetMaxValue(1.f);
    VolumeSlider->SetStepSize(.05f);
    VolumeSlider->OnValueChanged.AddDynamic(this, &UAlphaLobbyWidget::SettingsValueChanged);
    SettingsPanel->AddChildToVerticalBox(VolumeSlider)->SetPadding(FMargin(0, 0, 0, 12));
    ShakeText = AddText(SettingsPanel, TEXT("镜头震动"), 15, AlphaLobbyStyle::Text);
    ShakeSlider = WidgetTree->ConstructWidget<USlider>();
    ShakeSlider->SetMinValue(0.f);
    ShakeSlider->SetMaxValue(1.f);
    ShakeSlider->SetStepSize(.05f);
    ShakeSlider->OnValueChanged.AddDynamic(this, &UAlphaLobbyWidget::SettingsValueChanged);
    SettingsPanel->AddChildToVerticalBox(ShakeSlider)->SetPadding(FMargin(0, 0, 0, 12));
    ApplySettingsButton = AddButton(SettingsPanel, TEXT("应用并保存到本机"));
    ApplySettingsButton->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::ApplySettings);
    ProfileStatusText = AddText(Party, TEXT(""), 13, AlphaLobbyStyle::Muted);

    AddButton(Party, TEXT("局域网联机 · 展开 / 收起"))->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::ToggleLAN);
    LANPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    Party->AddChildToVerticalBox(LANPanel);
    LANPanel->SetVisibility(ESlateVisibility::Collapsed);
    AddText(LANPanel, TEXT("房主创建后，将本机局域网 IPv4 告诉队友。创建会重新载入场景。"), 13, AlphaLobbyStyle::Muted);
    AddButton(LANPanel, TEXT("创建局域网房间"))->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::Host);
    AddressBox = WidgetTree->ConstructWidget<UEditableTextBox>();
    AddressBox->SetHintText(FText::FromString(TEXT("房主 IPv4，例如 192.168.1.10:7777")));
    if (UVerticalBoxSlot* Slot = LANPanel->AddChildToVerticalBox(AddressBox)) Slot->SetPadding(FMargin(0, 6));
    AddButton(LANPanel, TEXT("加入此地址"))->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::Join);
    ErrorText = AddText(Party, TEXT(""), 13, FLinearColor(1.f, .5f, .35f));
    RestartButton = AddButton(Party, TEXT("重新开始当前章节 · 房主 / 单人"));
    RestartButton->OnClicked.AddDynamic(this, &UAlphaLobbyWidget::Restart);
    LoadPresentationSettings();
    UpdateResponsiveLayout(1000.f);
}

void UAlphaLobbyWidget::UpdateResponsiveLayout(float Width)
{
    if (!LayoutFrame || !StoryCard || !PartyCard) return;
    const bool bCompact = Width < 900.f;
    if (bHasLayout && bCompactLayout == bCompact) return;
    bHasLayout = true;
    bCompactLayout = bCompact;
    StoryCard->RemoveFromParent();
    PartyCard->RemoveFromParent();
    LayoutFrame->ClearChildren();
    Backdrop->SetPadding(bCompact ? FMargin(16, 16) : FMargin(32, 24));
    if (bCompact)
    {
        UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
        LayoutFrame->AddChild(Column);
        Column->AddChildToVerticalBox(StoryCard)->SetPadding(FMargin(0, 0, 0, 12));
        Column->AddChildToVerticalBox(PartyCard);
    }
    else
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        LayoutFrame->AddChild(Row);
        UHorizontalBoxSlot* StorySlot = Row->AddChildToHorizontalBox(StoryCard);
        StorySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        StorySlot->SetPadding(FMargin(0, 0, 8, 0));
        UHorizontalBoxSlot* PartySlot = Row->AddChildToHorizontalBox(PartyCard);
        PartySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        PartySlot->SetPadding(FMargin(8, 0, 0, 0));
    }
}

void UAlphaLobbyWidget::PrepareForDisplay()
{
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    bObservedPreparation = State && State->RoundState == EAlphaRoundState::Waiting;
    LoadPresentationSettings();
}

void UAlphaLobbyWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    if (!StatusText) return;
    UpdateResponsiveLayout(Geometry.GetLocalSize().X);
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    const AAlphaCharacter* Character = Cast<AAlphaCharacter>(GetOwningPlayerPawn());
    const bool bCanHostControl = GetWorld()->GetAuthGameMode<AAlphaGameMode>() != nullptr;
    const bool bWaiting = State && State->RoundState == EAlphaRoundState::Waiting;
    const bool bReady = State && State->StoryStage == EAlphaStoryStage::Ready;
    const bool bComplete = State && State->StoryStage == EAlphaStoryStage::Complete;
    const int32 Chapter = State ? FMath::Clamp(State->ChapterIndex, 0, 2) : 0;
    const int32 Hero = Character ? Character->GetArchetype() : -1;
    static const TCHAR* ChapterNames[] = {TEXT("第一章 · 灰渡码头"), TEXT("第二章 · 百炉工坊"), TEXT("第三章 · 悬灯水道")};
    static const TCHAR* ChapterStories[] = {
        TEXT("归炉城的灯，燃烧着人的记忆。\n灾变后，灰渡码头只剩最后一条回城的船。摆渡人渡伯知道封锁线后的路。"),
        TEXT("锻锤仍在落下，记忆却已无人归还。\n记录员穗守着工坊账册。铸炉官身后的命令，或许比怪物更值得追查。"),
        TEXT("穿过炉火，灯影照出了另一种真相。\n守灯人澄仍在为居民点灯。炉心必须改变，但城中的人也不能被留在黑暗里。")
    };
    ChapterText->SetText(FText::FromString(ChapterNames[Chapter]));
    StoryText->SetText(FText::FromString(ChapterStories[Chapter]));
    FString Objective = TEXT("正在同步章节信息……");
    if (State)
    {
        switch (State->StoryStage)
        {
        case EAlphaStoryStage::Arrival: Objective = TEXT("当前目标：进入场景，靠近 NPC 按 F 交谈，了解本章任务。"); break;
        case EAlphaStoryStage::Ready: Objective = bCanHostControl ? TEXT("任务已确认。准备好后，开始本章战斗。") : TEXT("任务已确认，等待房主开始战斗。"); break;
        case EAlphaStoryStage::Combat: Objective = TEXT("当前目标：清理敌人，留意地面预警；倒地队友可以被救援。"); break;
        case EAlphaStoryStage::Debrief: Objective = TEXT("战斗已结束。返回 NPC 身边按 F，听取后续线索。"); break;
        case EAlphaStoryStage::Complete: Objective = Chapter < 2 ? TEXT("本章故事已完成。房主可以带领全队前往下一章。") : TEXT("本次旅程完成。灯还未熄灭，归炉城的故事仍将继续。"); break;
        }
        if (State->RoundState == EAlphaRoundState::Defeat) Objective = TEXT("全队倒地。本章进度可以由房主重新开始。");
    }
    ObjectiveText->SetText(FText::FromString(Objective));
    StatusText->SetText(FText::FromString(FString::Printf(TEXT("%s  ·  %d 位玩家 + %d 位 AI  /  3 人队伍"),
        GetWorld()->GetNetMode() == NM_Client ? TEXT("局域网队员") : (GetWorld()->GetNetMode() == NM_Standalone ? TEXT("单人旅程") : TEXT("局域网房主")),
        State ? State->HumanPlayerCount : 0, State ? State->CompanionCount : 0)));
    StartButton->SetVisibility(bReady ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    StartButton->SetIsEnabled(bCanHostControl && bWaiting && bReady);
    ContinueButton->SetVisibility(bComplete && Chapter < 2 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ContinueButton->SetIsEnabled(bCanHostControl && bComplete && Chapter < 2);
    RestartButton->SetIsEnabled(bCanHostControl);
    CompanionButton->SetIsEnabled(bCanHostControl && bWaiting);
    if (UTextBlock* Caption = Cast<UTextBlock>(ExploreButton->GetContent()))
        Caption->SetText(FText::FromString(State && State->StoryStage == EAlphaStoryStage::Arrival
            ? TEXT("进入场景 · 寻找 NPC [F]") : TEXT("返回场景 · Tab 再次打开")));
    if (UTextBlock* Caption = Cast<UTextBlock>(CompanionButton->GetContent()))
        Caption->SetText(FText::FromString(State && State->bCompanionFillEnabled
            ? TEXT("AI 队友：开启 · 点击切换为纯单人") : TEXT("AI 队友：关闭 · 点击自动补齐队伍")));
    const bool bCanSelect = State && (bWaiting || State->RoundState == EAlphaRoundState::Victory || State->RoundState == EAlphaRoundState::Defeat);
    for (int32 Index = 0; Index < HeroButtons.Num(); ++Index)
    {
        HeroButtons[Index]->SetIsEnabled(bCanSelect);
        HeroButtons[Index]->SetBackgroundColor(Index == Hero ? FLinearColor(.65f, .19f, .065f) : FLinearColor(.075f, .13f, .22f));
    }
    const UAlphaGameInstance* Instance = Cast<UAlphaGameInstance>(GetWorld()->GetGameInstance());
    const int32 Unlocked = Instance ? FMath::Clamp(Instance->GetHighestUnlockedChapter(), 0, 2) : 0;
    for (int32 Index = 0; Index < ReplayButtons.Num(); ++Index)
    {
        const bool bUnlocked = Index <= Unlocked;
        ReplayButtons[Index]->SetIsEnabled(Instance && bCanHostControl && bCanSelect && bUnlocked);
        ReplayButtons[Index]->SetBackgroundColor(Index == Chapter ? FLinearColor(.65f, .19f, .065f) : FLinearColor(.075f, .13f, .22f));
        if (UTextBlock* Caption = Cast<UTextBlock>(ReplayButtons[Index]->GetContent()))
            Caption->SetText(FText::FromString(FString::Printf(TEXT("%s%s"), ChapterNames[Index],
                bUnlocked ? TEXT(" · 重玩") : TEXT(" · 尚未解锁"))));
    }
    ApplySettingsButton->SetIsEnabled(Instance && bSettingsDirty);
    ResumeButton->SetIsEnabled(Instance && bCanHostControl && bCanSelect);
    if (Instance)
        if (UTextBlock* Caption = Cast<UTextBlock>(ResumeButton->GetContent()))
            Caption->SetText(FText::FromString(FString::Printf(TEXT("继续旅程 · %s（从本章开头）"), ChapterNames[FMath::Clamp(Instance->GetLastChapter(),0,2)])));
    const FString ProfileError = Instance ? Instance->GetProfileStatusError() : TEXT("本地设置尚未就绪。");
    ProfileStatusText->SetText(FText::FromString(ProfileError.IsEmpty()
        ? (bSettingsDirty ? TEXT("有未应用的声音 / 镜头设置。") : TEXT("本地设置已载入；章节解锁与设置保存在本机。"))
        : ProfileError));
    ProfileStatusText->SetColorAndOpacity(FSlateColor(ProfileError.IsEmpty()
        ? AlphaLobbyStyle::Muted : FLinearColor(1.f, .5f, .35f)));
    FString Error = LocalError;
    if (Error.IsEmpty())
    {
        if (Instance) Error = Instance->LastConnectionError;
    }
    ErrorText->SetText(FText::FromString(Error));
    ErrorText->SetVisibility(Error.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (bWaiting) bObservedPreparation = true;
    if (bObservedPreparation && State && State->RoundState != EAlphaRoundState::Waiting)
    {
        bObservedPreparation = false;
        Close();
    }
}

void UAlphaLobbyWidget::Host()
{
    LocalError.Reset();
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->HostGame();
}
void UAlphaLobbyWidget::Join()
{
    const FString Address = AddressBox->GetText().ToString().TrimStartAndEnd();
    if (Address.IsEmpty()) { LocalError = TEXT("请填写房主的局域网 IPv4 地址。"); return; }
    if (!AAlphaPlayerController::IsValidLANAddress(Address))
    {
        LocalError = TEXT("地址格式不正确。请输入 IPv4，可加端口，例如 192.168.1.10:7777。");
        return;
    }
    LocalError.Reset();
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->JoinGame(Address);
}
void UAlphaLobbyWidget::Start()
{
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->StartRun();
}
void UAlphaLobbyWidget::Continue()
{
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->ContinueChapter();
}
void UAlphaLobbyWidget::Restart()
{
    if (AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer())) PC->RestartRound();
}
void UAlphaLobbyWidget::ToggleLAN()
{
    bLANVisible = !bLANVisible;
    LANPanel->SetVisibility(bLANVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UAlphaLobbyWidget::LoadPresentationSettings()
{
    if (!VolumeSlider || !ShakeSlider) return;
    const UAlphaGameInstance* Instance = GetWorld() ? Cast<UAlphaGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
    VolumeSlider->SetValue(Instance ? FMath::Clamp(Instance->GetAudioVolume(), 0.f, 1.f) : .75f);
    ShakeSlider->SetValue(Instance ? FMath::Clamp(Instance->GetCameraShake(), 0.f, 1.f) : .35f);
    bSettingsDirty = false;
    RefreshSettingsLabels();
}

void UAlphaLobbyWidget::RefreshSettingsLabels()
{
    if (!VolumeSlider || !ShakeSlider || !VolumeText || !ShakeText) return;
    VolumeText->SetText(FText::FromString(FString::Printf(TEXT("战斗音量  %d%%"), FMath::RoundToInt(VolumeSlider->GetValue() * 100.f))));
    ShakeText->SetText(FText::FromString(FString::Printf(TEXT("镜头震动  %d%%"), FMath::RoundToInt(ShakeSlider->GetValue() * 100.f))));
}

void UAlphaLobbyWidget::SettingsValueChanged(float Value)
{
    (void)Value;
    bSettingsDirty = true;
    RefreshSettingsLabels();
}

void UAlphaLobbyWidget::ApplySettings()
{
    AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer());
    if (!PC || !VolumeSlider || !ShakeSlider) return;
    PC->SetPresentationSettings(VolumeSlider->GetValue(), ShakeSlider->GetValue());
    const UAlphaGameInstance* Instance = Cast<UAlphaGameInstance>(GetWorld()->GetGameInstance());
    bSettingsDirty = !Instance || !Instance->GetProfileStatusError().IsEmpty();
}

void UAlphaLobbyWidget::ToggleSettings()
{
    bSettingsVisible = !bSettingsVisible;
    SettingsPanel->SetVisibility(bSettingsVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UAlphaLobbyWidget::ToggleReplay()
{
    bReplayVisible = !bReplayVisible;
    ReplayPanel->SetVisibility(bReplayVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UAlphaLobbyWidget::Replay(int32 Chapter)
{
    AAlphaPlayerController* PC = Cast<AAlphaPlayerController>(GetOwningPlayer());
    if (!PC || !PC->ReplayChapter(Chapter))
    {
        LocalError = TEXT("仅房主 / 单人可以在准备或结算时重玩已解锁章节。");
        return;
    }
    LocalError.Reset();
}

void UAlphaLobbyWidget::ReplayDock() { Replay(0); }
void UAlphaLobbyWidget::ReplayForge() { Replay(1); }
void UAlphaLobbyWidget::ReplayCanal() { Replay(2); }

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

void UAlphaLobbyWidget::ResumeSavedChapter()
{
    if (const UAlphaGameInstance* Instance = GetWorld()->GetGameInstance<UAlphaGameInstance>()) Replay(Instance->GetLastChapter());
}
