#pragma once

#ifdef __unix__
    #define DLL_PREFIX 
#elif defined(_WIN32) || defined(WIN32)
    #define DLL_PREFIX __declspec(dllexport)
#endif
