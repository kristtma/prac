#pragma once
#include <random>
#include <string>
#include <sstream>
#include <iomanip>
#include "tagged.h"

namespace detail {
struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

class PlayerTokens {
public:
    // Генерирует новый токен
    Token GenerateToken(){
        uint64_t part1 = generator1_();
        uint64_t part2 = generator2_();
        std::ostringstream oss;
        oss << std::hex << std::setfill('0')
            << std::setw(16) << part1
            << std::setw(16) << part2;
        return Token{oss.str()};
    }

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
};