#include "app.h"


namespace app {

std::pair<std::shared_ptr<Player>, Token> Players::AddPlayer(std::string& user_name,
                                                    std::shared_ptr<model::Dog> dog,
                                                    model::GameSession::Id game_session_id,
                                                    std::optional<Token> old_token) {

    Player::Id player_id{token_to_session_.size()};
    
    Token new_token = GenerateToken();
    if (old_token) {
        new_token = *old_token;
    }

    std::shared_ptr<Player> new_player = std::make_shared<Player>(player_id, user_name, dog);

    token_to_player_.emplace(new_token, new_player);
    session_to_token_[model::GameSession::Id{game_session_id}].emplace_back(new_token);
    token_to_session_.emplace(new_token, model::GameSession::Id{game_session_id});

    return {new_player, new_token};
}


const std::shared_ptr<Player> Players::FindPlayerByToken(const Token& token) const noexcept {
    if (auto it = token_to_player_.find(token); it != token_to_player_.end()) {
        return it->second;
    }
    return nullptr;
}

const std::vector<std::shared_ptr<Player>> Players::FindPlayersBySessionId(model::GameSession::Id game_session_index) const noexcept {
    std::vector<std::shared_ptr<Player>> players;
    for (const auto& token : session_to_token_.at(game_session_index)) {
        players.push_back(FindPlayerByToken(token));
    }
    return players;
}


Token Players::GenerateToken() {

    auto num1 = generator1_();
    auto num2 = generator2_();
    std::string str{util::MakeHexString(num1) + util::MakeHexString(num2) + util::MakeHexString(num1)};
    str = str.substr(0, 32);
    
    return Token{str};
}



void Ticker::ScheduleTick() {
    assert(strand_.running_in_this_thread());
    timer_.expires_after(period_);
    timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
        self->OnTick(ec);
    });
}

void Ticker::OnTick(sys::error_code ec) {
    using namespace std::chrono;
    assert(strand_.running_in_this_thread());

    if (!ec) {
        auto this_tick = Clock::now();
        auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
        last_tick_ = this_tick;
        try {
            handler_(delta);
        } catch (...) {
            throw std::runtime_error("Ticker::OnTick handler error");
        }
        ScheduleTick();
    }
}

std::pair<std::shared_ptr<Player>, Token> Application::AddPlayer(std::string& user_name, model::Map::Id map_id) {
    model::GameSession::Id game_session_id = game_sessions_->FindSessionIdByMap(map_id);
    model::GameSession* game_session = game_sessions_->GetGameSessionById(game_session_id);
    const model::Map& map = game_session->GetMap();


    model::Coords dog_coords;
    if (is_random_coord_) {
        dog_coords = game_session->GetRandomCoordInRoads();
    } else {
        dog_coords = game_session->GetInitCoordInFirstRoadinMap();
    }

    std::shared_ptr<model::Dog> dog = game_session->AddDog(dog_coords);

    return players_.AddPlayer(user_name, dog, game_session_id);
}

void Application::SetDogVelocityAndDirectionByToken(const Token& token, std::string move) {
    std::shared_ptr<app::Player> player = players_.FindPlayerByToken(token);
    double map_velocity = GetMaxDogSpeedByToken(token);
    
    if (move == "U"s) {
        player->GetDog()->SetDirection(move);
        player->GetDog()->SetVelocity(model::VelocityVector{0.0, -map_velocity});
    } else if (move == "D"s) {
        player->GetDog()->SetDirection(move);
        player->GetDog()->SetVelocity(model::VelocityVector{0.0, map_velocity});
    } else if (move == "L"s) {
        player->GetDog()->SetDirection(move);
        player->GetDog()->SetVelocity(model::VelocityVector{-map_velocity, 0.0});
    } else if (move == "R"s) {
        player->GetDog()->SetDirection(move);
        player->GetDog()->SetVelocity(model::VelocityVector{map_velocity, 0.0});
    } else if (move == ""s) {
        player->GetDog()->SetDirection(move);
        player->GetDog()->SetVelocity(model::VelocityVector{0.0, 0.0});
    }
}


std::vector<RetiredPlayerInfo> Players::OnTick(int delta, int dog_retirement_time) {

    std::vector<RetiredPlayerInfo> retired_players_info;
    std::vector<Token> tokens_to_remove;

    for (const auto& [token, player] : token_to_player_) {

        std::shared_ptr<model::Dog> dog = player->GetDog();

        if (!dog->GetVelocity().IsNull()) {
            continue;
        }

        int inactive_time = dog->AddInactiveTime(delta);

        if (inactive_time >= dog_retirement_time) {

            auto game_session_id = FindGameSessionIdByToken(token);
            std::string user_name = player->GetName();
            auto score = player->GetDog()->GetScore();
            int life_time = static_cast<int>(player->GetDog()->GetLifeTime()*1000);
            auto new_id = RetiredPlayerInfo::RetiredPlayerInfoId::New();
            model::Dog::Id dog_id = dog->GetId();

            retired_players_info.push_back({new_id, user_name, score, life_time, game_session_id, dog_id});
            tokens_to_remove.push_back(token);
        }
    }

    for (auto token : tokens_to_remove) {
        ErasePlayerByToken(token);
    }

    return retired_players_info;
}

} // namespace app