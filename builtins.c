#include "object.h"
#include "value.h"
#include "builtins.h"
#include <limits.h>
#include <math.h>
#include <time.h>

static value native_abs(int arg_count, value *args)
{
    return NUMBER_VAL(fabs(AS_NUMBER(args[0])));
}

static value native_clock(int arg_count, value *args)
{
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static value native_max(int arg_count, value *args)
{
    double maximum = __DBL_MIN__;
    for (value *arg = args; arg_count-- > 0; arg++) {
        double v = AS_NUMBER(*arg);
        if (v > maximum)
            maximum = v;
    }
    return NUMBER_VAL(maximum);
}

static value native_min(int arg_count, value *args)
{
    double minimum = __DBL_MAX__;
    for (value *arg = args; arg_count-- > 0; arg++) {
        double v = AS_NUMBER(*arg);
        if (v < minimum)
            minimum = v;
    }
    return NUMBER_VAL(minimum);
}

static value native_round(int argc, value *args)
{
    return NUMBER_VAL(round(AS_NUMBER(args[0])));
}

static value native_sum(int arg_count, value *args)
{
    double sum = 0;
    for (value *arg = args; arg_count-- > 0; arg++) { sum += AS_NUMBER(*arg); }
    return NUMBER_VAL(sum);
}

struct builtin_function_info builtins[] = {
    {"abs",   native_abs  },
    {"clock", native_clock},
    {"max",   native_max  },
    {"min",   native_min  },
    {"round", native_round},
    {"sum",   native_sum  },
    {NULL,    NULL        },
};
