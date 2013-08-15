%insert(runtime) %{
/* Python.h has to appear first */
// Resolve conflicting use of _DEBUG across projects.
// #defining _DEBUG causes Python.h to require pythonXX_d.lib (instead
// of pythonXX.lib).
// MSVC #defines _DEBUG for Debug configuration.
// Make sure our project, which uses the latter, doesn't trigger the
// former since we don't want to debug python itself.
// (http://old.nabble.com/problems-with-python24_d.lib-td4760095.html#a4760095)
// Also, disable a check in MSVC when building for Multi-threaded Debug DLL
// that complains about mix of headers with and without _DEBUG.
#ifdef _DEBUG
#if defined(_MSC_VER) && _MSC_VER >= 1400
#define _CRT_NOFORCE_MANIFEST 1
#endif
#undef _DEBUG
#include <Python.h>
#define _DEBUG
#else
#include <Python.h>
#endif
%}

%insert(runtime) "swigrun.swg";         /* SWIG API */
%insert(runtime) "swigerrors.swg";      /* SWIG errors */   
%insert(runtime) "pyhead.swg";          /* Python includes and fixes */
%insert(runtime) "pyerrors.swg";        /* Python errors */
%insert(runtime) "pythreads.swg";       /* Python thread code */
%insert(runtime) "pyapi.swg";           /* Python API */
%insert(runtime) "pyrun.swg";           /* Python run-time code */

