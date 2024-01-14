#include "collision_detector.h"
#include <cassert>
#include <stdexcept>
#include <cmath>

namespace collision_detector {

const double TOL_TIME = 1e-10;
const double TOL_DIST = 1e-10;

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c) {
    // Проверим, что перемещение ненулевое.
    // Тут приходится использовать строгое равенство, а не приближённое,
    // поскольку при сборе заказов придётся учитывать перемещение даже на небольшое
    // расстояние.
    if (b.x == a.x && b.y == a.y) {
        return CollectionResult(std::pow(b.x - c.x, 2) + std::pow(b.y - c.y, 2), 0.0);
    }
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult(sq_distance, proj_ratio);
}

std::vector<CollisionEvent> FindGatherEvents(
    const ItemGathererProvider& provider) {
    std::vector<CollisionEvent> detected_events;

    static auto eq_pt = [](geom::Point2D p1, geom::Point2D p2) {
        return p1.x == p2.x && p1.y == p2.y;
    };

    for (size_t g = 0; g < provider.GatherersCount(); ++g) {
        Gatherer gatherer = provider.GetGatherer(g);
        if (eq_pt(gatherer.start_pos, gatherer.end_pos)) {
            continue;
        }
        for (size_t i = 0; i < provider.ItemsCount(); ++i) {
            Item item = provider.GetItem(i);
            auto collect_result
                = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, item.position);

            if (collect_result.IsCollected((gatherer.width + item.width) / 2.0)) {
                CollisionEvent evt{.item_id = i,
                                   .gatherer_id = g,
                                   .sq_distance = collect_result.sq_distance,
                                   .time = collect_result.proj_ratio,
                                   .is_base = false};
                detected_events.push_back(evt);
            }
        }
        for (size_t i = 0; i < provider.BaseCount(); ++i) {
            Base base = provider.GetBase(i);
            auto corners = BaseCornersOnWay(base, gatherer);
            if (!corners) {
                continue;
            }

            double collision_time = 2.0;
            CollisionEvent evt{.item_id = 0,
                                .gatherer_id = g,
                                .sq_distance = 0.0,
                                .time = 0.0,
                                .is_base = true};

            for (auto corner : *corners) {
                auto collect_result
                    = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, corner);

                if (collect_result.IsCollected((gatherer.width + 0.0) / 2.0)) {
                    CollisionEvent evt_temp{.item_id = 0,
                                    .gatherer_id = g,
                                    .sq_distance = collect_result.sq_distance,
                                    .time = collect_result.proj_ratio,
                                    .is_base = true};
                    
                    if (evt_temp.time < collision_time) {
                        collision_time = evt_temp.time;
                        evt = evt_temp;
                    }
                }
            }
            detected_events.push_back(evt);
    }

    std::sort(detected_events.begin(), detected_events.end(),
              [](const CollisionEvent& left, const CollisionEvent& right) {
                if (std::abs(left.time - right.time) > TOL_TIME) {
                    return left.time < right.time;
                } else if (std::abs(left.sq_distance - right.sq_distance) > TOL_DIST) {
                    return left.sq_distance < right.sq_distance;
                } else if (left.item_id != right.item_id) {
                    return left.item_id < right.item_id;
                } else if (left.gatherer_id != right.gatherer_id) {
                    return left.gatherer_id < right.gatherer_id;
                }
                throw std::logic_error("logic error - two same item id or gatherer id");
              });
    }

    return detected_events;
}

std::optional<LineSegment> Intersect(LineSegment s1, LineSegment s2) {
    double left = std::max(s1.x1, s2.x1);
    double right = std::min(s1.x2, s2.x2);

    if (right < left) {
        return std::nullopt;
    }
    return LineSegment{.x1 = left, .x2 = right};
} 

// Вычисляем проекции на оси
LineSegment ProjectX(geom::Rect r) {
    return LineSegment{.x1 = r.x, .x2 = r.x + r.w};
}

LineSegment ProjectY(geom::Rect r) {
    return LineSegment{.x1 = r.y, .x2 = r.y + r.h};
}

std::optional<geom::Rect> RectIntersect(const Base& base, const Gatherer& gatherer) {
    
    double x_min_base = base.position.x;
    double x_max_base = x_min_base + base.offset.x;
    double y_min_base = base.position.y;
    double y_max_base = y_min_base + base.offset.y;
    
    
    geom::Rect r1{.x = x_min_base - base.width / 2.0, 
                 .y = y_min_base - base.width / 2.0, 
                 .w = x_max_base - x_min_base + base.width / 2.0,
                 .h = y_max_base - y_min_base + base.width / 2.0};

    double x_min_gatherer = gatherer.start_pos.x;
    double x_max_gatherer = gatherer.end_pos.x;
    double y_min_gatherer = gatherer.start_pos.y;
    double y_max_gatherer = gatherer.end_pos.y;

    if (gatherer.start_pos.x > gatherer.end_pos.x) {
        std::swap(x_min_gatherer, x_max_gatherer);
    }
    
    if (gatherer.start_pos.y > gatherer.end_pos.y) {
        std::swap(y_min_gatherer, y_max_gatherer);
    }

    geom::Rect r2{.x = x_min_gatherer - gatherer.width / 2.0, 
                 .y = y_min_gatherer - gatherer.width / 2.0, 
                 .w = x_max_gatherer - x_min_gatherer + gatherer.width / 2.0,
                 .h = y_max_gatherer - y_min_gatherer + gatherer.width / 2.0};
    
    auto px = Intersect(ProjectX(r1), ProjectX(r2));
    auto py = Intersect(ProjectY(r1), ProjectY(r2));

    if (!px || !py) {
        return std::nullopt;
    }

    // Составляем из проекций прямоугольник
    return geom::Rect{.x = px->x1, .y = py->x1, 
                .w = px->x2 - px->x1, .h = py->x2 - py->x1};
}

std::optional<std::vector<geom::Point2D>> BaseCornersOnWay(const Base& base, const Gatherer& gatherer) {

    if (auto rect_opt = RectIntersect(base, gatherer)) {
        auto rect = *rect_opt;
        return std::vector<geom::Point2D>{{rect.x, rect.y},
                                           {rect.x + rect.w, rect.y},
                                           {rect.x, rect.y + rect.h},
                                           {rect.x + rect.w, rect.y + rect.h}};
    }
    return std::nullopt;
}

}  // namespace collision_detector