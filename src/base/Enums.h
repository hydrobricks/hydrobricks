#ifndef HYDROBRICKS_ENUMS_H
#define HYDROBRICKS_ENUMS_H

enum {
    FAILED = -1,
    OUT_OF_RANGE = -2,
    NOT_FOUND = -3,
    EMPTY = -4,
    NONE = -5,
    NOT_VALID = -6,
    UNDEFINED = -7
};

enum TimeUnit {
    Year,
    Month,
    Week,
    Day,
    Hour,
    Minute,
    Second,
    Variable
};

enum VariableType {
    Precipitation,
    Temperature,
    ETP,
    MAX_VAR_TYPES
};

#endif  // HYDROBRICKS_ENUMS_H
