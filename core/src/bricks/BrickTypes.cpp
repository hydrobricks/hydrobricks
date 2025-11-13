#include "BrickTypes.h"

BrickType BrickTypeFromString(const string& typeStr) {
    if (typeStr == "storage") return BrickType::Storage;
    if (typeStr == "generic_land_cover" || typeStr == "ground" || typeStr == "generic")
        return BrickType::GenericLandCover;
    if (typeStr == "glacier") return BrickType::Glacier;
    if (typeStr == "urban") return BrickType::Urban;
    if (typeStr == "vegetation") return BrickType::Vegetation;
    if (typeStr == "snowpack") return BrickType::Snowpack;
    return BrickType::Unknown;
}
