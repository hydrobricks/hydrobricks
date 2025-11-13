#include "ContentTypes.h"

ContentType ContentTypeFromString(const string& typeStr) {
    if (typeStr == "water") return ContentType::Water;
    if (typeStr == "snow") return ContentType::Snow;
    if (typeStr == "ice") return ContentType::Ice;
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
