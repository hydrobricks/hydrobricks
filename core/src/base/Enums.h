#ifndef HYDROBRICKS_ENUMS_H
#define HYDROBRICKS_ENUMS_H

/**
 * Enumeration for general informative flags.
 */
enum {
    FAILED = -1,
    OUT_OF_RANGE = -2,
    NOT_FOUND = -3,
    EMPTY = -4,
    NONE = -5,
    NOT_VALID = -6,
    UNDEFINED = -7
};

/**
 * Temporal units.
 */
enum TimeUnit {
    Week,
    Day,
    Hour,
    Minute,
    Variable
};

/**
 * Types of forcing variables.
 */
enum VariableType {
    Precipitation,
    Temperature,
    PET,
    Custom1,
    Custom2,
    Custom3
};

/**
 * Formats for time/date formatting or parsing.
 */
enum TimeFormat {
    ISOdate,
    ISOdateTime,
    YYYYMMDD,
    YYYY_MM_DD,
    YYYY_MM_DD_hh,
    YYYYMMDD_hhmm,
    YYYY_MM_DD_hh_mm,
    YYYY_MM_DD_hh_mm_ss,
    DD_MM_YYYY,
    DD_MM_YYYY_hh_mm,
    DD_MM_YYYY_hh_mm_ss,
    hh_mm,
    guess
};

#endif  // HYDROBRICKS_ENUMS_H
