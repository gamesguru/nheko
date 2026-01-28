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
    bool isSql() const override { return true; }

    void saveRoom(StorageTransaction& txn, const std::string& roomId, const RoomInfo& info) override;
    std::optional<RoomInfo> getRoom(StorageTransaction& txn, const std::string& roomId) override;
    std::vector<std::string> getRoomIds(StorageTransaction& txn) override;
    void deleteRoom(StorageTransaction& txn, const std::string& roomId) override;
    
    void saveEvent(StorageTransaction& txn, const std::string& eventId, const std::string& roomId, const std::string& eventJson) override;
    void saveStateEvent(StorageTransaction& txn,
                        const std::string& eventId,
                        const std::string& roomId,
                        const std::string& type,
                        const std::string& stateKey,
                        const std::string& eventJson) override;
    void saveMember(StorageTransaction& txn,
                    const std::string& roomId,
                    const std::string& userId,
                    const std::string& memberInfoJson,
                    const std::string& membership) override;
    void saveMediaMetadata(StorageTransaction& txn,
                           const std::string& eventId,
                           const std::string& roomId,
                           const std::string& filename,
                           const std::string& mimetype,
                           int64_t size,
                           int width,
                           int height,
                           const std::string& blurhash) override;

private:
    std::string connectionUrl_;
    PGconn* conn_ = nullptr;
};

} // namespace cache
