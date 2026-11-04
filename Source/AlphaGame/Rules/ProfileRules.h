#pragma once

#include <cmath>

namespace AlphaProfileRules
{
constexpr int CurrentVersion = 1;
constexpr int ChapterCount = 3;

struct Data
{
    int Version = CurrentVersion;
    int HighestUnlocked = 0;
    int LastChapter = 0;
    int Hero = 0;
    double Volume = .75;
    double Shake = .35;
};

inline bool ValidChapter(int Chapter) { return Chapter >= 0 && Chapter < ChapterCount; }
inline bool ValidPresentation(double Value) { return std::isfinite(Value) && Value >= 0. && Value <= 1.; }
inline bool Compatible(const Data& Profile)
{
    return Profile.Version == CurrentVersion && ValidChapter(Profile.HighestUnlocked)
        && ValidChapter(Profile.LastChapter) && Profile.LastChapter <= Profile.HighestUnlocked
        && Profile.Hero >= 0 && Profile.Hero < 3 && ValidPresentation(Profile.Volume)
        && ValidPresentation(Profile.Shake);
}
inline bool CanSelectChapter(const Data& Profile, int Chapter)
{
    return Compatible(Profile) && ValidChapter(Chapter) && Chapter <= Profile.HighestUnlocked;
}
inline bool CanReplay(int Chapter, int Unlocked, bool Host, bool Safe)
{
    return Host && Safe && ValidChapter(Chapter) && ValidChapter(Unlocked) && Chapter <= Unlocked;
}
inline Data AfterCompletion(const Data& Profile, int Chapter)
{
    if (!Compatible(Profile) || !ValidChapter(Chapter)) return Profile;
    Data Result = Profile;
    // A valid authority may report a cooperative completion above local unlocks.
    const int Next = Chapter < ChapterCount - 1 ? Chapter + 1 : Chapter;
    if (Next > Result.HighestUnlocked) Result.HighestUnlocked = Next;
    return Result;
}
}
