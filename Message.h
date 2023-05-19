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
bool pause_messages();
void resume_messages();

struct ScopedMessagePauser {
    bool was_active;
    ScopedMessagePauser();
    ~ScopedMessagePauser();
};

#endif
