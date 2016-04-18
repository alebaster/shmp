#ifndef SHMP_H
#define SHMP_H

#include "shmp_decl.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#  ifdef SHMP_LIBRARY
#    define SHMP_API __declspec(dllexport)
#  else
#    define SHMP_API __declspec(dllimport)
#  endif
#else
#  define SHMP_API
#endif

SHMP_API shmpHandle shmCreateSegment(const char* segmentName, int maxSize);
SHMP_API shmpHandle shmAddStruct(const char* strName, const char* tagName, const char* hFilePath, void* initData, size_t dataSize, uint32_t flags);
SHMP_API bool shmLoadSymbolics();
SHMP_API bool shmFinalize();
SHMP_API bool shmDestroy();
SHMP_API void* shmGetAddrFromHadnle(shmpHandle handle);
SHMP_API shmpHandle shmOpenSegment(const char* segmentName);
SHMP_API int shmpGetSize();
SHMP_API bool shmpLockMutex();
SHMP_API void shmpUnlockMutex();

#ifdef __cplusplus
}
#endif

#endif // SHMP_H
