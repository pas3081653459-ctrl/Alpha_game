#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace AlphaLayoutRules
{
struct Point { double X; double Y; };
struct Rect { double MinX; double MaxX; double MinY; double MaxY; };
struct Segment { Point A; Point B; };
inline bool ValidChapter(int Chapter) { return Chapter >= 0 && Chapter < 3; }
inline bool Finite(Point P) { return std::isfinite(P.X) && std::isfinite(P.Y); }
inline int FloorCount(int Chapter) { return !ValidChapter(Chapter) ? 0 : (Chapter == 2 ? 7 : 5); }
inline Rect Floor(int Chapter, int Index)
{
    constexpr Rect Dock[] = {{-1400,-500,-360,360},{-550,650,-520,520},{600,1150,-180,180},{1100,2400,-420,420},{2350,2500,-250,250}};
    constexpr Rect Forge[] = {{-1400,-500,-340,340},{-550,650,-580,580},{600,1200,-280,120},{1150,2450,-560,560},{2400,2500,-250,250}};
    constexpr Rect Canal[] = {{-1400,-500,-360,360},{-550,650,-400,400},{600,1250,-200,200},{1200,2400,-300,300},{1350,2050,-650,-250},{1600,2250,250,650},{2350,2500,-230,230}};
    if (Index < 0 || Index >= FloorCount(Chapter)) return {0,0,0,0};
    return Chapter == 0 ? Dock[Index] : (Chapter == 1 ? Forge[Index] : Canal[Index]);
}
struct ElevationPoint { double X; double Z; };
inline const std::vector<ElevationPoint>& ElevationProfile(int Chapter)
{
    // All human starts and the story NPC remain on the level entry apron.
    // Continuous cross-route ramps let existing walking AI climb without jumps.
    static const std::vector<ElevationPoint> Dock={{-1400,0},{-200,0},{400,120},{650,120},{1150,40},{2500,40}};
    static const std::vector<ElevationPoint> Forge={{-1400,0},{-200,0},{400,100},{650,100},{1200,220},{2500,220}};
    static const std::vector<ElevationPoint> Canal={{-1400,0},{-200,0},{400,60},{600,60},{900,180},{1250,90},{2500,90}};
    static const std::vector<ElevationPoint> Empty;
    return Chapter==0?Dock:(Chapter==1?Forge:(Chapter==2?Canal:Empty));
}
inline double SurfaceHeight(int Chapter, Point P)
{
    if (!ValidChapter(Chapter) || !Finite(P)) return 0.;
    const auto& Profile = ElevationProfile(Chapter);
    if (P.X <= Profile.front().X) return Profile.front().Z;
    for (std::size_t I=1; I<Profile.size(); ++I)
        if (P.X <= Profile[I].X)
        {
            const double T=(P.X-Profile[I-1].X)/(Profile[I].X-Profile[I-1].X);
            return Profile[I-1].Z+(Profile[I].Z-Profile[I-1].Z)*T;
        }
    return Profile.back().Z;
}
inline std::vector<Rect> FloorTiles(int Chapter)
{
    // Disjoint slabs eliminate coplanar overlaps in visible/collision floors.
    std::vector<Rect> Tiles;
    std::vector<double> Cuts;
    for (int I=0;I<FloorCount(Chapter);++I) {const Rect R=Floor(Chapter,I);Cuts.push_back(R.MinX);Cuts.push_back(R.MaxX);}
    std::sort(Cuts.begin(),Cuts.end());Cuts.erase(std::unique(Cuts.begin(),Cuts.end()),Cuts.end());
    for (std::size_t X=1;X<Cuts.size();++X)
    {
        const double Mid=(Cuts[X-1]+Cuts[X])*.5;
        std::vector<std::array<double,2>> Intervals;
        for (int I=0;I<FloorCount(Chapter);++I)
        {
            const Rect R=Floor(Chapter,I);
            if (Mid>=R.MinX&&Mid<=R.MaxX) Intervals.push_back({R.MinY,R.MaxY});
        }
        std::sort(Intervals.begin(),Intervals.end());
        for (std::size_t I=0;I<Intervals.size();)
        {
            double MinY=Intervals[I][0],MaxY=Intervals[I][1];++I;
            while (I<Intervals.size()&&Intervals[I][0]<=MaxY) {MaxY=std::max(MaxY,Intervals[I][1]);++I;}
            bool Merged=false;
            for (Rect& Tile:Tiles)
                if (Tile.MaxX==Cuts[X-1]&&Tile.MinY==MinY&&Tile.MaxY==MaxY) {Tile.MaxX=Cuts[X];Merged=true;break;}
            if (!Merged) Tiles.push_back({Cuts[X-1],Cuts[X],MinY,MaxY});
        }
    }
    return Tiles;
}
struct TerrainTile { Rect Bounds; double StartZ; double EndZ; };
inline std::vector<TerrainTile> TerrainTiles(int Chapter)
{
    // Split the floor union at every slope break. Each collision slab and its
    // visible surface share this exact affine top plane, with no hidden floor.
    std::vector<TerrainTile> Result;
    const auto& Profile=ElevationProfile(Chapter);
    for (const Rect& R:FloorTiles(Chapter))
        for (std::size_t I=1; I<Profile.size(); ++I)
        {
            const double Begin=std::max(R.MinX,Profile[I-1].X),End=std::min(R.MaxX,Profile[I].X);
            if (End>Begin) Result.push_back({{Begin,End,R.MinY,R.MaxY},
                SurfaceHeight(Chapter,{Begin,0}),SurfaceHeight(Chapter,{End,0})});
        }
    return Result;
}
inline bool Contains(int Chapter, Point P)
{
    if (!Finite(P)) return false;
    for (int I = 0; I < FloorCount(Chapter); ++I)
    {
        const Rect R = Floor(Chapter, I);
        if (P.X >= R.MinX && P.X <= R.MaxX && P.Y >= R.MinY && P.Y <= R.MaxY) return true;
    }
    return false;
}
inline bool IsWalkable(int Chapter, Point P, double Margin = 0.)
{
    if (!ValidChapter(Chapter) || !Finite(P) || !std::isfinite(Margin) || Margin < 0. || Margin > 120. || !Contains(Chapter,P)) return false;
    if (Margin == 0.) return true;
    // Test the UNION, never shrink overlapping floor tiles independently.
    // 16 samples at margin45 leave >44cm clearance, exceeding a 34cm capsule.
    constexpr Point Circle[]={{1,0},{.9238795325112867,.3826834323650898},{.7071067811865476,.7071067811865476},{.3826834323650898,.9238795325112867},{0,1},{-.3826834323650898,.9238795325112867},{-.7071067811865476,.7071067811865476},{-.9238795325112867,.3826834323650898},{-1,0},{-.9238795325112867,-.3826834323650898},{-.7071067811865476,-.7071067811865476},{-.3826834323650898,-.9238795325112867},{0,-1},{.3826834323650898,-.9238795325112867},{.7071067811865476,-.7071067811865476},{.9238795325112867,-.3826834323650898}};
    for (Point Direction:Circle)
        if (!Contains(Chapter,{P.X+Direction.X*Margin,P.Y+Direction.Y*Margin})) return false;
    return true;
}
inline double DistanceSquared(Point A, Point B) { const double X=A.X-B.X,Y=A.Y-B.Y; return X*X+Y*Y; }
inline Point Project(int Chapter, Point P, double Margin = 45.)
{
    if (!ValidChapter(Chapter)) return {0,0};
    if (!Finite(P) || !std::isfinite(Margin) || Margin < 0. || Margin > 120.) return {-650,0};
    if (IsWalkable(Chapter,P,Margin)) return P;
    Point Best{-650,0};
    double Score = std::numeric_limits<double>::max();
    // Per-tile inset candidates are conservative; union membership above keeps
    // valid connector joins unchanged, including their 50cm overlap.
    for (int I=0;I<FloorCount(Chapter);++I)
    {
        const Rect R=Floor(Chapter,I);
        if (R.MaxX-R.MinX<2.*Margin||R.MaxY-R.MinY<2.*Margin) continue;
        const Point Candidate{std::clamp(P.X,R.MinX+Margin,R.MaxX-Margin),std::clamp(P.Y,R.MinY+Margin,R.MaxY-Margin)};
        const double Distance=std::hypot(P.X-Candidate.X,P.Y-Candidate.Y);
        if (IsWalkable(Chapter,Candidate,Margin) && Distance < Score) {Best=Candidate;Score=Distance;}
    }
    return Best;
}
inline double RouteCenterY(int Chapter, double X)
{
    return Chapter == 1 && std::isfinite(X) && X >= 500. && X <= 1300. ? -80. : 0.;
}
inline bool SegmentClear(int Chapter, Point A, Point B, double Margin = 45.)
{
    if (!Finite(A)||!Finite(B)||!IsWalkable(Chapter,A,Margin)||!IsWalkable(Chapter,B,Margin)) return false;
    const double Length=std::hypot(A.X-B.X,A.Y-B.Y);
    const int Steps=static_cast<int>(std::ceil(Length/16.))+1;
    for (int I=1;I<Steps;++I)
    {
        const double T=static_cast<double>(I)/Steps;
        if (!IsWalkable(Chapter,{A.X+(B.X-A.X)*T,A.Y+(B.Y-A.Y)*T},Margin)) return false;
    }
    return true;
}
struct RouteGraph
{
    std::array<Point,12> Nodes{};
    std::array<std::array<double,12>,12> Edges{};
};
inline RouteGraph BuildGraph(int Chapter)
{
    RouteGraph Graph;
    const double Xs[]={-1000.,-650.,0.,500.,650.,1100.,1250.,1750.,2250.,2400.};
    for (int I=0;I<10;++I) Graph.Nodes[I]={Xs[I],RouteCenterY(Chapter,Xs[I])};
    Graph.Nodes[10]=Chapter==2?Point{1650,-400}:Point{1750,0};
    Graph.Nodes[11]=Chapter==2?Point{1900,400}:Point{1750,0};
    for (int I=0;I<12;++I)
        for (int J=0;J<12;++J)
            Graph.Edges[I][J]=I!=J&&SegmentClear(Chapter,Graph.Nodes[I],Graph.Nodes[J])
                ? std::hypot(Graph.Nodes[I].X-Graph.Nodes[J].X,Graph.Nodes[I].Y-Graph.Nodes[J].Y)
                : std::numeric_limits<double>::max();
    return Graph;
}
inline const RouteGraph& GraphFor(int Chapter)
{
    // Fixed waypoint-to-waypoint visibility is computed once, not per enemy frame.
    static const std::array<RouteGraph,3> Graphs={BuildGraph(0),BuildGraph(1),BuildGraph(2)};
    return Graphs[static_cast<std::size_t>(std::clamp(Chapter,0,2))];
}
inline Point NextWaypoint(int Chapter, Point From, Point To)
{
    if (!ValidChapter(Chapter)) return {0,0};
    From=Project(Chapter,From); To=Project(Chapter,To);
    if (SegmentClear(Chapter,From,To)) return To;
    const RouteGraph& Graph=GraphFor(Chapter);
    std::array<Point,14> Nodes{};Nodes[0]=From;Nodes[1]=To;
    for (int I=0;I<12;++I) Nodes[I+2]=Graph.Nodes[I];
    std::array<std::array<double,14>,2> EndpointEdges{};
    for (int End=0;End<2;++End)
        for (int I=2;I<14;++I)
            EndpointEdges[End][I]=SegmentClear(Chapter,Nodes[End],Nodes[I])
                ? std::hypot(Nodes[End].X-Nodes[I].X,Nodes[End].Y-Nodes[I].Y)
                : std::numeric_limits<double>::max();
    std::array<double,14> Cost; Cost.fill(std::numeric_limits<double>::max()); Cost[0]=0.;
    std::array<int,14> Previous;Previous.fill(-1);
    std::array<bool,14> Used{};
    for (int Pass=0;Pass<14;++Pass)
    {
        int Best=-1;
        for (int I=0;I<14;++I) if (!Used[I]&&(Best<0||Cost[I]<Cost[Best])) Best=I;
        if (Best<0||Cost[Best]==std::numeric_limits<double>::max()||Best==1) break;
        Used[Best]=true;
        for (int I=1;I<14;++I)
        {
            if (Used[I]||I==Best) continue;
            const double Edge=Best==0?(I==1?std::numeric_limits<double>::max():EndpointEdges[0][I])
                :(I==1?EndpointEdges[1][Best]:Graph.Edges[Best-2][I-2]);
            if (Edge==std::numeric_limits<double>::max()||(I!=1&&Edge<1.)) continue;
            const double Candidate=Cost[Best]+Edge;
            if (Candidate<Cost[I]) {Cost[I]=Candidate;Previous[I]=Best;}
        }
    }
    if (Previous[1]<0) return From;
    int First=1;
    for (int Guard=0;Guard<14&&Previous[First]>0;++Guard) First=Previous[First];
    return Nodes[First];
}
inline Point EncounterAnchor(int Chapter,int Wave)
{
    if (!ValidChapter(Chapter)||Wave<1||Wave>(Chapter==1?3:2)) return {-650,0};
    if (Chapter==1) return Wave==1?Point{0,-160}:(Wave==2?Point{300,180}:Point{1750,0});
    return Wave==1?Point{50,0}:Point{1750,0};
}
inline Point EnemySpawn(int Chapter,int Wave,int Index)
{
    const Point Anchor=EncounterAnchor(Chapter,Wave);
    if (!ValidChapter(Chapter)||Index<0) return Project(Chapter,Anchor);
    const int Slot=Index%16;
    const Point Offsets[]={{300,-200},{450,200},{-300,200},{-350,-180},{150,280},{-100,-280},{500,-80},{-450,60},
        {0,360},{400,0},{-450,-300},{450,-300},{-150,380},{450,320},{-500,320},{500,350}};
    return Project(Chapter,{Anchor.X+Offsets[Slot].X,Anchor.Y+Offsets[Slot].Y},65.);
}
inline Point BossSpawn(int Chapter) { return Chapter==1?Point{1950,0}:Project(Chapter,{1750,0}); }
inline std::vector<Segment> BoundarySegments(int Chapter)
{
    // Cut each rectangle edge at every other rectangle's bounds; retain only
    // edges whose outward sample is outside the union. Internal joins disappear.
    std::vector<Segment> Result;
    if (!ValidChapter(Chapter)) return Result;
    for (int I=0;I<FloorCount(Chapter);++I)
    {
        const Rect R=Floor(Chapter,I);
        for (int Side=0;Side<4;++Side)
        {
            const bool Vertical=Side<2;
            const double Fixed=Side==0?R.MinX:(Side==1?R.MaxX:(Side==2?R.MinY:R.MaxY));
            const double Begin=Vertical?R.MinY:R.MinX,End=Vertical?R.MaxY:R.MaxX;
            std::vector<double> Cuts{Begin,End};
            for (int J=0;J<FloorCount(Chapter);++J)
            {
                const Rect Other=Floor(Chapter,J);
                for (double Cut : {Vertical?Other.MinY:Other.MinX,Vertical?Other.MaxY:Other.MaxX})
                    if (Cut>Begin&&Cut<End) Cuts.push_back(Cut);
            }
            std::sort(Cuts.begin(),Cuts.end());Cuts.erase(std::unique(Cuts.begin(),Cuts.end()),Cuts.end());
            for (std::size_t C=1;C<Cuts.size();++C)
            {
                Point P=Vertical?Point{Fixed,(Cuts[C-1]+Cuts[C])*.5}:Point{(Cuts[C-1]+Cuts[C])*.5,Fixed};
                Point Outside=P;
                if (Vertical) Outside.X+=Side==0?-.1:.1;else Outside.Y+=Side==2?-.1:.1;
                if (!Contains(Chapter,Outside))
                {
                    const Segment Edge=Vertical?Segment{{Fixed,Cuts[C-1]},{Fixed,Cuts[C]}}:Segment{{Cuts[C-1],Fixed},{Cuts[C],Fixed}};
                    bool Duplicate=false;
                    for (const Segment& Existing:Result)
                        if (DistanceSquared(Existing.A,Edge.A)<.001&&DistanceSquared(Existing.B,Edge.B)<.001) Duplicate=true;
                    if (!Duplicate) Result.push_back(Edge);
                }
            }
        }
    }
    return Result;
}
}
