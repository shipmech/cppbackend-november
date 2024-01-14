#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

namespace Catch {

template<>
struct StringMaker<collision_detector::CollisionEvent> {
  static std::string convert(collision_detector::CollisionEvent const& value) {
      std::ostringstream tmp;
      tmp << "(" << value.item_id << "," << value.gatherer_id << "," << value.sq_distance << "," << value.time << ")";

      return tmp.str();
  }
};

}  // namespace Catch

double EPSILON = 1e-10;

using namespace std::literals;

using Catch::Matchers::Contains;
using Catch::Matchers::Predicate;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::SizeIs;
using Catch::Matchers::IsEmpty;


class ItemGathererProviderTest : public collision_detector::ItemGathererProvider {

public:
    explicit ItemGathererProviderTest(std::vector<collision_detector::Item> items,
                                       std::vector<collision_detector::Gatherer> gatherers)
        : items_(std::move(items))
        , gatherers_(std::move(gatherers)) {
    }

    size_t ItemsCount() const override {
        return items_.size();
    }

    collision_detector::Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

    size_t BaseCount() const override {
        return base_.size();
    }

    collision_detector::Base GetBase(size_t idx) const override {
        return base_.at(idx);
    }

private:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
    std::vector<collision_detector::Base> base_;
};

struct ItemGathererTestData {

    ItemGathererTestData(double w, double W,
                         double x_0, double y_0, double angle, double path_length)
        : w(w)
        , W(W)
        , x_0(x_0)
        , y_0(y_0)
        , angle(angle)
        , path_length(path_length) {
            path_x = path_length * std::cos(angle);
            path_y = path_length * std::sin(angle);
        }

    double w;
    double W;
    double x_0;
    double y_0;
    double angle;
    double path_length;
    double path_x;
    double path_y;
};

void CheckGatheringOneItem(ItemGathererTestData& data, const double path_ratio) {
        std::vector<collision_detector::Item> items = {};
        std::vector<collision_detector::Gatherer> gatherers = {};
        
        geom::Point2D start_pos(data.x_0, data.y_0);
        geom::Point2D end_pos(data.x_0 + data.path_x, data.y_0 + data.path_y);
        geom::Point2D item_point(data.x_0 + path_ratio * data.path_x, data.y_0 + path_ratio * data.path_y);

        items.push_back({item_point, data.w});
        gatherers.push_back({start_pos, end_pos, data.W});

        // double collision_time = std::hypot(data.x_0 - item_point.x, data.y_0 - item_point.y) / data.path_length;

        ItemGathererProviderTest provider(items, gatherers);
        std::vector<collision_detector::CollisionEvent> events = collision_detector::FindGatherEvents(provider);

        CHECK_THAT(events, SizeIs(1));

        // Собирает один предмет
        REQUIRE(events.size() == 1);
        CHECK(events[0].item_id == 0);
        CHECK(events[0].gatherer_id == 0);
        CHECK_THAT(events[0].sq_distance, WithinAbs(0.0, EPSILON));
        CHECK_THAT(events[0].time, WithinAbs(path_ratio, EPSILON));

        items.clear();
        gatherers.clear();
}

auto equal_pred = [](collision_detector::CollisionEvent left, collision_detector::CollisionEvent right) {
    return left.item_id == right.item_id && left.gatherer_id == right.gatherer_id
           && std::abs(left.sq_distance - right.sq_distance) <= EPSILON
           && std::abs(left.time - right.time) <= EPSILON;
};

bool compare_pred(const collision_detector::CollisionEvent& left, const collision_detector::CollisionEvent& right) {
    if (std::abs(left.time - right.time) > EPSILON) {
        return left.time < right.time ? true : false;
    } else if (std::abs(left.sq_distance - right.sq_distance) > EPSILON) {
        return left.sq_distance < right.sq_distance ? true : false;
    } else if (left.item_id != right.item_id) {
        return left.item_id < right.item_id ? true : false;
    } else if (left.gatherer_id != right.gatherer_id) {
        return left.gatherer_id < right.gatherer_id ? true : false;
    }
    throw std::logic_error("logic error - two same item id or gatherer id");
}


struct EventsIsEqual : Catch::Matchers::MatcherGenericBase {
    EventsIsEqual(std::vector<collision_detector::CollisionEvent>&& events)
        : range_{std::move(events)} {
            std::sort(std::begin(range_), std::end(range_), compare_pred);
    }
    
    EventsIsEqual(EventsIsEqual&&) = default;

    bool match(std::vector<collision_detector::CollisionEvent> other) const {
        using std::begin;
        using std::end;

        std::sort(begin(other), end(other), compare_pred);
        return std::equal(begin(range_), end(range_), begin(other), end(other), equal_pred);
    }

    std::string describe() const override {
        return "EventsIsEqual: "s + Catch::rangeToString(range_);
    }

private:
    std::vector<collision_detector::CollisionEvent> range_;
}; 


SCENARIO("isEmpty", "[Gathering]") {
    GIVEN("A empty ItemGathererProvider") {  // Дано: Items, Gatherers
        
        std::vector<collision_detector::Item> items = {};
        std::vector<collision_detector::Gatherer> gatherers = {};

        ItemGathererProviderTest provider(items, gatherers);

        std::vector<collision_detector::CollisionEvent> events = collision_detector::FindGatherEvents(provider);

        // В этом тесте нет никаких событий
        CHECK_THAT(events, IsEmpty());
    }
}

SCENARIO("GatheringOnHorizontalLine", "[Gathering]") {
    GIVEN("A ItemGathererProvider") {  // Дано: Items, Gatherers

        ItemGathererTestData data{1.0, 1.0, 1.0, 2.0, M_PI * 0.0, 2.0};

        // Проверяет сбор одного предмета в начале пути одним собирателем
        SECTION("Collects one item with one gatherer") {
            double path_ratio = 0.0;
            CheckGatheringOneItem(data, path_ratio);
        }
        // Проверяет сбор одного предмета в начале пути одним собирателем
        SECTION("Collects one item with one gatherer") {
            double path_ratio = 1e-4;
            CheckGatheringOneItem(data, path_ratio);
        }
        // Проверяет сбор одного предмета внутри пути одним собирателем
        SECTION("Collects one item with one gatherer") {

            double path_ratio = 0.44;
            CheckGatheringOneItem(data, path_ratio);
        }
        // Проверяет сбор одного предмета конце пути одним собирателем
        SECTION("Collects one item with one gatherer") {

            double path_ratio = 1 - 1e-4;
            CheckGatheringOneItem(data, path_ratio);
        }
        // Проверяет сбор одного предмета конце пути одним собирателем
        SECTION("Collects one item with one gatherer") {
            double path_ratio = 1.0;
            CheckGatheringOneItem(data, path_ratio);
        }
    }
}

SCENARIO("GatheringOnVerticalLine", "[Gathering]") {
    GIVEN("A ItemGathererProvider") {  // Дано: Items, Gatherers

        ItemGathererTestData data{1.0, 1.0, -2.0, -1.0, M_PI * 0.5, 2.0};

        // Проверяет сбор одного предмета в начале пути одним собирателем
        SECTION("Collects one item with one gatherer") {
            double path_ratio = 0.0;
            CheckGatheringOneItem(data, path_ratio);
        }
        // Проверяет сбор одного предмета в начале пути одним собирателем
        SECTION("Collects one item with one gatherer") {
            double path_ratio = 1e-4;
            CheckGatheringOneItem(data, path_ratio);
        }
        // Проверяет сбор одного предмета внутри пути одним собирателем
        SECTION("Collects one item with one gatherer") {

            double path_ratio = 0.44;
            CheckGatheringOneItem(data, path_ratio);
        }
        // Проверяет сбор одного предмета конце пути одним собирателем
        SECTION("Collects one item with one gatherer") {

            double path_ratio = 1 - 1e-4;
            CheckGatheringOneItem(data, path_ratio);
        }
        // Проверяет сбор одного предмета конце пути одним собирателем
        SECTION("Collects one item with one gatherer") {
            double path_ratio = 1.0;
            CheckGatheringOneItem(data, path_ratio);
        }
    }
}

SCENARIO("ContainItemNotInStarightLine", "[Gathering]") {
    GIVEN("A ItemGathererData") {

        // Начало линии в (-2, -1) - идет вверх с длиною 2, конечная точка в (-2, 1)
        // Ширина w = 0.1, ширина W = 2.0
        ItemGathererTestData data{0.1, 2.0, -2.0, -1.0, M_PI * 0.5, 2.0};
        
        double collection_radius = (data.w + data.W) / 2.0;

        double x_min = data.x_0 - collection_radius;
        double x_mid = data.x_0;
        double x_max = data.x_0 + collection_radius;

        double y_min = data.y_0;
        double y_mid = data.y_0 + data.path_y / 2.0;
        double y_max = data.y_0 + data.path_y;

        std::vector<collision_detector::Item> items = {};
        std::vector<collision_detector::Gatherer> gatherers = {};
        
        geom::Point2D start_pos(data.x_0, data.y_0);
        geom::Point2D end_pos(data.x_0 + data.path_x, data.y_0 + data.path_y);
        gatherers.push_back({start_pos, end_pos, data.W});      

        WHEN("First points is not in rectangle") {
            items.push_back({{x_min - 1e-4, y_min - 1e-4}, data.w}); // #0
            items.push_back({{x_max + 1e-4, y_max + 1e-4}, data.w}); // #1
            items.push_back({{-22, -11}, data.w}); // #2
            items.push_back({{22, 11}, data.w}); // #3

            ItemGathererProviderTest provider(items, gatherers);
            std::vector<collision_detector::CollisionEvent> events = collision_detector::FindGatherEvents(provider);

            CHECK_THAT(events, IsEmpty());

            THEN("Points in rectangle gathering by second gatherer") {
                items.push_back({{x_min, y_min}, data.w});              // #4   #0
                items.push_back({{x_max, y_min}, data.w});              // #5   #1  #0

                items.push_back({{x_mid, 0}, data.w});                  // #6   #2  #1
                items.push_back({{x_mid, 0}, data.w});                  // #7   #3  #2
                items.push_back({{x_mid - 0.1, y_mid + 0.7}, data.w});  // #8   #4  #3
                items.push_back({{x_min, y_mid + 0.7}, data.w});        // #9   #5  #4

                items.push_back({{x_min, y_max}, data.w});              // #10  #6  #5
                items.push_back({{x_max, y_max}, data.w});              // #11  #7
                
                gatherers.push_back({start_pos, end_pos, data.W}); // добавить второго сборщика такого же

                provider = ItemGathererProviderTest(items, gatherers);
                events = collision_detector::FindGatherEvents(provider);

                double sq_collection_radius = collection_radius * collection_radius;

                CHECK_THAT(events, SizeIs(14));
                CHECK_THAT(events, EventsIsEqual({{5, 0, sq_collection_radius, 0.0},
                                                {5, 1, sq_collection_radius, 0.0},
                                                {6, 0, 0, 0.5},
                                                {6, 1, 0, 0.5},
                                                {7, 0, 0, 0.5},
                                                {7, 1, 0, 0.5},
                                                {8, 0, 0.01, 0.85},
                                                {8, 1, 0.01, 0.85},
                                                {9, 0, sq_collection_radius, 0.85},
                                                {9, 1, sq_collection_radius, 0.85},
                                                {10, 0, sq_collection_radius, 1.0},
                                                {10, 1, sq_collection_radius, 1.0},
                                                {11, 0, sq_collection_radius, 1.0},
                                                {11, 1, sq_collection_radius, 1.0}}));
            }
        }
    }
}