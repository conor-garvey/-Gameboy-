#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace picoboy {

enum class AvatarId : uint8_t {
    Hero = 0,
    Pac,
    Count,
};

struct ProfileStats {
    uint32_t plumber_total_coins = 0;
    uint32_t plumber_best_time_ms = 0;
    uint32_t sky_best_distance = 0;
};

struct StoredProfile {
    uint8_t in_use = 0;
    uint8_t avatar = static_cast<uint8_t>(AvatarId::Hero);
    char name[16] = {};
    ProfileStats stats{};
};

class ProfileStore {
public:
    static constexpr size_t MaxProfiles = 4;

    void load(std::array<StoredProfile, MaxProfiles>& profiles, uint8_t& next_profile_number) const;
    bool save(const std::array<StoredProfile, MaxProfiles>& profiles, uint8_t next_profile_number) const;
};

}  // namespace picoboy
