#ifndef PREPROCESSOR_PREPROCESSOR2_H
#define PREPROCESSOR_PREPROCESSOR2_H

#include "Preprocessor.h"

// TODO: figure out a better name for this.
// Preprocessor2 is passed the text of a whole translation unit already processed by Preprocessor1. It assumes that conditional and source inclusion directives
// and macros have already been evaluated and that the input is a single contiguous string, i.e. there is no need to splice in included files. This means that
// the text of a declaration can be held in a single string_view, since it cannot span multiple files. It still handles other directives not handled or only
// partially handled by Preprocessor1, such as #error, some #pragma, etc.
struct Preprocessor2: Preprocessor {
protected:
    virtual void handle_directive();
    void handle_error_directive();
    void handle_pragma_directive();
};

#endif

