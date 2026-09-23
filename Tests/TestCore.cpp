#include "TestHelpers.h"

using namespace ifx;

IFX_TEST (Core, rngIsDeterministicAndPortable)
{
    Rng a (42), b (42), c (43);
    bool same = true, differs = false;
    for (int i = 0; i < 1000; ++i)
    {
        const auto x = a.next(), y = b.next(), z = c.next();
        same &= x == y;
        differs |= x != z;
    }
    IFX_CHECK (same);
    IFX_CHECK (differs);
    // fixed reference values: the generator must never change between releases
    IFX_CHECK (splitmix64 (0) == 0xE220A8397B1DCDAFull);
    IFX_CHECK (hash3 (1, 2, 3) == hash3 (1, 2, 3));
    IFX_CHECK (hash3 (1, 2, 3) != hash3 (1, 2, 4));
}

IFX_TEST (Core, rngUniformRangeAndMean)
{
    Rng r (7);
    double sum = 0.0, lo = 1.0, hi = 0.0;
    for (int i = 0; i < 100000; ++i)
    {
        const double u = r.uniform();
        sum += u;
        lo = std::min (lo, u);
        hi = std::max (hi, u);
    }
    IFX_CHECK (lo >= 0.0 && hi < 1.0);
    IFX_CLOSE (sum / 100000.0, 0.5, 0.01);
}

IFX_TEST (Core, limitsTableHasEveryEntryAndExpressiveIsWider)
{
    for (int i = 0; i < limits::numLimits; ++i)
    {
        const auto& l = limitTable()[i];
        IFX_CHECK (l.id != nullptr && l.unit != nullptr && l.status != nullptr);
        IFX_CHECK (l.natural > 0.0f && l.expressive > 0.0f);
        if (i == limits::minRetriggerMs)
            IFX_CHECK (l.expressive <= l.natural); // a smaller spacing is the wider range
        else
            IFX_CHECK (l.expressive >= l.natural);
    }
}

IFX_TEST (Core, wearMapIsPureAndLocalised)
{
    WearMap::Settings s;
    s.seed = 99;
    s.wear = 1.0f;
    s.centre = 0.3f;
    s.width = 0.08f;
    s.background = 0.05f;
    WearMap a, b;
    a.update (s);
    b.update (s);
    bool same = true;
    for (int i = 0; i <= 100; ++i)
        same &= a.wearAt (i / 100.0f) == b.wearAt (i / 100.0f);
    IFX_CHECK (same);
    IFX_CHECK (a.wearAt (0.3f) > 0.3f);
    IFX_CHECK (a.wearAt (0.9f) < 0.08f);  // pristine far from the worn region
    s.wear = 0.0f;
    a.update (s);
    IFX_CHECK (a.wearAt (0.3f) == 0.0f);
}

IFX_TEST (Core, contactWithZeroHazardNeverLeavesGood)
{
    ContactModel c;
    c.prepare (48000.0);
    c.seed (1);
    ContactModel::Inputs in;
    in.baseline = 1.0f;
    in.hazard = 0.0f;
    in.intermittency = 1.0f;
    in.recovery = 0.0f;
    float minG = 1.0f;
    for (int i = 0; i < 48000 * 5; ++i)
        minG = std::min (minG, c.step (in, Policy::Natural));
    IFX_CHECK (minG == 1.0f);
    IFX_CHECK (c.openEvents == 0 && c.resistiveEvents == 0);
}

IFX_TEST (Core, contactEventRateIsCapped)
{
    for (auto pol : { Policy::Natural, Policy::Expressive })
    {
        ContactModel c;
        c.prepare (48000.0);
        c.seed (3);
        ContactModel::Inputs in;
        in.baseline = 1.0f;
        in.hazard = 1.0e6f;   // absurd request
        in.openProbability = 1.0f;
        in.intermittency = 1.0f;
        for (int i = 0; i < 48000 * 2; ++i)
            c.step (in, pol);
        const float cap = limit (limits::maxEventRate, pol);
        const float perSecond = (c.openEvents + c.resistiveEvents) / 2.0f;
        IFX_CHECK_MSG (perSecond <= cap * 2.0f + 1.0f, "events/s " + juce::String (perSecond) + " cap " + juce::String (cap));
    }
}

IFX_TEST (Core, contactOpenDurationIsCapped)
{
    ContactModel c;
    c.prepare (48000.0);
    c.seed (5);
    ContactModel::Inputs in;
    in.baseline = 1.0f;
    in.hazard = 50.0f;
    in.openProbability = 1.0f;
    in.recovery = 0.0f;
    int run = 0, longest = 0;
    for (int i = 0; i < 48000 * 10; ++i)
    {
        c.step (in, Policy::Natural);
        if (c.getState() == ContactModel::State::Open) longest = std::max (longest, ++run);
        else run = 0;
    }
    IFX_CHECK (c.openEvents > 0);
    IFX_CHECK (longest <= (int) (48000 * limit (limits::maxOpenMs, Policy::Natural) * 0.001f) + 2);
}

IFX_TEST (Core, contactTransitionsAreBandlimited)
{
    // the conductance never jumps by more than a fraction per sample
    ContactModel c;
    c.prepare (48000.0);
    c.seed (11);
    ContactModel::Inputs in;
    in.baseline = 1.0f;
    in.hazard = 200.0f;
    in.openProbability = 1.0f;
    float prev = 1.0f, maxStep = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float g = c.step (in, Policy::Expressive);
        maxStep = std::max (maxStep, std::abs (g - prev));
        prev = g;
    }
    IFX_CHECK (maxStep < 0.5f);
    IFX_CHECK (c.openEvents > 0);
}

IFX_TEST (Core, motionSpeedAndAccelerationAreCapped)
{
    for (auto pol : { Policy::Natural, Policy::Expressive })
    {
        MotionDriver m;
        m.prepare (48000.0);
        m.reset (0.0f);
        float maxV = 0.0f;
        for (int i = 0; i < 48000; ++i)
        {
            m.step ((i / 2400) % 2 ? 1.0f : 0.0f, 0.0f, 0.0f, pol); // square wave command
            maxV = std::max (maxV, std::abs (m.v));
            IFX_CHECK (m.q >= 0.0f && m.q <= 1.0f);
        }
        IFX_CHECK (maxV <= limit (limits::maxMotionSpeed, pol) + 1e-3f);
    }
}

IFX_TEST (Core, stationaryMotionStaysStill)
{
    MotionDriver m;
    m.prepare (48000.0);
    m.reset (0.4f);
    float maxV = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        m.step (0.4f, 0.0f, 0.0f, Policy::Natural);
        maxV = std::max (maxV, std::abs (m.v));
    }
    IFX_CHECK (maxV == 0.0f);
    IFX_CHECK (m.q == 0.4f);
}

IFX_TEST (Core, gestureRetriggerSpacingCoalesces)
{
    GestureGenerator g;
    g.prepare (48000.0);
    IFX_CHECK (g.trigger (GestureKind::Wiggle, 1, 0.1f, -1, 0.5f, 0, 1, Policy::Natural));
    for (int i = 0; i < 480; ++i) g.step();                      // 10 ms later
    IFX_CHECK (! g.trigger (GestureKind::Wiggle, 1, 0.1f, -1, 0.5f, 0, 2, Policy::Natural));
    IFX_CHECK (g.motionLimited);
    for (int i = 0; i < 48000 / 10; ++i) g.step();               // +100 ms
    IFX_CHECK (g.trigger (GestureKind::Wiggle, 1, 0.1f, -1, 0.5f, 0, 3, Policy::Natural));
    // Expressive allows the tighter spacing
    GestureGenerator e;
    e.prepare (48000.0);
    e.trigger (GestureKind::Tap, 1, 0.1f, -1, 0.5f, 0, 1, Policy::Expressive);
    for (int i = 0; i < (int) (48000 * 0.035); ++i) e.step();
    IFX_CHECK (e.trigger (GestureKind::Tap, 1, 0.1f, -1, 0.5f, 0, 2, Policy::Expressive));
}

IFX_TEST (Core, gestureEndsAndReleases)
{
    GestureGenerator g;
    g.prepare (48000.0);
    g.trigger (GestureKind::TouchRelease, 1.0f, -1.0f, -1, 0.0f, 0.2f, 9, Policy::Natural);
    float peak = 0;
    for (int i = 0; i < 4800; ++i) peak = std::max (peak, g.step().pressure);
    IFX_CHECK (peak > 0.9f);
    IFX_CHECK (g.isActive());
    g.release();
    for (int i = 0; i < 48000; ++i) g.step();
    IFX_CHECK (! g.isActive());
    // a held gesture is bounded to 8 s even without release
    g.trigger (GestureKind::Hold, 1.0f, -1.0f, -1, 0.0f, 0.0f, 10, Policy::Natural);
    for (int i = 0; i < 48000 * 9; ++i) g.step();
    IFX_CHECK (! g.isActive());
}

IFX_TEST (Core, environmentPhaseIsContinuousAndSeekable)
{
    Environment a;
    a.prepare (48000.0);
    a.setMainsFrequency (50.0f);
    a.reset (0.0);
    float prev = a.step().humClean, maxStep = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float v = a.step().humClean;
        maxStep = std::max (maxStep, std::abs (v - prev));
        prev = v;
    }
    IFX_CHECK (maxStep < 0.02f); // 50 Hz at 48 kHz: tiny per-sample change, no jumps
    // phase after reset is a pure function of host time
    Environment b, c;
    b.prepare (48000.0); c.prepare (48000.0);
    b.reset (1.2345); c.reset (1.2345);
    IFX_CLOSE (b.getPhase(), c.getPhase(), 1e-12);
    // 50 -> 60 Hz glides without a jump
    a.setMainsFrequency (60.0f);
    prev = a.step().humClean;
    maxStep = 0.0f;
    for (int i = 0; i < 9600; ++i)
    {
        const float v = a.step().humClean;
        maxStep = std::max (maxStep, std::abs (v - prev));
        prev = v;
    }
    IFX_CHECK (maxStep < 0.025f);
}

IFX_TEST (Core, paramTableIsConsistent)
{
    juce::StringArray ids;
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& p = paramInfo (i);
        IFX_CHECK_MSG (! ids.contains (p.id), juce::String ("duplicate id ") + p.id);
        ids.add (p.id);
        IFX_CHECK (p.min < p.max);
        IFX_CHECK_MSG (p.def >= p.min && p.def <= p.max, juce::String ("default out of range: ") + p.id);
        IFX_CHECK (juce::String (p.tooltip).isNotEmpty());
        if (p.kind == ParamKind::Choice)
            IFX_CHECK_MSG (juce::StringArray::fromTokens (p.choices, "|", "").size() == (int) p.max + 1, juce::String ("choices ") + p.id);
    }
}
