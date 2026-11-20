/////////////////////////////////////////////////////////
// File: I386DsoHandle.cpp
// Date: 2026-06-08
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Provides the C++ DSO lifecycle hooks for i386
//              builds linked without the standard CRT startup files.
/////////////////////////////////////////////////////////

extern "C"
{
    void* __dso_handle __attribute__((visibility("hidden"))) = &__dso_handle;

    void __cxa_finalize(void*);
}

// -nostartfiles omits crtbeginS.o's __do_global_dtors_aux
// Without it, __cxa_atexit callbacks survive dlclose and later call into unmapped code
__attribute__((destructor(101)))
static void FinalizeI386Dso()
{
    __cxa_finalize(__dso_handle);
}
