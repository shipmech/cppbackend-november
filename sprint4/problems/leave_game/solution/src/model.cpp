#include "model.h"

#include <stdexcept>
#include <iostream>

namespace model {

using namespace std::literals;

const double PI = 3.14159265358979323846;
const double TOL = 1e-3;

const int Road::GetLength() const noexcept {
    if (IsHorizontal()) {
        return std::abs(end_.x - start_.x);
    }
    
    return std::abs(end_.y - start_.y);
}

bool Road::IsPointOnRoad(Coords coords) const noexcept {
    
    return (coords.y > y_min || std::abs(coords.y - y_min) <= TOL)
            && (coords.y < y_max || std::abs(coords.y - y_max) <= TOL)
            && (coords.x > x_min || std::abs(coords.x - x_min) <= TOL)
            && (coords.x < x_max || std::abs(coords.x - x_max) <= TOL);
}

Coords Road::GetIntersectionCoords(Coords init_coords, Coords end_coords) const {
    Coords intersection_point;

    Coords init{init_coords.x, init_coords.y};
    Coords end_point{end_coords.x, end_coords.y};

    bool IsIntersected = IsTrajectoryIntersectBound(init, end_point, intersection_point);

    return {intersection_point.x, intersection_point.y};
}

double Road::cos_between_vectors(Coords v1, Coords v2) const {
    return (v1.x * v2.x + v1.y * v2.y) / (std::sqrt(v1.x * v1.x + v1.y * v1.y) * std::sqrt(v2.x * v2.x + v2.y * v2.y));
}

bool Road::IsTrajectoryIntersectBound(Coords init, Coords end_point, Coords& intersection_point) const {
    
    Coords n1 = Coords{-1, 0};
    Coords n2 = Coords{0, 1};
    Coords n3 = Coords{1, 0};
    Coords n4 = Coords{0, -1};

    Coords tau_traj = Coords{(end_point.x - init.x), (end_point.y - init.y)};

    Coords temp_point;

    if (std::abs(cos_between_vectors(tau_traj, n1) - 1) <= TOL) {
        intersection_point = Coords{x_min, init.y};
        return true;
    } else if (std::abs(cos_between_vectors(tau_traj, n2) - 1) <= TOL) {
        intersection_point = Coords{init.x, y_max};
        return true;
    } else if (std::abs(cos_between_vectors(tau_traj, n3) - 1) <= TOL) {
        intersection_point = Coords{x_max, init.y};
        return true;
    } else if (std::abs(cos_between_vectors(tau_traj, n4) - 1) <= TOL) {
        intersection_point = Coords{init.x, y_min};
        
        return true;
    }

    return true;
}




void Map::AddOffice(const Office& office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

Coords Road::GetRandomCoords(std::shared_ptr<util::DoubleGenerator>  double_generator) const noexcept {
    int road_length = GetLength();
    
    double random_road_length_ratio = double_generator->Get();
    double random_road_width_ratio = double_generator->Get();
   
    double x_new;
    double y_new;
    
    if (IsHorizontal()) {
        x_new = x_min + random_road_length_ratio * road_length;
        y_new = y_min + random_road_width_ratio * semi_road_width_ * 2;
    } else {
        x_new = x_min + random_road_width_ratio * semi_road_width_ * 2;
        y_new = y_min + random_road_length_ratio * road_length;
    }

    return {x_new, y_new};
}

Coords Map::GetRandomCoordInRoads(std::shared_ptr<util::DoubleGenerator>  double_generator) const noexcept {
    size_t roads_number = roads_.size();
    double temp = double_generator->Get();
    size_t random_road_index = std::round(temp * (roads_number - 1));

    Road road = roads_[random_road_index];

    return road.GetRandomCoords(double_generator);
}

Coords Map::GetInitCoordInFirstRoadinMap() const noexcept {
    Road road = roads_[0];
    
    int x_new = road.GetStart().x;
    int y_new = road.GetStart().y;
    

    return {x_new, y_new};
}

const Road* Map::GetRoadByCoords(const Coords& coords) const {
    for (auto& road : roads_) {
        if (road.IsPointOnRoad(coords)) {
            return &road;
        }
    }
    return nullptr;
}

const Road* Map::GetAnotherRoadInCoords(const Coords& coords, std::vector<const Road*> previous_roads) const {
    for (const auto& another_road : roads_) {
        if (!another_road.IsPointOnRoad(coords)) {
            continue;
        }
        bool one_of_previous_roads = (std::count(previous_roads.begin(), previous_roads.end(), &another_road) != 0);
        if (one_of_previous_roads) {
            continue;
        }
        return &another_road;
    }
    return nullptr;
}




void Game::AddMap(const Map& map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}


CollisionProvider::CollisionProvider(const GameSession& game_session,
                            std::map<Dog::Id, Coords> dog_id_to_init_coords,
                            std::map<Dog::Id, Coords> dog_id_to_end_coords) {
    auto lost_objects = game_session.GetLostObjects();
    auto dogs = game_session.GetDogs();
    auto offices = game_session.GetMap().GetOffices();

    int index = 0;
    for (auto& lost_object_ptr : lost_objects) {
        LostObject& lost_object = *(lost_object_ptr.second);
        items_.push_back(collision_detector::Item{{lost_object.GetPos().x, lost_object.GetPos().y}, OBJECTS_WIDTH});
        item_index_to_lost_object_id.emplace(index, lost_object.GetId());
        index++;
    }

    index = 0;
    for (auto& dog_ptr : dogs) {
        Dog& dog = *(dog_ptr.second);
        auto id = dog.GetId();
        auto init_pos = dog_id_to_init_coords.at(id);
        auto end_pos = dog_id_to_end_coords.at(id);

        gatherers_.push_back(collision_detector::Gatherer{{init_pos.x, init_pos.y}, {end_pos.x, end_pos.y}, DOG_WIDTH});
        gatherer_index_to_dog_id.emplace(index, id);
        index++;
    }
    for (auto& office : offices) {
        auto pos = office.GetPosition();
        auto offset = office.GetOffset();
        auto id = office.GetId();
        bases_.push_back(collision_detector::Base{{pos.x, pos.y}, {offset.dx, offset.dy}, BASE_WIDTH});
        base_index_to_office_id.emplace(index, id);
        index++;
    }
}


std::shared_ptr<Dog> GameSession::AddDog(const Coords coords) {
    std::shared_ptr<Dog> dog = std::make_shared<Dog>(Dog::Id{dogs_.size()}, coords, map_.GetBagCapacity());
    dogs_.insert({dog->GetId(), dog});
    dogs_in_roads_.emplace(dog->GetId(), map_.GetRoadByCoords(dog->GetCoords()));

    return dog;
}

void GameSession::AddDog(std::shared_ptr<Dog> dog) {
    dogs_.insert({dog->GetId(), dog});
    dogs_in_roads_.emplace(dog->GetId(), map_.GetRoadByCoords(dog->GetCoords()));
}

void GameSession::AddLostObject(const double& delta_time) {
    unsigned loot_amount_to_generate = IsNeedToGenerateLostObject(delta_time);
    
    if (loot_amount_to_generate) {
        for (unsigned i = 0; i < loot_amount_to_generate; ++i) {
            LostObject::Id id{lost_object_id_counter_};
            ++lost_object_id_counter_;
            
            size_t object_type = std::round(double_generator_->Get() * (map_.GetLootTypeAmount() - 1));
            int object_value = map_.GetLootValueByType(object_type);

            Coords pos = GetRandomCoordInRoads();

            std::shared_ptr<LostObject> lost_object = std::make_shared<LostObject>(id, object_type, object_value, pos);
            lost_objects_.emplace(id, lost_object);
        }
    }
}

void GameSession::UpdateOneDogPosition(std::shared_ptr<Dog> dog, Coords end_coords, std::vector<const Road*> previus_roads = {}) {
    
    Coords init_coords = dog->GetCoords();

    const Road* current_road = dogs_in_roads_.at(dog->GetId());
    

    if (current_road->IsPointOnRoad(end_coords)) {
        dog->SetCoords(end_coords);
        return;
    } else {
        Coords intersection_coords = current_road->GetIntersectionCoords(init_coords, end_coords);

        previus_roads.push_back(current_road);
        const Road* another_road = map_.GetAnotherRoadInCoords(intersection_coords, previus_roads);

        dog->SetCoords(intersection_coords);
        if (!another_road) {
            VelocityVector velocity{0, 0};
            dog->SetVelocity(velocity);
            return;
        } else {
            dogs_in_roads_[dog->GetId()] = another_road;
            UpdateOneDogPosition(dog, end_coords, previus_roads);
        }
    }
}

void GameSession::UpdateGatheringHandOverLoot(std::map<Dog::Id, Coords> dog_id_to_init_coords,
                                              std::map<Dog::Id, Coords> dog_id_to_end_coords) {

    CollisionProvider provider{*this, dog_id_to_init_coords, dog_id_to_end_coords};
    std::vector<collision_detector::CollisionEvent> events = collision_detector::FindGatherEvents(provider);

    for (auto& event : events) {
        auto gatherer_index = event.gatherer_id;
        auto dog_id = provider.GetDogIdByIndex(gatherer_index);

        if (event.is_base) {
            dogs_.at(dog_id)->HandOverLoot();
            continue;
        }

        auto item_index = event.item_id;
        auto lost_object_id = provider.GetLostObjectIdByIndex(item_index);

        if (lost_objects_.count(lost_object_id) == 0) { // если предмет уже был собран другим игроком
            continue;
        }

        bool is_gathered = dogs_.at(dog_id)->AddLootObject(lost_objects_.at(lost_object_id));

        if (is_gathered) {
            lost_objects_.erase(lost_object_id);
        }
    }
}

void GameSession::UpdateGameState(const double& delta_time) {


    std::map<Dog::Id, Coords> dog_id_to_init_coords;
    std::map<Dog::Id, Coords> dog_id_to_end_coords;

    for (auto& dog_id_to_ptr : dogs_) {

        auto dog = dog_id_to_ptr.second;
        dog->AddLifeTime(delta_time);

        Coords init_coords = dog->GetCoords();
        VelocityVector velocity = dog->GetVelocity();
        Coords end_coords{init_coords.x + velocity.vx * delta_time, init_coords.y + velocity.vy * delta_time};

        dog_id_to_init_coords.emplace(dog->GetId(), init_coords);
        UpdateOneDogPosition(dog, end_coords);
        
        dog_id_to_end_coords.emplace(dog->GetId(), dog->GetCoords());
    }

    UpdateGatheringHandOverLoot(dog_id_to_init_coords, dog_id_to_end_coords);

    AddLostObject(delta_time);
}


void GameSessions::AddSession(GameSession&& game_session) {
    const Map::Id map_id = game_session.GetMap().GetId();
    const size_t index = sessions_.size();

    session_id_to_index_.emplace(game_session.GetId(), index);
    sessions_.emplace_back(std::move(game_session));
    map_id_to_session_index_[map_id].emplace_back(index);
}

GameSession* GameSessions::GetGameSessionById(GameSession::Id id) {
    for (auto key : session_id_to_index_) {
    }

    return GetGameSessionByIndex(session_id_to_index_.at(id));
}

std::uint32_t GameSessions::FindByMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_session_index_.find(id); it != map_id_to_session_index_.end()) {
        // выдает индекс первой сессии из vectora сессий для заданной карты
        return it->second.at(0);
    }
    return 0;
}

void GameSessions::InitializeOneSessionPerMap() {
    std::uint32_t new_id = 0;
    for (const Map& map : game_.GetMaps()) {
        GameSession new_game_session{GameSession::Id{new_id}, map};
        AddSession(std::move(new_game_session));
        new_id++;
    }
}

int Bag::GetLootSumValue() const noexcept {
    int sum = 0;
    for (const auto& object : objects_) {
        sum += object.GetValue();
    }
    return sum;
}

}  // namespace model
