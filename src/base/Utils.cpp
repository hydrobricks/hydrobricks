#include <netcdf.h>

#include "Utils.h"

bool IsNaN(const int value) {
    return value == NAN_I;
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

Time GetTimeStructFromMJD(double mjd, int method) {
    wxASSERT(mjd > 0);

    // To Julian day
    double jd = mjd + 2400001;  // And not 2400000.5 (not clear why...)

    Time date;

    // Remaining seconds
    double rest = jd - floor(jd);
    double sec = round(rest * 86400);
    date.hour = (int)floor((float)(sec / 3600));
    sec -= date.hour * 3600;
    date.min = (int)floor((float)(sec / 60));
    sec -= date.min * 60;
    date.sec = (int)sec;

    switch (method) {
        case (1):

            long a, b, c, d, e, z;

            z = (long)jd;
            if (z < 2299161L)
                a = z;
            else {
                auto alpha = (long)((z - 1867216.25) / 36524.25);
                a = z + 1 + alpha - alpha / 4;
            }
            b = a + 1524;
            c = (long)((b - 122.1) / 365.25);
            d = (long)(365.25 * c);
            e = (long)((b - d) / 30.6001);
            date.day = (int)b - d - (long)(30.6001 * e);
            date.month = (int)(e < 13.5) ? e - 1 : e - 13;
            date.year = (int)(date.month > 2.5) ? (c - 4716) : c - 4715;
            if (date.year <= 0) date.year -= 1;

            break;

        case (2):

            long t1, t2, yr, mo;

            t1 = (long)jd + 68569L;
            t2 = 4L * t1 / 146097L;
            t1 = t1 - (146097L * t2 + 3L) / 4L;
            yr = 4000L * (t1 + 1L) / 1461001L;
            t1 = t1 - 1461L * yr / 4L + 31L;
            mo = 80L * t1 / 2447L;
            date.day = (int)(t1 - 2447L * mo / 80L);
            t1 = mo / 11L;
            date.month = (int)(mo + 2L - 12L * t1);
            date.year = (int)(100L * (t2 - 49L) + yr + t1);

            // Correct for BC years
            if (date.year <= 0) date.year -= 1;

            break;

        default:
            wxLogError(_("Incorrect method."));
    }

    return date;
}