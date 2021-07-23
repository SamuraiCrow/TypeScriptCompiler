#ifndef DEFINES_H_
#define DEFINES_H_

#define IDENTIFIER_ATTR_NAME "identifier"
#define RETURN_VARIABLE_NAME "__0return"
#define LABEL_ATTR_NAME "label"
#define UNDEFINED_NAME "undefined"
#define THIS_NAME "this"
#define LTHIS_NAME L"this"
#define SUPER_NAME "super"
#define CONSTRUCTOR_NAME "constructor"
#define CONSTRUCTOR_TEMPVAR_NAME ".ctor"
#define VTABLE_NAME ".vtbl"
#define LVTABLE_NAME L".vtbl"
#define LCONSTRUCTOR_NAME L"constructor"
#define LCONSTRUCTOR_TEMPVAR_NAME L".ctor"
#define MAIN_ENTRY_NAME "main"
#define TS_NEST_ATTRIBUTE "ts.nest"

#if __LP64__
#define TRAMPOLINE_SIZE 48
#else
#define TRAMPOLINE_SIZE 40
#endif

#endif // DEFINES_H_