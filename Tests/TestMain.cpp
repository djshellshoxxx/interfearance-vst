#include "TestFramework.h"
#include <juce_events/juce_events.h>

//==============================================================================
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (juce::String (argv[i]));

    const bool runSlow  = args.contains ("--slow") || args.contains ("--all");
    const bool listOnly = args.contains ("--list");
    juce::String filter;
    for (int i = 0; i < args.size() - 1; ++i)
        if (args[i] == "--filter")
            filter = args[i + 1];

    auto& reg = ifxtest::Registry::get();
    const auto& all = reg.all();

    if (listOnly)
    {
        for (const auto& e : all)
            std::cout << e.suite << "." << e.name << (e.slow ? "  [slow]" : "") << std::endl;
        return 0;
    }

    std::cout << "=====================================================" << std::endl;
    std::cout << " Interference test suite" << std::endl;
    std::cout << " " << all.size() << " tests registered"
              << (runSlow ? " (including slow)" : " (fast only; pass --slow for the rest)")
              << std::endl;
    std::cout << "=====================================================" << std::endl;

    const double t0 = juce::Time::getMillisecondCounterHiRes();
    juce::String lastSuite;
    int ran = 0, skipped = 0;

    for (const auto& e : all)
    {
        if (e.slow && ! runSlow) { ++skipped; continue; }
        if (filter.isNotEmpty() && ! (e.suite + "." + e.name).containsIgnoreCase (filter))
        { ++skipped; continue; }

        if (e.suite != lastSuite)
        {
            std::cout << std::endl << "[" << e.suite << "]" << std::endl;
            lastSuite = e.suite;
        }

        reg.currentSuite = e.suite;
        reg.currentTest = e.name;

        const int failsBefore = (int) reg.getFailures().size();
        const double tt = juce::Time::getMillisecondCounterHiRes();

        try
        {
            e.fn();
        }
        catch (const std::exception& ex)
        {
            reg.recordFail ({ e.suite, e.name, juce::String ("exception: ") + ex.what(), "", 0 });
        }
        catch (...)
        {
            reg.recordFail ({ e.suite, e.name, "unknown exception", "", 0 });
        }

        const int newFails = (int) reg.getFailures().size() - failsBefore;
        const double ms = juce::Time::getMillisecondCounterHiRes() - tt;

        std::cout << (newFails == 0 ? "  PASS  " : "  FAIL  ")
                  << e.name.paddedRight (' ', 46)
                  << juce::String (ms, 1).paddedLeft (' ', 9) << " ms";
        if (newFails > 0)
            std::cout << "   (" << newFails << " check"
                      << (newFails == 1 ? "" : "s") << " failed)";
        std::cout << std::endl;
        ++ran;
    }

    const double total = juce::Time::getMillisecondCounterHiRes() - t0;
    const auto& failures = reg.getFailures();

    std::cout << std::endl << "=====================================================" << std::endl;
    if (! failures.empty())
    {
        std::cout << " FAILURES (" << failures.size() << ")" << std::endl;
        std::cout << "-----------------------------------------------------" << std::endl;
        for (const auto& f : failures)
        {
            std::cout << "  " << f.suite << "." << f.test << std::endl
                      << "    " << f.message << std::endl;
            if (f.file.isNotEmpty())
                std::cout << "    at " << juce::File (f.file).getFileName() << ":" << f.line << std::endl;
        }
        std::cout << "-----------------------------------------------------" << std::endl;
    }

    std::cout << " tests run:   " << ran << "  (skipped " << skipped << ")" << std::endl;
    std::cout << " checks pass: " << reg.getPassCount() << std::endl;
    std::cout << " checks fail: " << failures.size() << std::endl;
    std::cout << " elapsed:     " << juce::String (total / 1000.0, 2) << " s" << std::endl;
    std::cout << " RESULT:      " << (failures.empty() ? "ALL TESTS PASSED" : "FAILED") << std::endl;
    std::cout << "=====================================================" << std::endl;

    return failures.empty() ? 0 : 1;
}
