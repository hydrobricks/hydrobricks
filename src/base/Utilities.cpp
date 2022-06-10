#include <netcdf.h>

#include "Utilities.h"

bool IsNaN(const int value) {
    return value == NaNi;
}

bool IsNaN(const float value) {
    return value != value;
}

bool IsNaN(const double value) {
    return value != value;
}

int Find(const int *start, const int *end, int value, int tolerance, bool showWarning) {
    return FindT<int>(start, end, value, tolerance, showWarning);
}

int Find(const float *start, const float *end, float value, float tolerance, bool showWarning) {
    return FindT<float>(start, end, value, tolerance, showWarning);
}

int Find(const double *start, const double *end, double value, double tolerance, bool showWarning) {
    return FindT<double>(start, end, value, tolerance, showWarning);
}

template <class T>
int FindT(const T *start, const T *end, T value, T tolerance, bool showWarning) {
    wxASSERT(start);
    wxASSERT(end);

    T *first = nullptr, *mid = nullptr, *last = nullptr;
    int length;

    // Initialize first and last variables.
    first = (T *)start;
    last = (T *)end;

    // Check array order
    if (*last > *first) {
        // Binary search
        while (first <= last) {
            length = (int)(last - first);
            mid = first + length / 2;
            if (value - tolerance > *mid) {
                first = mid + 1;
            } else if (value + tolerance < *mid) {
                last = mid - 1;
            } else {
                // Return found index
                return int(mid - start);
            }
        }

        // Check the pointers
        if (last - start < 0) {
            last = (T *)start;
        } else if (last - end > 0) {
            last = (T *)end - 1;
        } else if (last - end == 0) {
            last -= 1;
        }

        // If the value was not found, return closest value inside tolerance
        if (std::abs(value - *last) <= std::abs(value - *(last + 1))) {
            if (std::abs(value - *last) <= tolerance) {
                return int(last - start);
            } else {
                // Check that the value is within the array. Do it here to allow a margin for the tolerance
                if (value > *end || value < *start) {
                    if (showWarning) {
                        wxLogWarning(_("The value (%f) is out of the array range."), float(value));
                    }
                    return OUT_OF_RANGE;
                }
                if (showWarning) {
                    wxLogWarning(_("The value was not found in the array."));
                }
                return NOT_FOUND;
            }
        } else {
            if (std::abs(value - *(last + 1)) <= tolerance) {
                return int(last - start + 1);
            } else {
                // Check that the value is within the array. Do it here to allow a margin for the tolerance
                if (value > *end || value < *start) {
                    if (showWarning) {
                        wxLogWarning(_("The value (%f) is out of the array range."), float(value));
                    }
                    return OUT_OF_RANGE;
                }
                if (showWarning) {
                    wxLogWarning(_("The value was not found in the array."));
                }
                return NOT_FOUND;
            }
        }
    } else if (*last < *first) {
        // Binary search
        while (first <= last) {
            length = int(last - first);
            mid = first + length / 2;
            if (value - tolerance > *mid) {
                last = mid - 1;
            } else if (value + tolerance < *mid) {
                first = mid + 1;
            } else {
                // Return found index
                return int(mid - start);
            }
        }

        // Check the pointers
        if (first - start < 0) {
            first = (T *)start + 1;
        } else if (first - end > 0) {
            first = (T *)end;
        } else if (first - start == 0) {
            first += 1;
        }

        // If the value was not found, return closest value inside tolerance
        if (std::abs(value - *first) <= std::abs(value - *(first - 1))) {
            if (std::abs(value - *first) <= tolerance) {
                return int(first - start);
            } else {
                // Check that the value is within the array. Do it here to allow a margin for the tolerance.
                if (value < *end || value > *start) {
                    if (showWarning) {
                        wxLogWarning(_("The value (%f) is out of the array range."), float(value));
                    }
                    return OUT_OF_RANGE;
                }
                if (showWarning) {
                    wxLogWarning(_("The value was not found in the array."));
                }
                return NOT_FOUND;
            }
        } else {
            if (std::abs(value - *(first - 1)) <= tolerance) {
                return int(first - start - 1);
            } else {
                // Check that the value is within the array. Do it here to allow a margin for the tolerance.
                if (value < *end || value > *start) {
                    if (showWarning) {
                        wxLogWarning(_("The value (%f) is out of the array range."), float(value));
                    }
                    return OUT_OF_RANGE;
                }
                if (showWarning) {
                    wxLogWarning(_("The value was not found in the array."));
                }
                return NOT_FOUND;
            }
        }
    } else {
        if (last - first == 0) {
            if (*first >= value - tolerance && *first <= value + tolerance) {
                return 0;  // Value corresponds
            } else {
                return OUT_OF_RANGE;
            }
        }

        if (*first >= value - tolerance && *first <= value + tolerance) {
            return 0;  // Value corresponds
        } else {
            return OUT_OF_RANGE;
        }
    }
}

wxDateTime IncrementDateBy(const wxDateTime &date, int amount, TimeUnit unit) {
    wxDateTime newDate;
    switch (unit) {
        case Week:
            newDate = date.Add(wxDateSpan(0, 0, amount));
            break;
        case Day:
            newDate = date.Add(wxDateSpan(0, 0, 0, amount));
            break;
        case Hour:
            newDate = date.Add(wxTimeSpan(amount));
            break;
        case Minute:
            newDate = date.Add(wxTimeSpan(0, amount));
            break;
        default:
            wxLogError(_("The provided time step unit is not allowed."));
    }

    return newDate;
}

void CheckNcStatus(int status) {
    if (status != NC_NOERR) {
        throw InvalidArgument(nc_strerror(status));
    }
}