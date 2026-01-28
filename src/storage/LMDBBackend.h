#pragma once

#include "StorageBackend.h"

#if __has_include(<lmdbxx/lmdb++.h>)
#include <lmdbxx/lmdb++.h>
#else
#include <lmdb++.h>
#endif

struct CacheDb;

namespace cache {

class LMDBBackend : public StorageBackend {
public:
    LMDBBackend(::CacheDb* db);
    ~LMDBBackend() override = default;

    std::unique_ptr<StorageTransaction> createTransaction() override;
    bool isSql() const override { return false; }

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

    // Expose raw environment for legacy Cache compatibility
    lmdb::env& getEnv();

private:
   ::CacheDb* db_;
};

} // namespace cache
