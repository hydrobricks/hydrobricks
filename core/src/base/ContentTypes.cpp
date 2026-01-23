#include "ContentTypes.h"

#include <unordered_map>
#include <vector>

ContentType ContentTypeFromString(const string& typeStr) {
    static const std::unordered_map<string, ContentType> typeMap = {
        {"water", ContentType::Water}, {"snow", ContentType::Snow}, {"ice", ContentType::Ice}};

    auto it = typeMap.find(typeStr);
    if (it != typeMap.end()) {
        return it->second;
    }
    return ContentType::Unknown;
}

vector<string> GetValidContentTypes() {
    static const vector<string> validTypes = {"water", "snow", "ice"};
    return validTypes;
}

string GetContentTypeSuggestions() {
    auto types = GetValidContentTypes();
    string suggestions = "Valid content types: ";
    for (size_t i = 0; i < types.size(); ++i) {
        suggestions += types[i];
        if (i < types.size() - 1) {
            suggestions += ", ";
        }
    }
    return suggestions;
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
