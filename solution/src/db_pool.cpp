#include "db_pool.h"
#include <pqxx/pqxx>
#include <stdexcept>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <mutex>
#include <condition_variable>
// Определение класса ConnectionPool (внутренний для database.cpp)
class ConnectionPool {
public:
    class ConnectionWrapper {
    public:
        using PoolType = ConnectionPool;
        using ConnectionPtr = std::shared_ptr<pqxx::connection>;

        ConnectionWrapper(ConnectionPtr&& conn, PoolType& pool) noexcept
            : conn_{std::move(conn)}
            , pool_{&pool} {
        }

        ConnectionWrapper(const ConnectionWrapper&) = delete;
        ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;

        ConnectionWrapper(ConnectionWrapper&&) = default;
        ConnectionWrapper& operator=(ConnectionWrapper&&) = default;

        pqxx::connection& operator*() const& noexcept {
            return *conn_;
        }
        pqxx::connection& operator*() const&& = delete;

        pqxx::connection* operator->() const& noexcept {
            return conn_.get();
        }

        ~ConnectionWrapper() noexcept {
            if (conn_) {
                try {
                    pool_->ReturnConnection(std::move(conn_));
                } catch (...) {
                    // Логирование (если нужно), но НЕ пробрасываем исключение
                    // Иначе — std::terminate()
                }
        }
}

    private:
        ConnectionPtr conn_;
        PoolType* pool_;
    };

    template <typename ConnectionFactory>
    ConnectionPool(size_t capacity, ConnectionFactory&& connection_factory)
        : used_connections_(0) {
        if (capacity == 0) {
            throw std::invalid_argument("Connection pool capacity must be > 0");
        }
        pool_.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            pool_.emplace_back(connection_factory());
        }
    }

    ConnectionWrapper GetConnection() {
        std::unique_lock<std::mutex> lock{mutex_};
        cond_var_.wait(lock, [this] {
            return used_connections_ < pool_.size();
        });

        return {std::move(pool_[used_connections_++]), *this};
    }

private:
    void ReturnConnection(std::shared_ptr<pqxx::connection>&& conn) {
        {
            std::lock_guard<std::mutex> lock{mutex_};
            assert(used_connections_ > 0);
            pool_[--used_connections_] = std::move(conn);
        }
        cond_var_.notify_one();
    }

    std::mutex mutex_;
    std::condition_variable cond_var_;
    std::vector<std::shared_ptr<pqxx::connection>> pool_;
    size_t used_connections_;
};

// -----------------------------
// Реализация класса Database
// -----------------------------

Database::Database(const std::string& db_url)
    : db_url_(db_url) {
    if (db_url_.empty()) {
        throw std::runtime_error("Database URL is empty");
    }

    // Определяем размер пула как количество потоков CPU
    unsigned num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 2;

    try {
        pool_ = std::make_unique<ConnectionPool>(num_threads, [this]() {
            auto conn = std::make_shared<pqxx::connection>(db_url_);
            // Можно подготовить запросы здесь, но в нашем случае не обязательно
            return conn;
        });
    } catch (const pqxx::sql_error& e) {
        throw std::runtime_error("Failed to create connection pool: " + std::string(e.what()));
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create connection pool: " + std::string(e.what()));
    }
}

Database::~Database() = default;

void Database::EnsureSchema() {
    auto conn = pool_->GetConnection();
    pqxx::work tx{*conn};

    // Создаём таблицу, если не существует
    tx.exec(R"(
        CREATE TABLE IF NOT EXISTS retired_players (
            id SERIAL PRIMARY KEY,
            name TEXT NOT NULL,
            score INTEGER NOT NULL,
            play_time DOUBLE PRECISION NOT NULL
        );
    )");

    // Создаём составной индекс для быстрой сортировки по условиям задания:
    // score DESC, play_time ASC, name ASC
    tx.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_retired_players_sort
        ON retired_players (score DESC, play_time ASC, name ASC);
    )");

    tx.commit();
}

void Database::SaveRetiredDog(const model::Dog& dog, const model::Map& /*map*/, double play_time_seconds) {
    auto conn = pool_->GetConnection();
    pqxx::work tx{*conn};

    tx.exec_params(
        "INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3)",
        dog.GetName(),
        static_cast<int>(dog.GetScore()),
        play_time_seconds
    );

    tx.commit();
}

std::vector<Database::Record> Database::LoadRecords(size_t start, size_t max_items) {
    if (max_items > 100) {
        throw std::invalid_argument("max_items must not exceed 100");
    }

    auto conn = pool_->GetConnection();
    pqxx::read_transaction tx{*conn};

    // Запрос с LIMIT и OFFSET, отсортированный по требованиям
    auto result = tx.exec_params(
        R"(
            SELECT name, score, play_time
            FROM retired_players
            ORDER BY score DESC, play_time ASC, name ASC
            LIMIT $1 OFFSET $2
        )",
        static_cast<long>(max_items),
        static_cast<long>(start)
    );

    std::vector<Record> records;
    records.reserve(result.size());

    for (const auto& row : result) {
        records.push_back({
            std::string(row["name"].c_str()),
            row["score"].as<int>(),
            row["play_time"].as<double>()
        });
    }

    return records;
}
