#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <vector>

#include "geometry.h"


class Navigation {
public:
    using Polygon = ::Polygon;

    void Build(const Polygon &newBoundary, const std::vector<Polygon> &newObstacles,
               float newCellSize = 0.18f, float newClearance = 0.28f) {
        boundary.clear();
        obstacles.clear();
        cells.clear();
        width = height = 0;
        walkableCount = 0;
        if (newBoundary.size() < 3 || !(newCellSize > 0.0f) || !(newClearance >= 0.0f)) return;

        glm::vec2 minimum = newBoundary.front();
        glm::vec2 maximum = minimum;
        for (glm::vec2 p : newBoundary) {
            minimum = glm::min(minimum, p);
            maximum = glm::max(maximum, p);
        }
        const double columns = std::ceil((double(maximum.x) - minimum.x) / newCellSize);
        const double rows = std::ceil((double(maximum.y) - minimum.y) / newCellSize);
        
        if (columns < 1 || rows < 1 || columns > 2048 || rows > 2048 || columns * rows > 262144) return;

        boundary = newBoundary;
        obstacles = newObstacles;
        cellSize = newCellSize;
        clearance = newClearance;
        origin = minimum;
        width = int(columns);
        height = int(rows);
        cells.resize(std::size_t(width) * height);
        for (int i = 0; i < int(cells.size()); ++i) {
            cells[i].walkable = Contains(Position(i));
            if (cells[i].walkable) ++walkableCount;
        }

        for (int i = 0; i < int(cells.size()); ++i) {
            if (!cells[i].walkable) continue;
            for (int direction : {0, 2}) {
                const int other = Neighbor(i, direction);
                if (other >= 0 && cells[other].walkable && SegmentEdgesClear(Position(i), Position(other)))
                    Link(i, other, direction);
            }
        }
        for (int i = 0; i < int(cells.size()); ++i) {
            if (!cells[i].walkable) continue;
            for (int direction : {1, 3}) {
                const int other = Neighbor(i, direction);
                const int first = (direction + 7) % 8;
                const int second = (direction + 1) % 8;
                if (other < 0 || !cells[other].walkable ||
                    !Linked(i, first) || !Linked(i, second) ||
                    !Linked(other, (first + 4) % 8) || !Linked(other, (second + 4) % 8)) continue;
                if (SegmentEdgesClear(Position(i), Position(other))) Link(i, other, direction);
            }
        }


        int component = 0;
        std::vector<int> pending;
        for (int i = 0; i < int(cells.size()); ++i) {
            if (!cells[i].walkable || cells[i].component >= 0) continue;
            pending.clear();
            pending.push_back(i);
            cells[i].component = component;
            for (std::size_t cursor = 0; cursor < pending.size(); ++cursor) {
                const int current = pending[cursor];
                for (int direction = 0; direction < 8; ++direction) {
                    if (!Linked(current, direction)) continue;
                    const int next = Neighbor(current, direction);
                    if (cells[next].component >= 0) continue;
                    cells[next].component = component;
                    pending.push_back(next);
                }
            }
            ++component;
        }
    }

    bool Empty() const { return walkableCount == 0; }

    bool HasWalkablePoint(const std::function<bool(glm::vec2)> &accepts) const {
        for (int i = 0; i < int(cells.size()); ++i)
            if (cells[i].walkable && accepts(Position(i))) return true;
        return false;
    }

 
    glm::vec2 ClosestPoint(glm::vec2 point) const {
        if (Empty() || !Finite(point) || Contains(point)) return point;
        int closest = -1;
        double distance = std::numeric_limits<double>::infinity();
        for (int i = 0; i < int(cells.size()); ++i) {
            if (!cells[i].walkable) continue;
            const double candidateDistance = DistanceSquared(Position(i), point);
            if (candidateDistance < distance) {
                closest = i;
                distance = candidateDistance;
            }
        }
        return closest >= 0 ? Position(closest) : point;
    }

    bool Contains(glm::vec2 point) const {
        if (!Finite(point) || boundary.size() < 3 || !PointInPolygon(point, boundary)) return false;
        if (!PointClearOfEdges(point, boundary)) return false;
        for (const Polygon &obstacle : obstacles)
            if (PointInPolygon(point, obstacle) || !PointClearOfEdges(point, obstacle)) return false;
        return true;
    }

    bool SegmentClear(glm::vec2 a, glm::vec2 b) const {
        if (!Contains(a) || !Contains(b)) return false;
        return SegmentEdgesClear(a, b);
    }

    std::vector<glm::vec2> FindPath(glm::vec2 start, glm::vec2 goal,
                                    const std::function<bool(glm::vec2)> &acceptsGoal = {}) const {
        if (Empty() || !Finite(goal) || !Contains(start)) return {};
        const bool goalAllowed = !acceptsGoal || acceptsGoal(goal);
        if (DistanceSquared(start, goal) < 1e-12 && goalAllowed) return {};
        if (goalAllowed && SegmentClear(start, goal)) return {goal};

        std::vector<int> candidates;
        candidates.reserve(walkableCount);
        for (int i = 0; i < int(cells.size()); ++i)
            if (cells[i].walkable) candidates.push_back(i);
        SortByDistance(candidates, start);
        int source = -1;
        for (int candidate : candidates) {
            if (SegmentClear(start, Position(candidate))) {
                source = candidate;
                break;
            }
        }
        if (source < 0) return {};

        const int component = cells[source].component;
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
            [&](int candidate) { return cells[candidate].component != component; }),
            candidates.end());
        SortByDistance(candidates, goal);
        int destination = -1;
        glm::vec2 endpoint{};
        if (goalAllowed && Contains(goal)) {

            for (int candidate : candidates) {
                if (SegmentClear(Position(candidate), goal)) {
                    destination = candidate;
                    endpoint = goal;
                    break;
                }
            }
        }

        if (destination < 0) {
            for (int candidate : candidates) {
                if (acceptsGoal && !acceptsGoal(Position(candidate))) continue;
                destination = candidate;
                endpoint = Position(candidate);
                break;
            }
        }
        if (destination < 0) return {};

        auto route = SearchGridRoute(source, destination);
        if (route.empty()) return {};
        if (DistanceSquared(route.back(), endpoint) > 1e-12) route.push_back(endpoint);
        return SmoothRoute(start, route);
    }

private:
    struct Cell {
        bool walkable = false;
        std::uint8_t links = 0;
        int component = -1;
    };
    inline static constexpr std::array<int, 8> DX = {1, 1, 0, -1, -1, -1, 0, 1};
    inline static constexpr std::array<int, 8> DY = {0, 1, 1, 1, 0, -1, -1, -1};

    Polygon boundary;
    std::vector<Polygon> obstacles;
    std::vector<Cell> cells;
    glm::vec2 origin{0.0f};
    float cellSize = 0.18f;
    float clearance = 0.28f;
    int width = 0;
    int height = 0;
    std::size_t walkableCount = 0;

    std::vector<glm::vec2> SearchGridRoute(int source, int destination) const {
        struct QueueItem { double score; int index; };
        const auto later = [](const QueueItem &a, const QueueItem &b) {
            return a.score != b.score ? a.score > b.score : a.index > b.index;
        };
        std::priority_queue<QueueItem, std::vector<QueueItem>, decltype(later)> open(later);
        const double infinity = std::numeric_limits<double>::infinity();
        std::vector<double> cost(cells.size(), infinity);
        std::vector<int> parent(cells.size(), -1);
        std::vector<bool> closed(cells.size(), false);
        const auto heuristic = [&](int i) {
            return std::sqrt(DistanceSquared(Position(i), Position(destination)));
        };
        cost[source] = 0;
        open.push({heuristic(source), source});
        while (!open.empty()) {
            const int current = open.top().index;
            open.pop();
            if (closed[current]) continue;
            closed[current] = true;
            if (current == destination) break;
            for (int direction = 0; direction < 8; ++direction) {
                if (!Linked(current, direction)) continue;
                const int next = Neighbor(current, direction);
                if (closed[next]) continue;
                const double nextCost = cost[current] + std::sqrt(DistanceSquared(Position(current), Position(next)));
                if (nextCost >= cost[next]) continue;
                cost[next] = nextCost;
                parent[next] = current;
                open.push({nextCost + heuristic(next), next});
            }
        }
        if (!closed[destination]) return {};

        std::vector<glm::vec2> route;
        for (int current = destination; current >= 0; current = parent[current]) {
            route.push_back(Position(current));
            if (current == source) break;
        }
        std::reverse(route.begin(), route.end());
        return route;
    }

    std::vector<glm::vec2> SmoothRoute(glm::vec2 start, const std::vector<glm::vec2> &route) const {
        std::vector<glm::vec2> result;
        glm::vec2 current = start;
        for (std::size_t next = 0; next < route.size();) {
            std::size_t furthest = route.size() - 1;
            while (furthest > next && !SegmentClear(current, route[furthest])) --furthest;
            if (!SegmentClear(current, route[furthest])) return {};
            if (DistanceSquared(current, route[furthest]) > 1e-12) {
                result.push_back(route[furthest]);
                current = route[furthest];
            }
            next = furthest + 1;
        }
        return result;
    }

    bool SegmentEdgesClear(glm::vec2 a, glm::vec2 b) const {
        if (!SegmentClearOfEdges(a, b, boundary)) return false;
        for (const Polygon &obstacle : obstacles)
            if (!SegmentClearOfEdges(a, b, obstacle)) return false;
        return true;
    }

    static double SegmentDistanceSquared(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d) {
        if (SegmentsIntersect(a, b, c, d)) return 0;
        return std::min({PointSegmentDistanceSquared(a, c, d), PointSegmentDistanceSquared(b, c, d),
                         PointSegmentDistanceSquared(c, a, b), PointSegmentDistanceSquared(d, a, b)});
    }

    double MarginSquared() const {
        const double margin = double(clearance) + 1e-6;
        return margin * margin;
    }
    bool PointClearOfEdges(glm::vec2 p, const Polygon &polygon) const {
        for (std::size_t i = 0; i < polygon.size(); ++i)
            if (PointSegmentDistanceSquared(p, polygon[i], polygon[(i + 1) % polygon.size()]) <= MarginSquared())
                return false;
        return true;
    }
    bool SegmentClearOfEdges(glm::vec2 a, glm::vec2 b, const Polygon &polygon) const {
        for (std::size_t i = 0; i < polygon.size(); ++i)
            if (SegmentDistanceSquared(a, b, polygon[i], polygon[(i + 1) % polygon.size()]) <= MarginSquared())
                return false;
        return true;
    }
    glm::vec2 Position(int index) const {
        return {float(double(origin.x) + (index % width + 0.5) * cellSize),
                float(double(origin.y) + (index / width + 0.5) * cellSize)};
    }
    int Neighbor(int index, int direction) const {
        const int x = index % width + DX[direction];
        const int y = index / width + DY[direction];
        return x >= 0 && x < width && y >= 0 && y < height ? y * width + x : -1;
    }
    bool Linked(int index, int direction) const {
        return (cells[index].links & (1u << direction)) != 0;
    }
    void Link(int a, int b, int direction) {
        cells[a].links |= std::uint8_t(1u << direction);
        cells[b].links |= std::uint8_t(1u << ((direction + 4) % 8));
    }
    void SortByDistance(std::vector<int> &indices, glm::vec2 point) const {
        std::sort(indices.begin(), indices.end(), [&](int a, int b) {
            const double distanceA = DistanceSquared(Position(a), point);
            const double distanceB = DistanceSquared(Position(b), point);
            return distanceA != distanceB ? distanceA < distanceB : a < b;
        });
    }
};

#endif
