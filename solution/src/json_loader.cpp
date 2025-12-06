#include "json_loader.h"
#include <boost/json.hpp>
#include <fstream>
#include <sstream>

namespace json = boost::json;

namespace json_loader {

namespace {
    // Константы для ключей JSON
    constexpr std::string_view MAPS_KEY = "maps";
    constexpr std::string_view ID_KEY = "id";
    constexpr std::string_view NAME_KEY = "name";
    constexpr std::string_view ROADS_KEY = "roads";
    constexpr std::string_view BUILDINGS_KEY = "buildings";
    constexpr std::string_view OFFICES_KEY = "offices";
    
    // Константы для координат и размеров
    constexpr std::string_view X_KEY = "x";
    constexpr std::string_view Y_KEY = "y";
    constexpr std::string_view W_KEY = "w";
    constexpr std::string_view H_KEY = "h";
    constexpr std::string_view X0_KEY = "x0";
    constexpr std::string_view Y0_KEY = "y0";
    constexpr std::string_view X1_KEY = "x1";
    constexpr std::string_view Y1_KEY = "y1";
    constexpr std::string_view OFFSET_X_KEY = "offsetX";
    constexpr std::string_view OFFSET_Y_KEY = "offsetY";
    
    using model::Coord;
    using model::Dimension;
    
    // Метод для парсинга дорог
    void ParseRoads(const json::array& roads_array, model::Map& map) {
        for (const auto& road_value : roads_array) {
            const auto& road_obj = road_value.as_object();
            
            Coord x0 = json::value_to<Coord>(road_obj.at(X0_KEY));
            Coord y0 = json::value_to<Coord>(road_obj.at(Y0_KEY));
            model::Point start{x0, y0};
            
            if (road_obj.contains(X1_KEY)) {
                // Горизонтальная дорога
                Coord x1 = json::value_to<Coord>(road_obj.at(X1_KEY));
                map.AddRoad(model::Road(model::Road::HORIZONTAL, start, x1));
            } else if (road_obj.contains(Y1_KEY)) {
                // Вертикальная дорога
                Coord y1 = json::value_to<Coord>(road_obj.at(Y1_KEY));
                map.AddRoad(model::Road(model::Road::VERTICAL, start, y1));
            }
        }
    }
    
    // Метод для парсинга зданий
    void ParseBuildings(const json::array& buildings_array, model::Map& map) {
        for (const auto& building_value : buildings_array) {
            const auto& building_obj = building_value.as_object();
            
            Coord x = json::value_to<Coord>(building_obj.at(X_KEY));
            Coord y = json::value_to<Coord>(building_obj.at(Y_KEY));
            Dimension w = json::value_to<Dimension>(building_obj.at(W_KEY));
            Dimension h = json::value_to<Dimension>(building_obj.at(H_KEY));
            
            model::Rectangle bounds{{x, y}, {w, h}};
            map.AddBuilding(model::Building(bounds));
        }
    }
    
    // Метод для парсинга офисов
    void ParseOffices(const json::array& offices_array, model::Map& map) {
        for (const auto& office_value : offices_array) {
            const auto& office_obj = office_value.as_object();
            
            std::string office_id_str = json::value_to<std::string>(office_obj.at(ID_KEY));
            Coord x = json::value_to<Coord>(office_obj.at(X_KEY));
            Coord y = json::value_to<Coord>(office_obj.at(Y_KEY));
            Dimension offsetX = json::value_to<Dimension>(office_obj.at(OFFSET_X_KEY));
            Dimension offsetY = json::value_to<Dimension>(office_obj.at(OFFSET_Y_KEY));
            
            model::Office::Id office_id{std::move(office_id_str)};
            model::Point position{x, y};
            model::Offset offset{offsetX, offsetY};
            
            map.AddOffice(model::Office(std::move(office_id), position, offset));
        }
    }
    
    // Метод для парсинга карты
    model::Map ParseMap(const json::object& map_obj) {
        // Получаем id и name карты
        std::string id_str = json::value_to<std::string>(map_obj.at(ID_KEY));
        std::string name = json::value_to<std::string>(map_obj.at(NAME_KEY));
        
        // Создаем карту
        model::Map::Id map_id{std::move(id_str)};
        model::Map map(std::move(map_id), std::move(name));
        
        // Обрабатываем дороги
        ParseRoads(map_obj.at(ROADS_KEY).as_array(), map);
        
        // Обрабатываем здания (если есть)
        if (map_obj.contains(BUILDINGS_KEY)) {
            ParseBuildings(map_obj.at(BUILDINGS_KEY).as_array(), map);
        }
        
        // Обрабатываем офисы (если есть)
        if (map_obj.contains(OFFICES_KEY)) {
            ParseOffices(map_obj.at(OFFICES_KEY).as_array(), map);
        }
        
        return map;
    }
} // namespace

model::Game LoadGame(const std::filesystem::path& json_path) {
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
    const auto& root = value.as_object();
    
    model::Game game;
    
    // Обрабатываем массив карт
    const auto& maps_array = root.at(MAPS_KEY).as_array();
    for (const auto& map_value : maps_array) {
        const auto& map_obj = map_value.as_object();
        model::Map map = ParseMap(map_obj);
        game.AddMap(std::move(map));
    }
    
    return game;
}

}  // namespace json_loader
