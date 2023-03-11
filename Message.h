#include "Location.h"

enum class Severity {
    INFO,
    WARNING,
    ERROR,
};

ostream& message(Severity severity, const Location& location);
