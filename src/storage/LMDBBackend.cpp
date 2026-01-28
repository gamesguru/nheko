#include "LMDBBackend.h"
#include "Cache_p.h"
#include "Logging.h"

#include <nlohmann/json.hpp>

namespace cache {

LMDBBackend::LMDBBackend(::CacheDb* db) : db_(db) {
    nhlog::db()->debug("Initializing LMDBBackend with existing CacheDb");
}

lmdb::env& LMDBBackend::getEnv() {
    return db_->env_;
}


// LMDB Transaction Wrapper
class LMDBTransaction : public StorageTransaction {
public:
    LMDBTransaction(lmdb::env& env) : txn_(lmdb::txn::begin(env)) {}
    ~LMDBTransaction() override { 
        if (!committed_) txn_.abort(); 
    }
    void commit() override {
        txn_.commit();
        committed_ = true;
    }
    
    lmdb::txn& get() { return txn_; }

private:
    lmdb::txn txn_;
    bool committed_ = false;
};

std::unique_ptr<StorageTransaction> LMDBBackend::createTransaction() {
    return std::make_unique<LMDBTransaction>(db_->env_);
}

void LMDBBackend::saveRoom(StorageTransaction& txn, const std::string& roomId, const RoomInfo& info) {
    auto& lmdbTxn = static_cast<LMDBTransaction&>(txn).get();
    db_->rooms.put(lmdbTxn, roomId, nlohmann::json(info).dump());
}

std::optional<RoomInfo> LMDBBackend::getRoom(StorageTransaction& txn, const std::string& roomId) {
    auto& lmdbTxn = static_cast<LMDBTransaction&>(txn).get();
    std::string_view data;
    if (db_->rooms.get(lmdbTxn, roomId, data)) {
        try {
            auto info = nlohmann::json::parse(data).get<RoomInfo>();
            
            // Enrich with member count
            // Note: We use MDB_CREATE to ensure it mimics Cache behavior, 
            // though strictly for reading we might handle MDB_NOTFOUND.
            // But Cache uses getMembersDb which uses MDB_CREATE.
            try {
                auto membersDb = lmdb::dbi::open(lmdbTxn, (roomId + "/members").c_str(), MDB_CREATE);
                // LMDB++ wrapper for size() might be diff? 
                // Cache.cpp uses .size(txn). calling mdb_stat.
                MDB_stat stat;
                lmdb::dbi_stat(lmdbTxn, membersDb, &stat);
                info.member_count = stat.ms_entries;
            } catch (...) {
                // If failed to open members db, keep count as is (likely 0)
            }
            
            return info;
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::vector<std::string> LMDBBackend::getRoomIds(StorageTransaction& txn) {
    auto& lmdbTxn = static_cast<LMDBTransaction&>(txn).get();
    std::vector<std::string> rooms;
    auto cursor = lmdb::cursor::open(lmdbTxn, db_->rooms);
    std::string_view key, val;
    while (cursor.get(key, val, MDB_cursor_op::MDB_NEXT)) {
        rooms.emplace_back(key);
    }
    return rooms;
}



// No-op: Delete happens in Cache.cpp directly for LMDB
void LMDBBackend::deleteRoom(StorageTransaction& txn, const std::string& roomId) {
    (void)txn;
    (void)roomId;
}

// No-op: LMDB storage for events is handled directly by Cache.cpp logic currently.
// These methods exist to satisfy the interface for SQL backends.
void LMDBBackend::saveEvent(StorageTransaction& txn, const std::string& eventId, const std::string& roomId, const std::string& eventJson) {
    (void)txn;
    (void)eventId;
    (void)roomId;
    (void)eventJson;
}

void LMDBBackend::saveStateEvent(StorageTransaction& txn,
                                 const std::string& eventId,
                                 const std::string& roomId,
                                 const std::string& type,
                                 const std::string& stateKey,
                                 const std::string& eventJson) {
    (void)txn;
    (void)eventId;
    (void)roomId;
    (void)type;
    (void)stateKey;
    (void)eventJson;
}

void LMDBBackend::saveMember(StorageTransaction& txn,
                             const std::string& roomId,
                             const std::string& userId,
                             const std::string& memberInfoJson,
                             const std::string& membership) {
    (void)txn;
    (void)roomId;
    (void)userId;
    (void)memberInfoJson;
    (void)membership;
}

void LMDBBackend::saveMediaMetadata(StorageTransaction& txn,
                                    const std::string& eventId,
                                    const std::string& roomId,
                                    const std::string& filename,
                                    const std::string& mimetype,
                                    int64_t size,
                                    int width,
                                    int height,
                                    const std::string& blurhash) {
    (void)txn;
    (void)eventId;
    (void)roomId;
    (void)filename;
    (void)mimetype;
    (void)size;
    (void)width;
    (void)height;
    (void)blurhash;
}

} // namespace cache
