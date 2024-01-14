#pragma once

#include <boost/json.hpp>

#include <filesystem>

#include "model.h"
#include "extra_data.h"

namespace json_loader {

std::pair<model::Game, extra_data::Data> LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader
