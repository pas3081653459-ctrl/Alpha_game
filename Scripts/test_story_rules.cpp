#include "Rules/StoryRules.h"
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
int Checks = 0;
int Failures = 0;
void Check(bool Condition, const char* Label)
{
    ++Checks;
    if (!Condition) { ++Failures; std::cerr << "FAIL: " << Label << '\n'; }
}
}

int main()
{
    using namespace AlphaStoryRules;
    for (int Chapter = -2; Chapter <= 4; ++Chapter)
    {
        const bool Valid = Chapter >= 0 && Chapter <= 2;
        Check(ValidChapter(Chapter) == Valid, "Chapter validity");
        Check(WaveCount(Chapter) == (Valid ? (Chapter == 1 ? 3 : 2) : 0), "Chapter encounter count");
        Check(HasBoss(Chapter) == (Chapter == 1), "Only forge has a boss");
        for (int Progress = -1; Progress <= 5; ++Progress)
        {
            const Stage Current = static_cast<Stage>(Progress);
            for (int Safe = 0; Safe <= 1; ++Safe)
            {
                Check(CanStart(Chapter, Current, Safe) == (Valid && Safe && Current == Stage::Ready), "Briefing required to start");
                Check(CanAdvance(Chapter, Current, Safe) == (Valid && Chapter < 2 && Safe && Current == Stage::Complete), "No skipped or fourth chapter");
                for (int Eligible = 0; Eligible <= 1; ++Eligible)
                    Check(CanTalk(Chapter, Current, Eligible, Safe) == (Valid && Eligible && Safe
                        && (Current == Stage::Arrival || Current == Stage::Debrief)), "Story acceptance requires stage and eligibility");
            }
            Check(IsFinalComplete(Chapter, Current) == (Chapter == 2 && Current == Stage::Complete), "Final chapter ending only");
        }
    }
    Stage Progress = Stage::Arrival;
    for (int Chapter = 0; Chapter < ChapterCount; ++Chapter)
    {
        Check(!CanStart(Chapter, Progress, true), "Fresh arrival cannot start combat");
        Check(CanTalk(Chapter, Progress, true, true), "Arrival briefing is accepted");
        Progress = AfterTalk(Progress);
        Check(Progress == Stage::Ready && CanStart(Chapter, Progress, true), "Briefing unlocks chapter");
        Check(AfterTalk(Progress) == Progress, "Repeated briefing cannot skip combat");
        Progress = Stage::Combat;
        Check(!CanTalk(Chapter, Progress, true, true), "Combat does not accept dialogue progression");
        Progress = Stage::Debrief;
        Check(!CanAdvance(Chapter, Progress, true), "Debrief cannot skip final conversation");
        Progress = AfterTalk(Progress);
        Check(Progress == Stage::Complete, "Debrief completes chapter");
        Check(CanAdvance(Chapter, Progress, true) == (Chapter < 2), "Route advances only before final chapter");
        Progress = Stage::Arrival;
    }
    Check(WithinConversationRange(240., 160.), "Exact range and height are allowed");
    Check(WithinConversationRange(0., -160.), "Negative height direction is symmetric");
    Check(!WithinConversationRange(240.001, 0.), "Beyond horizontal range rejected");
    Check(!WithinConversationRange(0., 160.001), "Beyond vertical range rejected");
    Check(!WithinConversationRange(-1., 0.), "Negative distance rejected");
    for (double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()})
    {
        Check(!WithinConversationRange(Invalid, 0.), "Nonfinite distance rejected");
        Check(!WithinConversationRange(0., Invalid), "Nonfinite height rejected");
    }
    Check(WaveCount(std::numeric_limits<int>::max()) == 0, "Extreme chapter rejected without overflow");
    Check(CanActivateEncounter(420.,1100.), "Exact entrance and party bounds activate");
    Check(!CanActivateEncounter(420.01,1100.), "A human must reach the encounter entrance");
    Check(!CanActivateEncounter(0.,1100.01), "Distant downed or living teammate prevents abandonment");
    Check(!CanActivateEncounter(-1.,0.), "Negative human distance rejected");
    Check(!CanActivateEncounter(0.,-1.), "Negative party distance rejected");
    for (double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()})
    {
        Check(!CanActivateEncounter(Invalid,0.), "Nonfinite human position rejected");
        Check(!CanActivateEncounter(0.,Invalid), "Nonfinite teammate position rejected");
    }
    Check(!CanAdvance(std::numeric_limits<int>::min(), Stage::Complete, true), "Negative extreme chapter cannot advance");
    std::cout << "Portable story rules: " << Checks - Failures << '/' << Checks << " checks passed\n";
    std::cout << "Unreal dialogue, chapter travel, replication and rendering remain separate tests.\n";
    return Failures == 0 ? 0 : 1;
}
