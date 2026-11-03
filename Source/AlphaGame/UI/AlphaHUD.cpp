#include "UI/AlphaHUD.h"
#include "Abilities/AlphaAbilityComponent.h"
#include "Characters/AlphaCharacter.h"
#include "Characters/AlphaCompanion.h"
#include "Characters/Boss/AlphaForgeBoss.h"
#include "Combat/AlphaCombatComponent.h"
#include "Coop/AlphaReviveComponent.h"
#include "EngineUtils.h"
#include "World/AlphaGameState.h"
#include "World/AlphaGameInstance.h"
#include "World/AlphaStoryNPC.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void AAlphaHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !PlayerOwner || !GEngine) return;
    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const FLinearColor Panel(.018f, .035f, .045f, .88f);
    const FLinearColor Gold(.84f, .64f, .3f);
    const FLinearColor Muted(.57f, .68f, .7f);
    const float CardWidth = FMath::Max(200.f, FMath::Min(590.f, Width * .48f));
    const auto Text = [this](const FString& Label, FLinearColor Color, float X, float Y, float Scale = 1.f)
    {
        DrawText(Label, Color, X, Y, GEngine->GetSmallFont(), Scale);
    };
    const auto Card = [this, Panel, Gold](float X, float Y, float W, float H)
    {
        DrawRect(Panel, X, Y, W, H);
        DrawRect(Gold, X, Y, 3.f, H);
        DrawRect(FLinearColor(.3f, .4f, .42f, .4f), X + 3.f, Y, W - 3.f, 1.f);
    };
    const auto Bar = [this](float X, float Y, float W, float H, float Fraction, FLinearColor Color)
    {
        DrawRect(FLinearColor(.015f, .025f, .03f, .95f), X, Y, W, H);
        DrawRect(Color, X, Y, W * FMath::Clamp(Fraction, 0.f, 1.f), H);
    };

    AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    Card(16, 16, CardWidth, 91);
    const int32 Chapter = State ? FMath::Clamp(State->ChapterIndex, 0, 2) : 0;
    static const TCHAR* ChapterNames[] = {TEXT("GREY FERRY"), TEXT("HUNDRED FORGES"), TEXT("LANTERN CANAL")};
    Text(FString::Printf(TEXT("ASH TIDE / %d: %s"), Chapter + 1, ChapterNames[Chapter]), Gold, 29, 25, 1.1f);
    const TCHAR* Mode = GetNetMode() == NM_Standalone ? TEXT("SOLO") : (GetNetMode() == NM_Client ? TEXT("LAN CLIENT") : TEXT("LAN HOST"));
    Text(FString::Printf(TEXT("%s  |  %d human + %d AI / 3  |  TAB Room"), Mode,
        State ? State->HumanPlayerCount : 0, State ? State->CompanionCount : 0), Muted, 29, 50);
    FString Objective = TEXT("Preparing...");
    if (State)
    {
        switch (State->RoundState)
        {
        case EAlphaRoundState::Waiting:
            Objective = State->StoryStage == EAlphaStoryStage::Ready
                ? TEXT("Mission accepted. TAB: host starts this chapter.")
                : TEXT("Find the chapter guide. F: talk / accept mission.");
            break;
        case EAlphaRoundState::Fighting:
            Objective = State->EncounterStage == EAlphaEncounterStage::Boss ? TEXT("Defeat the Forge Warden. Watch the marked ground.")
                : FString::Printf(TEXT("WAVE %d / %d   |   %d enemies remaining"), State->Wave, State->GetChapterWaveCount(), State->RemainingEnemies);
            break;
        case EAlphaRoundState::Intermission:
            Objective = State->bAwaitingRouteAdvance ? TEXT("FOLLOW THE MARKER / Regroup before the next encounter")
                : TEXT("NEXT ENCOUNTER");
            break;
        case EAlphaRoundState::Victory:
            Objective = State->StoryStage == EAlphaStoryStage::Debrief
                ? TEXT("Return to the chapter guide. F: report back.")
                : (Chapter < 2 ? TEXT("Chapter complete. TAB: host continues the journey.")
                    : TEXT("Journey complete. The city's story continues..."));
            break;
        case EAlphaRoundState::Defeat: Objective = TEXT("DEFEAT  /  Host can reset in TAB."); break;
        }
    }
    Text(Objective, FLinearColor(.9f, .91f, .85f), 29, 76);
    if (State && State->bAwaitingRouteAdvance)
    {
        FVector2D Marker;
        if (PlayerOwner->ProjectWorldLocationToScreen(State->RouteObjective + FVector(0, 0, 80), Marker))
        {
            const APawn* Pawn = PlayerOwner->GetPawn();
            const float Meters = Pawn ? FVector::Dist2D(Pawn->GetActorLocation(), State->RouteObjective) / 100.f : 0.f;
            const TCHAR* Direction = Marker.X < 100.f ? TEXT("< ") : (Marker.X > Width - 100.f ? TEXT("> ") : TEXT(""));
            Marker.X = FMath::Clamp(Marker.X, 100., FMath::Max(100., static_cast<double>(Width) - 100.));
            Marker.Y = FMath::Clamp(Marker.Y, 145., FMath::Max(145., static_cast<double>(Height) - 220.));
            DrawRect(Gold, Marker.X - 4, Marker.Y - 12, 8, 24);
            Text(FString::Printf(TEXT("%sNEXT ENCOUNTER / %.0fm"), Direction, Meters), Gold, Marker.X - 80, Marker.Y - 36, .95f);
        }
    }

    for (TActorIterator<AAlphaStoryNPC> It(GetWorld()); It; ++It)
    {
        if (!State || It->ChapterIndex != State->ChapterIndex) continue;
        FVector2D Screen;
        if (!PlayerOwner->ProjectWorldLocationToScreen(It->GetActorLocation() + FVector(0, 0, 205), Screen)) continue;
        static const TCHAR* GuideNames[] = {TEXT("DU BO / FERRYMAN"), TEXT("SUI / ARCHIVIST"), TEXT("CHENG / LAMP KEEPER")};
        Text(GuideNames[Chapter], Gold, Screen.X - 65, Screen.Y, .85f);
        const APawn* Pawn = PlayerOwner->GetPawn();
        const bool bSafe = State->RoundState == EAlphaRoundState::Waiting || State->RoundState == EAlphaRoundState::Victory;
        if (Pawn && bSafe && FVector::DistSquared2D(Pawn->GetActorLocation(), It->GetActorLocation())
            <= FMath::Square(AAlphaStoryNPC::InteractionRadius))
            Text(TEXT("F  TALK"), FLinearColor(.4f, 1.f, .8f), Screen.X - 28, Screen.Y + 17, .95f);
    }

    if (AAlphaCharacter* Character = Cast<AAlphaCharacter>(PlayerOwner->GetPawn()))
    {
        UAlphaCombatComponent* Combat = Character->GetCombatComponent();
        const int32 Hero = FMath::Clamp(Character->GetArchetype(), 0, 2);
        static const TCHAR* Names[] = {TEXT("SHEN YAN / BLADE"), TEXT("A LING / WEAVER"), TEXT("WEN DENG / LANTERN")};
        const FLinearColor Accent = Hero == 0 ? FLinearColor(.8f, .1f, .18f) : (Hero == 1 ? FLinearColor(.08f, .75f, .64f) : Gold);
        const float Y = FMath::Max(120.f, Height - 180.f);
        Card(16, Y, CardWidth, 164);
        Text(Names[Hero], Accent, 29, Y + 9, 1.15f);
        Text(FString::Printf(TEXT("COMBO %d / 5%s"), Combat->GetComboStage(), Character->IsSprinting() ? TEXT("  RUN x2.5") : (Combat->IsAirComboActive() ? TEXT("  AIR") : TEXT(""))),
            Gold, 16 + CardWidth - 200, Y + 10, .9f);
        Text(FString::Printf(TEXT("HP %.0f / %.0f%s"), Combat->GetHealth(), Combat->GetMaxHealth(), Combat->IsInvulnerable() ? TEXT("  PROTECTED") : TEXT("")),
            FLinearColor::White, 29, Y + 33);
        Bar(29, Y + 55, CardWidth - 26, 9, Combat->GetHealth() / FMath::Max(1.f, Combat->GetMaxHealth()), Accent);
        Bar(29, Y + 69, CardWidth - 26, 4, Combat->GetPoise() / FMath::Max(1.f, Combat->GetMaxPoise()), Gold);
        if (UAlphaAbilityComponent* Ability = Character->GetAbilityComponent())
        {
            Text(FString::Printf(TEXT("ENERGY %.0f / 100"), Ability->GetEnergy()), Gold, 16 + CardWidth - 160, Y + 33, .9f);
            Bar(29, Y + 79, CardWidth - 26, 5, Ability->GetEnergy() / 100.f, Gold);
            Text(FString::Printf(TEXT("U +10  %.1fs"), Ability->GetSkillRemaining(0)), Ability->CanUseSkill(0) ? Gold : Muted, 29, Y + 92, .95f);
            Text(FString::Printf(TEXT("I -10  %.1fs"), Ability->GetSkillRemaining(1)), Ability->CanUseSkill(1) ? Gold : Muted, 190, Y + 92, .95f);
            Text(FString::Printf(TEXT("O 100  %.1fs"), Ability->GetSkillRemaining(2)), Ability->CanUseSkill(2) ? Gold : Muted, 350, Y + 92, .95f);
        }
        UAlphaReviveComponent* Revive = Character->GetReviveComponent();
        FString Coop = Combat->IsAlive() ? TEXT("Double-tap a direction: RUN x2.5   |   Hold E: revive") : TEXT("DOWNED  /  A teammate can hold E nearby.");
        if (Revive->GetReviveTarget()) Coop = FString::Printf(TEXT("REVIVING %.0f%%  /  Hold E and avoid damage"), Revive->GetReviveProgress() * 100.f);
        else if (AAlphaCharacter* Rescuer = Revive->GetCurrentRescuer()) Coop = FString::Printf(TEXT("BEING REVIVED %.0f%%"), Rescuer->GetReviveComponent()->GetReviveProgress() * 100.f);
        Text(Coop, Gold, 29, Y + 114, .9f);
        Text(TEXT("WASD Move/Face   J Combo   K Jump   L Roll   U/I/O Skills"), Muted, 29, Y + 137, .9f);
    }
    if (State && IsValid(State->ForgeBoss) && State->ForgeBoss->GetCombatComponent()->IsAlive())
    {
        UAlphaCombatComponent* Combat = State->ForgeBoss->GetCombatComponent();
        const float BossWidth = FMath::Max(180.f, FMath::Min(550.f, Width - CardWidth - 64.f));
        const float X = Width - BossWidth - 16.f;
        const float Y = Width < 1000.f ? 121.f : 16.f;
        Card(X, Y, BossWidth, 84);
        Text(FString::Printf(TEXT("FORGE WARDEN  /  PHASE %d"), State->BossPhase), Gold, X + 13, Y + 11, 1.05f);
        Bar(X + 13, Y + 39, BossWidth - 26, 12, Combat->GetHealth() / FMath::Max(1.f, Combat->GetMaxHealth()), FLinearColor(.85f, .23f, .045f));
        const bool bTell = State->ForgeBoss->GetAttackState() == EAlphaForgeAttackState::Windup;
        Text(bTell ? TEXT("DANGER  /  Leave the marked area") : TEXT("Break poise together to interrupt"), bTell ? FLinearColor(1.f, .6f, .2f) : Muted, X + 13, Y + 60, .95f);
    }
    if (UAlphaGameInstance* Instance = Cast<UAlphaGameInstance>(GetWorld()->GetGameInstance()))
    {
        if (!Instance->LastConnectionError.IsEmpty())
        {
            Card(16, 120, FMath::Max(200.f, Width - 32.f), 52);
            Text(Instance->LastConnectionError, FLinearColor(1.f, .48f, .3f), 29, 128);
            Text(TEXT("Check host/IP/firewall. Retry through the TAB room menu."), Muted, 29, 150);
        }
    }
    for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
    {
        UAlphaCombatComponent* Combat = It->GetCombatComponent();
        if (It->ActorHasTag(TEXT("Boss"))) continue;
        if (!Combat->IsAlive() && Combat->TeamId != 1) continue;
        FVector2D Screen;
        if (PlayerOwner->ProjectWorldLocationToScreen(It->GetActorLocation() + FVector(0, 0, 120), Screen))
        {
            if (!Combat->IsAlive()) Text(TEXT("DOWN / HOLD E"), Gold, Screen.X - 42, Screen.Y, .9f);
            else if (Combat->GetHealth() < Combat->GetMaxHealth() || Combat->TeamId == 1)
            {
                Bar(Screen.X - 23, Screen.Y, 46, 4, Combat->GetHealth() / FMath::Max(1.f, Combat->GetMaxHealth()),
                    Combat->TeamId == 1 ? FLinearColor(.12f, .72f, .6f) : FLinearColor(.85f, .24f, .05f));
                if (const AAlphaCompanion* Bot = Cast<AAlphaCompanion>(*It))
                    Text(FString::Printf(TEXT("AI / %s"), *Bot->GetBehaviorLabel()), Gold, Screen.X - 35, Screen.Y - 17, .8f);
            }
        }
    }
}
