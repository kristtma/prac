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

// === Ticker ===
class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

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

// === остальной код без изменений ===
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

bool IsSubPath(fs::path path, fs::path base) {
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

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
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return false;
        }
        
        std::string target = DecodeUrl(std::string_view{req.target().data(), req.target().size()});
        if (target.find("/api/") == 0) {
            return false;
        }
        
        if (target.empty() || target.back() == '/') {
            target += "index.html";
        }
        
        if (!target.empty() && target[0] == '/') {
            target = target.substr(1);
        }
        
        fs::path file_path = root_path_ / target;
        
        if (!IsSubPath(file_path, root_path_)) {
            return false;
        }
        
        if (fs::is_directory(file_path)) {
            file_path /= "index.html";
        }
        
        if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
            return false;
        }
        
        http::file_body::value_type file;
        sys::error_code ec;
        file.open(file_path.string().c_str(), beast::file_mode::read, ec);
        
        if (ec) {
            return false;
        }
        
        res.version(req.version());
        res.result(http::status::ok);
        std::string ext = file_path.extension().string();
        res.set(http::field::content_type, GetMimeType(ext));
        
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

// === Структура аргументов командной строки ===
struct Args {
    std::optional<int> tick_period_ms;  // Храним как int
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points = false;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    po::options_description desc("Allowed options");
    Args args;

    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<int>()->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value<std::string>(&args.config_file)->value_name("file")->required(), "set config file path")
        ("www-root,w", po::value<std::string>(&args.www_root)->value_name("dir")->required(), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points), "spawn dogs at random positions");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return std::nullopt;
        }
        
        if (vm.count("tick-period")) {
            args.tick_period_ms = vm["tick-period"].as<int>();
            if (*args.tick_period_ms <= 0) {
                std::cerr << "Error: tick-period must be positive" << std::endl;
                return std::nullopt;
            }
        }
        
        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << desc << std::endl;
        return std::nullopt;
    }

    return args;
}

}  // namespace

int main(int argc, const char* argv[]) {
    auto args = ParseCommandLine(argc, argv);
    if (!args) {
        return EXIT_FAILURE;
    }

    try {
        model::Game game = json_loader::LoadGame(args->config_file);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                std::cout << "Signal received, stopping..."sv << std::endl;
                ioc.stop();
            }
        });

        // Создаём strand для синхронизации доступа к API и игровому состоянию
        auto api_strand = net::make_strand(ioc);

        // Создаём обработчик с нужными флагами
        http_handler::RequestHandler handler{
            game, 
            args->randomize_spawn_points,
            args->tick_period_ms.has_value()  // is_auto_tick_mode_
        };

        StaticFileHandler static_handler{args->www_root};

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;
        const net::ip::tcp::endpoint endpoint{address, port};
        
        http_server::ServeHttp(ioc, endpoint, [&handler, &static_handler](auto&& req, auto&& send) {
            http::response<http::file_body> static_res;
            if (static_handler.HandleRequest(req, static_res)) {
                send(std::move(static_res));
                return;
            }
            
            std::string target = DecodeUrl(std::string_view{req.target().data(), req.target().size()});
            if (target.find("/api/") == 0) {
                handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
            } else {
                http::response<http::string_body> not_found_res;
                not_found_res.version(req.version());
                not_found_res.result(http::status::not_found);
                not_found_res.set(http::field::content_type, "text/plain");
                not_found_res.body() = "File not found";
                not_found_res.prepare_payload();
                send(std::move(not_found_res));
            }
        });

        // Запуск Ticker'а, если задан период
        std::shared_ptr<Ticker> ticker;
        if (args->tick_period_ms.has_value()) {
            ticker = std::make_shared<Ticker>(
                api_strand,
                std::chrono::milliseconds(*args->tick_period_ms),
                [&game](std::chrono::milliseconds delta) {
                    for (auto& session : game.GetSessions()) {
                        session.Tick(static_cast<int>(delta.count()));
                    }
                }
            );
            ticker->Start();
        }

        std::cout << "Server has started..."sv << std::endl;

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
