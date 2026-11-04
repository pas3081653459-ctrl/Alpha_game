#include "Rules/ProfileRules.h"

#include <iostream>
#include <limits>

namespace
{
int Checks = 0;
int Failures = 0;
void Check(bool Condition, const char* Message)
{
    ++Checks;
    if (!Condition) { ++Failures; std::cerr << "FAIL: " << Message << '\n'; }
}
}

int main()
{
    using namespace AlphaProfileRules;
    const Data Defaults;
    Check(Compatible(Defaults), "Default profile is usable");
    Check(Defaults.HighestUnlocked == 0 && Defaults.LastChapter == 0 && Defaults.Hero == 0,
        "Fresh profile begins at chapter one and the default hero");
    Check(Defaults.Volume == .75 && Defaults.Shake == .35, "Fresh presentation settings match runtime defaults");
    for (int Version : {-1, 0, 1, 2, 1000})
    {
        Data Profile;
        Profile.Version = Version;
        Check(Compatible(Profile) == (Version == 1), "Only the implemented schema version is accepted");
    }
    const Data InvalidProfiles[] = {
        {1,-1,0,0,.75,.35}, {1,3,0,0,.75,.35}, {1,0,-1,0,.75,.35},
        {1,0,1,0,.75,.35}, {1,2,3,0,.75,.35}, {1,0,0,-1,.75,.35}, {1,0,0,3,.75,.35},
    };
    for (const Data& Profile : InvalidProfiles)
    {
        Check(!Compatible(Profile), "Malformed chapter/hero data is not silently repaired");
        Check(!CanSelectChapter(Profile, 0), "Malformed profile does not authorize replay");
    }
    for (int Unlocked = 0; Unlocked < 3; ++Unlocked)
        for (int Chapter = -1; Chapter <= 3; ++Chapter)
        {
            Data Profile;
            Profile.HighestUnlocked = Unlocked;
            Check(CanSelectChapter(Profile, Chapter) == (Chapter >= 0 && Chapter <= Unlocked),
                "Only unlocked valid chapters may be selected");
            Check(!CanReplay(Chapter, Unlocked, false, true), "A client cannot authorize chapter replay");
            Check(!CanReplay(Chapter, Unlocked, true, false), "An active encounter cannot be replaced by replay");
            Check(CanReplay(Chapter, Unlocked, true, true) == CanSelectChapter(Profile, Chapter),
                "Safe authority replay follows the same unlock policy as local selection");
        }
    Check(!CanReplay(0,-1,true,true) && !CanReplay(0,3,true,true), "Malformed unlock count cannot authorize replay");
    for (double Value : {-1., -.001, 0., .25, .75, 1., 1.001, std::numeric_limits<double>::max(),
        std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()})
    {
        const bool Valid = std::isfinite(Value) && Value >= 0. && Value <= 1.;
        Check(ValidPresentation(Value) == Valid, "Presentation values must be finite and normalized");
        Data Profile;
        Profile.Volume = Value;
        Check(Compatible(Profile) == Valid, "Invalid volume blocks profile loading");
        Profile.Volume = .75;
        Profile.Shake = Value;
        Check(Compatible(Profile) == Valid, "Invalid shake blocks profile loading");
    }
    for (int Unlocked = 0; Unlocked < 3; ++Unlocked)
        for (int Chapter = -1; Chapter <= 3; ++Chapter)
        {
            Data Profile;
            Profile.HighestUnlocked = Unlocked;
            Profile.LastChapter = Unlocked;
            Profile.Hero = 2;
            Profile.Volume = .2;
            Profile.Shake = .6;
            const Data Result = AfterCompletion(Profile, Chapter);
            Check(Compatible(Result), "Completion always retains a compatible profile");
            Check(Result.HighestUnlocked >= Unlocked && Result.HighestUnlocked < 3,
                "Completion is monotonic and capped at the final chapter");
            Check(Result.LastChapter == Profile.LastChapter && Result.Hero == Profile.Hero
                && Result.Volume == Profile.Volume && Result.Shake == Profile.Shake,
                "Completion does not replace the replay choice or settings");
            const Data Again = AfterCompletion(Result, Chapter);
            Check(Again.HighestUnlocked == Result.HighestUnlocked, "Repeated completion is idempotent");
        }
    Data Progress = AfterCompletion(Defaults, 0);
    Check(Progress.HighestUnlocked == 1 && CanSelectChapter(Progress, 1), "First chapter completion unlocks the second");
    Progress = AfterCompletion(Progress, 1);
    Check(Progress.HighestUnlocked == 2 && CanSelectChapter(Progress, 2), "Second chapter completion unlocks the third");
    Check(AfterCompletion(Progress, 2).HighestUnlocked == 2, "Final chapter cannot unlock a nonexistent fourth chapter");
    Check(AfterCompletion(Defaults, std::numeric_limits<int>::max()).HighestUnlocked == 0,
        "Extreme chapter cannot overflow completion arithmetic");
    Check(!CanReplay(std::numeric_limits<int>::min(), 2, true, true), "Extreme negative chapter cannot replay");
    Data Future;
    Future.Version = 99;
    const Data Preserved = AfterCompletion(Future, 2);
    Check(Preserved.Version == 99 && Preserved.HighestUnlocked == 0, "Unsupported profile is left untouched");
    std::cout << "Portable profile rules: " << Checks - Failures << '/' << Checks << " checks passed\n";
    std::cout << "Unreal save serialization, disk failures and UI lifecycle remain separate checks.\n";
    return Failures ? 1 : 0;
}
