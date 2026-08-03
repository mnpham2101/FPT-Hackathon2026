#ifndef ADA_CRA_RELAYED_TRACK_HPP
#define ADA_CRA_RELAYED_TRACK_HPP

#include <cstdint>

namespace ada::cra {

enum class TrackState {
    ACTIVE,
    COASTING,
    EXPIRED
};

struct RelayedTrack {
    uint32_t object_id{0};
    double pos_x_m{0.0};        // Relative X position (meters)
    double pos_y_m{0.0};        // Relative Y position (meters)
    double vel_x_mps{0.0};      // Velocity X (m/s)
    double vel_y_mps{0.0};      // Velocity Y (m/s)
    double heading_rad{0.0};    // Heading angle (radians)
    uint64_t timestamp_ms{0};   // Last update timestamp in milliseconds
    TrackState state{TrackState::ACTIVE};
};

} // namespace ada::cra

#endif // ADA_CRA_RELAYED_TRACK_HPP
