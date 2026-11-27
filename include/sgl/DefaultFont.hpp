#pragma once

// sgl -- eingebettete Standardschrift (DejaVu Sans, Teilmenge)
//
// Die Bytes liegen als TrueType-Datei im Programm, damit Text ohne jede
// Datei von der Platte funktioniert. Geladen wird ueber Font::loadDefault();
// das Ergebnis steht global als sgl::g_defaultFont bereit und wird beim
// ersten Window::create einmalig erzeugt.
//
// Interner Header, nicht Teil der oeffentlichen API.

namespace sgl::detail {

extern const unsigned char g_defaultFontData[];
extern const unsigned int g_defaultFontDataSize;

// Anzahl der Zeichen in der Teilmenge (ASCII 32..126 plus Latin-1 160..255)
constexpr unsigned int DefaultFontGlyphCount = 191;

} // namespace sgl::detail
