#include "BrickTypes.h"

#include <unordered_map>

BrickType BrickTypeFromString(const string& typeStr) {
    static const std::unordered_map<string, BrickType> typeMap = {
        {"storage", BrickType::Storage},
        {"generic_land_cover", BrickType::GenericLandCover},
        {"ground", BrickType::GenericLandCover},
        {"generic", BrickType::GenericLandCover},
        {"glacier", BrickType::Glacier},
        {"urban", BrickType::Urban},
        {"vegetation", BrickType::Vegetation},
        {"snowpack", BrickType::Snowpack}
    };

    auto it = typeMap.find(typeStr);
    if (it != typeMap.end()) {
        return it->second;
    }
    return BrickType::Unknown;
}
