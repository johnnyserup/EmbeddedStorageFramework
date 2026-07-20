#pragma once

#include "esf/IRepository.hpp"
#include "esf/IStorageDriver.hpp"
#include "esf/StorageObject.hpp"

#include <concepts>
#include <cstring>

namespace esf {

/**
 * @brief CRTP-friendly base class that implements the full load/save/reset
 *        protocol on top of an IStorageDriver.
 *
 * Derived classes only need to supply:
 *  - a StorageAddress (byte offset into the driver)
 *  - a default value returned when no valid slot is found
 *
 * Usage:
 * @code
 * class SettingsRepository
 *     : public RepositoryBase<SettingsRepository, Settings, 1>
 * {
 *     static constexpr uint32_t kAddress = 0;
 *     static constexpr Settings kDefault = {};
 *     // ... see SettingsRepository.hpp for a full example
 * };
 * @endcode
 *
 * @tparam Derived   The concrete repository class (CRTP).
 * @tparam T         Data type to persist.
 * @tparam Ver       Object version for migration.
 */
template <typename Derived, typename T, uint16_t Ver = 1>
    requires std::is_trivially_copyable_v<T>
class RepositoryBase : public IRepository<T> {
public:
    using Object = StorageObject<T, Ver>;

    explicit RepositoryBase(IStorageDriver& driver) noexcept : driver_(driver) {}

    // -----------------------------------------------------------------------
    // IRepository<T>
    // -----------------------------------------------------------------------

    StorageError load(T& out) override {
        Object obj{};

        // Read header + data in a single transaction
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        if (!driver_.read(derived().storageAddress(),
                          reinterpret_cast<uint8_t*>(&obj),
                          Object::kTotalSize)) {
            return StorageError::DriverError;
        }

        if (!obj.hasValidHeader()) {
            out = derived().defaultValue();
            return StorageError::InvalidMagic;
        }

        if (!obj.hasValidCrc()) {
            out = derived().defaultValue();
            return StorageError::CrcMismatch;
        }

        // Version migration hook: derived class may override.
        if (obj.version() != Ver) {
            return derived().migrate(obj, out);
        }

        out = obj.data;
        return StorageError::Ok;
    }

    StorageError save(const T& value) override {
        const Object obj = Object::make(value);

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        if (!driver_.write(derived().storageAddress(),
                           reinterpret_cast<const uint8_t*>(&obj),
                           Object::kTotalSize)) {
            return StorageError::DriverError;
        }

        return StorageError::Ok;
    }

    StorageError reset() override {
        return save(derived().defaultValue());
    }

protected:
    IStorageDriver& driver_;

    /**
     * @brief Default migration handler — returns VersionMismatch.
     *
     * Derived classes may override this to implement forward/backward
     * migration between object versions.
     */
    StorageError migrate(const Object& /*obj*/, T& out) {
        out = derived().defaultValue();
        return StorageError::VersionMismatch;
    }

private:
    [[nodiscard]] Derived& derived() noexcept {
        return static_cast<Derived&>(*this);
    }
    [[nodiscard]] const Derived& derived() const noexcept {
        return static_cast<const Derived&>(*this);
    }
};

} // namespace esf
