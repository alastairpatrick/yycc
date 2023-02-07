#include "SymbolMap.h"
#include "CompileContext.h"
#include "Decl.h"

Decl* SymbolMap::lookup_decl(TypeNameKind kind, const string* name) const {
    // TODO: consider kind
    for (auto& scope : scopes) {
        auto it = scope.declarations.find(name);
        if (it != scope.declarations.end()) {
            auto decl = it->second;

            // Extern declarations with block scope are added to both their clock and to also to file scope.
            // Only return such declarations if found at block scope.
            if (decl->scope == IdentifierScope::FILE || &scope != &scopes.back()) return decl;
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
    auto& scope = decl->storage_class == StorageClass::EXTERN ? scopes.back() : scopes.front();

    auto it = scope.declarations.find(name);
    if (it != scope.declarations.end()) {
        auto existing_decl = it->second;
        decl->redundant = true;

        // int baz(void);
        // static int baz(void); // ERROR
        if (decl->storage_class == StorageClass::STATIC && existing_decl->storage_class != StorageClass::STATIC) {
            message(decl->location) << "error static declaration of '" << decl->identifier << "' follows non-static\n";
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
        // void foo(void) {
        //     extern int a(void);
        // }
        // void bar(void) {
        //     extern float a(void);  // ERROR
        // }

        // EXAMPLE 3
        //
        // inline int foo(void) {  // The second declaration of foo causes this to have external linkage, changing its meaning
        //     return 0;    
        // }
        //
        // void bar(void) {
        //     extern int foo(void);
        // }

        if (decl->storage_class == StorageClass::EXTERN && existing_decl->storage_class == StorageClass::NONE) {
            existing_decl->storage_class = StorageClass::EXTERN;
        }

        if (decl->scope == IdentifierScope::FILE) {
            existing_decl->scope = IdentifierScope::FILE;
        }

        existing_decl->redeclare(decl);
        decl = existing_decl;
    } else {
        scope.declarations[name] = decl;
    }

    scopes.front().declarations[name] = decl;
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
