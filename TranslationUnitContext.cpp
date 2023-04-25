#include "TranslationUnitContext.h"

#include "LLVM.h"
#include "Message.h"
#include "parse/ASTNode.h"
#include "parse/Type.h"

thread_local TranslationUnitContext* TranslationUnitContext::it;

TranslationUnitContext::TranslationUnitContext(ostream& message_stream): message_stream(message_stream) {
    assert(!it);
    it = this;

    llvm_context = LLVMContextCreate();

    // Even though TargetData is not explictly linked to the Context, it seems to be linked somehow and it
    // is necessary to create a unqiue TargetData for each Context. The problem the happens when reusing the
    // same TargetData with multiple Contexts is LLVMStoreSizeOfType starts reporting size of Int32 to be
    // 8 bytes instead of 4 bytes.
    llvm_target_data = LLVMCreateTargetDataLayout(g_llvm_target_machine);

    zero_size = LLVMConstInt(IntegerType::of_size(IntegerSignedness::UNSIGNED)->llvm_type(), 0, false);

    null_message_stream.setstate(ios_base::badbit);

    interned_views.insert(*empty_interned_string);
}

TranslationUnitContext::~TranslationUnitContext() {
    while (ast_nodes) {
        auto node = ast_nodes;
        ast_nodes = ast_nodes->next_delete;
        delete node;
    }

    LLVMDisposeTargetData(llvm_target_data);
    LLVMContextDispose(llvm_context);

    assert(it == this);
    it = nullptr;
}
