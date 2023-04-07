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
      case Severity::ERROR:
        stream << "error ";
        break;
    }

    context->highest_severity = max(context->highest_severity, severity);

    return stream;
}

void pause_messages() {
    auto context = TranslationUnitContext::it;
    context->messages_active = false;
}

void resume_messages() {
    auto context = TranslationUnitContext::it;
    context->messages_active = true;
}
