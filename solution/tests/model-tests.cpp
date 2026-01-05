#include <catch2/catch.hpp>
#include "model.h"

TEST_CASE("GameSession generates loot on roads") {
    model::Map map;
    map.AddRoad(model::Road::HORIZONTAL, {0, 0}, 10);
    map.AddRoad(model::Road::VERTICAL, {10, 0}, 10);
    map.SetLootTypesCount(2);

    model::GameSession session{map};
    std::mt19937 gen{42};

    session.GenerateLoot(5, 2, gen);
    auto items = session.GetLootItems();
    REQUIRE(items.size() == 5);

    for (const auto& item : items) {
        bool on_horiz = (item.position.y == 0 && item.position.x >= 0 && item.position.x <= 10);
        bool on_vert = (item.position.x == 10 && item.position.y >= 0 && item.position.y <= 10);
        REQUIRE(on_horiz || on_vert);
        REQUIRE(item.type == 0 || item.type == 1);
    }
}