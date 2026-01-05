#pragma once

#include "model.h"
#include <boost/json.hpp>
#include <map>
#include <string>
#include <filesystem>

namespace json_loader {
    struct ExtraMapData {
        boost::json::value loot_types;
    };
    
    using ExtraMapDataMap = std::map<std::string, ExtraMapData>;
    
    model::Game LoadGame(const std::filesystem::path& json_path, ExtraMapDataMap& extra_data_out);
}