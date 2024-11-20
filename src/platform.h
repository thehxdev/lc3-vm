#ifndef LC3_PLATFORM_H
#define LC3_PLATFORM_H


/* check https://stackoverflow.com/a/8249232/19005972 */
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #define LC3_PLAT_WINDOWS
#elif defined(__APPLE__) || defined(__MACH__)
    #define LC3_PLAT_DARWIN
#elif defined(__linux__)
    #define LC3_PLAT_LINUX
#elif defined(__FreeBSD__)
    #define LC3_PLAT_FREEBSD
#else
    #define LC3_PLAT_UNKNOWN
#endif // platform

#if defined(LC3_PLAT_LINUX) || defined(LC3_PLAT_FREEBSD)  \
    || defined(unix) || defined(__unix) || defined(__unix__)
    #define LC3_PLAT_UNIX
#endif // unix platform

#if defined(LC3_PLAT_UNKNOWN) && !defined(LC3_PLAT_UNIX)
    #error "unknown platform"
#endif


#endif // LC3_PLATFORM_H

