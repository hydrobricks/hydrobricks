#include "ContentTypes.h"

#include <unordered_map>

ContentType ContentTypeFromString(const string& typeStr) {
    static const std::unordered_map<string, ContentType> typeMap = {
        {"water", ContentType::Water},
        {"snow", ContentType::Snow},
        {"ice", ContentType::Ice}
    };

    auto it = typeMap.find(typeStr);
    if (it != typeMap.end()) {
        return it->second;
    }
    return ContentType::Unknown;
}

string ContentTypeToString(ContentType type) {
    switch (type) {
        case ContentType::Water:
            return "water";
        case ContentType::Snow:
            return "snow";
        case ContentType::Ice:
            return "ice";
        default:
            return "unknown";
    }
}
