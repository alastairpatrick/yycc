#ifndef VISIT_TYPE_CONVERTER_H
#define VISIT_TYPE_CONVERTER_H

#include "TypeVisitor.h"

struct Module;

ConvKind check_pointer_conversion(const Type* source_base_type, const Type* dest_base_type);

VisitTypeOutput convert_to_type(const Value& value, const Type* dest_type, Module* module, LLVMBuilderRef builder, EmitOutcome outcome);

#endif
