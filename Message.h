#ifndef MESSAGE_H
#define MESSAGE_H

#include "lex/Location.h"

enum class Severity {
    INFO,
    WARNING,
    CONTEXTUAL_ERROR,
    ERROR,
};

ostream& message(Severity severity, const Location& location, bool filter = true);
void pause_messages();
void resume_messages();

#endif
