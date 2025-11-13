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

#endif  // HYDROBRICKS_BRICK_TYPES_H
