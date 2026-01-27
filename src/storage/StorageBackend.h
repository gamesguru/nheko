#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <map>

#include <QString>

#include "CacheStructs.h"
#include <mtx/events.hpp>
#include <mtx/responses/sync.hpp>

// Forward declarations
namespace mtx::events::collections {
struct TimelineEvents;
}

namespace cache {

// Abstract transaction handle
class StorageTransaction {
public:
    virtual ~StorageTransaction() = default;
    virtual void commit() = 0;
};

class StorageBackend {
public:
    virtual ~StorageBackend() = default;

    virtual std::unique_ptr<StorageTransaction> createTransaction() = 0;
    virtual bool isSql() const = 0;

    // Room Info
    virtual void saveRoom(StorageTransaction& txn, const std::string& roomId, const RoomInfo& info) = 0;
    virtual std::optional<RoomInfo> getRoom(StorageTransaction& txn, const std::string& roomId) = 0;
    virtual std::vector<std::string> getRoomIds(StorageTransaction& txn) = 0;

    // Events
    virtual void saveEvent(StorageTransaction& txn, const std::string& eventId, const std::string& roomId, const std::string& eventJson) = 0;
    virtual void saveStateEvent(StorageTransaction& txn,
                                const std::string& eventId,
                                const std::string& roomId,
                                const std::string& type,
                                const std::string& stateKey,
                                const std::string& eventJson) = 0;
    virtual void saveMember(StorageTransaction& txn,
                            const std::string& roomId,
                            const std::string& userId,
                            const std::string& memberInfoJson,
                            const std::string& membership) = 0;
    virtual void saveMediaMetadata(StorageTransaction& txn,
                                   const std::string& eventId,
                                   const std::string& roomId,
                                   const std::string& filename,
                                   const std::string& mimetype,
                                   int64_t size,
                                   int width,
                                   int height,
                                   const std::string& blurhash) = 0;

    // ... (We will populate this iteratively as we migrate methods)
};

} // namespace cache
