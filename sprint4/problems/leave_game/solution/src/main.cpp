#include "sdk.h"
//
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>

#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <clocale>
#include <thread>
#include <memory>
#include <optional>
#include <vector>

#include "json_loader.h"
#include "extra_data.h"
#include "request_handler.h"
#include "app.h"
#include "server_logger.h"
#include "http_server.h"
#include "serialization.h"


using namespace std::literals;

namespace net = boost::asio;
namespace sys = boost::system;
namespace http = boost::beast::http;

namespace fs = std::filesystem;

namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned number_of_workers, const Fn& fn) {
    number_of_workers = std::max(1u, number_of_workers);
    std::vector<std::jthread> workers;
    workers.reserve(number_of_workers - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--number_of_workers) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace


struct Args {
    std::string tick_period;
    std::optional<std::string> tick_period_opt;
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points = false;
    std::string state_file_str;
    std::optional<std::string> state_file;
    std::string save_state_period_str;
    std::optional<std::string> save_state_period;
};


[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"All options"s};

    Args args;
    desc.add_options()
        // Добавляем опцию --help и её короткую версию -h
        ("help,h", "produce help message")
        // Опция --tick-period milliseconds, сохраняющая свой аргумент в поле args.tick_period
        ("tick-period,t", po::value(&args.tick_period)->value_name("milliseconds"s), "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("file"s), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"s), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions")
        ("state-file,s", po::value(&args.state_file_str)->value_name("file"s), "set state file path")
        ("save-state-period,p", po::value(&args.save_state_period_str)->value_name("milliseconds"s), "set save state period");

    // variables_map хранит значения опций после разбора
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        // Если был указан параметр --help, то выводим справку и возвращаем nullopt
        std::cout << desc;
        return std::nullopt;
    }


    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("Config file path have not been specified"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("Static files root have not been specified"s);
    }

    if (vm.contains("tick-period"s)) {
        args.tick_period_opt = args.tick_period;
    }

    if (vm.contains("randomize-spawn-points"s)) {
        args.randomize_spawn_points = true;
    }

    if (vm.contains("state-file"s)) {
        args.state_file = args.state_file_str;
    }

    if (vm.contains("save-state-period"s)) {
        args.save_state_period = args.save_state_period_str;
    }

    return args;
}


constexpr const char DB_URL_ENV_NAME[]{"GAME_DB_URL"};

std::string GetConfigFromEnv() {
    std::string db_url;
    if (const auto* url = std::getenv(DB_URL_ENV_NAME)) {
        db_url = url;
    } else {
        throw std::runtime_error(DB_URL_ENV_NAME + " environment variable not found"s);
    }
    return db_url;
}



int main(int argc, const char* argv[]) {

    std::setlocale(LC_ALL, "en_US.UTF-8");
  
    try {
        if (auto args = ParseCommandLine(argc, argv)) {
           
            server_logger::InitBoostLog();

            // 1. Загружаем карту из файла и создаем модель игры
            std::pair<model::Game, extra_data::Data> loaded_data = json_loader::LoadGame(args->config_file);
            model::Game game = loaded_data.first;
            extra_data::Data common_extra_data = loaded_data.second;

            // 2. Инициализируем io_context и общий strand поток в нем
            
            const unsigned num_threads = std::thread::hardware_concurrency();

            net::io_context ioc(num_threads);

            // strand для выполнения запросов к API
            auto api_strand = net::make_strand(ioc);

            // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
            net::signal_set signals(ioc, SIGINT, SIGTERM);
            signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
                if (!ec) {
                    ioc.stop();
                }
                server_logger::LogStopping(ec);
            });

            // 4. Создаем приложение (фасад для работы с моделью игры), и создаем внутри него сессии для каждой из карт
            std::string db_url = GetConfigFromEnv();

            app::Application application{game, api_strand, args->tick_period_opt, args->randomize_spawn_points, db_url, num_threads};

            if (args->state_file) {

                serialization::RestoreApplication(application, *args->state_file);

                if (args->save_state_period) {
                    auto serialization_listener = std::make_shared<serialization::SerializationListener>(*args->state_file, *args->save_state_period);
                    application.SetListener(serialization_listener);
                }
            }
            

            // 5. Создаём обработчик HTTP-запросов и связываем его с приложением
            auto handler = std::make_shared<http_handler::RequestHandler>(&application, api_strand, args->www_root, common_extra_data);
            server_logger::LoggingRequestHandler log_handler(handler);

            // 6. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
            const auto address = net::ip::make_address("0.0.0.0");
            constexpr net::ip::port_type port = 8080;

            http_server::ServeHttp(ioc, {address, port}, [&log_handler](std::string_view ip, auto&& req, auto&& send) {
                log_handler(ip, std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
            });

            // Сервер запущен
            server_logger::LogStarting(address, port);

            // 6. Запускаем обработку асинхронных операций
            RunWorkers(std::max(1u, num_threads), [&ioc] {
                ioc.run();
            });

            if (args->state_file) {
                serialization::SaveApplication(application, *args->state_file);
            }

        } else {
            // если args - nullopt, значит был указан параметр --help
            return EXIT_SUCCESS;

        }

    } catch (const std::exception& ex) {
        server_logger::LogStoppingException(ex);
        return EXIT_FAILURE;
    }

    return 0;
}
