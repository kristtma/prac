#pragma once

#include "model.h"
#include "http_server.h"
#include <boost/json.hpp>
#include <boost/beast.hpp>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game) 
        : game_(game) {
    }


    // Обработчик HTTP-запросов
    void operator()(http_server::StringRequest&& req, std::function<void(http_server::StringResponse&&)> send);

private:
    model::Game& game_;

    // Обработчики конкретных эндпоинтов
    http_server::StringResponse HandleApiMaps(const http_server::StringRequest& req);
    http_server::StringResponse HandleApiMap(const http_server::StringRequest& req);
    
    // Вспомогательные методы для формирования ответов
    http_server::StringResponse MakeJsonResponse(http::status status, std::string_view code, std::string_view message);
    http_server::StringResponse MakeBadRequestResponse(std::string_view message = "Bad request");
    http_server::StringResponse MakeMapNotFoundResponse(std::string_view message = "Map not found");
    http_server::StringResponse MakeMethodNotAllowedResponse(std::string_view message = "Method not allowed");
    
    // Проверка поддерживаемых методов
    bool IsValidMethod(http::verb method, const std::vector<http::verb>& allowed_methods);
};

}  // namespace http_handler
