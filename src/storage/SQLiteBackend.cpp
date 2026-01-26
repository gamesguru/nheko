#include "SQLiteBackend.h"
#include "Logging.h"

#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace cache {

// SQLite Transaction Wrapper
class SQLiteTransaction : public StorageTransaction {
public:
    SQLiteTransaction(sqlite3* db) : db_(db) {
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, "BEGIN", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string err = errMsg ? errMsg : "Unknown error";
            sqlite3_free(errMsg);
            throw std::runtime_error("Failed to start transaction: " + err);
        }
    }
    
    ~SQLiteTransaction() override { 
        if (!committed_) {
            char* errMsg = nullptr;
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, &errMsg);
            sqlite3_free(errMsg);
        }
    }
    
    void commit() override {
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string err = errMsg ? errMsg : "Unknown error";
            sqlite3_free(errMsg);
            throw std::runtime_error("Failed to commit transaction: " + err);
        }
        committed_ = true;
    }
    
    sqlite3* get() { return db_; }

private:
    sqlite3* db_;
    bool committed_ = false;
};

SQLiteBackend::SQLiteBackend(const std::string& dbPath) {
    nhlog::db()->info("Initializing SQLiteBackend with path: {}", dbPath);
    
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        throw std::runtime_error("Failed to open SQLite database: " + err);
    }
    
    initializeSchema();
}

SQLiteBackend::~SQLiteBackend() {
    if (db_) sqlite3_close(db_);
}

void SQLiteBackend::initializeSchema() {
    const char* ddl[] = {
        "CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY);",
        "CREATE TABLE IF NOT EXISTS rooms (room_id TEXT PRIMARY KEY, info TEXT);",
        "CREATE TABLE IF NOT EXISTS room_members (room_id TEXT, user_id TEXT, info TEXT, PRIMARY KEY(room_id, user_id));",
        "CREATE TABLE IF NOT EXISTS events (event_id TEXT PRIMARY KEY, room_id TEXT, idx INTEGER, body TEXT);",
        "CREATE INDEX IF NOT EXISTS idx_events_room ON events(room_id, idx);"
    };
    
    char* errMsg = nullptr;
    for (const auto& query : ddl) {
        if (sqlite3_exec(db_, query, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string err = errMsg ? errMsg : "Unknown error";
            sqlite3_free(errMsg);
            throw std::runtime_error("Schema initialization failed: " + err);
        }
    }
}

std::unique_ptr<StorageTransaction> SQLiteBackend::createTransaction() {
    return std::make_unique<SQLiteTransaction>(db_);
}

void SQLiteBackend::saveRoom(StorageTransaction& txn, const std::string& roomId, const RoomInfo& info) {
    auto db = static_cast<SQLiteTransaction&>(txn).get();
    
    std::string json = nlohmann::json(info).dump();
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO rooms (room_id, info) VALUES (?, ?) ON CONFLICT(room_id) DO UPDATE SET info=excluded.info";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
         // Log error
         return;
    }
    
    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, json.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        // Log error
    }
    
    sqlite3_finalize(stmt);
}

std::optional<RoomInfo> SQLiteBackend::getRoom(StorageTransaction& txn, const std::string& roomId) {
    auto db = static_cast<SQLiteTransaction&>(txn).get();
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT info FROM rooms WHERE room_id = ?";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
         return std::nullopt;
    }
    
    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_STATIC);
    
    std::optional<RoomInfo> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string jsonStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        try {
            result = nlohmann::json::parse(jsonStr).get<RoomInfo>();
        } catch (...) {}
    }
    
    sqlite3_finalize(stmt);
    return result;
}

std::vector<std::string> SQLiteBackend::getRoomIds(StorageTransaction& txn) {
    auto db = static_cast<SQLiteTransaction&>(txn).get();
    std::vector<std::string> rooms;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT room_id FROM rooms";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            rooms.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        }
        sqlite3_finalize(stmt);
    }
    
    return rooms;
}

} // namespace cache
