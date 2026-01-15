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
    model::Road LoadRoad(const boost::json::object& road_obj);
    model::Building LoadBuilding(const boost::json::object& bld_obj);
    model::Office LoadOffice(const boost::json::object& office_obj);
    model::Map LoadMap(const boost::json::object& map_obj, size_t default_bag_capacity);
}