#pragma once

#include "util.h"
#include "model.h"

#include <string>

namespace retired_players {

namespace detail {
struct RetiredPlayerInfoTag {};
}  // namespace detail

struct RetiredPlayerInfo {

    using RetiredPlayerInfoId = util::TaggedUUID<detail::RetiredPlayerInfoTag>;

    RetiredPlayerInfoId id;
    std::string user_name;
    int score = 0;
    int life_time_ms = 0;
    model::GameSession::Id game_session_id;
    model::Dog::Id dog_id;
};

class RecordsRepository {
public:
    virtual void Save(const RetiredPlayerInfo& author) = 0;
    virtual std::vector<std::tuple<std::string, int, int>> GetRecordsInfo(int start, int max_items) = 0;

protected:
    ~RecordsRepository() = default;
};

} // namespace retired_players