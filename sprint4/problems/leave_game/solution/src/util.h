#pragma once

#include <compare>
#include <random>
#include <sstream>
#include <string>
#include <chrono>

#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/uuid.hpp>

namespace util {

using namespace std;

/**
 * Вспомогательный шаблонный класс "Маркированный тип".
 * С его помощью можно описать строгий тип на основе другого типа.
 * Пример:
 *
 *  struct AddressTag{}; // метка типа для строки, хранящей адрес
 *  using Address = util::Tagged<std::string, AddressTag>;
 *
 *  struct NameTag{}; // метка типа для строки, хранящей имя
 *  using Name = util::Tagged<std::string, NameTag>;
 *
 *  struct Person {
 *      Name name;
 *      Address address;
 *  };
 *
 *  Name name{"Harry Potter"s};
 *  Address address{"4 Privet Drive, Little Whinging, Surrey, England"s};
 *
 * Person p1{name, address}; // OK
 * Person p2{address, name}; // Ошибка, Address и Name - разные типы
 */
template <typename Value, typename Tag>
class Tagged {
public:
    using ValueType = Value;
    using TagType = Tag;

    explicit Tagged(Value&& v)
        : value_(std::move(v)) {
    }
    explicit Tagged(const Value& v)
        : value_(v) {
    }

    const Value& operator*() const {
        return value_;
    }

    Value& operator*() {
        return value_;
    }

    // Так в C++20 можно объявить оператор сравнения Tagged-типов
    // Будет просто вызван соответствующий оператор для поля value_
    auto operator<=>(const Tagged<Value, Tag>&) const = default;

private:
    Value value_;
};

// Хешер для Tagged-типа, чтобы Tagged-объекты можно было хранить в unordered-контейнерах
template <typename TaggedValue>
struct TaggedHasher {
    size_t operator()(const TaggedValue& value) const {
        // Возвращает хеш значения, хранящегося внутри value
        return std::hash<typename TaggedValue::ValueType>{}(*value);
    }
};


template <typename T>
std::string MakeHexString(const T& value) {
    std::stringstream ss;
    ss << std::hex << value;
    std::string hexString = ss.str();
    return hexString;
}


class DoubleGenerator {
public:
    DoubleGenerator() {
        std::mt19937_64 rng; // A Mersenne Twister pseudo-random generator of 64-bit numbers with a state size of 19937 bits.
        
        // initialize the random number generator with time-dependent seed        
        uint64_t timeSeed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        std::seed_seq ss { uint32_t(timeSeed & 0xffffffff), uint32_t(timeSeed >> 32) };
        rng.seed(ss);

        std::uniform_real_distribution<double> unif(0.0, 1.0);

        rng_ = rng;
        unif_ = unif;
    }
    
    double Get() {
        return unif_(rng_);
    }


private:
    std::mt19937_64 rng_;
    std::uniform_real_distribution<double> unif_;
};



}  // namespace util


namespace util {

namespace detail {

using UUIDType = boost::uuids::uuid;

UUIDType NewUUID();
constexpr UUIDType ZeroUUID{{0}};

std::string UUIDToString(const UUIDType& uuid);
UUIDType UUIDFromString(std::string_view str);

}  // namespace detail

template <typename Tag>
class TaggedUUID : public Tagged<detail::UUIDType, Tag> {
public:
    using Base = Tagged<detail::UUIDType, Tag>;
    using Tagged<detail::UUIDType, Tag>::Tagged;

    TaggedUUID()
        : Base{detail::ZeroUUID} {
    }

    static TaggedUUID New() {
        return TaggedUUID{detail::NewUUID()};
    }

    static TaggedUUID FromString(const std::string& uuid_as_text) {
        return TaggedUUID{detail::UUIDFromString(uuid_as_text)};
    }


    std::string ToString() const {
        return detail::UUIDToString(**this);
    }
};

}  // namespace util