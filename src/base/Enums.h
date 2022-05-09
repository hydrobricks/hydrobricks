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
    Week,
    Day,
    Hour,
    Minute,
    Variable
};

enum VariableType {
    Precipitation,
    Temperature,
    PET,
    Custom1,
    Custom2,
    Custom3
};

#endif  // HYDROBRICKS_ENUMS_H
