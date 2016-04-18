#ifndef SHMP_DECL
#define SHMP_DECL

#include "stdint.h"

#include "shmp_containers.h"

#define SHM_VERSION         0
#define SHM_SUBVERSION      1

#define SHM_STR_NAME_LEN 40
#define SHM_TBL_NAME_LEN 32

#define OPNULL 1    // нулевой offset_ptr

struct ShmStruct;
struct ShmRecord;

#define SHMSF_AF_NSM    0x00000100

// Type description constants:
#define TYPE_CHAR              0x0001
#define TYPE_SHORT             0x0002
#define TYPE_INT               0x0003
#define TYPE_LONG              0x0004
#define TYPE_FLOAT             0x0005
#define TYPE_DOUBLE            0x0006
#define TYPE_BOOL              0x0008

#define TYPE_UNSIGNED      0x00010000

#define TYPE_UINT       (TYPE_INT   | TYPE_UNSIGNED)
#define TYPE_UINT8      (TYPE_CHAR  | TYPE_UNSIGNED)
#define TYPE_UINT16     (TYPE_SHORT | TYPE_UNSIGNED)
#define TYPE_UINT32     (TYPE_LONG  | TYPE_UNSIGNED)

#define TYPE_STRUCT           0xFFFF

#pragma pack (push, 1)
struct ShmTable
{
    uint16_t  ShmVer;               // SHM version (must be SHM_VERSION)
    uint16_t  ShmSubVer;            // SHM subversion (must be SHM_SUBVERSION)
    char  ShmTableName[SHM_TBL_NAME_LEN]; // SHM Table name
    uint16_t  ShmFlags;         // Bitfield flags for Table (see SHMTF_ const.)
    uint32_t StructDataSize;       // Size of area for struct data
    uint32_t EFFASize;             // Size of Exchange FreeForm Area (EFFA)
    //---------------------------------------------------------------------------
    uint32_t  DataAreaOffset;       // Offset from Table origin to area of struct data
    uint32_t  RecordsAreaOffset;    // Offset from Table origin to area of record arrays
    uint32_t  EFFAreaOffset;        // Offset from Table origin to EFFA
    //---------------------------------------------------------------------------
    short  NumOfStructs;          // Actually quantity of structs
    
    opVecofShmStruct VecOfStructs;
};

struct ShmStruct
{
    short IdxInTable;                     // Index of this struct in SHN
    char  TagName [SHM_STR_NAME_LEN+1];   // Name of struct tag (class) name
    char  ObjName [SHM_STR_NAME_LEN+1];   // Name of struct object
    uint32_t DataSize;                    // Size of Data
    uint64_t Flags;                       // User's flags for struct (see SHMSF_ const)
    uint8_t  IntFlags;                    // Internal flags for struct (see SHMSFI_ const)
    opData Data;
    opVecofOpShmRecord Description;       // 
    uint32_t AppFlags;                    // Приложения, которые использует эту структуру (маска)
    uint32_t WriteCount;                  // Counter of write to struct data
};

struct ShmRecord
{
    ShmRecord(ShmRecordAlloc alloc) : Name(alloc),Comment(alloc) {}
    
    uint32_t        Type;                  // Type of record
    short           NumOfItem;             // Quantity of units in record (1 if member isn't array)
    shmp_string     Name;                  // Name of record
    shmp_string     Comment;               // Commentary for record
    opVecofOpShmRecord   Descriptions;     // OPNULL if there is no inner structs
    uint32_t        Offset;                // Offset for this record in struct
    uint8_t         ReadOnly;              // True if this record is read-only
};

struct ShmStructFindTagNamePredicate
{
    const char* tagName;
    
    ShmStructFindTagNamePredicate(const char* name) : tagName(name){}
    bool operator() (const ShmStruct& outerstruct) const
    {
        return (strcmp(tagName,outerstruct.TagName) == 0);
    }
};

#pragma pack (pop)

#endif // SHMP_DECL

