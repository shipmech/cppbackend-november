#pragma once

// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/dispatch.hpp>

#include "model.h"
#include "util.h"
#include "retired_players.h"
#include "postgres.h"

#include <memory>
#include <string>
#include <iostream>
#include <chrono>
#include <cctype>
#include <cassert>
#include <optional>
#include <utility>
#include <cmath>


namespace app {

const double VEL_TOL = 1e-9;

namespace net = boost::asio;
namespace sys = boost::system;

using namespace std::literals;
using namespace std::chrono_literals;


struct TokenTag {};
using Token = util::Tagged<std::string, TokenTag>;

class Player {   
public:
    using Id = util::Tagged<std::uint32_t, Player>;
    Player(Id id, std::string& user_name,std::shared_ptr<model::Dog> dog)
        : id_{id}
        , user_name_{user_name}
        , dog_id_{dog->GetId()}
        , dog_{dog} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string GetName() const noexcept {
        return user_name_;
    }

    const std::shared_ptr<model::Dog> GetDog() const noexcept {
        return dog_;
    }

private:
    Id id_;
    model::Dog::Id dog_id_;
    std::shared_ptr<model::Dog> dog_;
    std::string user_name_;
};


using RetiredPlayerInfo = retired_players::RetiredPlayerInfo;
using RecordsRepository = retired_players::RecordsRepository;

class Players {
public:
    Players() = default;

    std::pair<std::shared_ptr<Player>, Token> AddPlayer(std::string& user_name,
                                                        std::shared_ptr<model::Dog> dog,
                                                        model::GameSession::Id game_session_id,
                                                        std::optional<Token> old_token = std::nullopt);

    const std::shared_ptr<Player> FindPlayerByToken(const Token& token) const noexcept;

    const std::vector<std::shared_ptr<Player>> FindPlayersBySessionId(model::GameSession::Id game_session_index) const noexcept;

    model::GameSession::Id FindGameSessionIdByToken(const Token& token) const noexcept {
        return token_to_session_.at(token);
    }

    model::Dog::Id FindDogIdByToken(const Token& token) const noexcept {
        auto player = FindPlayerByToken(token);
        return player->GetDog()->GetId();
    }

    void ErasePlayerByToken(const Token& token) {
        

        auto session_id = FindGameSessionIdByToken(token);
        auto token_it = std::find(session_to_token_[session_id].begin(), session_to_token_[session_id].end(), token);
        session_to_token_[session_id].erase(token_it);

        token_to_player_.erase(token);
        token_to_session_.erase(token);
    }

    std::vector<RetiredPlayerInfo> OnTick(int delta, int dog_retirement_time_);



private:

    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};

    // Чтобы сгенерировать токен, получите из generator1_ и generator2_
    // два 64-разрядных числа и, переведя их в hex-строки, склейте в одну.
    Token GenerateToken();


private:
    using TokenHasher = util::TaggedHasher<Token>;
    using TokenToPlayer = std::unordered_map<Token, std::shared_ptr<Player>, TokenHasher>;

    using GameSessionHasher = util::TaggedHasher<model::GameSession::Id>;
    using GameSessionToToken = std::unordered_map<model::GameSession::Id, std::vector<Token>, GameSessionHasher>;

    using TokenToGameSession = std::unordered_map<Token, model::GameSession::Id, TokenHasher>;

    
    TokenToPlayer token_to_player_;
    GameSessionToToken session_to_token_;
    TokenToGameSession token_to_session_;

public:
    GameSessionToToken GetSessionToToken() const {
        return session_to_token_;
    }
};





using Strand = net::strand<net::io_context::executor_type>;


class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    // Функция handler будет вызываться внутри strand с интервалом period
    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{strand}
        , period_{period}
        , handler_{std::move(handler)} {
    }

    void Start() {
        net::dispatch(strand_, [self = shared_from_this()] {
            self->last_tick_ = Clock::now();
            self->ScheduleTick();
        });
    }

private:
    void ScheduleTick();

    void OnTick(sys::error_code ec);

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_{strand_};
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};

class Application;

class ApplicationListener {
public:
    ApplicationListener() = default;
    virtual void OnTick(Application& application, std::chrono::milliseconds delta) = 0;
};


class Application {
public:

    Application(const model::Game& game, Strand strand,
                std::optional<std::string> tick_period, bool is_random_coord,
                std::string db_url, const unsigned num_threads)
        : game_{game}
        , is_random_coord_(is_random_coord)
        , conn_pool_{postgres::GetConnectionPool(num_threads, db_url)} {

        game_sessions_ = std::make_shared<model::GameSessions>(game_);
        game_sessions_->InitializeOneSessionPerMap();
        dog_retirement_time_ = game_sessions_->GetDogRetirementTime();

        if (tick_period) {
            auto tick_period_milliseconds = std::chrono::milliseconds{std::stoi(*tick_period)};

            auto ticker = std::make_shared<Ticker>(strand, tick_period_milliseconds,
                            [this](std::chrono::milliseconds delta) {
                                    this->UpdateGameState(delta);
                                }
            );

            ticker->Start();
            is_manual_tick_ = false;
        } else {
            is_manual_tick_ = true;
        }

        // для инициализации таблицы
        auto conn = conn_pool_->GetConnectionPtr();
        postgres::Database db = postgres::Database(conn);
    }

    void SetListener(std::shared_ptr<ApplicationListener> listener) {
        listener_ = listener;
    }

    bool MapExistById(const model::Map::Id& id) const noexcept {
        return game_sessions_->MapExist(id);
    }

    std::vector<std::tuple<std::string, int, int>> GetRecordsInfo(int start, int max_items) {
        auto conn = conn_pool_->GetConnectionPtr();
        postgres::Database db = postgres::Database(conn);
        return db.GetRecords().GetRecordsInfo(start, max_items);
    }

    void UpdateGameState(std::chrono::milliseconds delta) {

        double delta_time = delta / 1.0s; 
        game_sessions_->UpdateGameState(delta_time);
         
        if(listener_) {
            listener_->OnTick(*this, delta);
        }
        
        std::vector<RetiredPlayerInfo> retired_players_info{std::move(players_.OnTick(delta.count(), dog_retirement_time_))};

        for (const auto& player_info : retired_players_info) {
            EraseDogsInSessions({player_info.game_session_id, player_info.dog_id});
            auto conn = conn_pool_->GetConnectionPtr();
            postgres::Database db = postgres::Database(conn);
            db.GetRecords().Save(player_info);
        }
    }

    const std::vector<model::Map>& GetMaps() const noexcept {
        return game_.GetMaps();
    }

    const model::Map* FindMap(const model::Map::Id& id) const noexcept {
        return game_.FindMap(id);
    }

    std::pair<std::shared_ptr<Player>, Token> AddPlayer(std::string& user_name, model::Map::Id map_id);

    const std::shared_ptr<Player> FindPlayerByToken(const Token& token) const noexcept {
        return players_.FindPlayerByToken(token);
    }

    model::GameSession::Id FindGameSessionIdByToken(const Token& token) const {
        return players_.FindGameSessionIdByToken(token);
    }

    const std::vector<std::shared_ptr<Player>> FindPlayersInSessionByToken(const Token& token) const noexcept {
        return players_.FindPlayersBySessionId(FindGameSessionIdByToken(app::Token{token}));
    }

    const std::map<model::LostObject::Id, std::shared_ptr<model::LostObject>> FindLootInSessionByToken(const Token& token) const noexcept {
        return game_sessions_->GetLostObjectsBySessionId(FindGameSessionIdByToken(app::Token{token}));
    }

    double GetMaxDogSpeedByToken(const Token& token) const noexcept {
        return game_sessions_->GetMaxDogSpeedBySessionId(players_.FindGameSessionIdByToken(token));
    }

    void SetDogVelocityAndDirectionByToken(const Token& token, std::string move);

    bool IsManualTick() const noexcept {
        return is_manual_tick_;
    }


    // For serialization
    void AddPlayer(model::GameSession::Id game_session_id, std::string& user_name, Token& token, std::shared_ptr<model::Dog> dog) {
        model::GameSession* game_session = game_sessions_->GetGameSessionById(game_session_id);
        game_session->AddDog(dog);
        players_.AddPlayer(user_name, dog, game_session_id, token);
    }
    

    void AddLostObject(model::GameSession::Id game_session_id, std::shared_ptr<model::LostObject> lost_object) {
        model::GameSession* game_session = game_sessions_->GetGameSessionById(game_session_id);
        game_session->AddLostObject(lost_object);
    }

    using PlayerInfo = std::tuple<std::string, model::Dog, std::string>;
    std::vector<PlayerInfo> GetPlayerInfoBySessionId(model::GameSession::Id game_session_id) const noexcept {
        std::vector<PlayerInfo> info;

        auto tokens = players_.GetSessionToToken()[game_session_id];

        for (auto& token : tokens) {
            auto player = players_.FindPlayerByToken(token);
            info.push_back(std::make_tuple(player->GetName(), *(player->GetDog()), *token));
        }

        return info;
    }

    std::pair<unsigned, std::vector<model::LostObject>> GetLostObjectsBySessionId(model::GameSession::Id game_session_id) const noexcept {
        std::vector<model::LostObject> lost_objects;

        auto lost_objects_by_session_id = game_sessions_->GetLostObjectsBySessionId(game_session_id);
        for (auto& lost_object : lost_objects_by_session_id) {
            lost_objects.push_back(*(lost_object.second));
        }

        unsigned lost_object_id_counter = game_sessions_->GetLostObjectIdCounterBySessionId(game_session_id);

        return std::make_pair(lost_object_id_counter, lost_objects);
    }

    size_t GetSessionAmount() const {
        return game_sessions_->GetSessionAmount();
    }

    void EraseDogsInSessions(std::pair<model::GameSession::Id, model::Dog::Id> game_session_dog_id_pair) {
        auto session_id = game_session_dog_id_pair.first;
        auto dog_id = game_session_dog_id_pair.second;

        game_sessions_->EraseDogBySessionId(session_id, dog_id);
    }

private:
    const model::Game& game_;
    std::shared_ptr<model::GameSessions> game_sessions_;
    Players players_;

    bool is_manual_tick_;
    bool is_random_coord_;

    std::shared_ptr<ApplicationListener> listener_;

    std::shared_ptr<postgres::ConnectionPool> conn_pool_;

    int dog_retirement_time_;

};


} // namespace app 