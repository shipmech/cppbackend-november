
#pragma once

#include <boost/json.hpp>

#include <map>

namespace extra_data {

struct Map {
    boost::json::array loot_types;
};

struct Data
{
    std::map<model::Map::Id, extra_data::Map> map;
};


}  // namespace extra_data
