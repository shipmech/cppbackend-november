#include "request_handler.h"

#include <string>
#include <string_view>
#include <algorithm>



namespace api_handler {

using namespace std::literals;

const int DEFAULT_URL_PARAMS_START = 0;
const int DEFAULT_URL_PARAMS_MAXITEM = 100;


std::string MakeJsonBodyResponse(const std::string code, const std::string message) {
    boost::json::object obj;
    obj["code"] = code;
    obj["message"] = message;
    
    return boost::json::serialize(obj);
}

ApiResponse MakeApiResponse(http::status status, std::string_view body,
                            unsigned http_version, bool keep_alive) {
    ApiResponse response(status, http_version);
    response.set(http::field::content_type, ContentType::APP_JSON);
    response.set(http::field::cache_control, "no-cache"s);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    return response;
}

bool IsFindMapEndpoint(std::string_view target) {
    return target.substr(0, APIEndpoints::FIND_MAP.size()) == APIEndpoints::FIND_MAP;
}

bool CheckAuthorizationSyntax(std::string_view auth) {
    //Bearer 6516861d89ebfff147bf2eb2b5153ae1
    size_t requaried_size = 39;
    return auth.substr(0, AUTH_BEARER.size()) == AUTH_BEARER && auth.size() == requaried_size;
}

boost::json::object WriteJsonPlayersOnSession(const std::vector<std::shared_ptr<app::Player>>& players) {
    boost::json::object obj;
    for (auto player : players) {

        boost::json::object obj_name;
        obj_name["name"s] = player->GetName();

        obj[std::to_string(*(player->GetId()))] = obj_name;
    }

    return obj;
}

boost::json::object WriteSessionState(const std::vector<std::shared_ptr<app::Player>>& players,
                                      const std::map<model::LostObject::Id, std::shared_ptr<model::LostObject>> lost_objects_on_session) {
    boost::json::object players_obj;
    for (auto player : players) {

        auto dog_coord = player->GetDog()->GetCoords();
        auto dog_velocity = player->GetDog()->GetVelocity();
        std::string dog_dir = player->GetDog()->GetDirection();


        boost::json::array coord = {dog_coord.x, dog_coord.y};
        boost::json::array velocity = {dog_velocity.vx, dog_velocity.vy};

        boost::json::object player_obj;
        player_obj["pos"s] = coord;
        player_obj["speed"] = velocity;
        player_obj["dir"s] = dog_dir;

        boost::json::array items_in_bag_info;
        for (auto object : player->GetDog()->GetBag().GetObjects()) {
            boost::json::object item_in_bag_info;
            item_in_bag_info["id"] = *(object.GetId());
            item_in_bag_info["type"] = object.GetType();
            items_in_bag_info.push_back(item_in_bag_info);
        }

        player_obj["bag"s] = items_in_bag_info;
        player_obj["score"s] = player->GetDog()->GetScore();

        players_obj[std::to_string(*(player->GetId()))] = player_obj;
    }

    boost::json::object lost_objects;
    for (auto pair : lost_objects_on_session) {
        auto object = pair.second;
        auto pos = object->GetPos();
        boost::json::array coord = {pos.x, pos.y};


        boost::json::object lost_object_obj;
        lost_object_obj["type"] = object->GetType();
        lost_object_obj["pos"] = coord;

        lost_objects[std::to_string(*(object->GetId()))] = lost_object_obj;
    }

    boost::json::object global_obj;
    global_obj["players"s] = players_obj;
    global_obj["lostObjects"s] = lost_objects;

    return global_obj;
}

bool ChekMoveActionField(const std::string move_field) {
    bool success = false;
    if (move_field == "U" || move_field == "D" || move_field == "L" || move_field == "R" || move_field == "") {
        success = true;
    }
    return success;
}

bool StringIsNumber(const std::string& s)
{
    return !s.empty() && std::find_if(s.begin(), 
        s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}




ApiResponse ApiRequestHandler::operator()(http::request<http::string_body>&& req) {

    http::verb req_type = req.method();
    std::string_view target = req.target();

    // переделать ветвление под map - ???
    
    if (target == APIEndpoints::GET_MAPS || IsFindMapEndpoint(target)) {
        return MapsApi(std::forward<decltype(req)>(req));

    } else if(target == APIEndpoints::JOIN) {
        return JoinApi(std::forward<decltype(req)>(req));

    } else if(target == APIEndpoints::PLAYERS) {
        return PlayersApi(std::forward<decltype(req)>(req));

    } else if(target == APIEndpoints::STATE) {
        return StateApi(std::forward<decltype(req)>(req));

    } else if(target == APIEndpoints::ACTION) {
        return ActionApi(std::forward<decltype(req)>(req));

    } else if(target == APIEndpoints::TICK) {
        return TickApi(std::forward<decltype(req)>(req));
    } else if(target.substr(0, 20) == APIEndpoints::RECORDS) {
        return RecordsApi(std::forward<decltype(req)>(req));
    }

    return InvalidApi(std::forward<decltype(req)>(req));
    
}


std::optional<app::Token> ApiRequestHandler::TryExtractToken(StringRequest req) {
    std::string_view auth_sv = req.at(http::field::authorization);
    std::string_view token = auth_sv.substr(AUTH_BEARER.size());
    std::string token_str{token.begin(), token.end()};

    std::shared_ptr<app::Player> player = application_->FindPlayerByToken(app::Token{token_str});

    if (player) {
        return app::Token{token_str};
    }
    return std::nullopt;
}


ApiResponse ApiRequestHandler::SetPlayerAction(StringRequest&& req, auto&& json_object) {
    return ExecuteAuthorized(req,
        [this, &req, &json_object](const app::Token& token){

            std::string move = json_object.at("move"s).as_string().c_str();

            this->application_->SetDogVelocityAndDirectionByToken(token, move);
            
            std::string body_response = "{}";
            return MakeApiResponse(http::status::ok, body_response, req.version(), req.keep_alive());
    });
}


ApiResponse ApiRequestHandler::InvalidApi(StringRequest&& req) {
    auto body_response = MakeJsonBodyResponse("invalidApi"s, "Wrong target in API or smth else"s);
    return MakeApiResponse(http::status::bad_request, body_response, req.version(), req.keep_alive());  
}


ApiResponse ApiRequestHandler::MapsApi(StringRequest&& req) {

    http::verb req_type = req.method();
    std::string_view target = req.target();

    if (req_type != http::verb::get && req_type != http::verb::head) {
        std::string body_response = MakeJsonBodyResponse("invalidMethod"s, "Only GET method is expected for GET_MAPS and FIND_MAP"s);
        auto response = MakeApiResponse(http::status::method_not_allowed, body_response, req.version(), req.keep_alive());
        response.set(http::field::allow, "GET, HEAD"s);
        return response;
    }

    if (target == APIEndpoints::GET_MAPS) {
        auto maps = application_->GetMaps();
        std::string body_response = map_parser::GetMapsIdToName(maps);
        return MakeApiResponse(http::status::ok, body_response, req.version(), req.keep_alive());
    }
    
    if (IsFindMapEndpoint(target)) {
        std::string_view map_id = target.substr(APIEndpoints::FIND_MAP.size());
        auto map = application_->FindMap(model::Map::Id(std::string{map_id}));

        if (!map) {
            std::string body_response = MakeJsonBodyResponse("mapNotFound"s, "Map not found for FIND_MAP"s);
            return MakeApiResponse(http::status::not_found, body_response, req.version(), req.keep_alive());
        }

        std::string body_response = map_parser::GetMap(*map, common_extra_data_);
        return MakeApiResponse(http::status::ok, body_response, req.version(), req.keep_alive());
    }
    
    else {
        std::string body_response = MakeJsonBodyResponse("badRequest"s, "Bad requests for GET_MAPS and FIND_MAP"s);
        return MakeApiResponse(http::status::bad_request, body_response, req.version(), req.keep_alive());
    }
}


ApiResponse ApiRequestHandler::JoinApi(StringRequest&& req) {

    http::verb req_type = req.method();
    std::string_view target = req.target();

    if (req_type != http::verb::post) {
        std::string body_response = MakeJsonBodyResponse("invalidMethod"s, "Only POST method is expected for JOIN"s);
        auto response = MakeApiResponse(http::status::method_not_allowed, body_response, req.version(), req.keep_alive());
        response.set(http::field::allow, "POST"s);
        return response;
    }

    sys::error_code ec;
    auto json_object = boost::json::parse(req.body(), ec).as_object();

    if (    ec
            || req.at(http::field::content_type) != ContentType::APP_JSON 
            || json_object.size() != 2
            || !json_object.contains("userName"s)
            || !json_object.contains("mapId"s)) {
        
        std::string body_response = MakeJsonBodyResponse("invalidArgument"s, "JOIN game request parse error"s);
        return MakeApiResponse(http::status::bad_request, body_response, req.version(), req.keep_alive());
    }

    std::string user_name = json_object.at("userName"s).as_string().c_str();
    std::string map_id = json_object.at("mapId"s).as_string().c_str();

    if (user_name.empty()) {
        std::string body_response = MakeJsonBodyResponse("invalidArgument"s, "Invalid name for JOIN"s);
        return MakeApiResponse(http::status::bad_request, body_response, req.version(), req.keep_alive());
    }

    if (!(application_->MapExistById(model::Map::Id(map_id)))) {
        std::string body_response = MakeJsonBodyResponse("mapNotFound"s, "Map not found for JOIN"s);
        return MakeApiResponse(http::status::not_found, body_response, req.version(), req.keep_alive());
    }
    
    std::pair<std::shared_ptr<app::Player>, app::Token> added_player_pair = application_->AddPlayer(user_name, model::Map::Id{map_id});
    app::Player::Id player_id = added_player_pair.first->GetId();
    app::Token token = added_player_pair.second;

    boost::json::object obj;
    obj["authToken"s] = *token;
    obj["playerId"s] = *player_id;

    std::string body_response = boost::json::serialize(obj);
    return MakeApiResponse(http::status::ok, body_response, req.version(), req.keep_alive());
}


ApiResponse ApiRequestHandler::PlayersApi(StringRequest&& req) {


    http::verb req_type = req.method();
    std::string_view target = req.target();

    if (req_type != http::verb::get && req_type != http::verb::head) {
        std::string body_response = MakeJsonBodyResponse("invalidMethod"s, "Only GET and HEAD method is expected for PLAYERS"s);
        auto response = MakeApiResponse(http::status::method_not_allowed, body_response, req.version(), req.keep_alive());
        response.set(http::field::allow, "GET, HEAD"s);
        return response;    
    }

    if (req.count(http::field::authorization) != 1
        || !CheckAuthorizationSyntax(req.at(http::field::authorization))) {
        std::string body_response = MakeJsonBodyResponse("invalidToken"s, "Authorization header is missing or invalid for PLAYERS"s);
        return MakeApiResponse(http::status::unauthorized, body_response, req.version(), req.keep_alive());        
    }
    
    return GetPlayers(std::forward<decltype(req)>(req));
}


ApiResponse ApiRequestHandler::StateApi(StringRequest&& req) {
    http::verb req_type = req.method();
    std::string_view target = req.target();

    if (req_type != http::verb::get && req_type != http::verb::head) {
        std::string body_response = MakeJsonBodyResponse("invalidMethod"s, "Only GET and HEAD method is expected for STATE"s);
        auto response = MakeApiResponse(http::status::method_not_allowed, body_response, req.version(), req.keep_alive());
        response.set(http::field::allow, "GET, HEAD"s);
        return response;    
    }

    if (req.count(http::field::authorization) != 1
        || !CheckAuthorizationSyntax(req.at(http::field::authorization))) {
        std::string body_response = MakeJsonBodyResponse("invalidToken"s, "Authorization header is missing or invalid for STATE"s);
        return MakeApiResponse(http::status::unauthorized, body_response, req.version(), req.keep_alive());        
    }
    
    return GetGameState(std::forward<decltype(req)>(req));
}


ApiResponse ApiRequestHandler::ActionApi(StringRequest&& req) {

    http::verb req_type = req.method();
    std::string_view target = req.target();

    if (req_type != http::verb::post) {
        std::string body_response = MakeJsonBodyResponse("invalidMethod"s, "Only POST method is expected for ACTION"s);
        auto response = MakeApiResponse(http::status::method_not_allowed, body_response, req.version(), req.keep_alive());
        response.set(http::field::allow, "POST"s);
        return response;    
    }

    if (req.at(http::field::content_type) != ContentType::APP_JSON) {
        std::string body_response = MakeJsonBodyResponse("invalidArgument"s, "Invalid content type for ACTION"s);
        return MakeApiResponse(http::status::bad_request, body_response, req.version(), req.keep_alive());    
    }

    sys::error_code ec;
    auto json_object = boost::json::parse(req.body(), ec).as_object();

    if (    ec
            || json_object.size() != 1
            || !json_object.contains("move"s)
            || !ChekMoveActionField(json_object.at("move"s).as_string().c_str()) ) {
        
        std::string body_response = MakeJsonBodyResponse("invalidArgument"s, "Failed to parse jason for ACTION"s);
        return MakeApiResponse(http::status::bad_request, body_response, req.version(), req.keep_alive());
    }

    if (req.count(http::field::authorization) != 1
        || !CheckAuthorizationSyntax(req.at(http::field::authorization))) {
        std::string body_response = MakeJsonBodyResponse("invalidToken"s, "Authorization header is missing or invalid for ACTION"s);
        return MakeApiResponse(http::status::unauthorized, body_response, req.version(), req.keep_alive());        
    }
    
    return SetPlayerAction(std::forward<decltype(req)>(req), std::forward<decltype(json_object)>(json_object));
}



ApiResponse ApiRequestHandler::TickApi(StringRequest&& req) {
   
    http::verb req_type = req.method();
    std::string_view target = req.target();

    if (!application_->IsManualTick()) {
        std::string body_response = MakeJsonBodyResponse("badRequest"s, "Invalid endpoint"s);
        return MakeApiResponse(http::status::bad_request, body_response, req.version(), req.keep_alive());
    }

    if (req_type != http::verb::post) {
        std::string body_response = MakeJsonBodyResponse("invalidMethod"s, "Only POST method is expected for TICK"s);
        auto response = MakeApiResponse(http::status::method_not_allowed, body_response, req.version(), req.keep_alive());
        response.set(http::field::allow, "POST"s);
        return response;    
    }

    if (req.at(http::field::content_type) != ContentType::APP_JSON) {
        std::string body_response = MakeJsonBodyResponse("invalidArgument"s, "Invalid content type for TICK"s);
        return MakeApiResponse(http::status::bad_request, body_response, req.version(), req.keep_alive());    
    }

    sys::error_code ec;
    auto json_value = boost::json::parse(req.body(), ec);

    if (    ec
            || !json_value.is_object()
            || json_value.as_object().size() != 1
            || !json_value.as_object().contains("timeDelta"s)
            || !json_value.as_object().at("timeDelta"s).is_int64()
            ) {
        
        std::string body_response = MakeJsonBodyResponse("invalidArgument"s, "Failed to parse json for TICK"s);
        return MakeApiResponse(http::status::bad_request, body_response, req.version(), req.keep_alive());
    }

    auto tick_period = json_value.as_object().at("timeDelta"s).as_int64();
    auto delta_time = std::chrono::milliseconds{tick_period};
    
    application_->UpdateGameState(delta_time);

    std::string body_response = "{}";
    return MakeApiResponse(http::status::ok, body_response, req.version(), req.keep_alive());
}


ApiResponse ApiRequestHandler::GetRecordsInfo(StringRequest&& req, int start, int max_items) {
    boost::json::array array;

    for (auto& record : application_->GetRecordsInfo(start, max_items)) {
        boost::json::object player;

        player["name"s] = std::get<0>(record);
        player["score"s] = std::get<1>(record);
        player["playTime"s] = std::get<2>(record)/1000.0;

        array.push_back(player);
    }

    std::string body_response = boost::json::serialize(array);
    return MakeApiResponse(http::status::ok, body_response, req.version(), req.keep_alive());
}


ApiResponse ApiRequestHandler::RecordsApi(StringRequest&& req) {
    
    http::verb req_type = req.method();

    if (req_type != http::verb::get) {
        std::string body_response = MakeJsonBodyResponse("invalidMethod"s, "Only GET method is expected for RECORDS"s);
        auto response = MakeApiResponse(http::status::method_not_allowed, body_response, req.version(), req.keep_alive());
        response.set(http::field::allow, "GET"s);
        return response;
    }

    int start = DEFAULT_URL_PARAMS_START;
    int max_item = DEFAULT_URL_PARAMS_MAXITEM;

    std::string_view target_sv = req.target();
    std::string target = {target_sv.begin(), target_sv.end()};

    // parsing /api/v1/game/records?start=1&maxitem=50 for start and maxitem values
       
    if (target.find('?') != std::string::npos) {
            
            if (target.find("start"s) != std::string::npos) {
                std::string::size_type start_pos = target.find("start"s) + 1 + 5;
                std::string param_value = target.substr(start_pos);
                
                std::string::size_type end_pos = param_value.find('&');
                param_value = param_value.substr(0, end_pos);
                
                start = stoi(param_value);
            }
            
            if (target.find("maxItems"s) != std::string::npos) {
                std::string::size_type start_pos = target.find("maxItems"s) + 1 + 8;
                std::string param_value = target.substr(start_pos);
                
                std::string::size_type end_pos = param_value.find('&');
                param_value = param_value.substr(0, end_pos);
                           
                max_item = stoi(param_value);
            }
    }

    if (max_item > DEFAULT_URL_PARAMS_MAXITEM) {
        std::string body_response = MakeJsonBodyResponse("invalidArgument"s, "maxItem Required to be not more than 100"s);
        auto response = MakeApiResponse(http::status::bad_request, body_response, req.version(), req.keep_alive());
        return response;
    }

    return GetRecordsInfo(std::forward<decltype(req)>(req), start, max_item);
}

}  // namespace api_handler
