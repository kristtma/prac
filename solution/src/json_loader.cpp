#include "json_loader.h"
#include "model.h"
#include <boost/json.hpp>
#include <fstream>
#include <stdexcept>
#include <filesystem>
#include <chrono>

namespace json_loader {


using namespace model;
namespace json = boost::json;


Road LoadRoad(const json::object& road_obj) {
    if (auto* x1 = road_obj.if_contains("x1")) {
        return Road{Road::HORIZONTAL, Point{road_obj.at("x0").as_int64(), road_obj.at("y0").as_int64()}, x1->as_int64()};
    } else if (auto* y1 = road_obj.if_contains("y1")) {
        return Road{Road::VERTICAL, Point{road_obj.at("x0").as_int64(), road_obj.at("y0").as_int64()}, y1->as_int64()};
    }
    throw std::runtime_error("Invalid road format");
}

Building LoadBuilding(const json::object& bld_obj) {
    Rectangle rect{
        .position = Point{bld_obj.at("x").as_int64(), bld_obj.at("y"). as_int64()},
        .size = Size{bld_obj.at("w").as_int64(), bld_obj.at("h").as_int64()}
    };
    return Building{rect};
}

Office LoadOffice(const json::object& office_obj) {
    Office::Id id{std::string(office_obj.at("id").as_string())};
    Point pos{office_obj.at("x").as_int64(), office_obj.at("y").as_int64()};
    Offset offset{office_obj.at("offsetX").as_int64(), office_obj.at("offsetY").as_int64()};
    return Office{std::move(id), pos, offset};
}

Map LoadMap(const json::object& map_obj, size_t default_bag_capacity) {
    Map::Id id{std::string(map_obj.at("id").as_string())};
    std::string name = map_obj.at("name").as_string().c_str();
    Map map{std::move(id), std::move(name)};

    for (const auto& r : map_obj.at("roads").as_array()) {
        map.AddRoad(LoadRoad(r.as_object()));
    }

    if (auto* buildings = map_obj.if_contains("buildings")) {
        for (const auto& b : buildings->as_array()) {
            map.AddBuilding(LoadBuilding(b.as_object()));
        }
    }

    if (auto* offices = map_obj.if_contains("offices")) {
        for (const auto& o : offices->as_array()) {
            map.AddOffice(LoadOffice(o.as_object()));
        }
    }

    if (auto* cap = map_obj.if_contains("bagCapacity")) {
        map.SetBagCapacity(cap->as_int64());
    } else {
        map.SetBagCapacity(default_bag_capacity);
    }

    return map;
}

Game LoadGame(const std::filesystem::path& json_path, ExtraMapDataMap& extra_data_out) {
    std::ifstream file(json_path); // ifstream отлично работает с объектом path
    if (!file.is_open()) {
        // Чтобы вывести путь в ошибке, добавьте .string()
        throw std::runtime_error("Failed to open config file: " + json_path.string());
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    auto root = json::parse(content).as_object();

    Game game;
    game.SetLootGeneratorConfig(5.0, 0.5); // period=5.0, prob=0.5
    if (auto* cfg = root.if_contains("lootGeneratorConfig")) {
        auto& gen_cfg = cfg->as_object();
        double period = gen_cfg.at("period").as_double();
        double prob = gen_cfg.at("probability").as_double();
        game.SetLootGeneratorConfig(period, prob);
    }

    size_t default_bag_capacity = 3;
    if (auto* def_cap = root.if_contains("defaultBagCapacity")) {
        default_bag_capacity = def_cap->as_int64();
    }
    if (auto* time_val = root.if_contains("dogRetirementTime")) {
        double sec = time_val->as_double();
        auto retirement_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::duration<double>(sec)
        );
        game.SetDogRetirementTime(retirement_time);
    }
    // Maps
    auto& maps_array = root.at("maps").as_array();
    for (auto& map_value : maps_array) {
        auto& map_obj = map_value.as_object();
        Map map = LoadMap(map_obj, default_bag_capacity);

        ExtraMapData extra_data;
        if (auto* loot_types = map_obj.if_contains("lootTypes")) {
            extra_data.loot_types = loot_types->as_array();
        }
        extra_data_out[std::string(*map.GetId())] = std::move(extra_data);

        map.SetLootTypesCount(extra_data.loot_types.as_array().size());

        game.AddMap(std::move(map));
    }

    return game;
}

} // namespace json_loader