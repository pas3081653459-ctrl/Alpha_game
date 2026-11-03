#pragma once

#include <cmath>

namespace AlphaStoryRules
{
constexpr int ChapterCount = 3;
enum class Stage { Arrival, Ready, Combat, Debrief, Complete };
inline bool ValidChapter(int Chapter) { return Chapter >= 0 && Chapter < ChapterCount; }
inline int WaveCount(int Chapter) { return !ValidChapter(Chapter) ? 0 : (Chapter == 1 ? 3 : 2); }
inline bool HasBoss(int Chapter) { return Chapter == 1; }
inline bool CanStart(int Chapter, Stage Progress, bool Waiting)
{
    return ValidChapter(Chapter) && Waiting && Progress == Stage::Ready;
}
inline bool CanTalk(int Chapter, Stage Progress, bool EligibleSpeaker, bool SafeRound)
{
    return ValidChapter(Chapter) && EligibleSpeaker && SafeRound
        && (Progress == Stage::Arrival || Progress == Stage::Debrief);
}
inline Stage AfterTalk(Stage Progress)
{
    return Progress == Stage::Arrival ? Stage::Ready : (Progress == Stage::Debrief ? Stage::Complete : Progress);
}
inline bool CanAdvance(int Chapter, Stage Progress, bool SafeRound)
{
    return ValidChapter(Chapter) && Chapter < ChapterCount - 1 && SafeRound && Progress == Stage::Complete;
}
inline bool IsFinalComplete(int Chapter, Stage Progress)
{
    return Chapter == ChapterCount - 1 && Progress == Stage::Complete;
}
// Kept separate from UI: merely reading dialogue never overrides eligibility.
inline bool WithinConversationRange(double HorizontalDistance, double HeightDifference)
{
    return std::isfinite(HorizontalDistance) && std::isfinite(HeightDifference)
        && HorizontalDistance >= 0. && HorizontalDistance <= 240. && std::abs(HeightDifference) <= 160.;
}
}
