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

#include "json_loader.h"
#include "request_handler.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;
namespace beast = boost::beast;
namespace http = beast::http;
namespace po = boost::program_options;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
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

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    // Функция handler будет вызываться внутри strand с интервалом period
    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{strand}
        , period_{period}
        , handler_{std::move(handler)} {
    }

    void Start() {
            last_tick_ = Clock::now();
            net::dispatch(strand_, [self = shared_from_this()] {
                self->ScheduleTick();
        });
    }

private:
    void ScheduleTick() {
        assert(strand_.running_in_this_thread());
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnTick(ec);
        });
    }

    void OnTick(sys::error_code ec) {
        using namespace std::chrono;
        assert(strand_.running_in_this_thread());

        if (!ec) {
            auto this_tick = Clock::now();
            auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
            last_tick_ = this_tick;
            try {
                handler_(delta);
            } catch (...) {
            }
            ScheduleTick();
        }
    }

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_{strand_};
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};

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

struct Args {
    std::optional<std::chrono::milliseconds> tick_period;
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points = false;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    po::options_description desc("Allowed options");
    Args args;

    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<int>()->value_name("milliseconds"),
         "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("file")->required(),
         "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir")->required(),
         "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points),
         "spawn dogs at random positions");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return std::nullopt;
        }

        po::notify(vm);

        if (vm.count("tick-period")) {
            int period_ms = vm["tick-period"].as<int>();
            if (period_ms <= 0) {
                throw std::runtime_error("Tick period must be positive");
            }
            args.tick_period = std::chrono::milliseconds(period_ms);
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        std::cerr << desc << std::endl;
        return std::nullopt;
    }

    return args;
}



}  // namespace

int main(int argc, const char* argv[]) {
    try {
        auto args_opt = ParseCommandLine(argc, argv);
        if (!args_opt) {
            return EXIT_SUCCESS;
        }
        auto& args = *args_opt;

        // 1. Загружаем карту из файла и построить модель игры
        //model::Game game = json_loader::LoadGame(args.config_file);
        json_loader::ExtraMapDataMap extra_data;
        model::Game game = json_loader::LoadGame(args.config_file, extra_data);

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                std::cout << "Signal received, stopping..."sv << std::endl;
                ioc.stop();
            }
        });

        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        http_handler::RequestHandler handler{game, extra_data, args.randomize_spawn_points, args.tick_period.has_value()};
        // 5. Создаём обработчик статических файлов
        StaticFileHandler static_handler{args.www_root};
        
        // Создаём strand
        auto api_strand = net::make_strand(ioc);
        
        // Запускаем Ticker, если задан период
        std::shared_ptr<Ticker> ticker = nullptr;
        if (args.tick_period.has_value()) {
            ticker = std::make_shared<Ticker>(
                api_strand,
                *args.tick_period,
                [&game](std::chrono::milliseconds delta) {
                    for (auto& session : game.GetSessions()) {
                        session.Tick(static_cast<int>(delta.count()));
                    }
                }
            );
            ticker->Start();
        }

        // Запустить обработчик HTTP-запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;
        const net::ip::tcp::endpoint endpoint{address, port};
        
        http_server::ServeHttp(ioc, endpoint, 
            [&handler, &static_handler, api_strand](auto&& req, auto&& send) {
                net::dispatch(api_strand, 
                    [&handler, &static_handler, req = std::forward<decltype(req)>(req), send = std::move(send)]() mutable {
                        // 1. Сначала пробуем статические файлы
                        http::response<http::file_body> static_res;
                        if (static_handler.HandleRequest(req, static_res)) {
                            send(std::move(static_res));
                            return;
                        }
                        
                        // 2. Если не статика, проверяем API
                        std::string target = DecodeUrl(std::string(req.target()));
                        if (target.find("/api/") == 0) {
                            handler(std::move(req), std::move(send));
                        } else {
                            // Не API и не статика - возвращаем text/plain 404
                            http::response<http::string_body> not_found_res;
                            not_found_res.version(req.version());
                            not_found_res.result(http::status::not_found);
                            not_found_res.set(http::field::content_type, "text/plain");
                            not_found_res.body() = "File not found";
                            not_found_res.prepare_payload();
                            send(std::move(not_found_res));
                        }
                    });
            });

        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
        std::cout << "Server has started..."sv << std::endl;

        // 6. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
