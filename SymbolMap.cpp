#include "SymbolMap.h"
#include "CompileContext.h"
#include "Decl.h"

Decl* SymbolMap::lookup_decl(TypeNameKind kind, const string* name) const {
    // TODO: consider kind
    for (auto& scope : scopes) {
        auto it = scope.declarations.find(name);
        if (it != scope.declarations.end()) {
            return it->second;
        }
    }
    return nullptr;
}

const Type* SymbolMap::lookup_type(TypeNameKind kind, const string* name) const {
    auto decl = lookup_decl(kind, name);
    if (!decl) return nullptr;

    return decl->to_type();
}

void SymbolMap::add_decl(TypeNameKind kind, const string* name, Decl* decl) {
    auto it = scopes.front().declarations.find(name);
    if (it != scopes.front().declarations.end()) {
        auto existing_decl = it->second;
        decl->redundant = true;

        // int baz(void);
        // static int baz(void); // ERROR
        if (decl->storage_class == StorageClass::STATIC && existing_decl->storage_class != StorageClass::STATIC) {
            message(decl->location) << "error static declaration of '" << decl->identifier << "' follows non-static\n";
        }

        existing_decl->redeclare(decl);
    } else {
        scopes.front().declarations[name] = decl;
    }

    // C99 6.2.2
    // For an identifier declared with the storage-class specifier extern in a scope in which a
    // prior declaration of that identifier is visible, if the prior declaration specifies internal or
    // external linkage, the linkage of the identifier at the later declaration is the same as the
    // linkage specified at the prior declaration. If no prior declaration is visible, or if the prior
    // declaration specifies no linkage, then the identifier has external linkage.

    // EXAMPLE 1
    // 
    // void bar(void) {
    //    extern int foo(void);
    //    {
    //        int foo;
    //        {
    //            extern float foo(void);  // ERROR
    //        }
    //    }
    // }
    //

    // EXAMPLE 2
    //
    // inline int foo(void) {  // The second declaration of foo causes this to have external linkage, changing its meaning
    //     return 0;    
    // }
    //
    // void bar(void) {
    //     extern int foo(void);
    // }

    if (decl->storage_class == StorageClass::EXTERN) {
        for (auto& scope : scopes) {
            auto it = scope.declarations.find(name);
            if (it == scope.declarations.end()) continue;

            auto existing_decl = it->second;

            if (existing_decl->storage_class != StorageClass::EXTERN && &scope != &scopes.back()) continue;

            if (existing_decl->storage_class == StorageClass::NONE) {
                existing_decl->storage_class = StorageClass::EXTERN;
            }

            if (&scope != &scopes.front()) {
                existing_decl->redeclare(decl);
            }
        }
    }
}

void SymbolMap::push_scope() {
    scopes.push_front(Scope());
}

void SymbolMap::pop_scope() {
    scopes.pop_front();
}

SymbolMap::SymbolMap() {
    push_scope();
}
