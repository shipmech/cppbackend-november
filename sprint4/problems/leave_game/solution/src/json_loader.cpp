#include "json_loader.h"

#include <string>
#include <string_view>
#include <fstream>
#include <sstream>


namespace json_loader {

namespace json = boost::json;
using namespace std::literals;

const std::string GAME_CONFIG_PATH_ERROR = "<game-config-json> : Path does not exist"s ;

const std::string ROAD_X0 = "x0"s;
const std::string ROAD_Y0 = "y0"s;
const std::string ROAD_X1 = "x1"s;
const std::string ROAD_Y1 = "y1"s;

const std::string BUILDING_X = "x"s;
const std::string BUILDING_Y = "y"s;
const std::string BUILDING_WIDTH = "w"s;
const std::string BUILDING_HEIGHT = "h"s;

const std::string OFFICE_ID = "id"s;
const std::string OFFICE_X = "x"s;
const std::string OFFICE_Y = "y"s;
const std::string OFFICE_OFFSET_X = "offsetX"s;
const std::string OFFICE_OFFSET_Y = "offsetY"s;

const std::string MAP_ID = "id"s;
const std::string MAP_NAME = "name"s;

const std::string MAPS_TAG = "maps"s;
const std::string ROADS_TAG = "roads"s;
const std::string BUILDINGS_TAG = "buildings"s;
const std::string OFFICES_TAG = "offices"s;

const std::string DEFAULT_DOG_SPEED = "defaultDogSpeed"s;
const std::string DOG_SPEED = "dogSpeed"s;

const std::string LOOT_GENERATOR_CONFIG = "lootGeneratorConfig"s;
const std::string PERIOD = "period"s;
const std::string PROBABILITY = "probability"s;

const std::string LOOT_TYPES = "lootTypes"s;
const std::string LOOT_TYPES_VALUE = "value"s;

const std::string DEFAULT_BAG_CAPACITY = "defaultBagCapacity"s;
const std::string BAG_CAPACITY = "bagCapacity"s;

const std::string DOG_RETIREMENT_TIME = "dogRetirementTime"s;

bool PathIsValid(const std::filesystem::path& path) {
    return std::filesystem::exists(path);
}

void LoadRoads(model::Map& map, const json::array& roads) {
    for (const auto& road : roads) {
        
        auto x0 = road.at(ROAD_X0).as_int64();
        auto y0 = road.at(ROAD_Y0).as_int64();

        model::Point start_point(x0, y0);
        model::Point end_point(x0, y0);

        model::Road new_road(model::Road::HORIZONTAL, start_point, x0);

        if (road.as_object().if_contains(ROAD_X1)) {
            auto x1 = road.at(ROAD_X1).as_int64();
            new_road = model::Road(model::Road::HORIZONTAL ,start_point, x1);
        } else if (road.as_object().if_contains(ROAD_Y1)) {
            auto y1 = road.at(ROAD_Y1).as_int64();
            new_road = model::Road(model::Road::VERTICAL ,start_point, y1);
        }

        map.AddRoad(new_road);
    }
}

void LoadBuildings(model::Map& map, const json::array& buildings) {
    for (const auto& building : buildings) {
        auto x = building.at(BUILDING_X).as_int64();
        auto y = building.at(BUILDING_Y).as_int64();
        auto width = building.at(BUILDING_WIDTH).as_int64();
        auto height = building.at(BUILDING_HEIGHT).as_int64();

        model::Building new_building({{x, y}, {width, height}});
        map.AddBuilding(new_building);
    }
}

void LoadOffices(model::Map& map, const json::array& offices) {
    for (const auto& office : offices) {
        auto x = office.at(OFFICE_X).as_int64();
        auto y = office.at(OFFICE_Y).as_int64();
        auto offset_x = office.at(OFFICE_OFFSET_X).as_int64();
        auto offset_y = office.at(OFFICE_OFFSET_Y).as_int64();
        std::string id = office.at(OFFICE_ID).as_string().c_str();

        model::Office new_office(model::Office::Id(id), model::Point(x, y), model::Offset(offset_x, offset_y));
        map.AddOffice(new_office);
    }
}

void LoadMaps(model::Game& game, const json::array& maps,
             extra_data::Data& common_extra_data,
             const double& default_dog_speed,
             const size_t& default_bag_capacity,
             const double& default_loot_period,
             const double& default_loot_probability,
             const int& default_dog_retirement_time) {
    for (const auto& map : maps) {

        std::string id = map.at(MAP_ID).as_string().c_str();
        std::string name = map.at(MAP_NAME).as_string().c_str();
        double dog_speed = default_dog_speed;
        if (map.as_object().contains(DOG_SPEED)) {
            dog_speed = map.at(DOG_SPEED).as_double();
        }
        size_t bag_capacity = default_bag_capacity;
        if (map.as_object().contains(BAG_CAPACITY)) {
            bag_capacity = map.at(BAG_CAPACITY).as_int64();
        }

        auto roads = map.at(ROADS_TAG).as_array();
        auto buildings = map.at(BUILDINGS_TAG).as_array();
        auto offices = map.at(OFFICES_TAG).as_array();

        double loot_period = default_loot_period;
        double loot_probability = default_loot_probability;

        extra_data::Map map_extra_data;
        map_extra_data.loot_types = map.at(LOOT_TYPES).as_array();
        common_extra_data.map.emplace(model::Map::Id(id), map_extra_data);

        size_t loot_type_amount = common_extra_data.map.at(model::Map::Id(id)).loot_types.size();
        std::vector<int> loot_values;
        for (auto& loot_type : common_extra_data.map.at(model::Map::Id(id)).loot_types) {
            loot_values.push_back(loot_type.at(LOOT_TYPES_VALUE).as_int64());
        }

        int dog_retirement_time = default_dog_retirement_time;
        
        model::Map new_map(model::Map::Id(id), name, dog_speed, bag_capacity, loot_period, loot_probability, loot_type_amount, loot_values, dog_retirement_time);

        LoadRoads(new_map, roads);
        LoadBuildings(new_map, buildings);
        LoadOffices(new_map, offices);

        game.AddMap(new_map);
    }
}

std::pair<model::Game, extra_data::Data> LoadGame(const std::filesystem::path& json_path) {   
    // Загрузить модель игры из файла
    model::Game game;

    // Загрузить содержимое файла json_path, например, в виде строки
    std::ifstream file;

    if (PathIsValid(json_path)) {
        file = std::ifstream(json_path);
    } else {
        std::cerr << GAME_CONFIG_PATH_ERROR << std::endl;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    std::string jsonString = buffer.str();

    // Распарсить строку как JSON object, используя boost::json::parse
    boost::json::object json_object = boost::json::parse(jsonString).as_object();


    double default_dog_speed = 1.0;
    if (json_object.contains(DEFAULT_DOG_SPEED)) {
        default_dog_speed = json_object.at(DEFAULT_DOG_SPEED).as_double();
    }

    int default_dog_retirement_time = 60*1000;
    if (json_object.contains(DOG_RETIREMENT_TIME)) {
        default_dog_retirement_time = static_cast<int>(json_object.at(DOG_RETIREMENT_TIME).as_double()*1000);
    }

    size_t default_bag_capacity = 3;
    if (json_object.contains(DEFAULT_BAG_CAPACITY)) {
        default_bag_capacity = json_object.at(DEFAULT_BAG_CAPACITY).as_int64();
    }

    extra_data::Data common_extra_data;

    auto loot_generator_config = json_object.at(LOOT_GENERATOR_CONFIG).as_object();
    double default_loot_period = loot_generator_config.at(PERIOD).as_double();
    double default_loot_probability = loot_generator_config.at(PROBABILITY).as_double();

    auto maps = json_object.at(MAPS_TAG).as_array();
    LoadMaps(game, maps, common_extra_data, default_dog_speed, default_bag_capacity, default_loot_period, default_loot_probability, default_dog_retirement_time);

    return {game, common_extra_data};
}

}  // namespace json_loader
