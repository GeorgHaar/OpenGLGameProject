#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

using Polygon = std::vector<glm::vec2>;

inline bool Finite(glm::vec2 p) {
    return std::isfinite(p.x) && std::isfinite(p.y);
}

inline double DistanceSquared(glm::vec2 a, glm::vec2 b) {
    const double dx = double(a.x) - b.x;
    const double dy = double(a.y) - b.y;
    return dx * dx + dy * dy;
}

inline double Cross(glm::vec2 u, glm::vec2 v) {
    return double(u.x) * v.y - double(u.y) * v.x;
}

inline double Cross(glm::vec2 a, glm::vec2 b, glm::vec2 c) {
    return (double(b.x) - a.x) * (double(c.y) - a.y) - (double(b.y) - a.y) * (double(c.x) - a.x);
}

inline glm::vec2 ClosestPointOnSegment(glm::vec2 p, glm::vec2 a, glm::vec2 b) {
    const glm::vec2 edge = b - a;
    const float length2 = glm::dot(edge, edge);
    const float t = length2 > 0.0f ? glm::clamp(glm::dot(p - a, edge) / length2, 0.0f, 1.0f) : 0.0f;
    return a + t * edge;
}

inline double PointSegmentDistanceSquared(glm::vec2 p, glm::vec2 a, glm::vec2 b) {
    const double dx = double(b.x) - a.x;
    const double dy = double(b.y) - a.y;
    const double length2 = dx * dx + dy * dy;
    if (length2 == 0) return DistanceSquared(p, a);
    const double t = std::clamp(((double(p.x) - a.x) * dx + (double(p.y) - a.y) * dy) / length2, 0.0, 1.0);
    const double x = double(p.x) - (a.x + t * dx);
    const double y = double(p.y) - (a.y + t * dy);
    return x * x + y * y;
}

inline bool PointOnSegment(glm::vec2 p, glm::vec2 a, glm::vec2 b) {
    return PointSegmentDistanceSquared(p, a, b) <= 1e-12;
}

inline bool SegmentsIntersect(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d) {
    const double abc = Cross(a, b, c), abd = Cross(a, b, d);
    const double cda = Cross(c, d, a), cdb = Cross(c, d, b);
    if (((abc > 0 && abd < 0) || (abc < 0 && abd > 0)) &&
        ((cda > 0 && cdb < 0) || (cda < 0 && cdb > 0))) return true;
    return PointOnSegment(c, a, b) || PointOnSegment(d, a, b) ||
           PointOnSegment(a, c, d) || PointOnSegment(b, c, d);
}

inline bool PointInPolygon(glm::vec2 p, const Polygon &polygon) {
    if (polygon.size() < 3) return false;
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const glm::vec2 a = polygon[j], b = polygon[i];
        if (PointOnSegment(p, a, b)) return true;
        if ((a.y > p.y) != (b.y > p.y) &&
            double(p.x) < (double(b.x) - a.x) * (double(p.y) - a.y) / (double(b.y) - a.y) + a.x)
            inside = !inside;
    }
    return inside;
}

inline glm::vec2 ClosestPointOnOutline(glm::vec2 p, const Polygon &polygon) {
    glm::vec2 best = polygon.empty() ? p : polygon.front();
    double bestDistance = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const glm::vec2 candidate = ClosestPointOnSegment(p, polygon[i], polygon[(i + 1) % polygon.size()]);
        const double distance = DistanceSquared(p, candidate);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
        }
    }
    return best;
}

inline float PolygonDistance(glm::vec2 p, const Polygon &polygon) {
    if (PointInPolygon(p, polygon)) return 0.0f;
    return glm::length(p - ClosestPointOnOutline(p, polygon));
}

inline double SignedArea2(const Polygon &polygon) {
    double area = 0;
    for (std::size_t i = 0; i < polygon.size(); ++i)
        area += Cross(polygon[i], polygon[(i + 1) % polygon.size()]);
    return area;
}

inline bool SimplePolygon(const Polygon &polygon, std::string &error) {
    if (polygon.size() < 3) {
        error = "Eine Kontur braucht mindestens drei Eckpunkte.";
        return false;
    }
    for (glm::vec2 point : polygon) {
        if (!Finite(point)) {
            error = "Alle Eckpunkte muessen endliche Koordinaten haben.";
            return false;
        }
    }
    const std::size_t n = polygon.size();
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t next = (i + 1) % n;
        const std::size_t after = (i + 2) % n;
        const glm::vec2 edge = polygon[next] - polygon[i];
        if (glm::dot(edge, edge) < 1e-8f) {
            error = "Zwei benachbarte Eckpunkte liegen aufeinander.";
            return false;
        }
        if (std::abs(Cross(polygon[i], polygon[next], polygon[after])) <= 1e-6 &&
            glm::dot(edge, polygon[after] - polygon[next]) < 0.0f) {
            error = "Die Kontur darf nicht auf sich selbst zurueck laufen.";
            return false;
        }
        for (std::size_t j = i + 1; j < n; ++j) {
            const std::size_t jnext = (j + 1) % n;
            if (j == next || jnext == i) continue;
            if (SegmentsIntersect(polygon[i], polygon[next], polygon[j], polygon[jnext])) {
                error = "Die Kanten duerfen sich nicht schneiden oder beruehren.";
                return false;
            }
        }
    }
    if (std::abs(SignedArea2(polygon)) < 1e-4) {
        error = "Die Eckpunkte muessen eine Flaeche einschliessen.";
        return false;
    }
    return true;
}

#endif
