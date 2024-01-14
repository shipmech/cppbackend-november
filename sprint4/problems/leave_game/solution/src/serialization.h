#include <boost/serialization/vector.hpp>
#include <fstream>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <iostream>

#include "model.h"
#include "app.h"

namespace geom {

template <typename Archive>
void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
    ar& point.x;
    ar& point.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version) {
    ar& vec.x;
    ar& vec.y;
}

}  // namespace geom

namespace model {

template <typename Archive>
void serialize(Archive& ar, Coords& coords, [[maybe_unused]] const unsigned version) {
    ar& coords.x;
    ar& coords.y;
}

template <typename Archive>
void serialize(Archive& ar, VelocityVector& velocity_vector, [[maybe_unused]] const unsigned version) {
    ar& velocity_vector.vx;
    ar& velocity_vector.vy;
}

}  // namespace model


namespace serialization {

// LostObjectRepr (LostObjectRepresentation) - сериализованное представление класса LostObject
class LostObjectRepr {
public:
    LostObjectRepr() = default;

    explicit LostObjectRepr(const model::LostObject& lost_object)
        : id_(lost_object.GetId())
        , type_(lost_object.GetType())
        , value_(lost_object.GetValue())
        , pos_(lost_object.GetPos()) {
    }

    [[nodiscard]] model::LostObject Restore() const {
        model::LostObject lost_object{id_, type_, value_, pos_};
        return lost_object;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar&* id_;
        ar& type_;
        ar& value_;
        ar& pos_;
    }

private:
    model::LostObject::Id id_ = model::LostObject::Id{0u};
    size_t type_ = 0;
    int value_ = 0;
    model::Coords pos_ = model::Coords{0.0, 0.0};
};

// LootObjectInBagRepr (LootObjectInBagRepresentation) - сериализованное представление класса LootObjectInBag
class LootObjectInBagRepr {
public:
    LootObjectInBagRepr() = default;

    explicit LootObjectInBagRepr(const model::LootObjectInBag& loot_object)
        : id_(loot_object.GetId())
        , type_(loot_object.GetType())
        , value_(loot_object.GetValue()) {
    }

    [[nodiscard]] model::LootObjectInBag Restore() const {
        model::LootObjectInBag loot_object{id_, type_, value_};
        return loot_object;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar&* id_;
        ar& type_;
        ar& value_;
    }

private:
    model::LootObjectInBag::Id id_ = model::LootObjectInBag::Id{0u};
    size_t type_ = 0;
    int value_ = 0;
};

// BagRepr (BagRepresentation) - сериализованное представление класса Bag
class BagRepr {
public:
    BagRepr() = default;

    explicit BagRepr(const model::Bag& bag)
        : capacity_(bag.GetCapacity()) {
        for (const auto& object : bag.GetObjects()) {
            objects_.emplace_back(object);
        }
    }

    [[nodiscard]] model::Bag Restore() const {
        std::vector<model::LootObjectInBag> objects;
        for (const auto& object : objects_) {
            objects.emplace_back(object.Restore());
        }
        model::Bag bag{capacity_, objects};
        return bag;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& capacity_;
        ar& objects_;
    }

    size_t GetCapacity() const {
        return capacity_;
    }

private:
    size_t capacity_;
    std::vector<LootObjectInBagRepr> objects_;
};

// DogRepr (DogRepresentation) - сериализованное представление класса Dog
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog)
        : id_(dog.GetId())
        , coords_(dog.GetCoords())
        , velocity_vector_(dog.GetVelocity())
        , direction_(dog.GetDirection())
        , bag_(dog.GetBag())
        , score_(dog.GetScore()) {
    }

    [[nodiscard]] model::Dog Restore() const {
        model::Dog dog{id_, coords_, bag_.GetCapacity()};
        dog.SetBag(bag_.Restore());
        dog.SetScore(score_);
        dog.SetDirection(direction_);
        dog.SetVelocity(velocity_vector_);
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar&* id_;
        ar& coords_;
        ar& velocity_vector_;
        ar& direction_;
        ar& bag_;
        ar& score_;
    }

private:
    model::Dog::Id id_ = model::Dog::Id{0u};
    model::Coords coords_;
    model::VelocityVector velocity_vector_;
    std::string direction_;
    BagRepr bag_;
    int score_ = 0;
};


struct PlayerInfo {
    std::string name = "";
    DogRepr dog = DogRepr();
    std::string token = "";
};

template <typename Archive>
void serialize(Archive& ar, PlayerInfo& player_info, [[maybe_unused]] const unsigned version) {
    ar& player_info.name;
    ar& player_info.dog;
    ar& player_info.token;
}

// ApplicationRepr (ApplicationRepresentation) - сериализованное представление класса Application
class ApplicationRepr {
public:
    ApplicationRepr() = default;

    explicit ApplicationRepr(const app::Application& application) {
        size_t session_amount = application.GetSessionAmount();

        for (int i = 0; i < session_amount; i++) {

            auto players_info = application.GetPlayerInfoBySessionId(model::GameSession::Id{i});
            auto temp = application.GetLostObjectsBySessionId(model::GameSession::Id{i});
            unsigned lost_object_amount = temp.first;
            auto lost_objects = temp.second;

            std::vector<PlayerInfo> players_vector;
            std::vector<LostObjectRepr> lost_objects_vector;
            
            for (const auto& player_info : players_info) {
                PlayerInfo player = {std::get<0>(player_info), DogRepr(std::get<1>(player_info)), std::get<2>(player_info)};
                players_vector.push_back(player);
            }
            player_info_by_session_index.push_back(players_vector);
            
            for (const auto& lost_object : lost_objects) {
                lost_objects_vector.push_back(LostObjectRepr(lost_object));
            }
            lost_objects_by_session_index.push_back(lost_objects_vector);
            
            lost_objects_amount_by_session_index.push_back(lost_object_amount);
            
        }
    }

    void Restore(app::Application& application) const {
        for (int i = 0; i < player_info_by_session_index.size(); i++) {
            model::GameSession::Id id{i};
            for (const auto& player : player_info_by_session_index[i]) {
                std::string name = player.name;
                DogRepr dog_repr = player.dog;
                model::Dog restored_dog = dog_repr.Restore();
                std::shared_ptr<model::Dog> dog = std::make_shared<model::Dog>(restored_dog);
                app::Token token{player.token};
                application.AddPlayer(id, name, token, dog);
            }
            
            for (const auto& lost_object : lost_objects_by_session_index[i]) {
                std::shared_ptr<model::LostObject> lost_object_ptr = std::make_shared<model::LostObject>(lost_object.Restore());
                application.AddLostObject(id, lost_object_ptr);
            }
        }
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& lost_objects_by_session_index;
        ar& lost_objects_amount_by_session_index;
        ar& player_info_by_session_index;
    }

private:
    std::vector<std::vector<LostObjectRepr>> lost_objects_by_session_index;
    std::vector<unsigned> lost_objects_amount_by_session_index;
    std::vector<std::vector<PlayerInfo>> player_info_by_session_index;
};

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

void SaveApplication(const app::Application& application, std::string filename) {
    std::filesystem::path path{filename + ".tmp"};
    std::ofstream file(path);

    OutputArchive output_archive{file};

    ApplicationRepr application_repr{application};
    output_archive << application_repr;
    std::filesystem::rename(path, filename);
    std::filesystem::remove(path);
}

void RestoreApplication(app::Application& application, std::string filename) {

    std::filesystem::path path{filename};
    if (!std::filesystem::exists(path)) {
        return;
    }    
    std::ifstream file(filename);

    InputArchive input_archive{file};
    ApplicationRepr application_repr;
    input_archive >> application_repr;
    application_repr.Restore(application);
}

using namespace std::chrono_literals;

class SerializationListener : public app::ApplicationListener {

public:
    explicit SerializationListener(std::string filename, std::string save_period_str) {
        auto save_period = std::chrono::milliseconds{std::stoi(save_period_str)};
        filename_ = filename;
        save_period_ = save_period;
        time_since_last_save_ = 0ms;
    }

    void OnTick(app::Application& application, std::chrono::milliseconds delta) override {
        time_since_last_save_ += delta;
        if (time_since_last_save_ < save_period_) {
            return;
        }
        SaveApplication(application, filename_);
        time_since_last_save_ = 0ms;
    }

private:

    std::string filename_;
    std::chrono::milliseconds save_period_;
    std::chrono::milliseconds time_since_last_save_;


};


}  // namespace serialization