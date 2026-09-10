#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

namespace
{
/*  El UnitTestRunner por defecto manda logMessage al Logger de JUCE, que en
    Windows va a OutputDebugString y no a la consola: las mediciones que
    imprimen los tests (aliasing en dB, diferencia entre tamanos de buffer,
    duracion del stab por sample rate) se perdian. Este runner las imprime.
*/
class ConsoleRunner final : public juce::UnitTestRunner
{
public:
    void logMessage (const juce::String& message) override
    {
        std::cout << message.toStdString() << std::endl;
    }
};
}

int main (int, char**)
{
    // Los tests del procesador construyen un AudioProcessor, que necesita un
    // MessageManager vivo.
    juce::ScopedJuceInitialiser_GUI juceInit;

    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    int passes   = 0;

    std::cout << "\n----------------------------------------\n"
              << "Resumen por test\n"
              << "----------------------------------------" << std::endl;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* r = runner.getResult (i);

        if (r == nullptr)
            continue;

        failures += r->failures;
        passes   += r->passes;

        const auto status = r->failures > 0 ? "FALLO" : "ok   ";

        std::cout << "  " << status << "  "
                  << (r->unitTestName + " / " + r->subcategoryName).toStdString()
                  << "  (" << r->passes << " ok";

        if (r->failures > 0)
            std::cout << ", " << r->failures << " fallos";

        std::cout << ", " << juce::String (r->endTime.toMilliseconds()
                                           - r->startTime.toMilliseconds()).toStdString()
                  << " ms)" << std::endl;

        for (const auto& msg : r->messages)
            std::cout << "         " << msg.toStdString() << std::endl;
    }

    std::cout << "\n========================================\n"
              << "STABR: " << passes << " aserciones ok, " << failures << " fallos"
              << (failures > 0 ? "   <-- HAY FALLOS" : "   TODO VERDE")
              << "\n========================================" << std::endl;

    return failures > 0 ? 1 : 0;
}
