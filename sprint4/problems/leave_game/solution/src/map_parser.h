#pragma once

// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/json.hpp>

#include "model.h"
#include "json_loader.h"

namespace api_handler::map_parser{
    
std::string GetMapsIdToName(const std::vector<model::Map>& maps);

boost::json::object GetRoad(const model::Road& road);

boost::json::array GetRoads(const std::vector<model::Road>& roads);

boost::json::object GetBuilding(const model::Building& building);

boost::json::array GetBuildings(const std::vector<model::Building>& buildings);

boost::json::object GetOffice(const model::Office& office);

boost::json::array GetOffices(const std::vector<model::Office>& offices);

std::string GetMap(const model::Map& map, const extra_data::Data& common_extra_data);

} // end namespace api_handler::map_parser