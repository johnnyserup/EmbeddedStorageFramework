#pragma once

#include "esf/RepositoryBase.hpp"

#include <cstdint>

namespace esf {

// ---------------------------------------------------------------------------
// Example data type
// ---------------------------------------------------------------------------

/**
 * @brief Application settings persisted in storage.
 *
 * This is a minimal example struct.  In a real product you would add your
 * own fields here.  Rules:
 *  - Must be trivially copyable.
 *  - Must not contain pointers or references.
 *  - Fields are ordered largest-to-smallest to avoid compiler padding.
 *  - Update kVersion below and implement a migration step whenever the
 *    layout changes in a way that breaks backward compatibility.
 */
struct Settings {
    uint32_t deviceId          = 0u;    ///< Unique device identifier
    uint8_t  displayBrightness = 100u;  ///< 0–100 %
    uint8_t  volumeLevel       = 50u;   ///< 0–100 %
    bool     featureEnabled    = false; ///< Feature toggle
    uint8_t  _reserved         = 0u;   ///< Reserved — must be zero
};

static_assert(sizeof(Settings) == 8,
              "Settings layout changed — bump kVersion and add a migration step");

// ---------------------------------------------------------------------------
// Repository interface
// ---------------------------------------------------------------------------

/**
 * @brief Pure-virtual interface for the settings repository.
 *
 * The application layer programs against this interface so that the concrete
 * implementation (and the underlying storage driver) can be swapped freely.
 */
class ISettingsRepository : public IRepository<Settings> {
public:
    ~ISettingsRepository() override = default;
};

// ---------------------------------------------------------------------------
// Concrete implementation
// ---------------------------------------------------------------------------

/**
 * @brief Concrete settings repository backed by any IStorageDriver.
 *
 * The repository stores a single StorageObject<Settings, kVersion> slot
 * starting at byte offset kAddress within the driver.
 */
class SettingsRepository final
    : public RepositoryBase<SettingsRepository, Settings, 1u> {
public:
    /// Byte offset within the storage medium where the settings slot lives.
    static constexpr uint32_t kAddress = 0u;

    /// Factory-default settings returned when no valid slot is found.
    static constexpr Settings kDefault{};

    explicit SettingsRepository(IStorageDriver& driver) noexcept
        : RepositoryBase(driver) {}

    // -----------------------------------------------------------------------
    // Accessors required by RepositoryBase (CRTP contract)
    // -----------------------------------------------------------------------

    [[nodiscard]] static constexpr uint32_t storageAddress() noexcept {
        return kAddress;
    }

    [[nodiscard]] static constexpr Settings defaultValue() noexcept {
        return kDefault;
    }
};

} // namespace esf
