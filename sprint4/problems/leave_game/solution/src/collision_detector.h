#pragma once

#include "geom.h"

#include <algorithm>
#include <vector>
#include <optional>

namespace collision_detector {

struct CollectionResult {
    bool IsCollected(double collect_radius) const {
        return proj_ratio >= 0 && proj_ratio <= 1 && sq_distance <= collect_radius * collect_radius;
    }
    // квадрат расстояния до точки
    double sq_distance;
    // доля пройденного отрезка
    double proj_ratio;
};

// Движемся из точки a в точку b и пытаемся подобрать точку c.
CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c);

struct Item {
    geom::Point2D position;
    double width;
};

struct Gatherer {
    geom::Point2D start_pos;
    geom::Point2D end_pos;
    double width;
};

struct Base {
    geom::Point2D position;
    geom::Point2D offset;
    double width;
};

class ItemGathererProvider {
protected:
    ~ItemGathererProvider() = default;

public:
    virtual size_t ItemsCount() const = 0;
    virtual Item GetItem(size_t idx) const = 0;
    virtual size_t GatherersCount() const = 0;
    virtual Gatherer GetGatherer(size_t idx) const = 0;
    virtual size_t BaseCount() const = 0;
    virtual Base GetBase(size_t idx) const = 0;
};

struct CollisionEvent {
    size_t item_id;
    size_t gatherer_id;
    double sq_distance;
    double time;
    bool is_base;

    // auto operator<=>(const CollisionEvent&) const = default;
};

std::vector<CollisionEvent> FindGatherEvents(const ItemGathererProvider& provider);


struct LineSegment {
    // Предполагаем, что x1 <= x2
    double x1, x2;
};

std::optional<LineSegment> Intersect(LineSegment s1, LineSegment s2);

// Вычисляем проекции на оси
LineSegment ProjectX(geom::Rect r);

LineSegment ProjectY(geom::Rect r);

std::optional<geom::Rect> RectIntersect(const Base& base, const Gatherer& gatherer);

std::optional<std::vector<geom::Point2D>> BaseCornersOnWay(const Base& base, const Gatherer& gatherer);

}  // namespace collision_detector