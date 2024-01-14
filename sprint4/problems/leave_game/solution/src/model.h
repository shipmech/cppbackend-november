#pragma once
#include <algorithm>
#include <string>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <random>

#include <iostream>
#include <string>

#include "util.h"
#include "loot_generator.h"
#include "collision_detector.h"

namespace model {

using namespace std::literals;

const double OBJECTS_WIDTH = 0.0;
const double DOG_WIDTH = 0.6;
const double BASE_WIDTH = 0.5;


using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};


using Coord_double = double;


struct Coords {
    Coord_double x, y;

    bool IsSame(const Coords& coords) const {
        return std::hypot(x - coords.x, y - coords.y) < 1e-18;
    }
};

struct VelocityVector {
    double vx = 0;
    double vy = 0;

    bool IsNull() const {
        return std::hypot(vx, vy) < 1e-10;
    }
};


class Road {
    struct HorizontalTag {
        HorizontalTag() = default;
    };

    struct VerticalTag {
        VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

    const int GetLength() const noexcept;

    bool IsPointOnRoad(Coords coords) const noexcept;

    Coords GetIntersectionCoords(Coords init_coords, Coords end_coords) const;

    Coords GetRandomCoords(std::shared_ptr<util::DoubleGenerator>  double_generator) const noexcept;

private:

    double cos_between_vectors(Coords v1, Coords v2) const;

    bool IsTrajectoryIntersectBound(Coords init, Coords end_point, Coords& intersection_point) const;

private:
    Point start_;
    Point end_;
    double semi_road_width_ = 0.4;
    
    double x_min = std::min(start_.x, end_.x) - semi_road_width_;
    double x_max = std::max(start_.x, end_.x) + semi_road_width_;
    double y_min = std::min(start_.y, end_.y) - semi_road_width_;
    double y_max = std::max(start_.y, end_.y) + semi_road_width_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name, double dog_speed, size_t bag_capacity,
        double loot_period, double loot_probability, size_t loot_type_amount, std::vector<int> loot_values,
        int dog_retirement_time) noexcept
        : id_(std::move(id))
        , name_(std::move(name))
        , dog_speed_{dog_speed}
        , bag_capacity_{bag_capacity}
        , loot_period_{loot_period}
        , loot_probability_{loot_probability}
        , loot_type_amount_{loot_type_amount}
        , loot_values_{loot_values}
        , dog_retirement_time_(dog_retirement_time) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const double& GetDogSpeed() const noexcept {
        return dog_speed_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    const Office& GetOfficeById(size_t index) const noexcept {
        return offices_[index];
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(const Office& office);

    Coords GetRandomCoordInRoads(std::shared_ptr<util::DoubleGenerator>  double_generator) const noexcept;

    Coords GetInitCoordInFirstRoadinMap() const noexcept;

    const Road* GetRoadByCoords(const Coords& coords) const;

    const Road* GetAnotherRoadInCoords(const Coords& coords, std::vector<const Road*> previous_roads) const;

    size_t GetBagCapacity() const noexcept {
        return bag_capacity_;
    }
    
    double GetLootPeriod() const noexcept {
        return loot_period_;
    }

    double GetLootProbability() const noexcept {
        return loot_probability_;
    }

    size_t GetLootTypeAmount() const noexcept {
        return loot_type_amount_;
    }

    int GetLootValueByType(size_t type) const noexcept {
        return loot_values_[type];
    }

    int GetDogRetirementTime() const noexcept {
        return dog_retirement_time_;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    double dog_speed_;
    size_t bag_capacity_;

    double loot_period_;
    double loot_probability_;

    size_t loot_type_amount_;
    std::vector<int> loot_values_;

    int dog_retirement_time_;
};


class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(const Map& map);

    const std::vector<Map>& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept;

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
};

class LostObject {
public:
    using Id = util::Tagged<std::uint32_t, LostObject>;

    LostObject() = default;

    explicit LostObject(Id id, size_t type, int value, Coords pos) noexcept
        : id_{id}
        , type_{type}
        , value_{value}
        , pos_{pos} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const size_t GetType() const noexcept {
        return type_;
    }

    const Coords& GetPos() const noexcept {
        return pos_;
    }

    const int GetValue() const noexcept {
        return value_;
    }

private:
    Id id_ = Id{0u};
    size_t type_ = 0;
    int value_ = 0;
    Coords pos_ = Coords{0.0, 0.0};
};

class LootObjectInBag {
public:
    using Id = util::Tagged<std::uint32_t, LootObjectInBag>;

    LootObjectInBag() = default;

    explicit LootObjectInBag(const LostObject& lost_object) noexcept
        : id_{*lost_object.GetId()}
        , type_{lost_object.GetType()}
        , value_{lost_object.GetValue()} {
    }

    explicit LootObjectInBag(Id id, size_t type, int value) noexcept
        : id_{id}
        , type_{type}
        , value_{value} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const size_t GetType() const noexcept {
        return type_;
    }

    const int GetValue() const noexcept {
        return value_;
    }

private:
    Id id_ = Id{0u};
    size_t type_ = 0;
    int value_ = 0;
};

class Bag {
public:

    Bag() = default;

    explicit Bag(size_t capacity) noexcept
        : capacity_{capacity} {
    }

    explicit Bag(size_t capacity, const std::vector<LootObjectInBag>& objects) noexcept
        : capacity_{capacity}
        , objects_{objects} {
    }

    const size_t GetCapacity() const noexcept {
        return capacity_;
    }

    void AddObject(std::shared_ptr<LostObject> lost_object) {
        LootObjectInBag loot_object(*lost_object);
        objects_.push_back(loot_object);
    }

    std::vector<LootObjectInBag> GetObjects() const noexcept {
        return objects_;
    }

    size_t GetLootSize() const noexcept {
        return objects_.size();
    }

    int GetLootSumValue() const noexcept;

    void RemoveLoot() {
        objects_.clear();
    }

private:
    size_t capacity_ = 0;
    std::vector<LootObjectInBag> objects_;
};


class Dog {
public:

    using Id = util::Tagged<std::uint32_t, Dog>;

    Dog(Id id, const Coords coords, size_t bag_capacity) noexcept
        : id_{id}
        , coords_{coords}
        , velocity_vector_{0, 0}
        , direction_{"U"s}
        , bag_{bag_capacity} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const Coords& GetCoords() const noexcept {
        return coords_;
    }

    const VelocityVector& GetVelocity() const noexcept {
        return velocity_vector_;
    }

    std::string GetDirection() const noexcept {
        return direction_;
    }

    void SetCoords(Coords coords) noexcept {
        coords_ = coords;
    }

    void SetVelocity(VelocityVector velocity) noexcept {
        velocity_vector_ = velocity;
        if (!velocity.IsNull()) {
            inactive_time_sec_ = 0;
        }
    }

    int AddInactiveTime(int delta_time) noexcept {
        inactive_time_sec_ += delta_time;
        return inactive_time_sec_;
    }

    void SetDirection(const std::string direction) noexcept {
        direction_ = direction;
    }

    bool AddLootObject(std::shared_ptr<LostObject> lost_object) {
        if (bag_.GetLootSize() >= bag_.GetCapacity()) {
            return false;
        }
        bag_.AddObject(lost_object);
        return true;
    }

    void HandOverLoot() {
        score_ += bag_.GetLootSumValue();
        bag_.RemoveLoot();
    }

    int GetScore() const noexcept {
        return score_;
    }

    void SetScore(int score) noexcept {
        score_ = score;
    }

    Bag GetBag() const noexcept {
        return bag_;
    }

    void SetBag(Bag bag) noexcept {
        bag_ = bag;
    }

    void AddLifeTime(double delta_time_seconds) {
        life_time_seconds_ += delta_time_seconds;
    }

    double GetLifeTime() const noexcept {
        return life_time_seconds_;
    }

private:
    Id id_;
    Coords coords_;
    VelocityVector velocity_vector_;
    std::string direction_;
    Bag bag_;
    int score_ = 0;

    double life_time_seconds_ = 0;
    int inactive_time_sec_ = 0;
};

class GameSession {
public:
    using Id = util::Tagged<std::uint32_t, GameSession>;
    explicit GameSession(Id id, const Map& map) noexcept
        : id_{id}
        , map_{map}
        , loot_generator_{loot_gen::LootGenerator(loot_gen::LootGenerator::TimeInterval{static_cast<int>(map.GetLootPeriod()*10e3)},
                                                      map.GetLootProbability())} {
        
        double_generator_ = std::make_shared<util::DoubleGenerator>();
    }

    GameSession(const GameSession&& other) :
        id_(other.id_),
        map_(other.map_),
        dog_speed_(other.dog_speed_),
        loot_generator_(std::move(other.loot_generator_)),
        dogs_(std::move(other.dogs_)),
        dogs_in_roads_(std::move(other.dogs_in_roads_)),
        lost_objects_(std::move(other.lost_objects_)),
        double_generator_(std::move(other.double_generator_)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const Map& GetMap() const noexcept {
        return map_;
    }
    
    double GetDogSpeed() const noexcept {
        return dog_speed_;
    }

    Coords GetRandomCoordInRoads() {
        return map_.GetRandomCoordInRoads(double_generator_);
    }

    Coords GetInitCoordInFirstRoadinMap() const noexcept {
        return map_.GetInitCoordInFirstRoadinMap();
    }

    std::shared_ptr<Dog> AddDog(const Coords coords);

    void AddDog(std::shared_ptr<Dog> dog);

    std::map<Dog::Id, std::shared_ptr<Dog>> GetDogs() const noexcept {
        return dogs_;
    }

    unsigned IsNeedToGenerateLostObject(const double& delta_time) {
        unsigned loot_amount_to_generate = loot_generator_.Generate(loot_gen::LootGenerator::TimeInterval{static_cast<int>(delta_time*10e3)},
                                                                    lost_objects_.size(),
                                                                    dogs_.size());
    return loot_amount_to_generate;
    }

    void AddLostObject(const double& delta_time);

    void AddLostObject(std::shared_ptr<LostObject> lost_object) {
        lost_objects_.emplace(lost_object->GetId(), lost_object);
    }

    void UpdateGatheringHandOverLoot(std::map<Dog::Id, Coords> dog_id_to_init_coords,
                                     std::map<Dog::Id, Coords> dog_id_to_end_coords);

    void UpdateGameState(const double& delta_time);

    std::map<LostObject::Id, std::shared_ptr<LostObject>> GetLostObjects() const noexcept {
        return lost_objects_;
    }

    unsigned GetLostObjectIdCounter() const noexcept {
        return lost_object_id_counter_;
    }

    double GetDogRetirementTime() const noexcept {
        return map_.GetDogRetirementTime();
    }

    void EraseDog(const Dog::Id& dog_id) {
        dogs_.erase(dog_id);
        dogs_in_roads_.erase(dog_id);
    }


private:

    void UpdateOneDogPosition(std::shared_ptr<Dog> dog, Coords end_coords, std::vector<const Road*> previus_roads);

    double lower_bound_ = 0;
    double upper_bound_ = 1;
    std::random_device rd;
    std::uniform_real_distribution<double> dist_{lower_bound_, upper_bound_};

private:

    Id id_;
    const Map& map_;
    double dog_speed_ = map_.GetDogSpeed();

    std::map<Dog::Id, std::shared_ptr<Dog>> dogs_;
    std::map<Dog::Id, const Road*> dogs_in_roads_;

    std::map<LostObject::Id, std::shared_ptr<LostObject>> lost_objects_;
    unsigned lost_object_id_counter_ = 0;


    loot_gen::LootGenerator loot_generator_;
    std::shared_ptr<util::DoubleGenerator> double_generator_;

};

class CollisionProvider : public collision_detector::ItemGathererProvider {

public:
    explicit CollisionProvider(const GameSession& game_session,
                               std::map<Dog::Id, Coords> dog_id_to_init_coords,
                               std::map<Dog::Id, Coords> dog_id_to_end_coords);

    size_t ItemsCount() const override {
        return items_.size();
    }

    collision_detector::Item GetItem(size_t idx) const override {
        return items_[idx];
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_[idx];
    }

    size_t BaseCount() const override {
        return bases_.size();
    }

    collision_detector::Base GetBase(size_t idx) const override {
        return bases_[idx];
    }

    LostObject::Id GetLostObjectIdByIndex(size_t idx) const {
        return item_index_to_lost_object_id.at(idx);
    }

    Dog::Id GetDogIdByIndex(size_t idx) const {
        return gatherer_index_to_dog_id.at(idx);
    }

    Office::Id GetOfficeIdByIndex(size_t idx) const {
        return base_index_to_office_id.at(idx);
    }

private:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
    std::vector<collision_detector::Base> bases_;

    std::map<int, LostObject::Id> item_index_to_lost_object_id;
    std::map<int, Dog::Id> gatherer_index_to_dog_id;
    std::map<int, Office::Id> base_index_to_office_id;
};

class GameSessions {
public:
    explicit GameSessions(const Game& game) noexcept
        : game_{game} {
    }

    void AddSession(GameSession&& game_session);

    GameSession* GetGameSessionByIndex(std::uint32_t index) {
        return &sessions_.at(index);
    }

    GameSession* GetGameSessionById(GameSession::Id id);

    std::uint32_t FindByMap(const Map::Id& id) const noexcept;

    GameSession::Id FindSessionIdByMap(const Map::Id& id) const noexcept {
        std::uint32_t index = FindByMap(id);
        return sessions_.at(index).GetId();
    }

    bool MapExist(const Map::Id& id) const noexcept {
        return map_id_to_session_index_.count(id) != 0;
    }

    void InitializeOneSessionPerMap();

    const double GetMaxDogSpeedBySessionId(GameSession::Id game_session_id) {
        return GetGameSessionById(game_session_id)->GetDogSpeed();
    }

    void UpdateGameState(double delta_time) {
        for (auto& game_session : sessions_) {
            game_session.UpdateGameState(delta_time);
        }
    }

    std::map<LostObject::Id, std::shared_ptr<LostObject>> GetLostObjectsBySessionId(GameSession::Id game_session_id) {
        return GetGameSessionById(game_session_id)->GetLostObjects();
    }

    unsigned GetLostObjectIdCounterBySessionId(GameSession::Id game_session_id) {
        return GetGameSessionById(game_session_id)->GetLostObjectIdCounter();
    }

    size_t GetSessionAmount() const {
        return sessions_.size();
    }

    int GetDogRetirementTime() {
        if (sessions_.size() == 0) {
            throw std::runtime_error("There are no sessions for get the time of retirement");
        }
        return sessions_[0].GetDogRetirementTime();
    }

    void EraseDogBySessionId(GameSession::Id game_session_id, const Dog::Id& dog_id) {
        GetGameSessionById(game_session_id)->EraseDog(dog_id);
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToSessionsIndex = std::unordered_map<Map::Id, std::vector<size_t>, MapIdHasher>;

    using GameSessionIdHasher = util::TaggedHasher<GameSession::Id>;
    using GameSessionIdToIndex = std::unordered_map<GameSession::Id, size_t, GameSessionIdHasher>;

    std::vector<GameSession> sessions_;
    MapIdToSessionsIndex map_id_to_session_index_;
    GameSessionIdToIndex session_id_to_index_;

    const Game& game_;
};

}  // namespace model
