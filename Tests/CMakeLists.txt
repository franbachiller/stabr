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
    test_gui.cpp
    test_presets.cpp
)

target_link_libraries(STABR_Tests PRIVATE
    STABR
    juce::juce_recommended_config_flags
)

# Cada test de la GUI crea un StabrProcessor local por bloque, y MSVC reserva un
# lugar en la pila para cada uno: con 1 MB (el default) el runTest del editor
# desbordaba la pila al entrar. 16 MB sobra.
if(MSVC)
    target_link_options(STABR_Tests PRIVATE /STACK:16777216)
endif()

# ---------------------------------------------------------------------------
# Host minimo que carga el VST3, abre y cierra el editor, libera la instancia y
# descarga la DLL, como un DAW al cerrar. Detecta cuelgues al descargar el
# plugin (FL Studio se colgaba al cerrar, D-053). No linkea STABR: lo carga como
# cualquier host, desde el .vst3 compilado.
# ---------------------------------------------------------------------------
juce_add_console_app(STABR_UnloadCheck PRODUCT_NAME "STABR Unload Check")
target_sources(STABR_UnloadCheck PRIVATE unload_check.cpp)
target_compile_definitions(STABR_UnloadCheck PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(STABR_UnloadCheck PRIVATE
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
)
add_dependencies(STABR_UnloadCheck STABR_VST3)

if(WIN32)
    target_compile_definitions(STABR_Tests PRIVATE _CONSOLE=1)
    set_target_properties(STABR_Tests PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

set_target_properties(STABR_Tests PROPERTIES OUTPUT_NAME "STABR_Tests")

stabr_set_warnings(STABR_Tests)

add_test(NAME stabr_unit_tests COMMAND STABR_Tests)
