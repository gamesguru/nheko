#pragma once

#include "StorageBackend.h"

// Forward declaration for sqlite3 pointer
struct sqlite3;

namespace cache {

class SQLiteBackend : public StorageBackend {
public:
    SQLiteBackend(const std::string& dbPath);
    ~SQLiteBackend() override;

    std::unique_ptr<StorageTransaction> createTransaction() override;

    void saveRoom(StorageTransaction& txn, const std::string& roomId, const RoomInfo& info) override;
    std::optional<RoomInfo> getRoom(StorageTransaction& txn, const std::string& roomId) override;
    std::vector<std::string> getRoomIds(StorageTransaction& txn) override;

private:
    void initializeSchema();

    sqlite3* db_ = nullptr;
};

} // namespace cache
