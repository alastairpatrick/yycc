#include "Message.h"

#include "TranslationUnitContext.h"

ostream& message(Severity severity, const Location& location) {
    auto &context = TranslationUnitContext::it;
    auto& stream = context->message_stream;
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
