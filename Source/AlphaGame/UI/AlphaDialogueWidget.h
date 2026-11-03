#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AlphaDialogueWidget.generated.h"

class UTextBlock;
class UButton;

/** Local reading only. Completing a conversation submits intent to the server. */
UCLASS()
class ALPHAGAME_API UAlphaDialogueWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void OpenChapter(int32 Chapter, bool bDebrief, int32 Revision);
protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
private:
    UFUNCTION() void NextPage();
    UFUNCTION() void Dismiss();
    void UpdatePage();
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SpeakerText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DialogueText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PageText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> NextText;
    int32 ChapterIndex = 0;
    int32 PageIndex = 0;
    bool bDebriefDialogue = false;
    int32 StoryRevision = -1;
    double OpenedAt = 0.;
};
