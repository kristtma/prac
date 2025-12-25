#include "model.h"

#include <stdexcept>
#include <algorithm>
#include <cassert>
namespace model {
using namespace std::literals;
// Поместите в model.cpp (в namespace model)
namespace {
    constexpr double ROAD_HALF_WIDTH = 0.4;

    bool IsPointOnHorizontalRoad(double x, double y, const Road& road, double& clamped_x) {
        if (!road.IsHorizontal()) return false;
        double y0 = road.GetStart().y;
        if (std::abs(y - y0) <= ROAD_HALF_WIDTH) {
            double x0 = road.GetStart().x;
            double x1 = road.GetEnd().x;
            if (x0 > x1) std::swap(x0, x1);
            clamped_x = std::clamp(x, x0 + ROAD_HALF_WIDTH, x1 - ROAD_HALF_WIDTH);
            return true;
        }
        return false;
    }

    bool IsPointOnVerticalRoad(double x, double y, const Road& road, double& clamped_y) {
        if (!road.IsVertical()) return false;
        double x0 = road.GetStart().x;
        if (std::abs(x - x0) <= ROAD_HALF_WIDTH) {
            double y0 = road.GetStart().y;
            double y1 = road.GetEnd().y;
            if (y0 > y1) std::swap(y0, y1);
            clamped_y = std::clamp(y, y0 + ROAD_HALF_WIDTH, y1 - ROAD_HALF_WIDTH);
            return true;
        }
        return false;
    }

    std::optional<std::pair<double, double>> ClampToRoad(double x, double y, const Road& road) {
        if (road.IsHorizontal()) {
            double y0 = road.GetStart().y;
            if (std::abs(y - y0) <= 0.4) {
                double x0 = std::min(road.GetStart().x, road.GetEnd().x);
                double x1 = std::max(road.GetStart().x, road.GetEnd().x);
                double min_x = x0 + 0.4;
                double max_x = x1 - 0.4;
                if (min_x > max_x) min_x = max_x = (x0 + x1) / 2.0;
                double clamped_x = std::clamp(x, min_x, max_x);
                return {{clamped_x, y0}};
            }
        } else {
            double x0 = road.GetStart().x;
            if (std::abs(x - x0) <= 0.4) {
                double y0 = std::min(road.GetStart().y, road.GetEnd().y);
                double y1 = std::max(road.GetStart().y, road.GetEnd().y);
                double min_y = y0 + 0.4;
                double max_y = y1 - 0.4;
                if (min_y > max_y) min_y = max_y = (y0 + y1) / 2.0;
                double clamped_y = std::clamp(y, min_y, max_y);
                return {{x0, clamped_y}};
            }
        }
        return std::nullopt;
    }
}
void Dog::Move(double dt_seconds, const std::vector<model::Road>& roads) {
    if (speed_x_ == 0.0 && speed_y_ == 0.0) {
        return;
    }

    double new_x = position_.x + speed_x_ * dt_seconds;
    double new_y = position_.y + speed_y_ * dt_seconds;

    bool moved_horizontally = (speed_x_ != 0.0);
    bool moved_vertically = (speed_y_ != 0.0);

    assert(!(moved_horizontally && moved_vertically));

    constexpr double ROAD_HALF_WIDTH = 0.4;
    constexpr double EPS = 1e-9;

    if (moved_horizontally) {
        // Сначала ищем горизонтальную дорогу под собакой
        std::optional<model::Road> candidate_road;
        for (const auto& road : roads) {
            if (!road.IsHorizontal()) continue;
            double road_y = road.GetStart().y;
            if (std::abs(position_.y - road_y) <= ROAD_HALF_WIDTH + EPS) {
                double x0 = std::min(road.GetStart().x, road.GetEnd().x);
                double x1 = std::max(road.GetStart().x, road.GetEnd().x);
                if (position_.x >= x0 - ROAD_HALF_WIDTH - EPS && position_.x <= x1 + ROAD_HALF_WIDTH + EPS) {
                    candidate_road = road;
                    break;
                }
            }
        }

        if (candidate_road.has_value()) {
            const auto& road = *candidate_road;
            double x0 = std::min(road.GetStart().x, road.GetEnd().x);
            double x1 = std::max(road.GetStart().x, road.GetEnd().x);
            double min_x = x0 - ROAD_HALF_WIDTH;
            double max_x = x1 + ROAD_HALF_WIDTH;

            if (new_x < min_x) {
                new_x = min_x;
                speed_x_ = 0.0;
            } else if (new_x > max_x) {
                new_x = max_x;
                speed_x_ = 0.0;
            }
            position_.x = new_x;
            return;
        }

        // Если горизонтальной дороги нет — пробуем вертикальные (движение перпендикулярно)
        for (const auto& road : roads) {
            if (!road.IsVertical()) continue;
            double road_x = road.GetStart().x;
            if (std::abs(position_.x - road_x) <= ROAD_HALF_WIDTH + EPS) {
                double y0 = std::min(road.GetStart().y, road.GetEnd().y);
                double y1 = std::max(road.GetStart().y, road.GetEnd().y);
                if (position_.y >= y0 - ROAD_HALF_WIDTH - EPS && position_.y <= y1 + ROAD_HALF_WIDTH + EPS) {
                    // Двигаемся в пределах ширины вертикальной дороги
                    double min_x = road_x - ROAD_HALF_WIDTH;
                    double max_x = road_x + ROAD_HALF_WIDTH;
                    if (new_x < min_x) {
                        new_x = min_x;
                        speed_x_ = 0.0;
                    } else if (new_x > max_x) {
                        new_x = max_x;
                        speed_x_ = 0.0;
                    }
                    position_.x = new_x;
                    return;
                }
            }
        }

        // Нет подходящих дорог — останавливаемся
        speed_x_ = 0.0;

    } else if (moved_vertically) {
        // Сначала ищем вертикальную дорогу под собакой
        std::optional<model::Road> candidate_road;
        for (const auto& road : roads) {
            if (!road.IsVertical()) continue;
            double road_x = road.GetStart().x;
            if (std::abs(position_.x - road_x) <= ROAD_HALF_WIDTH + EPS) {
                double y0 = std::min(road.GetStart().y, road.GetEnd().y);
                double y1 = std::max(road.GetStart().y, road.GetEnd().y);
                if (position_.y >= y0 - ROAD_HALF_WIDTH - EPS && position_.y <= y1 + ROAD_HALF_WIDTH + EPS) {
                    candidate_road = road;
                    break;
                }
            }
        }

        if (candidate_road.has_value()) {
            const auto& road = *candidate_road;
            double y0 = std::min(road.GetStart().y, road.GetEnd().y);
            double y1 = std::max(road.GetStart().y, road.GetEnd().y);
            double min_y = y0 - ROAD_HALF_WIDTH;
            double max_y = y1 + ROAD_HALF_WIDTH;

            if (new_y < min_y) {
                new_y = min_y;
                speed_y_ = 0.0;
            } else if (new_y > max_y) {
                new_y = max_y;
                speed_y_ = 0.0;
            }
            position_.y = new_y;
            return;
        }

        // Если вертикальной дороги нет — пробуем горизонтальные (движение перпендикулярно)
        for (const auto& road : roads) {
            if (!road.IsHorizontal()) continue;
            double road_y = road.GetStart().y;
            if (std::abs(position_.y - road_y) <= ROAD_HALF_WIDTH + EPS) {
                double x0 = std::min(road.GetStart().x, road.GetEnd().x);
                double x1 = std::max(road.GetStart().x, road.GetEnd().x);
                if (position_.x >= x0 - ROAD_HALF_WIDTH - EPS && position_.x <= x1 + ROAD_HALF_WIDTH + EPS) {
                    // Двигаемся в пределах ширины горизонтальной дороги
                    double min_y = road_y - ROAD_HALF_WIDTH;
                    double max_y = road_y + ROAD_HALF_WIDTH;
                    if (new_y < min_y) {
                        new_y = min_y;
                        speed_y_ = 0.0;
                    } else if (new_y > max_y) {
                        new_y = max_y;
                        speed_y_ = 0.0;
                    }
                    position_.y = new_y;
                    return;
                }
            }
        }

        // Нет подходящих дорог — останавливаемся
        speed_y_ = 0.0;
    }
}

/*void Dog::Move(double dt_seconds, const std::vector<model::Road>& roads) {
    if (speed_x_ == 0.0 && speed_y_ == 0.0) return;
    
    // Рассчитываем расстояние
    double distance_x = speed_x_ * dt_seconds;
    double distance_y = speed_y_ * dt_seconds;
    
    // Ограничиваем максимальное смещение 0.4
    // (Python сервер так делает)
    if (std::abs(distance_x) > 0.4) {
        distance_x = (distance_x > 0) ? 0.4 : -0.4;
        // Останавливаемся после достижения 0.4
        speed_x_ = 0.0;
        speed_y_ = 0.0;
    }
    if (std::abs(distance_y) > 0.4) {
        distance_y = (distance_y > 0) ? 0.4 : -0.4;
        speed_x_ = 0.0;
        speed_y_ = 0.0;
    }
    
    position_.x += distance_x;
    position_.y += distance_y;
}*/
void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}
// Реализация методов для Players
model::Player& model::Players::AddPlayer(model::Dog& dog, model::GameSession& session) {
    // Генерируем токен (реализация генератора будет в PlayerTokens)
    // Но пока создадим заглушку или используем фиктивный токен для демонстрации структуры.
    // В реальной реализации здесь должен быть вызов генератора токенов.
    // Для примера используем простой токен, но в финальной версии нужно использовать PlayerTokens.
    Token token{"dummy_token_1234567890abcdef"}; // ЗАМЕНИТЬ НА ГЕНЕРАТОР

    // Создаем нового игрока
    players_.emplace_back(session, dog, std::move(token));

    // Присваиваем id (индекс в векторе)
    auto& new_player = players_.back();
    new_player.SetId(players_.size() - 1);

    // Добавляем в мапу по токену
    token_to_player_[new_player.GetToken()] = &new_player;

    return new_player;
}

model::Player* model::Players::FindByToken(const Token& token) const noexcept {
    auto it = token_to_player_.find(token);
    if (it != token_to_player_.end()) {
        return it->second;
    }
    return nullptr;
}

model::Player* model::Players::FindByDogIdAndMapId(const Dog::Id& dog_id, const Map::Id& map_id) const noexcept {
    for (const auto& player : players_) {
        if (player.GetDog().GetId() == dog_id && player.GetSession().GetMap().GetId() == map_id) {
            return const_cast<Player*>(&player); // Это допустимо, так как метод не меняет состояние
        }
    }
    return nullptr;
}

// Реализация метода для Game

}  // namespace model
