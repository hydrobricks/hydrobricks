#ifndef HYDROBRICKS_FLUX_TYPES_H
#define HYDROBRICKS_FLUX_TYPES_H

#include "Includes.h"

/**
 * Translation from string to ContentType enum.
 *
 * @param typeStr The string representation of the content type.
 * @return The corresponding ContentType enum value.
 */
ContentType ContentTypeFromString(const string& typeStr);

/**
 * Translation from ContentType enum to string.
 *
 * @param type The ContentType enum value.
 * @return The corresponding string representation of the content type.
 */
string ContentTypeToString(ContentType type);

/**
 * Get a list of all valid content type strings.
 *
 * @return vector of valid content type strings.
 */
vector<string> GetValidContentTypes();

/**
 * Get a formatted string with suggestions for valid content types.
 *
 * @return string with list of valid content types.
 */
string GetContentTypeSuggestions();

#endif  // HYDROBRICKS_FLUX_TYPES_H
