#include "SQLiteBackend.h"
#include "Logging.h"

#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <atomic>

namespace cache {

// SQLite Transaction Wrapper
class SQLiteTransaction : public StorageTransaction {
public:
    SQLiteTransaction(sqlite3* db) : db_(db) {
        // Generate a unique savepoint name
        static std::atomic<uint64_t> savepointId{0};
        savepointName_ = "SP_" + std::to_string(savepointId++);
        
        char* errMsg = nullptr;
        std::string sql = "SAVEPOINT " + savepointName_;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string err = errMsg ? errMsg : "Unknown error";
            sqlite3_free(errMsg);
            nhlog::db()->error("SQLite: Failed to start transaction (savepoint {}): {}", savepointName_, err);
            throw std::runtime_error("Failed to start transaction: " + err);
        }
        // nhlog::db()->debug("SQLite: Started transaction (savepoint {})", savepointName_);
    }
    ~SQLiteTransaction() override {
        if (!committed_) {
            char* errMsg = nullptr;
            std::string sql = "ROLLBACK TO " + savepointName_;
            sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg); // Rollback to savepoint
            sqlite3_free(errMsg);
            
            // Release the savepoint
            sql = "RELEASE " + savepointName_;
            sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);
            // nhlog::db()->debug("SQLite: Rolled back transaction (savepoint {})", savepointName_);
        }
    }
    
    void commit() override {
        if (committed_) return;
        
        char* errMsg = nullptr;
        // RELEASE commits the work for this savepoint (merging it into the parent transaction)
        std::string sql = "RELEASE " + savepointName_;
        // nhlog::db()->debug("SQLite: Committing transaction (releasing savepoint {})", savepointName_);
        
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string err = errMsg ? errMsg : "Unknown error";
            sqlite3_free(errMsg);
            nhlog::db()->error("SQLite: Failed to commit transaction (release savepoint {}): {}", savepointName_, err);
            throw std::runtime_error("Failed to commit transaction: " + err);
        }
        // nhlog::db()->debug("SQLite: Transaction committed (released savepoint {})", savepointName_);
        committed_ = true;
    }
    
    sqlite3* get() { return db_; }

private:
    sqlite3* db_;
    std::string savepointName_;
    bool committed_ = false;
};

SQLiteBackend::SQLiteBackend(const std::string& dbPath) {
    nhlog::db()->info("Initializing SQLiteBackend with path: {}", dbPath);
    
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        throw std::runtime_error("Failed to open SQLite database: " + err);
    }

    // Performance tuning
    const char* performance_ddl[] = {
        "PRAGMA journal_mode=WAL;",
        "PRAGMA synchronous=NORMAL;",
        "PRAGMA cache_size=-64000;", // 64MB cache
        "PRAGMA temp_store=MEMORY;",
        "PRAGMA mmap_size=1073741824;", // Use mmap with a conservative 1GB limit
    };

    for (const auto& pragma : performance_ddl) {
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, pragma, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            nhlog::db()->warn("SQLite: Failed to set {}: {}", pragma, errMsg ? errMsg : "unknown error");
            sqlite3_free(errMsg);
        }
    }

    // waiting for up to 15s to get the lock
    sqlite3_busy_timeout(db_, 15000);

    initializeSchema();
}

SQLiteBackend::~SQLiteBackend() {
    if (db_) sqlite3_close(db_);
}

void SQLiteBackend::initializeSchema() {
    const char* ddl[] = {
        "PRAGMA user_version = 1;",
        "CREATE TABLE IF NOT EXISTS rooms (room_id TEXT PRIMARY KEY, info TEXT);",
        "CREATE TABLE IF NOT EXISTS room_members (room_id TEXT, user_id TEXT, info TEXT, membership TEXT, PRIMARY KEY(room_id, user_id));",
        "CREATE TABLE IF NOT EXISTS events (event_id TEXT PRIMARY KEY, room_id TEXT, idx INTEGER, body TEXT);",
        "CREATE INDEX IF NOT EXISTS idx_events_room ON events(room_id, idx);",
        "CREATE TABLE IF NOT EXISTS state_events (room_id TEXT, type TEXT, state_key TEXT, event_id TEXT, PRIMARY KEY(room_id, type, state_key));",
        "CREATE TABLE IF NOT EXISTS media_metadata (event_id TEXT PRIMARY KEY, room_id TEXT, filename TEXT, mimetype TEXT, size INTEGER, width INTEGER, height INTEGER, blurhash TEXT);",
        "CREATE VIRTUAL TABLE IF NOT EXISTS media_search USING fts5(filename, tokenize='trigram');"
    };
    
    char* errMsg = nullptr;
    for (const auto& query : ddl) {
        if (sqlite3_exec(db_, query, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string sql_err = errMsg ? errMsg : "Unknown error";
            sqlite3_free(errMsg);
            
            if (sql_err.find("trigram") != std::string::npos) {
                nhlog::db()->warn("trigram tokenizer not supported, falling back to simple");
                sqlite3_exec(db_, "CREATE VIRTUAL TABLE IF NOT EXISTS media_search USING fts5(filename);", nullptr, nullptr, nullptr);
            } else {
                throw std::runtime_error("Schema initialization failed: " + sql_err);
            }
        }
    }
}

void SQLiteBackend::saveMediaMetadata(StorageTransaction& txn,
                                      const std::string& eventId,
                                      const std::string& roomId,
                                      const std::string& filename,
                                      const std::string& mimetype,
                                      int64_t size,
                                      int width,
                                      int height,
                                      const std::string& blurhash) {
    auto db = static_cast<SQLiteTransaction&>(txn).get();
    sqlite3_stmt* stmt;
    
    const char* sql = "INSERT OR REPLACE INTO media_metadata (event_id, room_id, filename, mimetype, size, width, height, blurhash) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare saveMediaMetadata statement: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_bind_text(stmt, 1, eventId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, roomId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, filename.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, mimetype.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, size);
    sqlite3_bind_int(stmt, 6, width);
    sqlite3_bind_int(stmt, 7, height);
    sqlite3_bind_text(stmt, 8, blurhash.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string err = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to save media metadata: " + err);
    }
    sqlite3_finalize(stmt);
    
    const char* fts_sql = "INSERT OR REPLACE INTO media_search(rowid, filename) VALUES ((SELECT rowid FROM media_metadata WHERE event_id = ?), ?)";
    if (sqlite3_prepare_v2(db, fts_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare media_search statement: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_bind_text(stmt, 1, eventId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, filename.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string err = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to update media search index: " + err);
    }
    sqlite3_finalize(stmt);
}

std::unique_ptr<StorageTransaction> SQLiteBackend::createTransaction() {
    nhlog::db()->debug("SQLite: Creating transaction");
    return std::make_unique<SQLiteTransaction>(db_);
}

void SQLiteBackend::saveRoom(StorageTransaction& txn, const std::string& roomId, const RoomInfo& info) {
    auto db = static_cast<SQLiteTransaction&>(txn).get();
    
    std::string json = nlohmann::json(info).dump();
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO rooms (room_id, info) VALUES (?, ?) ON CONFLICT(room_id) DO UPDATE SET info=excluded.info";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare saveRoom statement: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, json.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string err = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to save room: " + err);
    }
    
    sqlite3_finalize(stmt);
}

void SQLiteBackend::deleteRoom(StorageTransaction& txn, const std::string& roomId) {
    auto db = static_cast<SQLiteTransaction&>(txn).get();
    
    // List of tables to clean up linked by room_id
    const char* queries[] = {
        "DELETE FROM rooms WHERE room_id = ?",
        "DELETE FROM room_members WHERE room_id = ?",
        "DELETE FROM events WHERE room_id = ?",
        "DELETE FROM state_events WHERE room_id = ?",
        "DELETE FROM media_search WHERE room_id = ?",
        "DELETE FROM media_metadata WHERE room_id = ?" 
    };

    for (const auto& sql : queries) {
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
             nhlog::db()->warn("Failed to prepare delete statement for room {}: {}", roomId, sqlite3_errmsg(db));
             continue;
        }
        
        sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
             nhlog::db()->warn("Failed to execute delete statement for room {}: {}", roomId, sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
}

std::optional<RoomInfo> SQLiteBackend::getRoom(StorageTransaction& txn, const std::string& roomId) {
    auto db = static_cast<SQLiteTransaction&>(txn).get();
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT info FROM rooms WHERE room_id = ?";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare getRoom statement: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_STATIC);
    
    std::optional<RoomInfo> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (text) {
            std::string jsonStr = text;
            try {
                result = nlohmann::json::parse(jsonStr).get<RoomInfo>();
            } catch (...) {}
        }
    }
    
    sqlite3_finalize(stmt);
    return result;
}

std::vector<std::string> SQLiteBackend::getRoomIds(StorageTransaction& txn) {
    auto db = static_cast<SQLiteTransaction&>(txn).get();
    std::vector<std::string> rooms;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT room_id FROM rooms";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare getRoomIds statement: " + std::string(sqlite3_errmsg(db)));
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        rooms.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    
    return rooms;
}

// ... (existing helper methods)

void SQLiteBackend::saveEvent(StorageTransaction& txn, const std::string& eventId, const std::string& roomId, const std::string& eventJson) {
    auto db = static_cast<SQLiteTransaction&>(txn).get();
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO events (event_id, room_id, body) VALUES (?, ?, ?) ON CONFLICT(event_id) DO UPDATE SET body=excluded.body";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare saveEvent statement: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_bind_text(stmt, 1, eventId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, roomId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, eventJson.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string err = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to save event: " + err);
    }
    
    sqlite3_finalize(stmt);
}

void SQLiteBackend::saveStateEvent(StorageTransaction& txn,
                                   const std::string& eventId,
                                   const std::string& roomId,
                                   const std::string& type,
                                   const std::string& stateKey,
                                   const std::string& eventJson) {
    // Mirror content to generic events table
    saveEvent(txn, eventId, roomId, eventJson);

    auto db = static_cast<SQLiteTransaction&>(txn).get();
    sqlite3_stmt* stmt;
    
    const char* sql = "INSERT INTO state_events (room_id, type, state_key, event_id) VALUES (?, ?, ?, ?) ON CONFLICT(room_id, type, state_key) DO UPDATE SET event_id=excluded.event_id";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare saveStateEvent statement: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, stateKey.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, eventId.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string err = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to save state event: " + err);
    }
    
    sqlite3_finalize(stmt);
}

void SQLiteBackend::saveMember(StorageTransaction& txn,
                               const std::string& roomId,
                               const std::string& userId,
                               const std::string& memberInfoJson,
                               const std::string& membership) {
    auto db = static_cast<SQLiteTransaction&>(txn).get();
    sqlite3_stmt* stmt;
    
    const char* sql = "INSERT INTO room_members (room_id, user_id, info, membership) VALUES (?, ?, ?, ?) ON CONFLICT(room_id, user_id) DO UPDATE SET info=excluded.info, membership=excluded.membership";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare saveMember statement: " + std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, userId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, memberInfoJson.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, membership.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string err = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to save member: " + err);
    }
    
    sqlite3_finalize(stmt);
}

} // namespace cache
