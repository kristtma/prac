#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <chrono>
#include <memory>

#include "tagged.h"
#include "bag.h"
#include "player_tokens.h"
#include "loot_generator.h"
#include "collision_detector.h"

#include <random>
namespace model {
struct LootType {
    std::string name;
    std::string file;
    std::string type;
    int rotation = 0;
    std::string color;
    double scale = 1.0;
    int value = 0;  // Добавляем поле value
};
using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};
struct Position {
    double x = 0.0;
    double y = 0.0;
};

enum class Direction {
    NORTH = 'U',
    SOUTH = 'D',
    WEST = 'L',
    EAST = 'R'
};
struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }
    double GetDogSpeed() const noexcept {
        return dog_speed_;
    }
    void SetDogSpeed(double speed) noexcept {
        dog_speed_ = speed;
    }

    void AddOffice(Office office);
    size_t GetLootTypesCount() const noexcept { return loot_types_count_; }
    void SetLootTypesCount(size_t count) noexcept { loot_types_count_ = count; }
    size_t GetBagCapacity() const noexcept { return bag_capacity_; }
    void SetBagCapacity(size_t capacity) noexcept { bag_capacity_ = capacity; }
    const std::vector<LootType>& GetLootTypes() const noexcept { return loot_types_; }
    void AddLootType(LootType loot_type) { loot_types_.push_back(std::move(loot_type)); }
    void ClearLootTypes() { loot_types_.clear(); } 
    // Получить стоимость предмета по типу
    int GetLootValue(size_t type_index) const {
        if (type_index < loot_types_.size()) {
            return loot_types_[type_index].value;
        }
        return 0; // Значение по умолчанию
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;
    double dog_speed_ = 3.0;
    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    size_t loot_types_count_ = 0;
    size_t bag_capacity_ = 3;
    std::vector<LootType> loot_types_;  
};
// Объявляем Dog до Map, Players и других зависимых классов
class Dog {
public:
    using Id = util::Tagged<std::string, Dog>;

    explicit Dog(Id id, std::string name)
        : id_(std::move(id)), name_(std::move(name)) {}

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }
    // Новые методы
    const Position& GetPosition() const noexcept { return position_; }
    void SetPosition(Position pos) noexcept { position_ = pos; }

    double GetSpeedX() const noexcept { return speed_x_; }
    double GetSpeedY() const noexcept { return speed_y_; }
    void SetSpeed(double x, double y) noexcept {
        speed_x_ = x;
        speed_y_ = y;
    }
    void SetSpeedFromDirection(Direction dir, double speed_value) {
    switch (dir) {
        case Direction::WEST:  speed_x_ = -speed_value; speed_y_ = 0.0; break;
        case Direction::EAST:  speed_x_ =  speed_value; speed_y_ = 0.0; break;
        case Direction::NORTH: speed_x_ = 0.0; speed_y_ = -speed_value; break;
        case Direction::SOUTH: speed_x_ = 0.0; speed_y_ =  speed_value; break;
    }
    direction_ = dir;
    }
    Direction GetDirection() const noexcept { return direction_; }
    void SetDirection(Direction dir) noexcept { direction_ = dir; }
    // Позже сюда можно добавить позицию, направление и т.д.
    void SetPlayerId(size_t id) noexcept {
        player_id_ = id;
    }
    void Move(double dt_seconds, const std::vector<model::Road>& roads);
    size_t GetPlayerId() const noexcept {
        return player_id_;
    }
    size_t GetPlayerId() noexcept {
        return player_id_;
    }
    void SetCurrentRoad(const Road* road) noexcept { current_road_ = road; }
    const Road* GetCurrentRoad() const noexcept { return current_road_; }
    Bag& GetBag() noexcept { return bag_; }
    const Bag& GetBag() const noexcept { return bag_; }
    void SetBagCapacity(size_t capacity) { bag_ = Bag(capacity); }
    int GetScore() const noexcept { return score_; }
    void AddScore(int value) noexcept { score_ += value; }
    void SetScore(int score) noexcept { score_ = score; }

private:
    size_t player_id_ = 0;
    Id id_;
    std::string name_;
    double speed_x_ = 0.0;
    double speed_y_ = 0.0;
    Position position_;
    Direction direction_ = Direction::NORTH;
    const Road* current_road_ = nullptr;
    Bag bag_{3};
    int score_ = 0;
};

using Dogs = std::vector<Dog>;  // удобный алиас
struct LootItem {
    size_t id = 0;
    size_t type = 0;        
    Position position{0.0, 0.0};
};

class GameSession {
public:
    using Id = util::Tagged<std::string, GameSession>;
    using Dogs = std::vector<Dog>;

    explicit GameSession(Map& map) : map_(map) {}

    const Map& GetMap() const noexcept {
        return map_;
    }
    Dog* FindDogById(size_t player_id) noexcept {
        if (player_id < dogs_.size()) {
            return &dogs_[player_id];
        }
        return nullptr;
    }
    // Метод для добавления собаки в сессию
    Dog& AddDog(Dog dog) {
        dogs_.emplace_back(std::move(dog));
        return dogs_.back();
    }

    Dogs& GetDogs()  noexcept {
        return dogs_;
    }
    // GameSession
    double GetDogSpeed() const noexcept {
        return map_.GetDogSpeed(); // уже содержит default, если не задан
    }
    void SetPlayerId(size_t id) noexcept { player_id_ = id; }
    size_t GetPlayerId() const noexcept { return player_id_; }
    void Tick(int time_delta_ms, size_t loot_types_count, std::mt19937& random_gen) {
        double dt = time_delta_ms / 1000.0; // секунды
        // Сохраняем начальные позиции
        std::vector<Position> start_positions;
        for (auto& dog : dogs_) {
            start_positions.push_back(dog.GetPosition());
        }
        
        // Двигаем собак
        for (auto& dog : dogs_) {
            dog.Move(dt, map_.GetRoads());
        }
        
        // Обрабатываем коллизии
        ProcessCollisions(dt);
    }
    void Tick(int time_delta_ms) {
        double dt = time_delta_ms / 1000.0; // секунды
        for(auto& dog : dogs_){
            dog.Move(dt, map_.GetRoads());
        }
    }
    void GenerateLoot(size_t count, size_t loot_types_count, std::mt19937& random_gen) {
        if (count > 0) {
            LootItem loot;
            loot.id = next_loot_id_++;
            loot.type = random_gen() % loot_types_count; // Случайный тип
            loot.position = GenerateRandomPosition(random_gen, map_.GetRoads());
            loot_items_.push_back(loot);
        }
    }
    std::vector<LootItem>& GetLootItems() { return loot_items_; }
    const std::vector<LootItem>& GetLootItems() const { return loot_items_; }
    void AddLootItem(LootItem item) { loot_items_.push_back(std::move(item)); }
    void ProcessCollisions(double dt);
    void ReturnLootToOffice(Dog& dog, const Office& office);
    void CollectLoot(Dog& dog, LootItem& loot);
private:
    Position GenerateRandomPosition(std::mt19937& gen, const std::vector<Road>& roads) {
        if (roads.empty()) {
            return {0.0, 0.0};
        }
        
        // Выбираем случайную дорогу
        std::uniform_int_distribution<> road_dist(0, roads.size() - 1);
        const auto& road = roads[road_dist(gen)];
        
        // Генерируем позицию на дороге
        if (road.IsHorizontal()) {
            double x0 = std::min(road.GetStart().x, road.GetEnd().x);
            double x1 = std::max(road.GetStart().x, road.GetEnd().x);
            double road_y = road.GetStart().y;
            
            // Сужаем диапазон, чтобы лут был на дороге, а не по краям
            double min_x = x0 + 0.5;
            double max_x = x1 - 0.5;
            if (min_x > max_x) {
                min_x = max_x = (x0 + x1) / 2.0;
            }
            
            std::uniform_real_distribution<> x_dist(min_x, max_x);
            return {x_dist(gen), static_cast<double>(road_y)};
        } else {
            double y0 = std::min(road.GetStart().y, road.GetEnd().y);
            double y1 = std::max(road.GetStart().y, road.GetEnd().y);
            double road_x = road.GetStart().x;
            
            double min_y = y0 + 0.5;
            double max_y = y1 - 0.5;
            if (min_y > max_y) {
                min_y = max_y = (y0 + y1) / 2.0;
            }
            
            std::uniform_real_distribution<> y_dist(min_y, max_y);
            return {static_cast<double>(road_x), y_dist(gen)};
        }
    }
    Map& map_;
    size_t player_id_ = 0;
    Dogs dogs_;
    std::vector<LootItem> loot_items_;
    size_t next_loot_id_ = 0;
    collision_detector::ItemGathererProvider* CreateCollisionProvider();
    void HandleCollisionEvents(const std::vector<collision_detector::GatheringEvent>& events);
};
class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);
    void AddSession(Map& map) {
        sessions_.emplace_back(map);
    }
    std::vector<GameSession>& GetSessions() noexcept {
        return sessions_;
    }
    const Maps& GetMaps() const noexcept {
        return maps_;
    }
    // Найти сессию по id карты
    GameSession* FindSession(const Map::Id& map_id) noexcept {
        for (auto& session : sessions_) {
            if (session.GetMap().GetId() == map_id) {
                return &session;
            }
        }
        return nullptr;
    }
    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }
    double GetDefaultDogSpeed() const noexcept { return default_dog_speed_; }
    void SetDefaultDogSpeed(double speed) noexcept { default_dog_speed_ = speed; }
    void SetLootGeneratorConfig(double period_sec, double prob);
    void Tick(std::chrono::milliseconds dt);
    size_t GetDefaultBagCapacity() const noexcept { return default_bag_capacity_; }
    void SetDefaultBagCapacity(size_t capacity) noexcept { default_bag_capacity_ = capacity; }

private:
    double default_dog_speed_ = 3.0;
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    std::vector<GameSession> sessions_; // Сессии игры
    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    std::optional<loot_gen::LootGenerator> loot_gen_;
    std::mt19937 random_gen_{std::random_device{}()};
    size_t default_bag_capacity_ = 3;
};
class Player {
public:
    using Id = util::Tagged<size_t, Player>; // Используем size_t для id игрока

    // Конструктор для создания нового игрока
    Player(GameSession& session, Dog& dog, Token token)
        : session_(&session), dog_(&dog), token_(std::move(token)) {}

    // Получить id игрока (индекс в векторе players_)
    Id GetId() const noexcept {
        return Id{player_id_};
    }

    // Установить id игрока (назначается при добавлении в Players)
    void SetId(size_t id) noexcept {
        player_id_ = id;
    }

    // Получить токен
    const Token& GetToken() const noexcept {
        return token_;
    }

    // Получить ссылку на сессию
    GameSession& GetSession() const noexcept {
        return *session_;
    }

    // Получить ссылку на собаку
    Dog& GetDog() const noexcept {
        return *dog_;
    }

    // Получить имя игрока (имя собаки)
    std::string GetName() const noexcept {
        return dog_->GetName();
    }

private:
    GameSession* session_;
    Dog* dog_;
    Token token_;
    size_t player_id_{0}; // Будет установлен при добавлении в Players
};

// Добавляем определение Players
class Players {
public:
    // Добавить нового игрока. Возвращает ссылку на созданного игрока.
    Player& AddPlayer(Dog& dog, GameSession& session);

    // Найти игрока по токену
    Player* FindByToken(const Token& token) const noexcept;

    // Найти игрока по id собаки и id карты
    Player* FindByDogIdAndMapId(const Dog::Id& dog_id, const Map::Id& map_id) const noexcept;

    // Получить всех игроков
    const std::vector<Player>& GetPlayers() const noexcept {
        return players_;
    }

private:
    std::vector<Player> players_;
    std::unordered_map<Token, Player*, util::TaggedHasher<Token>> token_to_player_;
};

}  // namespace model

