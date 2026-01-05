#include "request_handler.h"
#include <boost/json.hpp>
#include <stdexcept>
#include <algorithm>
#include <optional>
#include <fstream>
#include <syslog.h>
namespace http_handler {
using namespace model;
namespace json = boost::json;
using namespace std::literals;
model::Position GetRandomSpawnPoint(const model::Road& road) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    if (road.IsHorizontal()) {
        double x0 = std::min(road.GetStart().x, road.GetEnd().x);
        double x1 = std::max(road.GetStart().x, road.GetEnd().x);
        // Сужаем диапазон на 0.4 с каждой стороны (ширина дороги)
        double min_x = x0 + 0.4;
        double max_x = x1 - 0.4;
        if (min_x > max_x) {
            min_x = max_x = (x0 + x1) / 2.0;
        }
        std::uniform_real_distribution<> dis_x(min_x, max_x);
        return {dis_x(gen), (road.GetStart().y)};
    } else {
        double y0 = std::min(road.GetStart().y, road.GetEnd().y);
        double y1 = std::max(road.GetStart().y, road.GetEnd().y);
        double min_y = y0 + 0.4;
        double max_y = y1 - 0.4;
        if (min_y > max_y) {
            min_y = max_y = (y0 + y1) / 2.0;
        }
        std::uniform_real_distribution<> dis_y(min_y, max_y);
        return {(road.GetStart().x), dis_y(gen)};
    }
}
template <typename Fn>
http_server::StringResponse RequestHandler::ExecuteAuthorized(
    const http_server::StringRequest& req, Fn&& action) {
    auto token_opt = ExtractTokenFromAuthHeader(req);
    if (!token_opt) {
        return MakeUnauthorizedResponse("Authorization header is missing");
    }

    const Token& token = *token_opt;
    auto it = token_to_player_.find(token);
    if (it == token_to_player_.end()) {
        return MakeJsonResponse(
            http::status::unauthorized,
            "unknownToken",
            "Player token has not been found"
        );
    }

    return action(token, it->second);
}
///api/v1/game/tick
http_server::StringResponse RequestHandler::HandleGameTick(const http_server::StringRequest& req){
    if (req[http::field::content_type] != "application/json") {
        return MakeBadRequestResponse("Invalid content type");
    }
    try{
        auto json_value = json::parse(req.body());
        auto& obj = json_value.as_object();

        if (!obj.contains("timeDelta")) {
            return MakeBadRequestResponse("Failed to parse tick request JSON");
        }
        auto time_delta = obj.at("timeDelta").as_int64();
        if (time_delta < 0) {
            return MakeBadRequestResponse("Failed to parse tick request JSON");
        }
        for(auto& session : game_.GetSessions()){
            session.Tick(static_cast<int>(time_delta));
        }
        game_.Tick(std::chrono::milliseconds(time_delta));
        http_server::StringResponse response;
        response.result(http::status::ok);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.body() = "{}";
        response.prepare_payload();
        return response;
    }catch(const std::exception&){
        return MakeBadRequestResponse("Failed to parse action");
    }
}

http_server::StringResponse RequestHandler::HandlePlayerAction(const http_server::StringRequest& req) {
    // Валидация метода и Content-Type — делаем внутри лямбды или до
    if (req[http::field::content_type] != "application/json") {
        return MakeBadRequestResponse("Invalid content type");
    }

    return ExecuteAuthorized(req, [&](const Token&, const PlayerInfo& info) {
        try {
            json::value json_value = json::parse(req.body());
            json::object& obj = json_value.as_object();

            if (!obj.contains("move")) {
                return MakeBadRequestResponse("Failed to parse action");
            }

            std::string move_str = obj.at("move").as_string().c_str();

            Direction dir;
            if (move_str == "L") dir = Direction::WEST;
            else if (move_str == "R") dir = Direction::EAST;
            else if (move_str == "U") dir = Direction::NORTH;
            else if (move_str == "D") dir = Direction::SOUTH;
            else if (move_str == "") {
                // Остановка
                model::GameSession* session = game_.FindSession(info.map_id);
                if (session) {
                    model::Dog* dog = nullptr;
                    for (auto& d : session->GetDogs()) {
                        if (d.GetPlayerId() == info.player_id) {
                            dog = &d;
                            break;
                        }
                    }
                    if (dog) {
                        dog->SetSpeed(0.0, 0.0);
                        dog->SetDirection(Direction::NORTH); // или оставить прежнее?
                    }
                }
            } else {
                return MakeBadRequestResponse("Failed to parse action");
            }

            if (move_str != "") {
                // Находим сессию и скорость
                model::GameSession* session = game_.FindSession(info.map_id);
                if (session) {
                    double speed_val = session->GetMap().GetDogSpeed();
                    model::Dog* dog = nullptr;
                    for (auto& d : session->GetDogs()) {
                        if (d.GetPlayerId() == info.player_id) {
                            dog = &d;
                            break;
                        }
                    }
                    if (dog) {
                        dog->SetSpeedFromDirection(dir, speed_val);
                    }
                }
            }

            // Возвращаем {}
            http_server::StringResponse response;
            response.result(http::status::ok);
            response.set(http::field::content_type, "application/json");
            response.set(http::field::cache_control, "no-cache");
            response.body() = "{}";
            response.prepare_payload();
            return response;

        } catch (const std::exception&) {
            return MakeBadRequestResponse("Failed to parse action");
        }
    });
}
http_server::StringResponse RequestHandler::HandleApiMaps(const http_server::StringRequest& req) {
    json::array maps_array;
    
    for (const auto& map : game_.GetMaps()) {
        json::object map_obj;
        map_obj["id"] = *map.GetId();
        map_obj["name"] = map.GetName();
        maps_array.push_back(std::move(map_obj));
    }
    
    
    http_server::StringResponse response;
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");  // Добавьте это
    // Сериализуем массив напрямую, а не объект
    response.body() = json::serialize(maps_array);  // ← ИЗМЕНЕНИЕ ЗДЕСЬ
    response.prepare_payload();
    
    return response;
}

http_server::StringResponse RequestHandler::HandleApiMap(const http_server::StringRequest& req) {
    const auto& target = req.target();
    
    // Извлекаем ID карты из URL
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
    
    // Безопасное получение lootTypes из extra_map_data_
    auto it = extra_map_data_.find(*map_id);
    if (it != extra_map_data_.end()) {
        map_obj["lootTypes"] = it->second.loot_types;
    } else {
        // Если данных нет, возвращаем пустой массив
        map_obj["lootTypes"] = json::array();
    }
    
    http_server::StringResponse response;
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache"); 
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
    return MakeJsonResponse(http::status::bad_request, "invalidArgument", message);
}

http_server::StringResponse RequestHandler::MakeMapNotFoundResponse(std::string_view message) {
    return MakeJsonResponse(http::status::not_found, "mapNotFound", message);
}

http_server::StringResponse RequestHandler::MakeMethodNotAllowedResponse(std::string_view allowed_methods) {
    // Формируем сообщение: "Only POST method is expected"
    std::string message = "Only ";
    message += allowed_methods;
    message += " method is expected";

    auto response = MakeJsonResponse(
        http::status::method_not_allowed,
        "invalidMethod",
        message
    );
    // 🔥 Главное: добавляем заголовок Allow
    response.set(http::field::allow, std::string(allowed_methods));
    return response;
}
bool RequestHandler::IsValidMethod(http::verb method, const std::vector<http::verb>& allowed_methods) {
    return std::find(allowed_methods.begin(), allowed_methods.end(), method) != allowed_methods.end();
}


void RequestHandler::operator()(http_server::StringRequest&& req, std::function<void(http_server::StringResponse&&)> send) {
    auto response = [&]() -> http_server::StringResponse {
        const auto& target = req.target();
        
        // Проверяем, что запрос начинается с /api/
        if (target.starts_with("/api/")) {
            // Обрабатываем API endpoints
            if (target == "/api/v1/maps") {
                if (!IsValidMethod(req.method(), {http::verb::get})) {
                    return MakeMethodNotAllowedResponse("GET");
                }
                return HandleApiMaps(req);
            } 
            else if (target.starts_with("/api/v1/maps/")) {
                if (!IsValidMethod(req.method(), {http::verb::get, http::verb::head})) {
                    return MakeMethodNotAllowedResponse("GET, HEAD");
                }
                return HandleApiMap(req);
            }
            else if (target == "/api/v1/game/join") {
                if (!IsValidMethod(req.method(), {http::verb::post})) {
                    return MakeMethodNotAllowedResponse("POST");
                }
                return HandleJoinGame(req);
            }
            else if (target == "/api/v1/game/players") {
                if (!IsValidMethod(req.method(), {http::verb::get, http::verb::head})) {
                    return MakeMethodNotAllowedResponse("GET, HEAD");
                }
                return HandleGetPlayers(req);
            }
            else if (target == "/api/v1/game/state") {
                if (is_auto_tick_mode_) {
                    return MakeJsonResponse(
                        http::status::bad_request,
                        "badRequest",
                        "Invalid endpoint"
                    );
                }
                if (!IsValidMethod(req.method(), {http::verb::get, http::verb::head})) {
                    return MakeMethodNotAllowedResponse("GET, HEAD");
                }
                return HandleGameState(req);
            }
            else if (target == "/api/v1/game/player/action") {
                if (!IsValidMethod(req.method(), {http::verb::post})) {
                    return MakeMethodNotAllowedResponse("POST");
                }
                return HandlePlayerAction(req);
            }
            else if(target == "/api/v1/game/tick"){
                if (!IsValidMethod(req.method(), {http::verb::post})) {
                    return MakeMethodNotAllowedResponse("POST");
                }
                return HandleGameTick(req);
            }
            else {
                // Неизвестный API endpoint
                //return MakeBadRequestResponse("Invalid API endpoint");
                return MakeJsonResponse(
                    http::status::bad_request,
                    "badRequest",
                    "Bad request"
                );
            }
        }
        
        // Для не-API запросов возвращаем 404
        return MakeJsonResponse(http::status::not_found, "pageNotFound", "Page not found");
    }();

    return send(std::move(response));
}

// Обработчик для /api/v1/game/join
http_server::StringResponse RequestHandler::HandleJoinGame(const http_server::StringRequest& req) {
    // Проверка Content-Type
    if (req[http::field::content_type] != "application/json") {
        return MakeBadRequestResponse("Content-Type must be application/json");
    }

    try {
        // Парсим JSON тело запроса
        json::value json_value = json::parse(req.body());
        json::object& obj = json_value.as_object();

        // Извлекаем userName и mapId
        if (!obj.contains("userName") || !obj.contains("mapId")) {
            return MakeBadRequestResponse("Join game request parse error");
        }

        std::string user_name = obj.at("userName").as_string().c_str();
        std::string map_id_str = obj.at("mapId").as_string().c_str();

        // Проверка на пустое имя
        if (user_name.empty()) {
            return MakeBadRequestResponse("Invalid name");
        }

        // Находим карту
        model::Map::Id map_id{std::move(map_id_str)};
        model::Map* map = const_cast<model::Map*>(game_.FindMap(map_id));
        if (!map) {
            return MakeMapNotFoundResponse();
        }

        // Находим или создаем сессию для этой карты
        model::GameSession* session = game_.FindSession(map_id);
        if (!session) {
            // Создаем новую сессию
            game_.AddSession(*map);
            session = game_.FindSession(map_id);
        }
        size_t player_id = next_player_id_++; // ← СНАЧАЛА инициализируйте
        // Создаем собаку
        model::Dog::Id dog_id{std::to_string(game_.GetSessions().size() + game_.GetMaps().size())}; // Простой способ генерации id
        model::Dog dog{dog_id, user_name}; // Имя собаки = имя пользователя
        dog.SetPlayerId(player_id); // глобальный ID — допустимо
        // Добавляем собаку в сессию
        model::Dog& added_dog = session->AddDog(std::move(dog));

        // Генерируем токен
        Token token = player_tokens_->GenerateToken();
        // Сохраняем игрока

        const auto& roads = map->GetRoads();
        
        if (!roads.empty()) {
            const auto& first_road = roads.front();
            added_dog.SetPosition({static_cast<double>(first_road.GetStart().x),
                           static_cast<double>(first_road.GetStart().y)});
            added_dog.SetCurrentRoad(&first_road);
        } 
        else if (randomize_spawn_points_) {
            // Выбираем случайную дорогу
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_int_distribution<> road_dis(0, roads.size() - 1);
            const auto& random_road = roads[road_dis(gen)];
            auto spawn_point = GetRandomSpawnPoint(random_road);
            added_dog.SetPosition(spawn_point);
            added_dog.SetCurrentRoad(&random_road);
        }else {
            added_dog.SetPosition({0.0, 0.0});
        } 
        // Формируем ответ
        json::object response_obj;
        response_obj["authToken"] = *token;
        response_obj["playerId"] = static_cast<int>(player_id); 
        response_obj["posx"] = static_cast<int>(added_dog.GetPosition().x); 
        response_obj["posy"] = static_cast<int>(added_dog.GetPosition().y); 

        token_to_player_.emplace(token, PlayerInfo{user_name, map_id, player_id});
        size_t bag_capacity = map->GetBagCapacity();
        dog.SetBagCapacity(bag_capacity);


        http_server::StringResponse response;
        response.result(http::status::ok);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.body() = json::serialize(response_obj);
        response.prepare_payload();

        return response;

    } catch (const std::exception& e) {
        return MakeBadRequestResponse("Join game request parse error");
    }
}

// Обработчик для /api/v1/game/state
http_server::StringResponse RequestHandler::HandleGameState(const http_server::StringRequest& req) {
    auto token_opt = ExtractTokenFromAuthHeader(req);
    if (!token_opt) {
        return MakeUnauthorizedResponse("Authorization header is missing");
    }

    const Token& token = *token_opt;
    auto it = token_to_player_.find(token);
    if (it == token_to_player_.end()) {
        return MakeJsonResponse(
            http::status::unauthorized,
            "unknownToken",
            "Player token has not been found"
        );
    }

    const auto& player_map_id = it->second.map_id;

    model::GameSession* session = game_.FindSession(player_map_id);
    if (!session) {
        return MakeJsonResponse(
            http::status::internal_server_error,
            "internalError",
            "Game session not found"
        );
    }

    json::object players_obj;
    for (const auto& [tok, info] : token_to_player_) {
        if (info.map_id == player_map_id) {
            const model::Dog* dog = nullptr;
            for (auto& d : session->GetDogs()) {
                if (d.GetPlayerId() == info.player_id) {
                    dog = &d;
                    break;
                }
            }
            if (!dog) {
                continue; 
            }
            // В нужном месте (например, в HandleJoinGame):
            const auto& pos = dog->GetPosition();
            char dir_char = static_cast<char>(dog->GetDirection());

            json::array pos_arr{pos.x, pos.y};
            json::array speed_arr{dog->GetSpeedX(), dog->GetSpeedY()};
            json::array bag_arr;
            for (const auto& item : dog->GetBag().GetItems()) {
                bag_arr.push_back(json::object{
                    {"id", static_cast<int>(item.id)},
                    {"type", static_cast<int>(item.type)}
                });
            }
            players_obj[std::to_string(info.player_id)] = json::object{
                {"pos", std::move(pos_arr)},
                {"speed", std::move(speed_arr)},
                {"dir", std::string(1, dir_char)},
                {"bag", std::move(bag_arr)},
                {"score", dog->GetScore()}
            };
        }
    }

    json::object response_obj;
    response_obj["players"] = std::move(players_obj);
    json::object lost_objects;
    for (const auto& loot : session->GetLootItems()) {
        json::array pos_arr{static_cast<double>(loot.position.x), static_cast<double>(loot.position.y)};
        lost_objects[std::to_string(loot.id)] = json::object{
            {"type", static_cast<int>(loot.type)},
            {"pos", std::move(pos_arr)}
        };
    }
    response_obj["lostObjects"] = std::move(lost_objects);

    http_server::StringResponse response;
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");
    response.body() = json::serialize(response_obj);
    response.prepare_payload();
    return response;
}
// Обработчик для /api/v1/game/players
http_server::StringResponse RequestHandler::HandleGetPlayers(const http_server::StringRequest& req) {
    auto token_opt = ExtractTokenFromAuthHeader(req);
    if (!token_opt) {
        return MakeUnauthorizedResponse("Authorization header is missing");
    }

    const Token& token = *token_opt;
    auto it = token_to_player_.find(token);
    if (it == token_to_player_.end()) {
        return MakeJsonResponse(
            http::status::unauthorized,
            "unknownToken",
            "Player token has not been found"
        );
    }

    // Находим карту игрока
    const auto& player_map_id = it->second.map_id;

    // Собираем всех игроков на той же карте
    // Собираем всех игроков на той же карте
    json::object players_obj;
    for (const auto& [tok, info] : token_to_player_) {
        if (info.map_id == player_map_id) {
            // Используем стабильный player_id из PlayerInfo
            players_obj[std::to_string(info.player_id)] = json::object{{"name", info.name}};
        }
    }
    // Сбросим fake_id или лучше храните настоящий player_id

    http_server::StringResponse response;
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");
    response.body() = json::serialize(players_obj);
    response.prepare_payload();
    return response;
}

// Вспомогательный метод для извлечения токена из заголовка Authorization
std::optional<Token> RequestHandler::ExtractTokenFromAuthHeader(const http_server::StringRequest& req) const {
    auto auth_header = req[http::field::authorization];
    if (auth_header.empty()) {
        return std::nullopt;
    }

    // Проверяем, начинается ли строка с "Bearer "
    const std::string bearer_prefix = "Bearer ";
    if (auth_header.substr(0, bearer_prefix.size()) != bearer_prefix) {
        return std::nullopt;
    }

    // Извлекаем токен (все, что после "Bearer ")
    std::string token_str = std::string(auth_header.substr(bearer_prefix.size()));

    // Проверяем длину токена (должно быть 32 символа)
    if (token_str.length() != 32) {
        return std::nullopt;
    }

    // Проверяем, что все символы являются шестнадцатеричными
    if (!std::all_of(token_str.begin(), token_str.end(), ::isxdigit)) {
        return std::nullopt;
    }

    return Token{token_str};
}

// Новый метод для ответа 401 Unauthorized
http_server::StringResponse RequestHandler::MakeUnauthorizedResponse(std::string_view message) {
    return MakeJsonResponse(http::status::unauthorized, "invalidToken", message);
}

// Остальные методы остаются без изменений...

}  // namespace http_handler
