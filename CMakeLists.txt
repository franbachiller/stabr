cmake_minimum_required(VERSION 3.22)

# macOS universal: tiene que definirse ANTES de project().
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Arquitecturas macOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Deployment target macOS")
endif()

project(STABR VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STABR_BUILD_TESTS "Compilar el target de tests headless" ON)
option(STABR_WERROR      "Tratar warnings como errores en targets propios" OFF)

# Copiar el VST3 a la carpeta de plugins del sistema al terminar el build.
# Se puede apagar: escribir en Program Files necesita permisos, y falla tambien
# si un DAW tiene el plugin cargado y el archivo bloqueado. Apagarlo permite
# compilar y correr los tests igual; el plugin queda en build/STABR_artefacts.
option(STABR_COPY_PLUGIN "Copiar el plugin a la carpeta del sistema tras compilar" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchJUCE)
include(Warnings)

# ---------------------------------------------------------------------------
# Formatos.
#
# El target de entrega de la v1 es VST3 en Windows (ver DECISIONS.md D-018):
# no hay acceso a una mac en este proyecto, asi que el codigo del wrapper AU
# nunca se compila y por lo tanto esta SIN VERIFICAR. Se deja declarado para no
# tener que reconstruir el CMake el dia que aparezca una mac o un runner de CI
# con macOS, pero no se promete AU como entregable.
# ---------------------------------------------------------------------------
set(STABR_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND STABR_FORMATS AU)
    message(STATUS "STABR: macOS detectado, agregando target AU (sin verificar previamente)")
endif()

juce_add_plugin(STABR
    COMPANY_NAME                "Narfbach"
    COMPANY_WEBSITE             "https://narfbach.com"
    COMPANY_EMAIL               "bachillerfran@gmail.com"
    BUNDLE_ID                   "com.narfbach.stabr"
    PLUGIN_MANUFACTURER_CODE    Nrfb
    PLUGIN_CODE                 Stbr
    PRODUCT_NAME                "STABR"
    DESCRIPTION                 "House & techno stab machine"
    FORMATS                     ${STABR_FORMATS}
    VST3_CATEGORIES             "Instrument" "Synth"
    AU_MAIN_TYPE                "kAudioUnitType_MusicDevice"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     ${STABR_COPY_PLUGIN}
)

# ---------------------------------------------------------------------------
# Codigo compartido. Todo lo testeable vive aca; el target de tests linkea
# contra STABR (la static lib de shared code que crea juce_add_plugin).
# ---------------------------------------------------------------------------
target_sources(STABR PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params/ParameterLayout.cpp
    Source/DSP/Voice.cpp
    Source/DSP/VoiceManager.cpp
    Source/Chord/ChordGenerator.cpp
    Source/Chord/NoteRouter.cpp
)

target_include_directories(STABR PUBLIC Source)

target_compile_definitions(STABR PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0        # ver LICENSING.md antes de distribuir
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0
)

target_link_libraries(STABR
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

stabr_set_warnings(STABR)

if(STABR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
