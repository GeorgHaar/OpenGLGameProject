#ifndef WALKDOCUMENT_H
#define WALKDOCUMENT_H

#include "assetfiles.h"
#include "walkarea.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

struct SceneTransition {
    std::string name;
    Polygon polygon;
    std::string targetScene;
};

inline bool operator==(const SceneTransition &a, const SceneTransition &b) {
    return a.name == b.name && a.polygon == b.polygon && a.targetScene == b.targetScene;
}

struct SceneProp {
    std::string model;
    std::string name;
    glm::vec2 foot{0.0f};
    float height = 0.5f;
    float rotation = 0.0f;
};

inline bool operator==(const SceneProp &a, const SceneProp &b) {
    return a.model == b.model && a.name == b.name && a.foot == b.foot &&
        a.height == b.height && a.rotation == b.rotation;
}

struct WalkDocument {
    std::string background;
    WalkArea area;
    Camera camera{0.62f, 6.2f, 45.0f};
    glm::vec2 spawn{0.0f};
    std::vector<SceneTransition> transitions;
    float spongebobSize = 1.0f;
    std::vector<SceneProp> props;
};

inline bool operator==(const WalkDocument &a, const WalkDocument &b) {
    return a.background == b.background && a.area == b.area && a.camera == b.camera &&
        a.spawn == b.spawn && a.transitions == b.transitions &&
        a.spongebobSize == b.spongebobSize && a.props == b.props;
}
inline bool operator!=(const WalkDocument &a, const WalkDocument &b) { return !(a == b); }

constexpr float PROP_MIN_HEIGHT = 0.01f;
constexpr float PROP_MAX_HEIGHT = 100.0f;
constexpr std::size_t NAME_MAX_BYTES = 128;

inline bool ValidText(const std::string &text, std::size_t maxBytes) {
    if (text.empty() || text.size() > maxBytes) return false;
    for (unsigned char c : text) if (c < 32 || c == 127) return false;
    return true;
}

inline float PropHorizonPixel(const Camera &camera, glm::vec2 imageSize) {
    const float tangent = std::tan(glm::radians(camera.sichtfeld * 0.5f));
    const float minBelow = std::max(0.001f, camera.augenhoehe / (2.0f * tangent * Camera::FERN));
    return (1.0f - camera.horizont + minBelow) * imageSize.y;
}

inline bool ValidContour(const Polygon &polygon, float horizon, const std::string &label, std::string &error) {
    for (glm::vec2 point : polygon) {
        if (point.y <= horizon) {
            error = label + ": Die Kontur muss auf dem Boden liegen. Verschiebe die betroffenen Punkte weiter nach unten.";
            return false;
        }
    }
    std::string reason;
    if (!SimplePolygon(polygon, reason)) {
        error = label + ": " + reason;
        return false;
    }
    return true;
}

inline bool ValidateWalkDocument(const WalkDocument &document, std::string &error) {
    error.clear();
    if (!ValidRelativeAssetPath(document.background)) {
        error = "Der Hintergrund braucht einen relativen Dateinamen ohne '..' oder absolute Pfade.";
        return false;
    }
    const WalkArea &area = document.area;
    const Camera &camera = document.camera;
    if (!Finite(area.imageSize) || area.imageSize.x <= 0 || area.imageSize.y <= 0) {
        error = "Die Bildgroesse muss groesser als 0 sein.";
        return false;
    }
    if (!std::isfinite(camera.horizont) || camera.horizont < 0 || camera.horizont > 1 ||
        !std::isfinite(camera.augenhoehe) || camera.augenhoehe <= 0 ||
        !std::isfinite(camera.sichtfeld) || camera.sichtfeld <= 0 || camera.sichtfeld >= 179) {
        error = "Ungueltige Kamera: Horizont 0 bis 1, positive Augenhoehe und Sichtfeld zwischen 0 und 179 Grad.";
        return false;
    }
    if (!std::isfinite(area.clearance) || area.clearance < 0 ||
        !std::isfinite(area.cellSize) || area.cellSize <= 0) {
        error = "Koerperabstand muss mindestens 0 und Rasterweite groesser als 0 sein.";
        return false;
    }
    if (!std::isfinite(document.spongebobSize) || document.spongebobSize < 0.25f || document.spongebobSize > 4.0f) {
        error = "SpongeBobs Groesse muss zwischen 25 und 400 Prozent liegen.";
        return false;
    }
    const float horizon = (1.0f - camera.horizont + 0.001f) * area.imageSize.y;
    if (!ValidContour(area.boundary, horizon, "Laufbereich", error)) return false;
    for (std::size_t i = 0; i < area.obstacles.size(); ++i)
        if (!ValidContour(area.obstacles[i], horizon, "Hindernis " + std::to_string(i + 1), error)) return false;
    for (std::size_t i = 0; i < document.transitions.size(); ++i) {
        const SceneTransition &transition = document.transitions[i];
        const std::string label = "Szenenuebergang " + std::to_string(i + 1);
        if (!ValidText(transition.name, NAME_MAX_BYTES)) {
            error = label + ": Der Name muss 1 bis 128 Bytes lang sein und darf keine Steuerzeichen enthalten.";
            return false;
        }
        if (!ValidText(transition.targetScene, 1024)) {
            error = label + ": Waehle eine Zielszene.";
            return false;
        }
        if (!ValidContour(transition.polygon, -std::numeric_limits<float>::infinity(), label, error)) return false;
    }
    if (!Finite(document.spawn) || document.spawn.y <= horizon) {
        error = "Der Startpunkt muss unterhalb des Horizonts auf dem Boden liegen.";
        return false;
    }
    const float propHorizon = PropHorizonPixel(camera, area.imageSize);
    for (std::size_t i = 0; i < document.props.size(); ++i) {
        const SceneProp &prop = document.props[i];
        const std::string label = "3D-Objekt " + std::to_string(i + 1) +
            (prop.name.empty() ? std::string() : " '" + prop.name + "'");
        if (!ValidModelName(prop.model)) {
            error = label + ": Das Modell braucht einen relativen Pfad unter assets/models mit der Endung .glb oder .gltf.";
            return false;
        }
        if (!ValidText(prop.name, NAME_MAX_BYTES)) {
            error = label + ": Der Name muss 1 bis 128 Bytes lang sein und darf keine Steuerzeichen enthalten.";
            return false;
        }
        if (!Finite(prop.foot) || prop.foot.y <= propHorizon) {
            error = label + ": Der Fusspunkt liegt am oder ueber dem Horizont; so weit weg stuende das Objekt hinter der Sichtweite der Kamera. Verschiebe es weiter nach unten.";
            return false;
        }
        if (!std::isfinite(prop.height) || prop.height < PROP_MIN_HEIGHT || prop.height > PROP_MAX_HEIGHT) {
            error = label + ": Die Hoehe muss zwischen 0,01 und 100 SpongeBob-Hoehen liegen.";
            return false;
        }
        if (!std::isfinite(prop.rotation)) {
            error = label + ": Die Drehung ist ungueltig.";
            return false;
        }
    }

    const float aspect = area.imageSize.x / area.imageSize.y;
    std::vector<Polygon> obstacles;
    for (const Polygon &polygon : area.obstacles) obstacles.push_back(area.ToGround(polygon, camera, aspect));
    Navigation navigation;
    navigation.Build(area.ToGround(area.boundary, camera, aspect), obstacles, area.cellSize, area.clearance);
    if (navigation.Empty()) {
        error = "Der Laufbereich enthaelt keinen begehbaren Boden. Pruefe Flaechen, Rasterweite und Koerperabstand.";
        return false;
    }
    const glm::vec3 spawn = area.ToGround(document.spawn, camera, aspect);
    if (!navigation.Contains({spawn.x, spawn.z})) {
        error = "Der Startpunkt liegt ausserhalb des begehbaren Bereichs oder zu nah an einem Hindernis.";
        return false;
    }
    return true;
}

class WalkDocumentStore {
public:
    explicit WalkDocumentStore(std::filesystem::path assetsRoot) : assetsRoot(std::move(assetsRoot)) {}

    std::filesystem::path PathFor(const std::string &background) const {
        if (!ValidRelativeAssetPath(background)) return {};
        std::filesystem::path result = assetsRoot / "navigation" / background;
        result += ".walk";
        return result;
    }

    bool Load(const std::string &background, WalkDocument &out, std::string &error) const {
        error.clear();
        const auto path = PathFor(background);
        if (path.empty()) {
            error = "Ungueltiger Hintergrundpfad fuer die Laufkarte.";
            return false;
        }
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) return false;
        std::ifstream input(path);
        if (!input) {
            error = "Die Laufkartendatei kann nicht geoeffnet werden.";
            return false;
        }
        input.imbue(std::locale::classic());
        WalkDocument candidate;
        auto token = [&](const char *expected) {
            std::string actual;
            if (!(input >> actual) || actual != expected) {
                error = std::string("Ungueltiges Laufkartenformat bei '") + expected + "'.";
                return false;
            }
            return true;
        };
        auto polygon = [&](const char *keyword, Polygon &result) {
            int count = 0;
            if (!token(keyword) || !(input >> count) || count < 0) {
                error = "Die Laufkartendatei enthaelt eine ungueltige Anzahl Eckpunkte.";
                return false;
            }
            result.clear();
            for (int i = 0; i < count; ++i) {
                glm::vec2 point;
                if (!(input >> point.x >> point.y)) {
                    error = "Die Laufkartendatei enthaelt unvollstaendige Eckpunkte.";
                    return false;
                }
                result.push_back(point);
            }
            return true;
        };
        int version = 0;
        if (!token("SPONGEBOB_WALK") || !(input >> version) || version < 1 || version > 4) {
            error = "Die Laufkartendatei hat ein unbekanntes Format oder eine nicht unterstuetzte Version.";
            return false;
        }
        if (!token("background") || !(input >> std::quoted(candidate.background)) ||
            !token("image_size") || !(input >> candidate.area.imageSize.x >> candidate.area.imageSize.y) ||
            !token("camera") || !(input >> candidate.camera.horizont >> candidate.camera.augenhoehe >> candidate.camera.sichtfeld) ||
            !token("spawn") || !(input >> candidate.spawn.x >> candidate.spawn.y) ||
            !token("clearance") || !(input >> candidate.area.clearance) ||
            !token("cell_size") || !(input >> candidate.area.cellSize)) {
            if (error.empty()) error = "Die Laufkartendatei enthaelt fehlende oder ungueltige Werte.";
            return false;
        }
        if (!polygon("boundary", candidate.area.boundary)) return false;
        int obstacleCount = 0;
        if (!token("obstacles") || !(input >> obstacleCount) || obstacleCount < 0) {
            error = "Die Laufkartendatei enthaelt eine ungueltige Anzahl Hindernisse.";
            return false;
        }
        candidate.area.obstacles.resize(obstacleCount);
        for (Polygon &obstacle : candidate.area.obstacles)
            if (!polygon("polygon", obstacle)) return false;
        if (version >= 2) {
            int transitionCount = 0;
            if (!token("transitions") || !(input >> transitionCount) || transitionCount < 0) {
                error = "Die Laufkartendatei enthaelt eine ungueltige Anzahl Szenenuebergaenge.";
                return false;
            }
            candidate.transitions.resize(transitionCount);
            for (SceneTransition &transition : candidate.transitions) {
                if (!token("name") || !(input >> std::quoted(transition.name)) ||
                    !token("target") || !(input >> std::quoted(transition.targetScene))) {
                    if (error.empty()) error = "Die Laufkartendatei enthaelt unvollstaendige Szenenuebergaenge.";
                    return false;
                }
                if (!polygon("polygon", transition.polygon)) return false;
            }
        }
        if (version >= 3) {
            int sizeCount = 0;
            if (!token("scene_sizes") || !(input >> sizeCount) || sizeCount < 0) {
                error = "Die Laufkartendatei enthaelt eine ungueltige Anzahl Szenengroessen.";
                return false;
            }
            for (int i = 0; i < sizeCount; ++i) {
                std::string sceneId;
                float size = 1.0f;
                if (!token("scene_size") || !(input >> std::quoted(sceneId) >> size)) {
                    error = "Die Laufkartendatei enthaelt eine unvollstaendige oder ungueltige Szenengroesse.";
                    return false;
                }
                if (sceneId == "background:" + candidate.background) candidate.spongebobSize = size;
            }
        }
        if (version >= 4) {
            int propCount = 0;
            if (!token("props") || !(input >> propCount) || propCount < 0) {
                error = "Die Laufkartendatei enthaelt eine ungueltige Anzahl 3D-Objekte.";
                return false;
            }
            candidate.props.resize(propCount);
            for (SceneProp &prop : candidate.props) {
                if (!token("model") || !(input >> std::quoted(prop.model)) ||
                    !token("name") || !(input >> std::quoted(prop.name)) ||
                    !token("foot") || !(input >> prop.foot.x >> prop.foot.y) ||
                    !token("height") || !(input >> prop.height) ||
                    !token("rotation") || !(input >> prop.rotation)) {
                    if (error.empty()) error = "Die Laufkartendatei enthaelt unvollstaendige 3D-Objekte.";
                    return false;
                }
            }
        }
        if (!token("end")) return false;
        if (candidate.background != background) {
            error = "Die gespeicherte Laufkarte gehoert zu einem anderen Hintergrund.";
            return false;
        }
        if (!ValidateWalkDocument(candidate, error)) return false;
        out = std::move(candidate);
        return true;
    }

    bool Save(const WalkDocument &document, std::string &error) const {
        if (!ValidateWalkDocument(document, error)) return false;
        const auto path = PathFor(document.background);
        std::ostringstream text;
        text.imbue(std::locale::classic());
        text << std::setprecision(std::numeric_limits<float>::max_digits10);
        text << "SPONGEBOB_WALK 4\nbackground " << std::quoted(document.background)
             << "\nimage_size " << document.area.imageSize.x << ' ' << document.area.imageSize.y
             << "\ncamera " << document.camera.horizont << ' ' << document.camera.augenhoehe << ' ' << document.camera.sichtfeld
             << "\nspawn " << document.spawn.x << ' ' << document.spawn.y
             << "\nclearance " << document.area.clearance
             << "\ncell_size " << document.area.cellSize << '\n';
        auto polygon = [&](const char *keyword, const Polygon &points) {
            text << keyword << ' ' << points.size() << '\n';
            for (glm::vec2 point : points) text << "  " << point.x << ' ' << point.y << '\n';
        };
        polygon("boundary", document.area.boundary);
        text << "obstacles " << document.area.obstacles.size() << '\n';
        for (const Polygon &obstacle : document.area.obstacles) polygon("polygon", obstacle);
        text << "transitions " << document.transitions.size() << '\n';
        for (const SceneTransition &transition : document.transitions) {
            text << "name " << std::quoted(transition.name) << '\n'
                 << "target " << std::quoted(transition.targetScene) << '\n';
            polygon("polygon", transition.polygon);
        }
        text << "scene_sizes 1\nscene_size " << std::quoted("background:" + document.background)
             << ' ' << document.spongebobSize << '\n';
        text << "props " << document.props.size() << '\n';
        for (const SceneProp &prop : document.props) {
            text << "model " << std::quoted(prop.model) << '\n'
                 << "name " << std::quoted(prop.name) << '\n'
                 << "foot " << prop.foot.x << ' ' << prop.foot.y << '\n'
                 << "height " << prop.height << '\n'
                 << "rotation " << prop.rotation << '\n';
        }
        text << "end\n";

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            error = "Der Laufkartenordner kann nicht angelegt werden: " + ec.message();
            return false;
        }
        std::filesystem::path temporary = path;
        temporary += ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            error = "Die Laufkartendatei kann nicht angelegt werden.";
            return false;
        }
        output << text.str();
        output.close();
        if (!output) {
            std::filesystem::remove(temporary, ec);
            error = "Die Laufkarte konnte nicht vollstaendig geschrieben werden. Die bisherige Datei bleibt erhalten.";
            return false;
        }
        std::filesystem::rename(temporary, path, ec);
        if (ec) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            error = "Die Laufkarte konnte nicht ersetzt werden. Die bisherige Datei bleibt erhalten: " + ec.message();
            return false;
        }
        return true;
    }

private:
    std::filesystem::path assetsRoot;
};

#endif
