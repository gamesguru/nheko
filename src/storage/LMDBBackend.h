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

    void saveRoom(StorageTransaction& txn, const std::string& roomId, const RoomInfo& info) override;
    std::optional<RoomInfo> getRoom(StorageTransaction& txn, const std::string& roomId) override;
    std::vector<std::string> getRoomIds(StorageTransaction& txn) override;

    // Expose raw environment for legacy Cache compatibility
    lmdb::env& getEnv();

private:
   ::CacheDb* db_;
};

} // namespace cache
