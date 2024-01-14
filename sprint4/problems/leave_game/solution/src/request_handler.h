#pragma once

// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/json.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/dispatch.hpp>

#include "app.h"
#include "api_handler.h"
#include "extra_data.h"

#include <algorithm>
#include <cctype>
#include <cassert>
#include <filesystem>
#include <string>
#include <string_view>
#include <chrono>
#include <utility>
#include <memory>

#include <iostream>

namespace http_handler {
namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;

using namespace std::literals;
namespace fs = std::filesystem;


struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    constexpr static std::string_view APP_JSON = "application/json"sv;
    constexpr static std::string_view CSS = "text/css"sv;
    constexpr static std::string_view TXT = "text/plain"sv;
    constexpr static std::string_view JS = "text/javascript"sv;
    constexpr static std::string_view JSON = "application/json"sv;
    constexpr static std::string_view XML = "application/xml"sv;
    constexpr static std::string_view PNG = "image/png"sv;
    constexpr static std::string_view JPG_JPE_JPEG = "image/jpeg"sv;
    constexpr static std::string_view GIF = "image/gif"sv;
    constexpr static std::string_view BMP = "image/bmp"sv;
    constexpr static std::string_view ICO = "image/vnd.microsoft.icon"sv;
    constexpr static std::string_view TIFF_TIF = "image/tiff"sv;
    constexpr static std::string_view SVG_SVGZ = "image/svg+xml"sv;
    constexpr static std::string_view MP3 = "audio/mpeg"sv;
    constexpr static std::string_view EMPTY_UNKNOWN = "application/octet-stream"sv;
};



std::string_view GetContentType(std::string extension);

inline unsigned char from_hex (unsigned char ch);

std::string UrlDecode(std::string_view sv);

bool IsSubPath(fs::path path, fs::path base);

bool PathIsValid(const std::filesystem::path& path);

std::string_view GetFileExtension(std::string_view path);

using StringRequest = http::request<http::string_body>;

// Ответ, тело которого представлено в виде строки
using StringResponse = http::response<http::string_body>;

// Ответ, тело которого представлено в виде файла
using FileResponse = http::response<http::file_body>;

// Создаёт StringResponse с заданными параметрами
StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                  bool keep_alive,
                                  std::string_view content_type);

FileResponse MakeFileResponse(http::status status, http::file_body::value_type& body, unsigned http_version,
                                  bool keep_alive,
                                  std::string_view content_type);

bool IsApiEndpoint(std::string_view target);

void str_to_lower(std::string& str);

using Strand = net::strand<net::io_context::executor_type>;

class FileRequestHandler {
public:
    explicit FileRequestHandler(fs::path base_path)
        : base_path_{fs::weakly_canonical(base_path)} {
    }

    FileRequestHandler(const FileRequestHandler&) = delete;
    FileRequestHandler& operator=(const FileRequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        
        http::verb req_type = req.method();
        std::string_view target = req.target();

        if (req_type == http::verb::get || req_type == http::verb::post) {
            
            http::verb req_type = req.method();
            std::string_view target = req.target();
            
            if (target == "/"sv) {
                target = "/index.html"sv;
            }

            fs::path rel_path(base_path_.string() + "/"s + std::string(target));
            rel_path = fs::weakly_canonical(rel_path);

            if (!IsSubPath(rel_path, base_path_)) {
                send(MakeStringResponse(http::status::bad_request, "File not found"sv, req.version(), req.keep_alive(), ContentType::TXT));
                return;
            }
            else if (!PathIsValid(rel_path)) {
                send(MakeStringResponse(http::status::not_found, "File not found"sv, req.version(), req.keep_alive(), ContentType::TXT));
                return;
            }
            else {

                http::file_body::value_type file;
                if (sys::error_code ec; file.open(rel_path.string().c_str(), beast::file_mode::read, ec), ec) {
                    send(MakeStringResponse(http::status::not_found, "Failed to open file"sv, req.version(), req.keep_alive(), ContentType::TXT));
                    return;
                }

                std::string file_extension = rel_path.extension().string();
                str_to_lower(file_extension);
                send(MakeFileResponse(http::status::ok, file, req.version(), req.keep_alive(), GetContentType(file_extension)));
                return;
            }
        } else {
            send(MakeStringResponse(http::status::method_not_allowed, "Invalid method"sv, req.version(), req.keep_alive(), ContentType::TEXT_HTML));
            return;
        }
    }
private:
    fs::path base_path_;
};



class RequestHandler : public std::enable_shared_from_this<RequestHandler>{
public:
    explicit RequestHandler(app::Application* application,
                            Strand api_strand,
                            fs::path base_path,
                            extra_data::Data& common_extra_data)
        : api_handler_{application, common_extra_data}
        , api_strand_{api_strand}
        , base_path_{fs::weakly_canonical(base_path)} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
 
        std::string_view target = req.target();

        if (IsApiEndpoint(target)) {
            auto handle = [self = this->shared_from_this(), send,
                           &req]() {
                                try {
                                    assert(self->api_strand_.running_in_this_thread());
                                } catch (...) {
                                    return send(MakeStringResponse(http::status::internal_server_error, "Not strand"sv, req.version(), req.keep_alive(), ContentType::TXT));
                                }
                                return send(self->api_handler_(std::forward<decltype(req)>(req)));
                           };
            return net::dispatch(api_strand_, handle);
            
        }

        else {
            http::verb req_type = req.method();
            std::string_view target = req.target();

            if (req_type == http::verb::get || req_type == http::verb::post) {
                
                http::verb req_type = req.method();
                std::string_view target = req.target();
                
                std::string target_str = UrlDecode(target);
                
                
                if (target_str == "/"s) {
                    target_str = "/index.html"s;
                }

                fs::path rel_path(base_path_.string() + "/"s + target_str);
                rel_path = fs::weakly_canonical(rel_path);


                if (!IsSubPath(rel_path, base_path_)) {
                    send(MakeStringResponse(http::status::bad_request, "File not found"sv, req.version(), req.keep_alive(), ContentType::TXT));
                    return;
                }
                else if (!PathIsValid(rel_path)) {
                    send(MakeStringResponse(http::status::not_found, "File not found"sv, req.version(), req.keep_alive(), ContentType::TXT));
                    return;
                }
                else {

                    http::file_body::value_type file;
                    if (sys::error_code ec; file.open(rel_path.string().c_str(), beast::file_mode::read, ec), ec) {
                        send(MakeStringResponse(http::status::not_found, "Failed to open file"sv, req.version(), req.keep_alive(), ContentType::TXT));
                        return;
                    }

                    std::string file_extension = rel_path.extension().string();
                    str_to_lower(file_extension);
                    send(MakeFileResponse(http::status::ok, file, req.version(), req.keep_alive(), GetContentType(file_extension)));
                    return;
                }
            } else {
                send(MakeStringResponse(http::status::method_not_allowed, "Invalid method"sv, req.version(), req.keep_alive(), ContentType::TEXT_HTML));
                return;
            }
            return;
        }

        send(MakeStringResponse(http::status::method_not_allowed, "Invalid method"sv, req.version(), req.keep_alive(), ContentType::TEXT_HTML));
        return;
    }

private:
    api_handler::ApiRequestHandler api_handler_;
    Strand api_strand_;
    fs::path base_path_;
};


}  // namespace http_handler
