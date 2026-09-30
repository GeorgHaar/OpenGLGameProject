#ifndef SCENETRIGGERS_H
#define SCENETRIGGERS_H

#include "walkdocument.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

class SceneTriggers {
public:
    void Reset(const std::vector<SceneTransition> &transitions, glm::vec2 position) {
        inside.clear();
        for (const auto &transition : transitions)
            inside.push_back(PointInPolygon(position, transition.polygon));
    }

    int Advance(const std::vector<SceneTransition> &transitions,
                const std::vector<glm::vec2> &trace) {
        if (trace.empty()) return -1;
        if (inside.size() != transitions.size()) Reset(transitions, trace.front());
        for (std::size_t step = 1; step < trace.size(); ++step) {
            const glm::vec2 a = trace[step - 1], b = trace[step];
            if (a == b) continue;
            double first = std::numeric_limits<double>::infinity();
            int hit = -1;
            for (std::size_t i = 0; i < transitions.size(); ++i) {
                const auto &transition = transitions[i];
                if (!transition.targetScene.empty()) {
                    const double entry = Entry(transition.polygon, a, b, inside[i]);
                    if (entry < first) {
                        first = entry;
                        hit = static_cast<int>(i);
                    }
                }
                inside[i] = PointInPolygon(b, transition.polygon);
            }
            if (hit >= 0) return hit;
        }
        return -1;
    }

private:
    std::vector<bool> inside;

    static double Entry(const Polygon &polygon, glm::vec2 a, glm::vec2 b, bool initiallyInside) {
        const double none = std::numeric_limits<double>::infinity();
        if (polygon.size() < 3) return none;
        const glm::vec2 direction = b - a;
        std::vector<double> cuts{0, 1};
        for (std::size_t i = 0; i < polygon.size(); ++i) {
            const glm::vec2 p = polygon[i], q = polygon[(i + 1) % polygon.size()];
            const glm::vec2 edge = q - p;
            const double denominator = Cross(direction, edge);
            if (std::abs(denominator) < 1e-12) continue;
            const double t = Cross(p - a, edge) / denominator;
            const double u = Cross(p - a, direction) / denominator;
            if (t >= 0 && t <= 1 && u >= 0 && u <= 1) cuts.push_back(t);
        }
        std::sort(cuts.begin(), cuts.end());
        bool wasInside = initiallyInside;
        for (std::size_t i = 0; i < cuts.size(); ++i) {
            const glm::vec2 point = a + direction * static_cast<float>(cuts[i]);
            if (!wasInside && PointInPolygon(point, polygon)) return cuts[i];
            if (i + 1 < cuts.size()) {
                const float midpoint = static_cast<float>((cuts[i] + cuts[i + 1]) * 0.5);
                const bool nextInside = PointInPolygon(a + direction * midpoint, polygon);
                if (!wasInside && nextInside) return cuts[i];
                wasInside = nextInside;
            }
        }
        return none;
    }
};

#endif
