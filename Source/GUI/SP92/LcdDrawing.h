#pragma once

#include <array>

#include "DotMatrix.h"
#include "../EchoModel.h"
#include "../../Chord/ChordTypes.h"

/*  Lo que dibujan las pantallas del SP-92, separado de la vista para poder
    testearlo punto por punto. Ver D-052. */
namespace stabr::gui::sp92::lcdraw
{
//==============================================================================
/*  Ultima fila del LCD: seis columnas de etiqueta y valor. Parte de las
    posiciones fijas y las separa cuando dos textos anchos quedan pegados
    ("SCALEVOICE"), con el mayor aire que entre en la pantalla. */
std::array<int, 6> layoutCells (const std::array<int, 6>& widths, int screenWidth);

/*  El tamano de letra mas grande (maxScale..1) con el que el texto entra en
    maxWidth puntos. El nombre del acorde no puede pisar el teclado. */
int fitTextScale (const DotMatrix&, const juce::String&, int maxWidth, int maxScale = 3);

//==============================================================================
/*  Teclado del acorde: 2 a 4 octavas segun lo abierto del voicing, las teclas
    que suenan encendidas y los nombres de las notas debajo, del bajo al agudo. */
struct KeyboardLayout
{
    int x = 0, y = 0;
    int octaves = 2, lowC = 48;
    int keyWidth = 7, blackWidth = 3;

    static constexpr int whiteHeight = 22;
    static constexpr int blackHeight = 13;

    int width() const noexcept { return octaves * 7 * keyWidth + 1; }
    bool contains (int midiNote) const noexcept { return midiNote >= lowC && midiNote < lowC + octaves * 12; }

    /*  Interior de una tecla (lo que se enciende), en puntos. Vacio si la nota
        no entra en el teclado. */
    juce::Rectangle<int> keyInterior (int midiNote) const;
    int blackKeyCentre (int midiNote) const;
};

bool isBlackKey (int midiNote) noexcept;
KeyboardLayout keyboardFor (const chord::ChordResult&, int x0, int x1, int y0);

/*  hit: el acorde acaba de sonar (subraya las teclas). */
void drawKeyboard (DotMatrix&, const chord::ChordResult&, int x0, int x1, int y0, bool hit);

//==============================================================================
/*  Pantalla de eco, 96 x 22: carriles L y R, el golpe original, cada repeticion
    como una barra de altura proporcional a su volumen, regla de tiempos y
    compases, y las repeticiones que estan sonando encendidas. */
struct EchoScreen
{
    static constexpr int width = 96;
    static constexpr int laneStart = 10;   // aire entre la letra del canal y el golpe
    static constexpr int laneL = 6, laneR = 16, rulerY = 11;
    static constexpr int maxBar = 4;

    static int xOf (float seconds, float window) noexcept;
};

void drawEcho (DotMatrix&, const DelayViewInfo&, float secondsSinceHit, float bpm);
}
