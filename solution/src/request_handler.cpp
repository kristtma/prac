#include "request_handler.h"
#include <boost/json.hpp>

namespace http_handler {

namespace json = boost::json;
using namespace std::literals;

void RequestHandler::operator()(http_server::StringRequest&& req, std::function<void(http_server::StringResponse&&)> send) {
    auto response = [&]() -> http_server::StringResponse {
        const auto& target = req.target();
        
        // Проверяем, что запрос начинается с /api/
        if (target.starts_with("/api/"sv)) {
            // Обрабатываем API endpoints
            if (target == "/api/v1/maps"sv) {
                if (!IsValidMethod(req.method(), {http::verb::get})) {
                    return MakeMethodNotAllowedResponse("Only GET method is allowed");
                }
                return HandleApiMaps(req);
            } 
            else if (target.starts_with("/api/v1/maps/"sv)) {
                if (!IsValidMethod(req.method(), {http::verb::get})) {
                    return MakeMethodNotAllowedResponse("Only GET method is allowed");
                }
                return HandleApiMap(req);
            }
            else {
                // Неизвестный API endpoint
                return MakeBadRequestResponse("Invalid API endpoint");
            }
        }
        
        // Для не-API запросов возвращаем 404
        return MakeJsonResponse(http::status::not_found, "pageNotFound", "Page not found");
    }();

    return send(std::move(response));
}
http_server::StringResponse RequestHandler::HandleApiMaps(const http_server::StringRequest& req) {
    json::array maps_array;
    
    for (const auto& map : game_.GetMaps()) {
        json::object map_obj;
        map_obj["id"] = *map.GetId();
        map_obj["name"] = map.GetName();
        maps_array.push_back(std::move(map_obj));
    }
    
    // ВМЕСТО этого (возвращает объект с полем maps):
    // json::object response_obj;
    // response_obj["maps"] = std::move(maps_array);
    
    // ДОЛЖНО БЫТЬ так (возвращает массив напрямую):
    // response_obj = std::move(maps_array);
    
    http_server::StringResponse response;
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json");
    
    // Сериализуем массив напрямую, а не объект
    response.body() = json::serialize(maps_array);  // ← ИЗМЕНЕНИЕ ЗДЕСЬ
    response.prepare_payload();
    
    return response;
}
/*http_server::StringResponse RequestHandler::HandleApiMaps(const http_server::StringRequest& req) {
    json::array maps_array;
    
    for (const auto& map : game_.GetMaps()) {
        json::object map_obj;
        map_obj["id"] = *map.GetId();
        map_obj["name"] = map.GetName();
        maps_array.push_back(std::move(map_obj));
    }
    
    json::object response_obj;
    response_obj["maps"] = std::move(maps_array);
    
    http_server::StringResponse response;
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json");
    response.body() = json::serialize(response_obj);
    response.prepare_payload();
    
    return response;
}*/

http_server::StringResponse RequestHandler::HandleApiMap(const http_server::StringRequest& req) {
    const auto& target = req.target();
    
    // Извлекаем ID карты из URL (формат: /api/v1/maps/{id})
    std::string map_id_str = std::string(target.substr(std::string("/api/v1/maps/"sv).size()));
    
    if (map_id_str.empty()) {
        return MakeBadRequestResponse("Map ID is required");
    }
    
    model::Map::Id map_id{std::move(map_id_str)};
    const auto* map = game_.FindMap(map_id);
    
    if (!map) {
        return MakeMapNotFoundResponse();
    }
    
    // Формируем JSON с детальной информацией о карте
    json::object map_obj;
    map_obj["id"] = *map->GetId();
    map_obj["name"] = map->GetName();
    
    // Добавляем дороги
    json::array roads_array;
    for (const auto& road : map->GetRoads()) {
        json::object road_obj;
        road_obj["x0"] = road.GetStart().x;
        road_obj["y0"] = road.GetStart().y;
        
        if (road.IsHorizontal()) {
            road_obj["x1"] = road.GetEnd().x;
        } else {
            road_obj["y1"] = road.GetEnd().y;
        }
        
        roads_array.push_back(std::move(road_obj));
    }
    map_obj["roads"] = std::move(roads_array);
    
    // Добавляем здания
    json::array buildings_array;
    for (const auto& building : map->GetBuildings()) {
        const auto& bounds = building.GetBounds();
        json::object building_obj;
        building_obj["x"] = bounds.position.x;
        building_obj["y"] = bounds.position.y;
        building_obj["w"] = bounds.size.width;
        building_obj["h"] = bounds.size.height;
        buildings_array.push_back(std::move(building_obj));
    }
    map_obj["buildings"] = std::move(buildings_array);
    
    // Добавляем офисы
    json::array offices_array;
    for (const auto& office : map->GetOffices()) {
        json::object office_obj;
        office_obj["id"] = *office.GetId();
        office_obj["x"] = office.GetPosition().x;
        office_obj["y"] = office.GetPosition().y;
        office_obj["offsetX"] = office.GetOffset().dx;
        office_obj["offsetY"] = office.GetOffset().dy;
        offices_array.push_back(std::move(office_obj));
    }
    map_obj["offices"] = std::move(offices_array);
    
    http_server::StringResponse response;
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json");
    response.body() = json::serialize(map_obj);
    response.prepare_payload();
    
    return response;
}

http_server::StringResponse RequestHandler::MakeJsonResponse(
    http::status status, std::string_view code, std::string_view message) {
    
    json::object error_obj;
    // Используем конструкторы boost::json::string
    error_obj["code"] = json::string(code.data(), code.size());
    error_obj["message"] = json::string(message.data(), message.size());
    
    http_server::StringResponse response;
    response.result(status);
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");
    response.body() = json::serialize(error_obj);
    response.prepare_payload();
    
    return response;
}

http_server::StringResponse RequestHandler::MakeBadRequestResponse(std::string_view message) {
    return MakeJsonResponse(http::status::bad_request, "badRequest", message);
}

http_server::StringResponse RequestHandler::MakeMapNotFoundResponse(std::string_view message) {
    return MakeJsonResponse(http::status::not_found, "mapNotFound", message);
}

http_server::StringResponse RequestHandler::MakeMethodNotAllowedResponse(std::string_view message) {
    return MakeJsonResponse(http::status::method_not_allowed, "methodNotAllowed", message);
}

bool RequestHandler::IsValidMethod(http::verb method, const std::vector<http::verb>& allowed_methods) {
    return std::find(allowed_methods.begin(), allowed_methods.end(), method) != allowed_methods.end();
}

}  // namespace http_handler
