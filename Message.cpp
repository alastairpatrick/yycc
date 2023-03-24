#include "Message.h"

#include "Context.h"

ostream& message(Severity severity, const Location& location) {
    auto &stream = Context::it->message_stream;
    stream << location.filename << ':' << location.line << ':' << location.column << ": ";

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
