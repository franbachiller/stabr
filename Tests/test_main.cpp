#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

int main (int, char**)
{
    // Los tests del procesador construyen un AudioProcessor, que necesita un
    // MessageManager vivo.
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    int total    = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (const auto* r = runner.getResult (i))
        {
            failures += r->failures;
            total    += r->passes + r->failures;
        }
    }

    std::cout << "\n========================================\n"
              << "STABR tests: " << (total - failures) << "/" << total << " pasaron"
              << (failures > 0 ? "  <-- FALLOS" : "  OK")
              << "\n========================================\n";

    return failures > 0 ? 1 : 0;
}
