#pragma once
#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <random>
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;
namespace beast = boost::beast;
namespace http = beast::http;
namespace po = boost::program_options;

std::string GetMimeType(const std::string& extension) {
    static const std::unordered_map<std::string, std::string> mime_types = {
        {".html", "text/html"},
        {".htm", "text/html"},
        {".css", "text/css"},
        {".txt", "text/plain"},
        {".js", "text/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpe", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".bmp", "image/bmp"},
        {".ico", "image/vnd.microsoft.icon"},
        {".tiff", "image/tiff"},
        {".tif", "image/tiff"},
        {".svg", "image/svg+xml"},
        {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"}
    };
    std::string ext_lower = extension;
    std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    auto it = mime_types.find(ext_lower);
    if (it != mime_types.end()) {
        return it->second;
    }
    
    return "application/octet-stream";
}

bool IsSubPath(fs::path path, fs::path base) {
    // Приводим оба пути к каноничному виду (без . и ..)
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    // Проверяем, что все компоненты base содержатся внутри path
    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}
std::string DecodeUrl(std::string_view url){
    std::string result;
    result.reserve(url.size());
    for (size_t i = 0; i < url.size(); ++i) {
        if (url[i] == '%' && i + 2 < url.size()) {
            int value;
            std::istringstream iss(std::string(url.substr(i + 1, 2)));
            if (iss >> std::hex >> value) {
                result += static_cast<char>(value);
                i += 2;
            } else {
                result += url[i];
            }
        } else if (url[i] == '+') {
            result += ' ';
        } else {
            result += url[i];
        }
    }
    return result;
}
class StaticFileHandler {
public:
    explicit StaticFileHandler(const fs::path& root_path) 
        : root_path_(root_path) {
    }
    
    bool HandleRequest(const http::request<http::string_body>& req, 
                      http::response<http::file_body>& res) {
        // Обрабатываем только GET и HEAD запросы
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return false;
        }
        
        // Проверяем, что путь не начинается с /api/
        std::string target = DecodeUrl(std::string(req.target()));
        if (target.find("/api/") == 0) {
            return false;
        }
        
        // Если путь заканчивается на /, добавляем index.html
        if (target.empty() || target.back() == '/') {
            target += "index.html";
        }
        
        // Убираем начальный слэш
        if (!target.empty() && target[0] == '/') {
            target = target.substr(1);
        }
        
        // Строим полный путь к файлу
        fs::path file_path = root_path_ / target;
        
        // Проверяем безопасность пути
        if (!IsSubPath(file_path, root_path_)) {
            return false;
        }
        
        // Если это директория, пробуем index.html
        if (fs::is_directory(file_path)) {
            file_path /= "index.html";
        }
        
        // Проверяем существование файла
        if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
            return false;
        }
        
        // Открываем файл
        http::file_body::value_type file;
        sys::error_code ec;
        file.open(file_path.string().c_str(), beast::file_mode::read, ec);
        
        if (ec) {
            return false;
        }
        
        // Устанавливаем ответ
        res.version(req.version());
        res.result(http::status::ok);
        
        // Определяем MIME-тип
        std::string ext = file_path.extension().string();
        res.set(http::field::content_type, GetMimeType(ext));
        
        // Для HEAD запроса не передаем тело
        if (req.method() == http::verb::head) {
            res.content_length(fs::file_size(file_path));
        } else {
            res.body() = std::move(file);
        }
        
        res.prepare_payload();
        return true;
    }
    
private:
    fs::path root_path_;
};