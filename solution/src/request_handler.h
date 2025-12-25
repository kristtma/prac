#pragma once

#include "model.h"
#include "http_server.h"
#include <boost/json.hpp>
#include <boost/beast.hpp>

#include <memory>
#include <optional> 

#include "player_tokens.h"
namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
class RequestHandler {
public:
    // Конструктор с правильным порядком параметров
    explicit RequestHandler(model::Game& game, bool randomize_spawn_points, bool is_auto_tick_mode)
        : game_(game)
        , randomize_spawn_points_(randomize_spawn_points)
        , is_auto_tick_mode_(is_auto_tick_mode)
        , player_tokens_(std::make_unique<PlayerTokens>())
        , next_player_id_(0) {
    }

    // Обработчик HTTP-запросов
    void operator()(http_server::StringRequest&& req, std::function<void(http_server::StringResponse&&)> send);

private:
    model::Game& game_;
    bool randomize_spawn_points_;  // Порядок: сначала этот параметр
    bool is_auto_tick_mode_;       // Затем этот
    struct PlayerInfo {
        // Явный конструктор (опционально, но рекомендуется)
        std::string name;
        model::Map::Id map_id;
        size_t player_id;
        
        PlayerInfo(std::string name_, model::Map::Id map_id_, size_t id)
        : name(std::move(name_)), map_id(std::move(map_id_)), player_id(id) {}
    };
    std::unordered_map<Token, PlayerInfo, util::TaggedHasher<Token>> token_to_player_;
    std::unique_ptr<PlayerTokens> player_tokens_; // Управление генератором токенов
    size_t next_player_id_ = 0;

    // Обработчики конкретных эндпоинтов
    http_server::StringResponse HandleApiMaps(const http_server::StringRequest& req);
    http_server::StringResponse HandleApiMap(const http_server::StringRequest& req);
    http_server::StringResponse HandleJoinGame(const http_server::StringRequest& req);
    http_server::StringResponse HandleGetPlayers(const http_server::StringRequest& req);
    http_server::StringResponse HandleGameState(const http_server::StringRequest& req);
    http_server::StringResponse HandleGameTick(const http_server::StringRequest& req);
    http_server::StringResponse HandlePlayerAction(const http_server::StringRequest& req);

    // Вспомогательные методы для формирования ответов
    http_server::StringResponse MakeJsonResponse(http::status status, std::string_view code, std::string_view message);
    http_server::StringResponse MakeBadRequestResponse(std::string_view message = "Bad request");
    http_server::StringResponse MakeMapNotFoundResponse(std::string_view message = "Map not found");
    http_server::StringResponse MakeMethodNotAllowedResponse(std::string_view message = "Method not allowed");
    http_server::StringResponse MakeUnauthorizedResponse(std::string_view message = "Unauthorized");
    
    template <typename Fn>
    http_server::StringResponse ExecuteAuthorized(const http_server::StringRequest& req, Fn&& action);

    // Проверка поддерживаемых методов
    bool IsValidMethod(http::verb method, const std::vector<http::verb>& allowed_methods);

    // Метод для извлечения токена из заголовка Authorization
    std::optional<Token> ExtractTokenFromAuthHeader(const http_server::StringRequest& req) const;
};

}  // namespace http_handler
