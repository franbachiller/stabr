# Warnings agresivos SOLO en nuestros targets. JUCE genera warnings propios y no
# vamos a parchear JUCE. Ver DECISIONS.md D-003.
function(stabr_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4                 # equivalente practico de -Wall -Wextra
            /permissive-        # conformidad estricta
            /w44062             # switch sin manejar todos los enumeradores
            /w44265             # clase con virtuales pero sin destructor virtual
            /w44826             # conversion sign-extended
            /wd4100             # parametro sin usar: ruido en callbacks de JUCE
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Woverloaded-virtual
            -Wno-unused-parameter
        )
    endif()

    if(STABR_WERROR)
        if(MSVC)
            target_compile_options(${target} PRIVATE /WX)
        else()
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
