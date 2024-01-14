#pragma once

// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/json.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <memory>
#include <optional>

#include "model.h"
#include "app.h"
#include "map_parser.h"
//#include "request_handler.h"


#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

#include <iostream>
#include <string>


namespace api_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;

using namespace std::literals;

// Повтор структуры из-за ошибки линковки http_handler
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



struct APIEndpoints {
    APIEndpoints() = delete;
    constexpr static std::string_view API = "/api/"sv;
    constexpr static std::string_view GET_MAPS = "/api/v1/maps"sv;
    constexpr static std::string_view FIND_MAP = "/api/v1/maps/"sv;
    constexpr static std::string_view JOIN = "/api/v1/game/join"sv;
    constexpr static std::string_view PLAYERS = "/api/v1/game/players"sv;
    constexpr static std::string_view STATE = "/api/v1/game/state"sv;
    constexpr static std::string_view ACTION = "/api/v1/game/player/action"sv;
    constexpr static std::string_view TICK = "/api/v1/game/tick"sv;
    constexpr static std::string_view RECORDS = "/api/v1/game/records"sv;
};

bool IsFindMapEndpoint(std::string_view target);

std::string MakeJsonBodyResponse(const std::string code, const std::string messgae);

// Ответ, тело которого представлено в виде строки
using ApiResponse = http::response<http::string_body>;

using StringRequest = http::request<http::string_body>;

// Создаёт ApiResponse с заданными параметрами
ApiResponse MakeApiResponse(http::status status, std::string_view body,
                            unsigned http_version, bool keep_alive);


const std::string AUTH_BEARER = "Bearer "s;

bool CheckAuthorizationSyntax(std::string_view auth);

boost::json::object WriteJsonPlayersOnSession(const std::vector<std::shared_ptr<app::Player>>& players);

boost::json::object WriteSessionState(const std::vector<std::shared_ptr<app::Player>>& players,
                                      const std::map<model::LostObject::Id, std::shared_ptr<model::LostObject>>);

bool ChekMoveActionField(const std::string move_field);

bool StringIsNumber(const std::string& s);




class ApiRequestHandler{
public:
    
    explicit ApiRequestHandler(app::Application* application, extra_data::Data& common_extra_data)
        : application_{application}
        , common_extra_data_{common_extra_data} {
    }

    ApiRequestHandler(const ApiRequestHandler&) = delete;
    ApiRequestHandler& operator=(const ApiRequestHandler&) = delete;


    ApiResponse operator()(http::request<http::string_body>&& req);
    

private:
    std::optional<app::Token> TryExtractToken(StringRequest req);

    template <typename Fn>
    ApiResponse ExecuteAuthorized(StringRequest req, Fn&& action) {
        if (auto token = TryExtractToken(req)) {
            return action(*token);
        }
        
        std::string body_response = MakeJsonBodyResponse("unknownToken"s, "Player token has not been found"s);
        return MakeApiResponse(http::status::unauthorized, body_response, req.version(), req.keep_alive());
        
    }

    template <typename Fn>
    ApiResponse GetPlayersInfo(StringRequest&& req, Fn&& writer) {
        return ExecuteAuthorized(req,
            [this, &req, writer = std::forward<decltype(writer)>(writer)](const app::Token& token){
                auto players_on_session = this->application_->FindPlayersInSessionByToken(app::Token{token});
                
                boost::json::object obj = writer(players_on_session);

                std::string body_response = boost::json::serialize(obj);
                return MakeApiResponse(http::status::ok, body_response, req.version(), req.keep_alive());
        });
    }

    ApiResponse GetPlayers(StringRequest&& req) {
        return GetPlayersInfo(std::forward<decltype(req)>(req),
                              std::forward<decltype(WriteJsonPlayersOnSession)>(WriteJsonPlayersOnSession));
    }

    template <typename Fn>
    ApiResponse GetStateInfo(StringRequest&& req, Fn&& writer) {
        return ExecuteAuthorized(req,
            [this, &req, writer = std::forward<decltype(writer)>(writer)](const app::Token& token){
                auto players_on_session = this->application_->FindPlayersInSessionByToken(app::Token{token});
                auto lost_objects_on_session = this->application_->FindLootInSessionByToken(app::Token{token});
                
                boost::json::object obj = writer(players_on_session, lost_objects_on_session);

                std::string body_response = boost::json::serialize(obj);
                return MakeApiResponse(http::status::ok, body_response, req.version(), req.keep_alive());
        });
    }

    ApiResponse GetGameState(StringRequest&& req) {
        return GetStateInfo(std::forward<decltype(req)>(req),
                              std::forward<decltype(WriteSessionState)>(WriteSessionState));
    }

    ApiResponse GetRecordsInfo(StringRequest&& req, int start, int max_items);

    ApiResponse SetPlayerAction(StringRequest&& req, auto&& json_object);

    ApiResponse InvalidApi(StringRequest&& req);

    ApiResponse MapsApi(StringRequest&& req);

    ApiResponse JoinApi(StringRequest&& req);

    ApiResponse PlayersApi(StringRequest&& req);

    ApiResponse StateApi(StringRequest&& req);

    ApiResponse ActionApi(StringRequest&& req);

    ApiResponse TickApi(StringRequest&& req);
    
    ApiResponse RecordsApi(StringRequest&& req);

private:
    app::Application* application_;
    extra_data::Data& common_extra_data_;
};

}  // namespace api_handler

