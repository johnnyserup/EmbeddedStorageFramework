#pragma once

#include "esf/RepositoryBase.hpp"

#include <cstdint>

namespace esf::examples {

// ---------------------------------------------------------------------------
// Example data type
// ---------------------------------------------------------------------------

/**
 * @brief Example settings struct demonstrating a persisted data model.
 *
 * This is a framework example only.  In your application replace this with
 * your own domain-specific struct.  Rules:
 *  - Must be trivially copyable.
 *  - Must not contain pointers or references.
 *  - Fields are ordered largest-to-smallest to avoid compiler padding.
 *  - Update kVersion in ExampleSettingsRepository and implement a migration
 *    step whenever the layout changes in a way that breaks backward
 *    compatibility.
 */
struct ExampleSettings {
    uint32_t deviceId          = 0u;    ///< Unique device identifier
    uint8_t  displayBrightness = 100u;  ///< 0–100 %
    uint8_t  volumeLevel       = 50u;   ///< 0–100 %
    bool     featureEnabled    = false; ///< Feature toggle
    uint8_t  _reserved         = 0u;   ///< Reserved — must be zero
};

static_assert(sizeof(ExampleSettings) == 8,
              "ExampleSettings layout changed — bump kVersion and add a migration step");

// ---------------------------------------------------------------------------
// Repository interface
// ---------------------------------------------------------------------------

/**
 * @brief Example pure-virtual interface for an example settings repository.
 *
 * Application code programs against this interface so that the concrete
 * implementation (and the underlying storage driver) can be swapped freely.
 */
class IExampleSettingsRepository : public esf::IRepository<ExampleSettings> {
public:
    ~IExampleSettingsRepository() override = default;
};

// ---------------------------------------------------------------------------
// Concrete implementation
// ---------------------------------------------------------------------------

/**
 * @brief Example concrete settings repository backed by any IStorageDriver.
 *
 * Demonstrates how to derive from RepositoryBase<Derived, T, Ver>.
 * The repository stores a single StorageObject<ExampleSettings, kVersion>
 * slot starting at byte offset kAddress within the driver.
 *
 * In your application, replace ExampleSettings with your own data type and
 * give the repository a domain-appropriate name.
 */
class ExampleSettingsRepository final
    : public esf::RepositoryBase<ExampleSettingsRepository, ExampleSettings, 1u> {
public:
    /// Byte offset within the storage medium where the settings slot lives.
    static constexpr uint32_t kAddress = 0u;

    /// Factory-default settings returned when no valid slot is found.
    static constexpr ExampleSettings kDefault{};

    explicit ExampleSettingsRepository(esf::IStorageDriver& driver) noexcept
        : RepositoryBase(driver) {}

    // -----------------------------------------------------------------------
    // Accessors required by RepositoryBase (CRTP contract)
    // -----------------------------------------------------------------------

    [[nodiscard]] static constexpr uint32_t storageAddress() noexcept {
        return kAddress;
    }

    [[nodiscard]] static constexpr ExampleSettings defaultValue() noexcept {
        return kDefault;
    }
};

} // namespace esf::examples
