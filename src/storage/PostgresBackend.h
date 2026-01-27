#pragma once

#include "StorageBackend.h"
#include <string>

// We'll use libpq or pqxx forward declarations here
typedef struct pg_conn PGconn;

namespace cache {

class PostgresBackend : public StorageBackend {
public:
    PostgresBackend(const std::string& connectionUrl);
    ~PostgresBackend() override;

    void initializeSchema();

    std::unique_ptr<StorageTransaction> createTransaction() override;

    void saveRoom(StorageTransaction& txn, const std::string& roomId, const RoomInfo& info) override;
    std::optional<RoomInfo> getRoom(StorageTransaction& txn, const std::string& roomId) override;
    std::vector<std::string> getRoomIds(StorageTransaction& txn) override;
    
    void saveEvent(StorageTransaction& txn, const std::string& eventId, const std::string& roomId, const std::string& eventJson) override;
    void saveStateEvent(StorageTransaction& txn,
                        const std::string& eventId,
                        const std::string& roomId,
                        const std::string& type,
                        const std::string& stateKey,
                        const std::string& eventJson) override;

private:
    std::string connectionUrl_;
    PGconn* conn_ = nullptr;
};

} // namespace cache
