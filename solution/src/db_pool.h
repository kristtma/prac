#pragma once
#include <memory>
#include <string>
#include <chrono>
#include "model.h"

class ConnectionPool;

class Database {
public:
    explicit Database(const std::string& db_url);
    ~Database();

    void EnsureSchema();
    void SaveRetiredDog(const model::Dog& dog, const model::Map& map, double play_time_seconds);
    // Для /api/v1/game/records
    struct Record {
        std::string name;
        int score;
        double play_time_seconds; // double для дробных секунд
    };
    std::vector<Record> LoadRecords(size_t start, size_t max_items);

private:
    std::unique_ptr<ConnectionPool> pool_;
    std::string db_url_;
};