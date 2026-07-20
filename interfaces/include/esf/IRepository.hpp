#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

namespace esf {

/**
 * @brief Error codes returned by repository operations.
 */
enum class StorageError : uint8_t {
    Ok = 0,
    DriverError,    ///< Underlying driver reported a failure
    CrcMismatch,    ///< Stored CRC does not match computed CRC
    InvalidMagic,   ///< Magic number in header is wrong (slot is empty/corrupt)
    VersionMismatch,///< Object version is not handled by current migration chain
    OutOfBounds,    ///< Computed address exceeds driver capacity
};

/**
 * @brief Interface for a typed persistent repository.
 *
 * A repository owns the load/save lifecycle for a single data type T.
 * The physical storage address is encapsulated inside the concrete
 * implementation — callers use domain-level operations only.
 *
 * @tparam T  Plain data type to persist.  Must be trivially copyable.
 */
template <typename T>
class IRepository {
public:
    virtual ~IRepository() = default;

    /**
     * @brief Load the stored value into @p out.
     * @return Ok on success, or an appropriate StorageError.
     */
    virtual StorageError load(T& out) = 0;

    /**
     * @brief Persist @p value to storage.
     * @return Ok on success, or an appropriate StorageError.
     */
    virtual StorageError save(const T& value) = 0;

    /**
     * @brief Erase persisted data and restore factory defaults.
     * @return Ok on success, or an appropriate StorageError.
     */
    virtual StorageError reset() = 0;
};

} // namespace esf
