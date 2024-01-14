#pragma once

// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/date_time.hpp>

#include <boost/json.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <string>
#include <string_view>
#include <chrono>
#include <utility>
#include <memory>

namespace server_logger {

namespace logging = boost::log;
namespace keywords = boost::log::keywords;

namespace json = boost::json;

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;

using namespace std::literals;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)


void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm);

void InitBoostLog();

void BoostLogDataMessage(json::value data, std::string_view message);

void LogStarting(net::ip::address address, net::ip::port_type port);

void LogStopping(const sys::error_code& ec);

void LogStoppingException(const std::exception& ex);

void LogRequest(std::string_view ip, std::string_view uri, std::string_view method);

void LogResponse(unsigned response_time, unsigned code, std::string_view content_type);

void LogNetError(beast::error_code ec, std::string_view where);


template<class SomeRequestHandler>
class LoggingRequestHandler {
    static void LogRequest(std::string_view ip, const auto& req) {
        
        std::string_view uri = req.target();
        std::string_view method = req.method_string();
        server_logger::LogRequest(ip, uri, method);
    }

    template <typename ResponseType>
    static void LogResponse(const ResponseType& resp, const auto t_start) {
        const auto t_end = std::chrono::high_resolution_clock::now();
        const auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();

        int code = resp.result_int();
        std::string_view content_type;
        try {
            content_type = resp.at(http::field::content_type);
        } catch (const std::exception& ex) {
            content_type = "null"sv;
        }

        server_logger::LogResponse(response_time, code, content_type);
    }

public:
    explicit LoggingRequestHandler(std::shared_ptr<SomeRequestHandler> handler)
        : decorated_{handler} {
    }


    LoggingRequestHandler(const LoggingRequestHandler&) = delete;
    LoggingRequestHandler& operator=(const LoggingRequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(std::string_view ip, http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {

        LogRequest(ip, req);
        

        const auto t_start = std::chrono::high_resolution_clock::now();

        auto new_send = [t_start, send = std::forward<decltype(send)>(send)](auto&& response) {
            LogResponse(response, t_start);
            send(std::move(response));
        } ;

        decorated_->operator()(std::forward<decltype(req)>(req), std::forward<decltype(new_send)>(new_send));

    }

private:
    std::shared_ptr<SomeRequestHandler> decorated_;
};

}  // namespace server_logger