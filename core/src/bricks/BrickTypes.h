#ifndef HYDROBRICKS_BRICK_TYPES_H
#define HYDROBRICKS_BRICK_TYPES_H

#include "Includes.h"

/**
 * Convert a brick type string (from settings) into a strongly-typed BrickType.
 * Recognizes synonyms (e.g., "ground" maps to GenericLandCover).
 * Returns BrickType::Unknown if not recognized.
 *
 * @param typeStr string representing the brick type.
 * @return corresponding BrickType enum value.
 */
BrickType BrickTypeFromString(const string& typeStr);

/**
 * Get a list of all valid brick type strings (including synonyms).
 *
 * @return vector of valid brick type strings.
 */
vector<string> GetValidBrickTypes();

/**
 * Get a formatted string with suggestions for valid brick types.
 *
 * @return string with list of valid brick types.
 */
string GetBrickTypeSuggestions();

#endif  // HYDROBRICKS_BRICK_TYPES_H
