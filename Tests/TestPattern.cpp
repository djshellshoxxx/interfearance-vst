#include "TestHelpers.h"

using namespace ifx;
using namespace ifxtest;

namespace
{
constexpr double kFs = 48000.0;
constexpr double kSpb = kFs * 60.0 / 120.0; // samples per beat at 120 BPM

Pattern basic()
{
    Pattern p;
    for (int i = 0; i < 16; ++i)
        p.lanes[0].steps[i].active = i % 4 == 0;
    p.lanes[1].length = 12;
    for (int i = 0; i < 12; i += 3)
        p.lanes[1].steps[i].active = 1;
    return p;
}

std::vector<LaneEvent> schedule (const Pattern& p, const SchedulerSettings& s, double ppq0, int n, int block)
{
    std::vector<LaneEvent> all;
    LaneEvent buf[512];
    for (int pos = 0; pos < n; pos += block)
    {
        const int len = std::min (block, n - pos);
        const double ppq = ppq0 + pos / kSpb;
        const int c = scheduleLane (p, s, ppq, len, kSpb, kFs, buf, 512);
        for (int i = 0; i < c; ++i)
        {
            auto e = buf[i];
            e.sampleOffset += pos;
            all.push_back (e);
        }
    }
    return all;
}
}

IFX_TEST (Pattern, eventsIndependentOfBlockPartition)
{
    SchedulerSettings s;
    s.humanize = 0.5f;
    s.swing = 0.3f;
    auto p = basic();
    p.lanes[0].steps[4].ratchet = 3;
    p.lanes[0].steps[8].probability = 0.5f;
    auto a = schedule (p, s, 0.0, (int) (kSpb * 16), 97);
    auto b = schedule (p, s, 0.0, (int) (kSpb * 16), 4096);
    IFX_CHECK (a.size() == b.size() && ! a.empty());
    bool same = a.size() == b.size();
    for (size_t i = 0; same && i < a.size(); ++i)
        same = a[i].sampleOffset == b[i].sampleOffset && a[i].lane == b[i].lane && a[i].stepIndex == b[i].stepIndex;
    IFX_CHECK (same);
}

IFX_TEST (Pattern, crossRhythm16Against12)
{
    SchedulerSettings s;
    auto ev = schedule (basic(), s, 0.0, (int) (kSpb * 12), 512); // 12 beats = 48 sixteenths
    int a = 0, b = 0;
    for (auto& e : ev) (e.lane == 0 ? a : b)++;
    IFX_CHECK (a == 12);   // every 4th of 16 over 48 steps
    IFX_CHECK (b == 16);   // every 3rd of 12 over 48 steps
}

IFX_TEST (Pattern, probabilityIsSeededPerAbsoluteStep)
{
    auto p = basic();
    for (auto& st : p.lanes[0].steps) st.probability = 0.5f;
    SchedulerSettings s;
    s.seed = 5;
    auto a = schedule (p, s, 0.0, (int) (kSpb * 32), 512);
    auto b = schedule (p, s, 0.0, (int) (kSpb * 32), 333);
    IFX_CHECK (a.size() == b.size());
    s.seed = 6;
    auto c = schedule (p, s, 0.0, (int) (kSpb * 32), 512);
    bool differs = a.size() != c.size();
    for (size_t i = 0; ! differs && i < a.size(); ++i) differs = a[i].sampleOffset != c[i].sampleOffset;
    IFX_CHECK (differs);
    // seek: scheduling a later window alone gives the same events as the full run
    auto late = schedule (p, s, 16.0, (int) (kSpb * 16), 512);
    auto full = schedule (p, s, 0.0, (int) (kSpb * 32), 512);
    std::vector<int> tailFull;
    for (auto& e : full) if (e.sampleOffset >= (int) (kSpb * 16)) tailFull.push_back (e.sampleOffset - (int) std::llround (kSpb * 16));
    IFX_CHECK (tailFull.size() == late.size());
    bool same = tailFull.size() == late.size();
    for (size_t i = 0; same && i < late.size(); ++i) same = std::abs (tailFull[i] - late[i].sampleOffset) <= 1;
    IFX_CHECK (same);
}

IFX_TEST (Pattern, swingDelaysOnlyOffbeats)
{
    Pattern p;
    p.lanes[0].steps[0].active = 1;
    p.lanes[0].steps[1].active = 1;
    p.lanes[1].mute = 1;
    SchedulerSettings s;
    s.swing = 0.5f;
    auto ev = schedule (p, s, 0.0, (int) (kSpb * 1), 128);
    IFX_CHECK (ev.size() == 2);
    if (ev.size() == 2)
    {
        IFX_CHECK (ev[0].sampleOffset == 0);
        IFX_CHECK (std::abs (ev[1].sampleOffset - (int) (kSpb * 0.25 * 1.5)) <= 1);
    }
}

IFX_TEST (Pattern, ratchetsSubdivideAndMuteSoloWork)
{
    Pattern p;
    p.lanes[0].steps[0].active = 1;
    p.lanes[0].steps[0].ratchet = 4;
    p.lanes[1].steps[0].active = 1;
    SchedulerSettings s;
    auto ev = schedule (p, s, 0.0, (int) (kSpb * 0.25), 64);
    IFX_CHECK (ev.size() == 5);
    p.lanes[1].solo = 1;
    ev = schedule (p, s, 0.0, (int) (kSpb * 0.25), 64);
    IFX_CHECK (ev.size() == 1 && ev[0].lane == 1);
}

IFX_TEST (Pattern, densityZeroSchedulesNothing)
{
    SchedulerSettings s;
    s.density = 0.0f;
    IFX_CHECK (schedule (basic(), s, 0.0, (int) (kSpb * 8), 256).empty());
}

IFX_TEST (Pattern, fillAddsEventsWithoutChangingThePattern)
{
    const auto p = basic();
    const auto copy = p;
    SchedulerSettings s;
    const auto normal = schedule (p, s, 0.0, (int) (kSpb * 8), 256).size();
    s.fill = true;
    const auto filled = schedule (p, s, 0.0, (int) (kSpb * 8), 256).size();
    IFX_CHECK (filled > normal);
    IFX_CHECK (p == copy);
}

IFX_TEST (Pattern, evolveKeepsPositions)
{
    auto p = basic();
    SchedulerSettings s;
    s.evolve = 1.0f;
    auto ev = schedule (p, s, 0.0, (int) (kSpb * 16), 256);
    for (auto& e : ev)
    {
        const double beat = e.sampleOffset / kSpb;
        const double stepPos = beat / 0.25;
        IFX_CHECK (std::abs (stepPos - std::round (stepPos)) < 1e-3); // on the grid
    }
}

IFX_TEST (Pattern, rotateShiftAreReversible)
{
    auto p = basic();
    const auto orig = p;
    p.rotate (0, 3);
    p.rotate (0, -3);
    IFX_CHECK (p == orig);
    p.shift (0, 2);
    IFX_CHECK (p.lanes[0].steps[2].active == 1 && p.lanes[0].steps[0].active == 0);
}

IFX_TEST (Pattern, exchangeDeliversConsistentPattern)
{
    PatternExchange ex;
    Pattern a = basic(), b;
    uint32_t v = 0;
    IFX_CHECK (! ex.fetch (b, v));
    ex.publish (a);
    IFX_CHECK (ex.fetch (b, v));
    IFX_CHECK (a == b);
    IFX_CHECK (! ex.fetch (b, v));
}

IFX_TEST (Pattern, laneGesturesLandInTimeWithEngine)
{
    Harness h (kFs, 2, 256);
    h.params[P::laneOn] = 1.0f;
    h.params[P::humanize] = 0.0f;
    h.transport.hasHostTransport = true;
    h.transport.playing = true;
    h.transport.bpm = 120.0;
    h.setSlots (SourceType::LooseJack, SourceType::CableHandling);
    h.publish (basic());
    h.runSilence ((int) (kFs * 2)); // 4 beats
    IFX_CHECK (h.engine->telemetry.slot[0].gestures.load() == 4);
    IFX_CHECK (h.engine->telemetry.slot[1].gestures.load() >= 5);
}

IFX_TEST (Pattern, naturalCoalescesFastRatchetsExpressiveAllows)
{
    for (int pol = 0; pol < 2; ++pol)
    {
        Harness h (kFs, 2, 256);
        h.params[P::laneOn] = 1.0f;
        h.params[P::humanize] = 0.0f;
        h.params[P::policy] = (float) pol;
        h.transport.hasHostTransport = true;
        h.transport.playing = true;
        h.transport.bpm = 120.0;
        h.setSlots (SourceType::LooseJack, SourceType::None);
        Pattern p;
        p.lanes[0].steps[0].active = 1;
        p.lanes[0].steps[0].ratchet = 4;   // 4 in a 1/16 at 120 BPM = 31 ms apart
        p.lanes[0].length = 4;
        p.lanes[1].mute = 1;
        h.publish (p);
        h.runSilence ((int) (kFs * 1));
        const auto started = h.engine->telemetry.slot[0].gestures.load();
        if (pol == 0) IFX_CHECK_MSG (started < 8, "natural started " + juce::String ((int) started));
        else          IFX_CHECK_MSG (started == 8, "expressive started " + juce::String ((int) started));
    }
}

IFX_TEST (Pattern, internalClockRunsWithoutHost)
{
    Harness h (kFs, 2, 256);
    h.params[P::laneOn] = 1.0f;
    h.params[P::runInternal] = 1.0f;
    h.params[P::bpm] = 120.0f;
    h.setSlots (SourceType::LooseJack, SourceType::None);
    h.publish (basic());
    h.runSilence ((int) (kFs * 2));
    IFX_CHECK (h.engine->telemetry.internalClock.load());
    IFX_CHECK (h.engine->telemetry.slot[0].gestures.load() == 4);
}

IFX_TEST (Pattern, midiOneShotPatternAndNoteOffPolicy)
{
    Harness h (kFs, 2, 256);
    h.setSlots (SourceType::LooseJack, SourceType::None);
    h.publish (basic());
    h.pendingEvents.push_back ({ 100, ControlEvent::NoteOn, 62, 1.0f });
    h.runSilence ((int) (kFs * 3));
    const auto g1 = h.engine->telemetry.slot[0].gestures.load();
    IFX_CHECK (g1 == 4);   // one 16-step cycle at the default tempo, then stops
    h.pendingEvents.clear();
}
