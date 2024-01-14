#include "map_parser.h"
#include "model.h"

#include <string>
#include <string_view>
#include <vector>

namespace api_handler::map_parser{

using namespace std::literals;

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

const std::string LOOT_TYPES = "lootTypes"s;

std::string GetMapsIdToName(const std::vector<model::Map>& maps) {
    boost::json::array maps_id_to_name;

    for (auto& map : maps) {
        boost::json::object obj;
        obj[MAP_ID] = *map.GetId();
        obj[MAP_NAME] = map.GetName();
        maps_id_to_name.push_back(obj);
    }

    return boost::json::serialize(maps_id_to_name);

}

boost::json::object GetRoad(const model::Road& road) {
    boost::json::object obj;
    obj[ROAD_X0] = road.GetStart().x;
    obj[ROAD_Y0] = road.GetStart().y;
    if (road.IsHorizontal()) {
        obj[ROAD_X1] = road.GetEnd().x;
    } else {
        obj[ROAD_Y1] = road.GetEnd().y;
    }

    return obj;
}

boost::json::array GetRoads(const std::vector<model::Road>& roads) {
    boost::json::array roads_json;

    for (auto& road : roads) {
        roads_json.push_back(GetRoad(road));
    }

    return roads_json;
}

boost::json::object GetBuilding(const model::Building& building) {
    boost::json::object obj;
    obj[BUILDING_X] = building.GetBounds().position.x;
    obj[BUILDING_Y] = building.GetBounds().position.y;
    obj[BUILDING_WIDTH] = building.GetBounds().size.width;
    obj[BUILDING_HEIGHT] = building.GetBounds().size.height;

    return obj;
}

boost::json::array GetBuildings(const std::vector<model::Building>& buildings) {
    boost::json::array buildings_json;

    for (auto& building : buildings) {
        buildings_json.push_back(GetBuilding(building));
    }

    return buildings_json;

}

boost::json::object GetOffice(const model::Office& office) {
    boost::json::object obj;
    obj[OFFICE_ID] = *office.GetId();
    obj[OFFICE_X] = office.GetPosition().x;
    obj[OFFICE_Y] = office.GetPosition().y;
    obj[OFFICE_OFFSET_X] = office.GetOffset().dx;
    obj[OFFICE_OFFSET_Y] = office.GetOffset().dy;

    return obj;
}

boost::json::array GetOffices(const std::vector<model::Office>& offices) {
    boost::json::array offices_json;

    for (auto& office : offices) {
        offices_json.push_back(GetOffice(office));
    }

    return offices_json;

}

std::string GetMap(const model::Map& map, const extra_data::Data& common_extra_data) {
    boost::json::object obj;
    obj[MAP_ID] = *map.GetId();
    obj[MAP_NAME] = map.GetName();
    obj[ROADS_TAG] = GetRoads(map.GetRoads());
    obj[BUILDINGS_TAG] = GetBuildings(map.GetBuildings());
    obj[OFFICES_TAG] = GetOffices(map.GetOffices());
    auto map_extra_data = common_extra_data.map.at(map.GetId());
    obj[LOOT_TYPES] = map_extra_data.loot_types;

    return boost::json::serialize(obj);
}

} // end namespace api_handler::map_parser