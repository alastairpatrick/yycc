#include "Message.h"

#include "TranslationUnitContext.h"

ostream& message(Severity severity, const Location& location, bool filter) {
    auto context = TranslationUnitContext::it;

    auto& stream = (context->messages_active || !filter) ? context->message_stream : context->null_message_stream;
    stream << *location.filename << ':' << location.line << ':' << location.column << ": ";

    switch (severity) {
      case Severity::WARNING:
        stream << "warning ";
        break;
      case Severity::CONTEXTUAL_ERROR:
        stream << "contextual error ";
        break;
      case Severity::ERROR:
        stream << "error ";
        break;
    }

    context->highest_severity = max(context->highest_severity, severity);

    return stream;
}

bool pause_messages() {
    auto context = TranslationUnitContext::it;
    bool was_active = context->messages_active;
    context->messages_active = false;
    return was_active;
}

void resume_messages() {
    auto context = TranslationUnitContext::it;
    context->messages_active = true;
}

ScopedMessagePauser::ScopedMessagePauser() {
    was_active = pause_messages();
}

ScopedMessagePauser::~ScopedMessagePauser() {
    if (was_active) resume_messages();
}

