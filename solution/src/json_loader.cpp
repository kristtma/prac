#include "json_loader.h"
#include <boost/json.hpp>
#include <fstream>
#include <sstream>
namespace json = boost::json;

namespace json_loader {
using model::Coord;
using model::Dimension;

model::Game LoadGame(const std::filesystem::path& json_path, ExtraMapDataMap& extra_data_out) {
    // Загрузить содержимое файла json_path
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + json_path.string());
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_str = buffer.str();
    
    // Распарсить строку как JSON
    auto value = json::parse(json_str);
    auto& root = value.as_object();
    
    model::Game game;
    double default_speed = 1.0;

    if (root.contains("defaultDogSpeed")) {
        default_speed = root.at("defaultDogSpeed").as_double();
        game.SetDefaultDogSpeed(default_speed);
    }
    if (root.contains("lootGeneratorConfig")) {
        auto& cfg = root.at("lootGeneratorConfig").as_object();
        double period = cfg.at("period").as_double();
        double prob = cfg.at("probability").as_double();
        
        game.SetLootGeneratorConfig(period, prob);
    }
    size_t default_bag_capacity = 3;
    if (root.contains("defaultBagCapacity")) {
        default_bag_capacity = root.at("defaultBagCapacity").as_int64();
        game.SetDefaultBagCapacity(default_bag_capacity);
    }
    // Обрабатываем массив карт
    auto& maps_array = root.at("maps").as_array();
    for (auto& map_value : maps_array) {
        auto& map_obj = map_value.as_object();
        
        // Получаем id и name карты
        std::string id_str = json::value_to<std::string>(map_obj.at("id"));
        std::string name = json::value_to<std::string>(map_obj.at("name"));
        
        // СОХРАНЯЕМ КОПИЮ ID ДО ПЕРЕМЕЩЕНИЯ
        std::string map_id_str = id_str;
        
        // Создаем карту
        model::Map::Id map_id{std::move(id_str)};
        model::Map map(std::move(map_id), std::move(name));
        
        // Обрабатываем дороги
        auto& roads_array = map_obj.at("roads").as_array();
        for (auto& road_value : roads_array) {
            auto& road_obj = road_value.as_object();
            
            Coord x0 = json::value_to<Coord>(road_obj.at("x0"));
            Coord y0 = json::value_to<Coord>(road_obj.at("y0"));
            model::Point start{x0, y0};
            
            // Установка скорости карты
            if (map_obj.contains("dogSpeed")) {
                map.SetDogSpeed(map_obj.at("dogSpeed").as_double());
            } else {
                map.SetDogSpeed(default_speed);
            }
            
            // Определяем тип дороги (горизонтальная или вертикальная)
            if (road_obj.contains("x1")) {
                // Горизонтальная дорога
                Coord x1 = json::value_to<Coord>(road_obj.at("x1"));
                map.AddRoad(model::Road(model::Road::HORIZONTAL, start, x1));
            } else if (road_obj.contains("y1")) {
                // Вертикальная дорога
                Coord y1 = json::value_to<Coord>(road_obj.at("y1"));
                map.AddRoad(model::Road(model::Road::VERTICAL, start, y1));
            }
        }
        
        // Обрабатываем здания
        if (map_obj.contains("buildings")) {
            auto& buildings_array = map_obj.at("buildings").as_array();
            for (auto& building_value : buildings_array) {
                auto& building_obj = building_value.as_object();
                
                Coord x = json::value_to<Coord>(building_obj.at("x"));
                Coord y = json::value_to<Coord>(building_obj.at("y"));
                Dimension w = json::value_to<Dimension>(building_obj.at("w"));
                Dimension h = json::value_to<Dimension>(building_obj.at("h"));
                
                model::Rectangle bounds{{x, y}, {w, h}};
                map.AddBuilding(model::Building(bounds));
            }
        }
        
        // Обрабатываем офисы
        if (map_obj.contains("offices")) {
            auto& offices_array = map_obj.at("offices").as_array();
            for (auto& office_value : offices_array) {
                auto& office_obj = office_value.as_object();
                
                std::string office_id_str = json::value_to<std::string>(office_obj.at("id"));
                Coord x = json::value_to<Coord>(office_obj.at("x"));
                Coord y = json::value_to<Coord>(office_obj.at("y"));
                Dimension offsetX = json::value_to<Dimension>(office_obj.at("offsetX"));
                Dimension offsetY = json::value_to<Dimension>(office_obj.at("offsetY"));
                
                model::Office::Id office_id{std::move(office_id_str)};
                model::Point position{x, y};
                model::Offset offset{offsetX, offsetY};
                
                map.AddOffice(model::Office(std::move(office_id), position, offset));
            }
        }
        
        size_t loot_types_count = 0;
        // УДАЛЕНО: boost::json::array loot_types_json; // Эта строка создает конфликт

        // Создаем JSON массив для extra_data вне блока if
        json::array loot_types_json_array;

        if (map_obj.contains("lootTypes")) {
            auto& loot_arr = map_obj.at("lootTypes").as_array();
            loot_types_count = loot_arr.size();
            
            // Очищаем существующие типы
            map.ClearLootTypes();
            
            for (auto& loot_type_value : loot_arr) {
                auto& loot_obj = loot_type_value.as_object();
                
                // Используем model::LootType вместо json_loader::LootType
                model::LootType loot_type;
                loot_type.name = json::value_to<std::string>(loot_obj.at("name"));
                loot_type.file = json::value_to<std::string>(loot_obj.at("file"));
                loot_type.type = json::value_to<std::string>(loot_obj.at("type"));
                
                if (loot_obj.contains("rotation")) {
                    loot_type.rotation = json::value_to<int>(loot_obj.at("rotation"));
                }
                if (loot_obj.contains("color")) {
                    loot_type.color = json::value_to<std::string>(loot_obj.at("color"));
                }
                if (loot_obj.contains("scale")) {
                    loot_type.scale = json::value_to<double>(loot_obj.at("scale"));
                }
                
                // Читаем поле value (если есть)
                if (loot_obj.contains("value")) {
                    loot_type.value = json::value_to<int>(loot_obj.at("value"));
                } else {
                    loot_type.value = 0; // Значение по умолчанию
                }
                
                map.AddLootType(std::move(loot_type));
                
                // Сохраняем также в JSON для extra_data
                loot_types_json_array.push_back(loot_obj);
            }
        }

        // Сохраняем в extra_data используя сохраненную копию ID
        extra_data_out[map_id_str] = ExtraMapData{std::move(loot_types_json_array)};

        // Устанавливаем только количество в модель
        map.SetLootTypesCount(loot_types_count);
        
        // Добавляем карту в игру
        game.AddMap(std::move(map));
    }
    
    return game;
}
}  // namespace json_loader
