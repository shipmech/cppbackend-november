#include "postgres.h"

#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

constexpr auto select_records = "select_records"_zv;

std::shared_ptr<ConnectionPool> GetConnectionPool(size_t capacity, std::string db_url) {
    return std::make_shared<ConnectionPool>(capacity, [db_url] { 

                        auto conn = std::make_shared<pqxx::connection>(db_url);
                        pqxx::work work{*conn};

                        work.exec(R"(
                            CREATE TABLE IF NOT EXISTS retired_players (
                                id UUID CONSTRAINT player_id_constraint PRIMARY KEY UNIQUE NOT NULL,
                                name varchar(100) NOT NULL,
                                score int NOT NULL,
                                play_time_ms int NOT NULL
                            );
                            )"_zv);

                        work.exec(R"(
                            CREATE INDEX IF NOT EXISTS players_score_time_name_idx ON retired_players (score DESC, play_time_ms, name); 
                            )"_zv);

                        // коммитим изменения
                        work.commit(); 

                        conn->prepare(select_records, "SELECT name, score, play_time_ms FROM retired_players ORDER BY score DESC, play_time_ms, name OFFSET $1 LIMIT $2;"_zv);
                        return conn; 

                    });
}

void RecordsRepositoryImpl::Save(const retired_players::RetiredPlayerInfo& info) {
    pqxx::work work{**connection_};
    work.exec_params(
        R"(
            INSERT INTO retired_players (id, name, score, play_time_ms) VALUES ($1, $2, $3, $4);
            )"_zv,
        info.id.ToString(), info.user_name, info.score, info.life_time_ms);
    work.commit();
}

std::vector<std::tuple<std::string, int, int>> RecordsRepositoryImpl::GetRecordsInfo(int start, int max_items) {

    pqxx::work work(**connection_);

    auto result = work.exec_prepared(select_records, start, max_items);
    work.commit();

    std::vector<std::tuple<std::string, int, int>> records;

    for (int rownum = 0; rownum < result.size(); ++rownum) {
        const auto row = result[rownum];
        records.push_back(std::make_tuple(row[0].as<std::string>(), row[1].as<int>(), row[2].as<int>()));
    }
    
    return records;
}

Database::Database(std::shared_ptr<ConnectionWrapper> connection)
    : connection_{connection} {

    }

}  // namespace postgres