# JUCE pinneada a un tag exacto: build reproducible, y una release nueva de JUCE
# no puede romper el build a mitad de camino. Ver DECISIONS.md D-002.
include(FetchContent)

set(STABR_JUCE_TAG "8.0.15" CACHE STRING "Tag de JUCE a usar")

# Cache local opcional: si Tools/JUCE-<tag> existe, se usa esa copia en vez de
# clonar de nuevo. Ahorra ~200 MB de descarga en cada build limpio.
set(STABR_JUCE_CACHE "${CMAKE_CURRENT_SOURCE_DIR}/Tools/JUCE-${STABR_JUCE_TAG}")

if(EXISTS "${STABR_JUCE_CACHE}/CMakeLists.txt")
    message(STATUS "STABR: usando JUCE cacheada en ${STABR_JUCE_CACHE}")
    set(FETCHCONTENT_SOURCE_DIR_JUCE "${STABR_JUCE_CACHE}" CACHE PATH "" FORCE)
endif()

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${STABR_JUCE_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(JUCE)
