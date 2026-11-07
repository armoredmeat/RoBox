// Test runner: runs every RoBox unit test. Exit code 0 = all passed.
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

namespace
{
    // Send logMessage() output from tests to the console.
    struct ConsoleLogger final : juce::Logger
    {
        void logMessage (const juce::String& message) override
        {
            if (message.startsWith ("  ")) // our own measurement lines
                std::cout << "       " << message.trimStart() << std::endl;
        }
    };
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce; // the plugin's parameter system needs JUCE's message system

    ConsoleLogger logger;
    juce::Logger::setCurrentLogger (&logger);

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("RoBox"); // only our tests, not JUCE's built-in ones

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* r = runner.getResult (i);
        failures += r->failures;
        std::cout << (r->failures == 0 ? "[pass] " : "[FAIL] ") << r->unitTestName << " / " << r->subcategoryName << std::endl;
        for (const auto& m : r->messages)
            std::cout << "       " << m << std::endl;
    }

    juce::Logger::setCurrentLogger (nullptr);
    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED: " + std::to_string (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
