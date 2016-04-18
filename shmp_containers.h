#ifndef SHMP_CONTAINERS
#define SHMP_CONTAINERS

#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>

struct ShmStruct;
struct ShmRecord;

using namespace boost::interprocess;

typedef managed_shared_memory::handle_t shmpHandle;

typedef offset_ptr<void> opData;

typedef allocator<char,managed_shared_memory::segment_manager> charAlloc;
typedef boost::interprocess::basic_string<char,std::char_traits<char>,charAlloc> shmp_string;
typedef allocator<shmp_string,managed_shared_memory::segment_manager> stringAlloc;

typedef allocator<ShmStruct, managed_shared_memory::segment_manager> ShmStructAlloc;
typedef boost::interprocess::vector<ShmStruct, ShmStructAlloc> vecofShmStruct;
typedef offset_ptr<vecofShmStruct> opVecofShmStruct;

typedef allocator<ShmRecord, managed_shared_memory::segment_manager> ShmRecordAlloc;

//typedef boost::interprocess::vector<ShmRecord, ShmRecordAlloc> vecofShmRecord;
//typedef offset_ptr<vecofShmRecord> opVecofShmRecord;

typedef offset_ptr<ShmRecord> opShmRecord;
typedef allocator<opShmRecord, managed_shared_memory::segment_manager> opShmRecordAlloc;
typedef boost::interprocess::vector<opShmRecord, opShmRecordAlloc> vecofOpShmRecord;

typedef offset_ptr<vecofOpShmRecord> opVecofOpShmRecord;

#endif // SHMP_CONTAINERS
