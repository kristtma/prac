#include "model.h"

#include <stdexcept>
#include <algorithm>
#include <cassert>
namespace model {
using namespace std::literals;
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
void Game::SetLootGeneratorConfig(double period_sec, double prob) {
    auto ms = static_cast<int>(period_sec * 1000);
    loot_gen_.emplace(std::chrono::milliseconds(ms), prob);
}

void Game::Tick(std::chrono::milliseconds dt) {
    // Генерация лута (если нужно)
    /*if (loot_gen_) {
        for (auto& session : sessions_) {
            auto looter_count = session.GetDogs().size();
            auto loot_count = session.GetLootItems().size();
            auto new_count = loot_gen_->Generate(dt, static_cast<unsigned>(loot_count), static_cast<unsigned>(looter_count));
            session.GenerateLoot(new_count, session.GetMap().GetLootTypesCount(), random_gen_);
        }
    }*/

    // Движение и проверка бездействия
    for (auto& session : sessions_) {
        // Обновляем состояние сессии (движение собак, обновление времени игры)
        session.Tick(static_cast<int>(dt.count()));

        // Проверяем бездействие для всех собак в сессии
        auto current_time = session.GetCurrentGameTime();
        
        // Преобразуем время ухода на покой из секунд в миллисекунды
        auto retirement_time_ms = std::chrono::milliseconds(dog_retirement_time_.count() * 1000);
        
        std::cout << "=== DEBUG Game::Tick ===" << std::endl;
        std::cout << "dt: " << dt.count() << "ms" << std::endl;
        std::cout << "current_time: " << current_time.count() << "ms" << std::endl;
        std::cout << "dog_retirement_time_: " << dog_retirement_time_.count() << "s = " 
                  << retirement_time_ms.count() << "ms" << std::endl;
        
        for (auto& dog : session.GetDogs()) {
            if (dog.IsRetired()) {
                std::cout << "Dog '" << dog.GetName() << "' already retired" << std::endl;
                continue;
            }

            // Время бездействия = текущее время - время последней активности
            auto inactive_duration = current_time - dog.GetLastActiveTime();
            
            std::cout << "Dog '" << dog.GetName() << "':" << std::endl;
            std::cout << "  last_active: " << dog.GetLastActiveTime().count() << "ms" << std::endl;
            std::cout << "  inactive_duration: " << inactive_duration.count() << "ms" << std::endl;
            std::cout << "  retirement_threshold: " << retirement_time_ms.count() << "ms" << std::endl;
            std::cout << "  should_retire: " << (inactive_duration >= retirement_time_ms ? "YES" : "NO") << std::endl;
            
            if (inactive_duration >= retirement_time_ms) {
                std::cout << "*** RETIRING DOG: " << dog.GetName() << " ***" << std::endl;
                dog.Retire();
                if (dog_retired_cb_) {
                    std::cout << "Calling dog_retired_cb_ for: " << dog.GetName() << std::endl;
                    dog_retired_cb_(dog, session.GetMap());
                } else {
                    std::cout << "WARNING: dog_retired_cb_ is not set!" << std::endl;
                }
            }
        }
        std::cout << "=== END DEBUG ===" << std::endl;
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

namespace {

class SessionCollisionProvider : public collision_detector::ItemGathererProvider {
public:
    SessionCollisionProvider(GameSession& session) : session_(session) {}
    
    size_t ItemsCount() const override {
        return session_.GetLootItems().size() + session_.GetMap().GetOffices().size();
    }
    
    collision_detector::Item GetItem(size_t idx) const override {
        const auto& loot_items = session_.GetLootItems();
        if (idx < loot_items.size()) {
            // Лут-предметы
            const auto& loot = loot_items[idx];
            return collision_detector::Item{
                geom::Point2D{loot.position.x, loot.position.y},
                0.0  // Ширина предмета = 0
            };
        } else {
            // Офисы
            idx -= loot_items.size();
            const auto& offices = session_.GetMap().GetOffices();
            if (idx < offices.size()) {
                const auto& office = offices[idx];
                return collision_detector::Item{
                    geom::Point2D{static_cast<double>(office.GetPosition().x),
                                 static_cast<double>(office.GetPosition().y)},
                    0.5  // Ширина офиса = 0.5
                };
            }
            throw std::out_of_range("Item index out of range");
        }
    }
    
    size_t GatherersCount() const override {
        return session_.GetDogs().size();
    }
    
    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        const auto& dogs = session_.GetDogs();
        if (idx >= dogs.size()) {
            throw std::out_of_range("Gatherer index out of range");
        }
        
        const auto& dog = dogs[idx];
        // Для коллизий используем половину ширины игрока (0.6 / 2 = 0.3)
        double gatherer_width = 0.3;
        
        // Вычисляем конечную позицию на основе скорости
        Position end_pos = dog.GetPosition();
        double speed = std::hypot(dog.GetSpeedX(), dog.GetSpeedY());
        if (speed > 0) {
            // Предполагаем движение в течение одного тика (dt)
            // В реальности dt будет передаваться извне
            double dt = 0.1; // Примерное значение
            end_pos.x += dog.GetSpeedX() * dt;
            end_pos.y += dog.GetSpeedY() * dt;
        }
        
        return collision_detector::Gatherer{
            geom::Point2D{dog.GetPosition().x, dog.GetPosition().y},
            geom::Point2D{end_pos.x, end_pos.y},
            gatherer_width
        };
    }
    
private:
    GameSession& session_;
};

}  // namespace

void GameSession::ProcessCollisions(double dt) {
    auto provider = std::make_unique<SessionCollisionProvider>(*this);
    auto events = collision_detector::FindGatherEvents(*provider);
    
    // Сортируем события по времени
    std::sort(events.begin(), events.end(),
        [](const collision_detector::GatheringEvent& a,
           const collision_detector::GatheringEvent& b) {
            return a.time < b.time;
        });
    
    // Обрабатываем события в хронологическом порядке
    for (const auto& event : events) {
        auto& dogs = GetDogs();
        if (event.gatherer_id >= dogs.size()) continue;
        
        Dog& dog = dogs[event.gatherer_id];
        const auto& loot_items = GetLootItems();
        const auto& offices = map_.GetOffices();
        
        if (event.item_id < loot_items.size()) {
            // Столкновение с лут-предметом
            LootItem& loot = const_cast<LootItem&>(loot_items[event.item_id]);
            if (!dog.GetBag().IsFull()) {
                CollectLoot(dog, loot);
            }
        } else {
            // Столкновение с офисом
            size_t office_idx = event.item_id - loot_items.size();
            if (office_idx < offices.size()) {
                ReturnLootToOffice(dog, offices[office_idx]);
            }
        }
    }
}

void GameSession::CollectLoot(Dog& dog, LootItem& loot) {
    if (dog.GetBag().TryAddItem(loot.id, loot.type)) {
        // Удаляем лут из игры
        auto& loot_items = GetLootItems();
        auto it = std::find_if(loot_items.begin(), loot_items.end(),
            [&loot](const LootItem& item) { return item.id == loot.id; });
        
        if (it != loot_items.end()) {
            loot_items.erase(it);
        }
    }
}

void GameSession::ReturnLootToOffice(Dog& dog, const Office& office) {
    (void)office;
    
    auto returned_items = dog.GetBag().Clear();
    if (!returned_items.empty()) {
        // Начисляем очки за каждый сданный предмет
        for (const auto& item : returned_items) {
            // Получаем стоимость предмета из конфигурации карты
            int item_value = map_.GetLootValue(item.type);
            dog.AddScore(item_value);
        }
    }
}


}  // namespace model
