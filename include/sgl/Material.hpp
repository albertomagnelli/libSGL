#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// sgl · Material
//
// CPU-seitige PBR-Materialdaten nach dem Metallic-Roughness-Workflow aus
// LearnOpenGL (PBR Theory):
//
//   Albedo           Basisfarbe ohne Beleuchtung (RGB)
//   Metallic         0 = Dielektrikum, 1 = Metall (Graustufen)
//   Roughness        0 = spiegelglatt, 1 = vollständig rau (Graustufen)
//   AmbientOcclusion 0 = vollständig verdeckt, 1 = unverdeckt (Graustufen)
//   Normal           Tangent-Space-Normalen (RGB, 0.5 = flach)
//   Height           Höhen-/Displacement-Map (Graustufen)
//   Emissive         Selbstleuchtende Farbe (RGB)
//
// Alle Maps sind sgl::Image-Objekte. Eine Map mit nur einem konstanten Wert
// wird als 1x1-Image gespeichert – auch die Standardwerte des Konstruktors.
// Es gibt bewusst keine OpenGL-/Texture-/Shader-Bindung: Material beschreibt
// nur die Daten, die ein Renderer später selbst hochladen und binden kann.
//
// Skalare Setter (setMetallic, setRoughness, …) erzeugen ein 1x1-Bild.
// Bild-Setter normalisieren auf das passende Format:
//   Albedo/Normal/Emissive -> RGB
//   Metallic/Roughness/AO/Height -> Graustufen
// Bei gepackten Maps (z. B. ORM) wählt der channel-Parameter den Quellkanal.
//
// Beispiel:
//
//   Material material;
//   material.setAlbedo(ImageIO::load("assets/tex/albedo.png"));
//   material.setMetallic(1.0f);          // 1x1-Bild
//   material.setRoughness(0.35f);        // 1x1-Bild
//   material.setAmbientOcclusion(1.0f);  // 1x1-Bild
//   material.setNormal(vec3(0.0f, 0.0f, 1.0f));
// ─────────────────────────────────────────────────────────────────────────────

#include "Color.hpp"
#include "Image.hpp"
#include "Types.hpp"

#include <stdexcept>
#include <string>

namespace sgl {

class MaterialError : public std::runtime_error {
public:
    explicit MaterialError(const std::string& message) : std::runtime_error(message) {}
};

class Material {
public:
    // Legt alle Maps als 1x1-Bilder mit sinnvollen PBR-Standardwerten an:
    // Albedo weiß, Metallic 0, Roughness 0.5, AO 1, flache Normale,
    // Height 0, Emissive schwarz.
    Material();

    // ── Zustand ──────────────────────────────────────────────────────────────
    [[nodiscard]] bool isValid() const noexcept;
    // Alle nicht-konstanten Maps haben dieselbe Größe; 1x1-Maps sind erlaubt.
    [[nodiscard]] bool hasConsistentTextureSize() const noexcept;
    // Größe der Albedo-Map (bei einer Konstante 1x1).
    [[nodiscard]] i32 getWidth() const noexcept;
    [[nodiscard]] i32 getHeight() const noexcept;

    // ── Albedo ───────────────────────────────────────────────────────────────
    void setAlbedo(const Color& color);
    void setAlbedo(const Image& image);
    [[nodiscard]] const Image& getAlbedoMap() const noexcept { return m_albedoMap; }
    [[nodiscard]] bool isAlbedoConstant() const noexcept;

    // ── Metallic ─────────────────────────────────────────────────────────────
    void setMetallic(f32 value);
    // channel: 0 = R/Grau, 1 = G, 2 = B, 3 = A (für gepackte Maps)
    void setMetallic(const Image& image, i32 channel = 0);
    [[nodiscard]] const Image& getMetallicMap() const noexcept { return m_metallicMap; }
    // Nur bei einer konstanten 1x1-Map aussagekraeftig; sonst erster Pixel.
    [[nodiscard]] f32 getMetallicValue() const noexcept;
    [[nodiscard]] bool isMetallicConstant() const noexcept;

    // ── Roughness ────────────────────────────────────────────────────────────
    void setRoughness(f32 value);
    void setRoughness(const Image& image, i32 channel = 0);
    [[nodiscard]] const Image& getRoughnessMap() const noexcept { return m_roughnessMap; }
    // Nur bei einer konstanten 1x1-Map aussagekraeftig; sonst erster Pixel.
    [[nodiscard]] f32 getRoughnessValue() const noexcept;
    [[nodiscard]] bool isRoughnessConstant() const noexcept;

    // ── Ambient Occlusion ────────────────────────────────────────────────────
    void setAmbientOcclusion(f32 value);
    void setAmbientOcclusion(const Image& image, i32 channel = 0);
    [[nodiscard]] const Image& getAmbientOcclusionMap() const noexcept { return m_ambientOcclusionMap; }
    // Nur bei einer konstanten 1x1-Map aussagekraeftig; sonst erster Pixel.
    [[nodiscard]] f32 getAmbientOcclusionValue() const noexcept;
    [[nodiscard]] bool isAmbientOcclusionConstant() const noexcept;

    // ── Normale ──────────────────────────────────────────────────────────────
    // Erwartet eine Tangent-Space-Normalenmap im 0..1-kodierten RGB-Format.
    void setNormal(const Image& image);
    // Unkodierte Normale in [-1, 1]; erzeugt ein 1x1-RGB-Bild.
    void setNormal(const vec3& normal);
    [[nodiscard]] const Image& getNormalMap() const noexcept { return m_normalMap; }
    [[nodiscard]] bool isNormalConstant() const noexcept;

    // ── Height ───────────────────────────────────────────────────────────────
    void setHeight(f32 value);
    void setHeight(const Image& image, i32 channel = 0);
    [[nodiscard]] const Image& getHeightMap() const noexcept { return m_heightMap; }
    // Nur bei einer konstanten 1x1-Map aussagekraeftig; sonst erster Pixel.
    [[nodiscard]] f32 getHeightValue() const noexcept;
    [[nodiscard]] bool isHeightConstant() const noexcept;

    // ── Emissive ─────────────────────────────────────────────────────────────
    void setEmissive(const Color& color);
    void setEmissive(const Image& image);
    [[nodiscard]] const Image& getEmissiveMap() const noexcept { return m_emissiveMap; }
    [[nodiscard]] bool isEmissiveConstant() const noexcept;

private:
    void updateSize() noexcept;

    Image m_albedoMap;
    Image m_metallicMap;
    Image m_roughnessMap;
    Image m_ambientOcclusionMap;
    Image m_normalMap;
    Image m_heightMap;
    Image m_emissiveMap;

    i32 m_width{1};
    i32 m_height{1};
};

}  // namespace sgl
