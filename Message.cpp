#include "Message.h"

#include "TranslationUnitContext.h"

ostream& message(Severity severity, const Location& location) {
    auto &stream = TranslationUnitContext::it->message_stream;
    stream << *location.filename << ':' << location.line << ':' << location.column << ": ";

    switch (severity) {
    case Severity::WARNING:
        stream << "warning ";
        break;
    case Severity::ERROR:
        stream << "error ";
        break;
    }

    return stream;
}
