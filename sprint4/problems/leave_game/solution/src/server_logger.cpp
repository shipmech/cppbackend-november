#include "server_logger.h"

namespace server_logger {


void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {

    boost::json::object obj;

    auto ts = *rec[timestamp];
    auto iso_string_ts = to_iso_extended_string(ts);

    auto message = rec[logging::expressions::smessage];

    obj["timestamp"s] = iso_string_ts;
    obj["data"s] = *rec[additional_data];
    obj["message"s] = *message;
    strm << json::value(obj);
}


void InitBoostLog() {
    logging::add_common_attributes();

    logging::add_console_log(
        std::cout,
        logging::keywords::format = &MyFormatter,
        logging::keywords::auto_flush = true
    );
}

void BoostLogDataMessage(json::value data, std::string_view message) {
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << message;
}

void LogStarting(net::ip::address address, net::ip::port_type port) {
    std::string_view message = "server started"sv;
    json::value data{{"address"s, address.to_string()}, {"port"s, port}};
    BoostLogDataMessage(data, message);
}

void LogStopping(const sys::error_code& ec) {
    std::string_view message = "server exited"sv;
    json::value data;
    if (!ec) {
        data = json::value{{"code"s, 0}};
    } else {
        data = json::value{{"code"s, "EXIT_FAILURE"s}};
    }
    BoostLogDataMessage(data, message);
}

void LogStoppingException(const std::exception& ex) {
    std::string_view message = "server exited"sv;
    json::value data{{"code"s, "EXIT_FAILURE"s}, {"exception"s, ex.what()}};
    BoostLogDataMessage(data, message);
}

void LogRequest(std::string_view ip, std::string_view uri, std::string_view method) {
    std::string_view message = "request received"sv;
    json::value data{{"ip"s, ip}, {"URI"s, uri}, {"method"s, method}};
    BoostLogDataMessage(data, message);
}

void LogResponse(unsigned response_time, unsigned code, std::string_view content_type) {
    std::string_view message = "response sent"sv;
    json::value data{{"response_time"s, response_time}, {"code"s, code}, {"content_type"s, content_type}};
    BoostLogDataMessage(data, message);
}

void LogNetError(beast::error_code ec, std::string_view where) {
    std::string_view message = "error"s;
    json::value data{{"code"s, ec.value()}, {"text"s, ec.message()}, {"where"s, where}};
    BoostLogDataMessage(data, message);
}

}  // namespace server_logger