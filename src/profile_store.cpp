#include "picoboy/profile_store.hpp"

#include <cstddef>
#include <cstring>

#include "hardware/flash.h"
#include "hardware/sync.h"

namespace picoboy {

namespace {

constexpr uint32_t storage_magic = 0x50424F59u;
constexpr uint16_t storage_version = 2;
constexpr uint16_t legacy_storage_version = 1;
constexpr size_t legacy_max_profiles = 3;
constexpr uint32_t storage_offset = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;

struct LegacyStorageImageV1 {
    uint32_t magic = storage_magic;
    uint16_t version = legacy_storage_version;
    uint8_t next_profile_number = 1;
    uint8_t reserved = 0;
    std::array<StoredProfile, legacy_max_profiles> profiles{};
    uint32_t checksum = 0;
};

struct StorageImage {
    uint32_t magic = storage_magic;
    uint16_t version = storage_version;
    uint8_t next_profile_number = 1;
    uint8_t reserved = 0;
    std::array<StoredProfile, ProfileStore::MaxProfiles> profiles{};
    uint32_t checksum = 0;
};

static_assert(sizeof(StorageImage) <= FLASH_SECTOR_SIZE, "Profile storage image must fit in one flash sector.");

void clearProfiles(std::array<StoredProfile, ProfileStore::MaxProfiles>& profiles) {
    for (StoredProfile& profile : profiles) {
        profile = StoredProfile{};
    }
}

uint32_t checksumBytes(const uint8_t* data, size_t length) {
    uint32_t hash = 2166136261u;

    for (size_t index = 0; index < length; ++index) {
        hash ^= data[index];
        hash *= 16777619u;
    }

    return hash;
}

AvatarId sanitizeAvatar(uint8_t avatar) {
    if (avatar >= static_cast<uint8_t>(AvatarId::Count)) {
        return AvatarId::Hero;
    }

    return static_cast<AvatarId>(avatar);
}

const StorageImage* flashImage() {
    return reinterpret_cast<const StorageImage*>(XIP_BASE + storage_offset);
}

bool isValidImage(const StorageImage& image) {
    if (image.magic != storage_magic || image.version != storage_version) {
        return false;
    }

    const uint32_t expected_checksum = checksumBytes(reinterpret_cast<const uint8_t*>(&image),
                                                     offsetof(StorageImage, checksum));
    return expected_checksum == image.checksum;
}

bool isValidLegacyImage(const LegacyStorageImageV1& image) {
    if (image.magic != storage_magic || image.version != legacy_storage_version) {
        return false;
    }

    const uint32_t expected_checksum = checksumBytes(reinterpret_cast<const uint8_t*>(&image),
                                                     offsetof(LegacyStorageImageV1, checksum));
    return expected_checksum == image.checksum;
}

void sanitizeLoadedProfiles(std::array<StoredProfile, ProfileStore::MaxProfiles>& profiles) {
    for (StoredProfile& profile : profiles) {
        if (profile.in_use == 0) {
            profile = StoredProfile{};
            continue;
        }

        profile.in_use = 1;
        profile.avatar = static_cast<uint8_t>(sanitizeAvatar(profile.avatar));
        profile.name[sizeof(profile.name) - 1] = '\0';
    }
}

}  // namespace

void ProfileStore::load(std::array<StoredProfile, MaxProfiles>& profiles, uint8_t& next_profile_number) const {
    clearProfiles(profiles);
    next_profile_number = 1;

    const StorageImage& image = *flashImage();
    if (!isValidImage(image)) {
        const LegacyStorageImageV1& legacy_image = *reinterpret_cast<const LegacyStorageImageV1*>(flashImage());
        if (!isValidLegacyImage(legacy_image)) {
            return;
        }

        for (size_t index = 0; index < legacy_image.profiles.size(); ++index) {
            profiles[index] = legacy_image.profiles[index];
        }
        sanitizeLoadedProfiles(profiles);
        next_profile_number = legacy_image.next_profile_number == 0 ? 1 : legacy_image.next_profile_number;
        return;
    }

    profiles = image.profiles;
    sanitizeLoadedProfiles(profiles);
    next_profile_number = image.next_profile_number == 0 ? 1 : image.next_profile_number;
}

bool ProfileStore::save(const std::array<StoredProfile, MaxProfiles>& profiles, uint8_t next_profile_number) const {
    StorageImage image;
    image.next_profile_number = next_profile_number == 0 ? 1 : next_profile_number;
    image.profiles = profiles;

    for (StoredProfile& profile : image.profiles) {
        if (profile.in_use == 0) {
            profile = StoredProfile{};
            continue;
        }

        profile.in_use = 1;
        profile.avatar = static_cast<uint8_t>(sanitizeAvatar(profile.avatar));
        profile.name[sizeof(profile.name) - 1] = '\0';
    }

    image.checksum = checksumBytes(reinterpret_cast<const uint8_t*>(&image),
                                   offsetof(StorageImage, checksum));

    std::array<uint8_t, FLASH_SECTOR_SIZE> sector_buffer{};
    std::memset(sector_buffer.data(), 0xFF, sector_buffer.size());
    std::memcpy(sector_buffer.data(), &image, sizeof(image));

    const uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(storage_offset, FLASH_SECTOR_SIZE);
    flash_range_program(storage_offset, sector_buffer.data(), FLASH_SECTOR_SIZE);
    restore_interrupts(interrupts);
    return true;
}

}  // namespace picoboy
