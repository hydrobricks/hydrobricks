#include "BrickTypes.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

BrickType BrickTypeFromString(const string& typeStr) {
    static const std::unordered_map<string, BrickType> typeMap = {
        {"storage", BrickType::Storage},         {"generic_land_cover", BrickType::GenericLandCover},
        {"ground", BrickType::GenericLandCover}, {"generic", BrickType::GenericLandCover},
        {"glacier", BrickType::Glacier},         {"urban", BrickType::Urban},
        {"vegetation", BrickType::Vegetation},   {"snowpack", BrickType::Snowpack}};

    auto it = typeMap.find(typeStr);
    if (it != typeMap.end()) {
        return it->second;
    }
    return BrickType::Unknown;
}

vector<string> GetValidBrickTypes() {
    static const vector<string> validTypes = {"storage",    "generic_land_cover",
                                              "ground",     "generic",  // Synonyms for GenericLandCover
                                              "glacier",    "urban",
                                              "vegetation", "snowpack"};
    return validTypes;
}

string GetBrickTypeSuggestions() {
    auto types = GetValidBrickTypes();
    string suggestions = "Valid brick types: ";
    for (size_t i = 0; i < types.size(); ++i) {
        suggestions += types[i];
        if (i < types.size() - 1) {
            suggestions += ", ";
        }
    }
    return suggestions;
}
