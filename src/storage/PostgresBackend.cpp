#include "PostgresBackend.h"
#include "Logging.h"

#include <libpq-fe.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace cache {

// Postgres Transaction Wrapper
class PostgresTransaction : public StorageTransaction {
public:
    PostgresTransaction(PGconn* conn) : conn_(conn) {
        auto res = PQexec(conn_, "BEGIN");
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            PQclear(res);
            throw std::runtime_error("Failed to start transaction");
        }
        PQclear(res);
    }

    ~PostgresTransaction() override { 
        if (!committed_) {
            auto res = PQexec(conn_, "ROLLBACK");
            PQclear(res);
        }
    }

    void commit() override {
        auto res = PQexec(conn_, "COMMIT");
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            PQclear(res);
            throw std::runtime_error("Failed to commit transaction");
        }
        PQclear(res);
        committed_ = true;
    }

    PGconn* get() { return conn_; }

private:
    PGconn* conn_;
    bool committed_ = false;
};

PostgresBackend::PostgresBackend(const std::string& connectionUrl) 
    : connectionUrl_(connectionUrl) {
    nhlog::db()->info("Initializing PostgresBackend with url: {}", connectionUrl);

    conn_ = PQconnectdb(connectionUrl.c_str());
    if (PQstatus(conn_) != CONNECTION_OK) {
        std::string err = PQerrorMessage(conn_);
        PQfinish(conn_);
        throw std::runtime_error("Connection to database failed: " + err);
    }

    initializeSchema();
}

PostgresBackend::~PostgresBackend() {
    if (conn_) PQfinish(conn_);
}

void PostgresBackend::initializeSchema() {
    const char* ddl[] = {
        "CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY);",
        "CREATE TABLE IF NOT EXISTS rooms (room_id TEXT PRIMARY KEY, info JSONB);",
        "CREATE TABLE IF NOT EXISTS room_members (room_id TEXT, user_id TEXT, info JSONB, PRIMARY KEY(room_id, user_id));",
        "CREATE TABLE IF NOT EXISTS events (event_id TEXT PRIMARY KEY, room_id TEXT, idx BIGINT, body JSONB);",
        "CREATE INDEX IF NOT EXISTS idx_events_room ON events(room_id, idx);"
    };

    for (const auto& query : ddl) {
        auto res = PQexec(conn_, query);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::string err = PQerrorMessage(conn_);
            PQclear(res);
            throw std::runtime_error("Schema initialization failed: " + err);
        }
        PQclear(res);
    }
}

std::unique_ptr<StorageTransaction> PostgresBackend::createTransaction() {
    nhlog::db()->debug("Postgres: Creating transaction");
    return std::make_unique<PostgresTransaction>(conn_);
}

void PostgresBackend::saveRoom(StorageTransaction& txn, const std::string& roomId, const RoomInfo& info) {
    nhlog::db()->debug("Postgres: Saving room");
    auto conn = static_cast<PostgresTransaction&>(txn).get();

    std::string json = nlohmann::json(info).dump();
    const char* paramValues[2] = { roomId.c_str(), json.c_str() };

    auto res = PQexecParams(conn,
        "INSERT INTO rooms (room_id, info) VALUES ($1, $2) ON CONFLICT (room_id) DO UPDATE SET info = EXCLUDED.info",
        2, nullptr, paramValues, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        nhlog::db()->error("Postgres: Failed to save room");
        PQclear(res);
        // TODO: handle error
    }
    PQclear(res);
}

std::optional<RoomInfo> PostgresBackend::getRoom(StorageTransaction& txn, const std::string& roomId) {
    auto conn = static_cast<PostgresTransaction&>(txn).get();

    const char* paramValues[1] = { roomId.c_str() };

    auto res = PQexecParams(conn,
        "SELECT info FROM rooms WHERE room_id = $1",
        1, nullptr, paramValues, nullptr, nullptr, 0);

    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        std::string jsonStr = PQgetvalue(res, 0, 0);
        PQclear(res);
        try {
            return nlohmann::json::parse(jsonStr).get<RoomInfo>();
        } catch (...) {
            return std::nullopt;
        }
    }

    PQclear(res);
    return std::nullopt;
}

std::vector<std::string> PostgresBackend::getRoomIds(StorageTransaction& txn) {
    auto conn = static_cast<PostgresTransaction&>(txn).get();
    std::vector<std::string> rooms;

    auto res = PQexec(conn, "SELECT room_id FROM rooms");
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; i++) {
            rooms.emplace_back(PQgetvalue(res, i, 0));
        }
    }
    PQclear(res);
    return rooms;
}

} // namespace cache

void PostgresBackend::saveEvent(StorageTransaction& txn, const std::string& eventId, const std::string& roomId, const std::string& eventJson) {
    // Stub implementation for now
    (void)txn;
    (void)eventId;
    (void)roomId;
    (void)eventJson;
}

} // namespace cache
