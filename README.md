# Summary of Differences with C
This is not a sales pitch, more a reminder. The most significant changes to C are order independent declarations and initializing all variables by default.

## Context-Free Grammar
OIC has a context-free grammar. In contrast, C's grammar is not context-free; the parser or lexer needs to determine whether identifiers correspond to types or not. For OIC, this is not possible because declarations are order independent, so the declaration of a type identifier might follow some usage of that identifier.

OIC actually has two grammars. The first is a context-free grammar for a useful superset of the language, crucially, one that can determine those identifiers corresponding to type names. The second grammar is for OIC proper, is not context free, but is parseable with the benefit of an initial analysis performed using the context-free grammar.

## Declarations
Declarations at file scope are order independent. For example, this is valid:
```c
struct A {
    B* b;  // okay
};
struct B {
    A* a;
};
```
The translation unit is expanded to span all source files in a module. Combined with order independence, this renders forward declaration largely redundant. For example, there is no reason to put function prototypes in a header file in order to share the declaration with other source files in the module.

In the example below, separate source files “file1.c” and “file2.c” are part of the same module, and so are part of the same translation unit. Due to order independence, the prototypes of functions “f” and “g” are known everywhere in the translation unit. Unlike C, with regard to declarations with file scope, there is no notion of “before” or “after.”
```c
// file1.c
void g(int x) {
    f(x); // argument known to have type ‘float’
}

// file2.c
void f(float x) {
}
```
Internal linkage means the same thing as in C. However with translation units being expanded to span multiple source files, this means that objects and functions with internal linkage are visible to all source files in a module.

## Preprocessing
### Expanded Translation Unit
OIC expands the translation unit to span all source files in a module. This happens during what in C would be the seventh phase, i.e. compilation. Specifically, it happens after preprocessing.  This means that preprocessor macros defined in one source file are not visible in other source files, even though they will later be combined into the same translation unit.

### Namespaces
Preprocessing directives are added to support namespaces. These are resolved during preprocessing, just before macros are expanded. The advantage of doing this during preprocessing is macros and keywords can be put in namespaces.

```c
#include <windows.h>     // #defines “min” macro in root namespace
#namespace X::Y
#using std::min          // binds to “std::min”, not “min” macro

int foo(int a, int b) {  // defines “X::Y::foo” identifier
  return min(a, b);      // binds to “std::min”, not “min” macro
}
```

## Types
### Signed Integers
Overflow of signed integer arithmetic operations, including e.g. addition and multiplication, never yields undefined behavior. Instead, signed integer overflow is defined to be representationally consistent with two’s complement arithmetic.
```c
int x = INT_MAX;
++x;  // okay
assert(x == INT_MIN);
```

### Enumerations
In C, enumerated types are integer types. In OIC, enumerated types are distinct from integer types. Enumerated types may be implicitly converted to their underlying integer type. Applied to an enumerated type, the integer promotion rules apply as though an enumerated type was its underlying integer type.

### Pointers
Implicit conversions between pointer types are much stricter than C. Implicit pointer conversions may never lose qualifiers. Conversions between unrelated base types are not allowed without an explicit cast. Any pointer type may be implicitly converted to void pointer, so long as qualifiers are preserved.

### Tag Types
Similar to C++, there is no separate namespace for tags so the keywords “struct”, “union”, etc may be omitted. For example, this is valid.
```c
struct Node {
    Node* next;  // “struct” omitted
    Node* prev;
};
```
Unlike C and similar to C++, a new scope is introduced within “struct” and “union” definitions. The “dot” operator is used in a nested type specifier.
```c
struct S {
    struct T {};
};
S.T x;    // nested type specifier, okay
// T x;   // ERROR
```

### Reference Types
Reference type semantics resemble C++ but are more limited. Only variables with automatic storage duration, including parameters, and function return types may have reference type. Typedefs may not have reference type.

Both rvalue and lvalue reference types are available, having semantics resembling C++. The biggest difference is for parameters of rvalue reference type, where the callee takes ownership of a passed rvalue reference and is responsible for calling its destructor; the caller never calls the destructors for an rvalue passed by rvalue reference.

```c
void f(int& lvalue_reference_param);
void g(int&& rvalue_reference_param);

int& h(int x) {
	int& lvalue_reference_var = x;
}
```
By default, reference types parameters have nocapture semantics so, among other things, their address may not be taken. The "captured" variant of reference type does not have nocapture semantics.
```c
int& f(int& a, int& nocapture b) {
    // int* p = &a;	// ERROR; "a" has nocapture semantics
	// return a;	// ERROR; "a" has nocapture semantics
    int* q = &b;
    return b;
}
```

### Member Functions

Structs may have member functions. Unlike C++, the "this" parameter is explicit.

```c
struct S {
    void call_me(S& this, int param);
};

void g() {
    S s;
    s.call_me(7);
}
```

### Destructors
Similar to C++, a struct may have a destructor. This is called at the end of an object's lifetime, but not always. Specifically, if an object is not in its default state, the destructor must be called. However, if the compiler can prove that an object _is_ in its default state, it should preferably _not_ generate code to call the destructor. The compiler's ability to do this can depend on optimization level.
```c
struct T {
    FILE* file;  // initialized to null

    // ...
    void destructor(T& this) {
        if (file) fclose(file);
    }
};
```
Destructors are also automatically called on the left side of a move expression, assuming the compiler cannot prove the left side is not in its default state, as above.

### Adjustment of Function Parameter Types
In C, array parameter types are "adjusted" to pointer types. Instead, OIC adjusts array typed parameters to lvalue reference to array type, even if they are not written as such. Consider:
```c
void f(int a[3][2]);
```
In C, this is adjusted as follows, losing static type checking information:
```c
void f(int *a[2]);
```
In contrast, OIC loses no static type information, while remaining ABI compatible with C:
```c
void f(int (&a)[3][2]);
```

## Variables
### Initialization
All variables, including variables with automatic duration, are initialized by default. The default value is the same as for variables with static duration, i.e. some kind of zero value. It is still possible to skip initialization with a switch or goto statement.
```c
SensitiveInfo* p;  // initializes p to null
```
Default initialization of variables may be prevented with an uninitializer expression. This might be used as an optimization, for example.
```c
int x = {void};  // leaves x uninitialized
```
An uninitializer expression may be used to leave a “hole” in the initialization of a value with aggregate type.
```c
int x[3] = {{void}, 1};  // x[0] is uninitialized, x[1] is initialized
```
The primary motivation for this change is to reduce bugs while allowing variables to be explicitly uninitialized where default initialization would have a negative performance impact.

### Constants
Similar to C++, variables declared at file scope with const qualified type can be used in constant expressions.
```c
const int array_size = 3;
int array[array_size];
```

## Expressions
### Move Operator
The move operator invokes the destructor on the left hand side (unless the compiler can prove redundant), trivially moves (in C++ terms) the right hand side to the left hand side, and resets the state of the right hand side to its default state.
```c
struct T {
    int x;
    void destructor(T&);
};

void swap_T(T& a, T &b) {
    T t = &&a;
    a = &&b;
    b = &&t;  // now compiler can prove variable "t" has default state so swap_T should not call any destructors
}
```

## Exceptions
Throw may be part of function return type. Try/catch statement. Throw statement. Exception propagation. Uses return channel.
