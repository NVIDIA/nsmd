#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

namespace nsm
{

/**
 * @brief Reserved out-of-domain marker for a numeric property with no reading.
 *
 * A producer publishes Sentinel<T>::notAvailable on a numeric D-Bus property
 * when the backend has no genuine reading to report - device unreachable,
 * command failed, or not yet polled. Publishing the marker (instead of leaving
 * a stale or default value in place) lets bmcweb tell a real reading apart
 * from "no reading" and render the Redfish property as null, or omit it for a
 * non-nullable property.
 *
 * A single marker covers every non-success case: the top of the type's range,
 * which can never collide with a plausible real reading. Keep this in
 * lock-step with bmcweb's numeric classification in redfish_response_utils.hpp.
 *
 * @tparam T Numeric (integer) property type.
 */
template <typename T>
struct Sentinel
{
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>,
                  "Sentinel<T> supports non-bool integral types only; "
                  "max() is not a safe marker for other types");
    static constexpr T notAvailable = std::numeric_limits<T>::max();
};

/**
 * @brief Availability status passed as the shared-memory cache rc.
 *
 * cacheTALData() caches the real value for TELEMETRY_AVAILABLE, and caches nan
 * (rendered as Redfish null) for TELEMETRY_NOT_AVAILABLE.
 */
enum TelemetryStatus : uint8_t
{
    TELEMETRY_AVAILABLE = 0,
    TELEMETRY_NOT_AVAILABLE = 1,
};

} // namespace nsm
