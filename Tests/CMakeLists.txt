# Target headless de tests.
#
# add_executable plano en vez de juce_add_console_app A PROPOSITO: el helper de
# JUCE define JUCE_STANDALONE_APPLICATION=1 (PRIVATE), y el target de codigo
# compartido del plugin ya exporta JUCE_STANDALONE_APPLICATION=JucePlugin_Build_Standalone
# (PUBLIC). Las dos definiciones juntas dan redefinicion de macro (C4005), que
# con -Werror rompe el build.
#
# Linkeando contra STABR los tests corren sobre EXACTAMENTE el mismo codigo
# objeto que termina dentro del VST3, sin recompilar JUCE.
add_executable(STABR_Tests
    test_main.cpp
    test_envelope.cpp
    test_oscillator.cpp
    test_processor.cpp
    test_performance.cpp
    test_chord.cpp
    test_router.cpp
    test_character.cpp
    test_fx.cpp
    test_sequencer.cpp
)

target_link_libraries(STABR_Tests PRIVATE
    STABR
    juce::juce_recommended_config_flags
)

if(WIN32)
    target_compile_definitions(STABR_Tests PRIVATE _CONSOLE=1)
    set_target_properties(STABR_Tests PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

set_target_properties(STABR_Tests PROPERTIES OUTPUT_NAME "STABR_Tests")

stabr_set_warnings(STABR_Tests)

add_test(NAME stabr_unit_tests COMMAND STABR_Tests)
