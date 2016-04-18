#include <iostream>
#include <fstream>
#include <map>
#include <algorithm>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/algorithm/string.hpp>

#include "shmp.h"

#ifdef BOOST_OS_WINDOWS
    #include <windows.h>
    #define SHM_REGISTRY_KEY  "Software\\Aurora\\TSS\\Shmp Table"
#endif

static managed_shared_memory segment;
static named_mutex* mutex;
static shmpHandle tableHandle;
std::string shmSegmentName;
std::string shmMutexName;
static std::vector<std::string> hFiles;
static std::vector<std::string> hStructs;
std::map<std::string,shmpHandle> mapOfInnerStructs;

bool _checkFreeMemory(size_t size);
bool _isSegmentValid();

template <class T, class A>
T* _constructAnonymousObject(A);

int _registerShmInSystem();
int _hFileParser(std::string filename);
std::string _findKeyword(const char* keyword, std::ifstream& file);
void _parseString(std::string s, shmpHandle shmd_handle);
bool _doINeedThisStruct(std::string s);
std::string _getStructName(std::string s);
uint32_t _getTypeid(std::string s);
size_t _getTypeSize(opShmRecord d);
void fixOffset(ShmRecord* d);


shmpHandle shmCreateSegment(const char* segmentName, int maxSize)
{
    if (!segmentName || !(*segmentName))    // не задано имя сегмента
    {
        // TODO log
        std::cout << "in \"shmCreateTable()\" arg \'segmentName\' is empty" << std::endl;
        
        return 0;
    }

    std::string mutexName;
    std::string segName;
    
    segName = std::string(segmentName) + std::string("_TABLE");
    mutexName = std::string(segmentName) + std::string("_MUTEX");
    
    shared_memory_object::remove(segName.c_str());
    named_mutex::remove(mutexName.c_str());
    
    ShmTable* tablePtr = NULL;
        
    try // создаем сегмент
    {
        segment = managed_shared_memory(create_only, segName.c_str(), maxSize);
    }
    catch (interprocess_exception& e)
    {
        // TODO log
        std::cout << e.what() << std::endl;
        return 0;
    }
    
    try // создаем мьютекс
    {
        mutex = new named_mutex(create_only, mutexName.c_str());
        mutex->lock();
    }
    catch (interprocess_exception& e)
    {
        // TODO log
        std::cout << e.what() << std::endl;
        return 0;
    }
    
    shmSegmentName = segName;   // признак успешного создания сегмента
    shmMutexName = mutexName;   // признак успешного создания мьютекса
    
    if (!_checkFreeMemory(sizeof(ShmTable)))
        return 0;
    try // засовываем туда таблицу
    {
        mutex->unlock(); // segment.construct<> захватывает мьютекс
        tablePtr = static_cast<ShmTable*>(segment.construct<ShmTable>("ShmTable")());
        mutex->lock();
    }
    catch (interprocess_exception& e)
    {
        // TODO log
        std::cout << e.what() << std::endl;
        return 0;
    }
    
    tablePtr->NumOfStructs = 0;
    strcpy_s(tablePtr->ShmTableName,SHM_TBL_NAME_LEN,segmentName);
    tablePtr->ShmVer = SHM_VERSION;
    tablePtr->ShmSubVer = SHM_SUBVERSION;
    
    if (!_checkFreeMemory(sizeof(vecofShmStruct)))
        return 0;
    try // создаем вектор структур
    {
        if (!_checkFreeMemory(sizeof(vecofShmStruct)))
            return 0;
        const ShmStructAlloc alloc_inst(segment.get_segment_manager());
        tablePtr->VecOfStructs = _constructAnonymousObject<vecofShmStruct>(alloc_inst);
    }
    catch (interprocess_exception& e)
    {
        // TODO log
        std::cout << e.what() << std::endl;
        return 0;
    }
    
    tableHandle = segment.get_handle_from_address(tablePtr);

    mutex->unlock();
    
    return tableHandle;
}

shmpHandle shmOpenSegment(const char* segmentName)
{
    if (!segmentName || !(*segmentName))
    {
        // TODO log
        std::cout << "in \"shmOpenSegment()\" arg \'segmentName\' is empty" << std::endl;
        
        return 0;
    }
    
    std::string mutexName;
    std::string segName;
    
    segName = std::string(segmentName) + std::string("_TABLE");
    mutexName = std::string(segmentName) + std::string("_MUTEX");
    
    ShmTable* tablePtr = NULL;
    
    try // открываем сегмент
    {
        segment = managed_shared_memory(open_only, segName.c_str());
    }
    
    catch (interprocess_exception& e)
    {
        // TODO log
        std::cout << e.what() << std::endl;
        return 0;
    }

    try // открываем мьютекс
    {
        mutex = new named_mutex(open_only, mutexName.c_str());
    }
    catch (interprocess_exception& e)
    {
        // TODO log
        std::cout << e.what() << std::endl;
        return 0;
    }
    
    shmSegmentName = segName.c_str();
    shmMutexName = mutexName.c_str();
    
    std::pair<ShmTable*, managed_shared_memory::size_type> res;
    res = segment.find<ShmTable>("ShmTable");
    tablePtr = res.first;
    
    tableHandle = segment.get_handle_from_address(tablePtr);
    
    return tableHandle;
}

shmpHandle shmAddStruct(const char* strName, const char* tagName, const char* hFilePath, void* initData, size_t dataSize, uint32_t flags)
{
    if (!strName || !(*strName))
    {
        // TODO log
        std::cout << "in \"shmAddStruct()\" arg \'strName\' is empty" << std::endl;
        
        return 0;
    }
    
    ShmTable* tablePtr = static_cast<ShmTable*>(shmGetAddrFromHadnle(tableHandle));
    
    if (!tablePtr)
    {
        // TODO log
        std::cout << "in \"shmAddStruct()\" tablePtr is NULL" << std::endl;
        
        return 0;
    }
    
    if (!mutex)
    {
        std::cout << "in shmAddStruct() " << "static variable mutex is NULL" << std::endl;
        return 0;
    }
    
    ShmStruct tempStruct;
    
    if (strlen(strName) >= sizeof(tempStruct.ObjName))
    {
        // TODO log
        std::cout << "in \"shmAddStruct()\" arg strName is too long" << std::endl;
        
        return 0;
    }

    mutex->lock();
    strcpy_s(tempStruct.ObjName,SHM_STR_NAME_LEN,strName);
    strcpy_s(tempStruct.TagName,SHM_STR_NAME_LEN,tagName);  // !!!
    tempStruct.IdxInTable = tablePtr->NumOfStructs;
    tempStruct.Flags = flags;
    tempStruct.DataSize = dataSize;
    
    //void* data;
    if (!_checkFreeMemory(dataSize))
        return 0;
    try // выделяем память для данных
    {
        mutex->unlock();
        tempStruct.Data = segment.allocate(dataSize);
        mutex->lock();
    }
    catch (interprocess_exception& e)
    {
        // TODO log
        std::cout << e.what() << " in: data = segment.allocate(dataSize);" << std::endl;
        return 0;
    }

    //tempStruct.Data = segment.get_handle_from_address(data);
    
    if (!_checkFreeMemory(sizeof(ShmStruct)))
        return 0;
    try // засовываем новую структуру в вектор
    {
        mutex->unlock();
        tablePtr = static_cast<ShmTable*>(shmGetAddrFromHadnle(tableHandle));
        //std::cout << tablePtr->VecOfStructs->max_size() << std::endl;
        tablePtr->VecOfStructs->push_back(tempStruct);
        mutex->lock();
    }
    catch (interprocess_exception& e)
    {
        // TODO log
        std::cout << e.what() << " in: (tablePtr->VecOfStructs)->push_back(tempStruct);" << std::endl;
        return 0;
    }
    
    if(initData)    // копируем данные инициализации
        memcpy(tempStruct.Data.get(),initData,dataSize);
    else
        memset(tempStruct.Data.get(),0,dataSize);
    
    tablePtr->NumOfStructs += 1;
    
    if(hFilePath && *hFilePath)
    {
        if (hFiles.end() == find(hFiles.begin(),hFiles.end(),hFilePath))
            hFiles.push_back(hFilePath);
        hStructs.push_back(tagName);
    }
    
    mutex->unlock();
    
    return 0;
}

bool shmLoadSymbolics()
{
    mutex->lock();
    for (unsigned int i=0;i<hFiles.size();i++)
        _hFileParser(hFiles.at(i));
    mutex->unlock();
    
    return true;
}

bool shmFinalize()
{
    try
    {
        segment = managed_shared_memory();
        managed_shared_memory::shrink_to_fit(shmSegmentName.c_str());
        segment = managed_shared_memory(open_only, shmSegmentName.c_str());
    }
    catch (interprocess_exception& e)
    {
        // TODO log
        std::cout << e.what() << std::endl;
        return false;
    }
    
    _registerShmInSystem();
    
    return true;
}

bool shmDestroy()
{
    shared_memory_object::remove(shmSegmentName.c_str());
    named_mutex::remove(shmMutexName.c_str());
    
    return true;
}

bool _checkFreeMemory(size_t size)
{
    if (segment.get_free_memory() < (size+1000))
    {
        // TODO log
        try
        {
            segment = managed_shared_memory();
            managed_shared_memory::grow(shmSegmentName.c_str(),size+1000);
            segment = managed_shared_memory(open_only, shmSegmentName.c_str());
            std::cout << "GROW" << std::endl;
        }
        catch (interprocess_exception& e)
        {
            // TODO log
            std::cout << e.what() << std::endl;
            return false;
        }
    }
    
    return true;
}

bool _isSegmentValid()
{
    if (!shmSegmentName.size())
    {
        // TODO log
        std::cout << "in \"_isSegmentValid()\" \'shmSegmentName\' is empty" << std::endl;
        
        return false;
    }
    
    return true;
}

template <class T, class A>
T* _constructAnonymousObject(A alloc)
{
    mutex->unlock();
    T* object = segment.construct<T>(anonymous_instance)(alloc);
    mutex->lock();
    
    return object;
}

void* shmGetAddrFromHadnle(shmpHandle handle)
{
    if(!_isSegmentValid())
        return 0;
    
    return segment.get_address_from_handle(handle);
}

int shmpGetSize()
{
    if(!_isSegmentValid())
        return 0;
    
    return segment.get_size();
}

bool shmpLockMutex()
{
    bool ret = false;
    
    if (!mutex)
    {
        std::cout << "in shmpLockMutex() " << "static variable mutex is NULL" << std::endl;
        return false;
    }
    
    ret = mutex->try_lock();
    
    return ret;
}

void shmpUnlockMutex()
{
    if (!mutex)
    {
        // TODO log
        std::cout << "in shmpUnlockMutex() " << "static variable mutex is NULL" << std::endl;
        return;
    }
    
    mutex->unlock();
}

int _registerShmInSystem()
{
#ifdef BOOST_OS_WINDOWS
    HKEY  keyHandle;
    DWORD regErrCode = 666;
    // create or open reg key
    if((regErrCode = RegCreateKeyEx(HKEY_CURRENT_USER, TEXT(SHM_REGISTRY_KEY), 0, NULL,
        REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &keyHandle, NULL)) == ERROR_SUCCESS)
    {
        ShmTable* tablePtr = static_cast<ShmTable*>(shmGetAddrFromHadnle(tableHandle)); // ??? проверить
        char  shmTableName[SHM_TBL_NAME_LEN];
        std::strncpy(shmTableName,tablePtr->ShmTableName,SHM_TBL_NAME_LEN);
        wchar_t wshmTableName[SHM_TBL_NAME_LEN];
        mbstowcs(wshmTableName,shmTableName,SHM_TBL_NAME_LEN);
        regErrCode = RegSetValueEx(keyHandle, wshmTableName, 0, REG_SZ,NULL, 0);
        RegCloseKey(keyHandle);
    }
    
    if (regErrCode)
    {
        // TODO log
        std::cout << "in _registerShmInSystem() regErrCode = " << regErrCode << std::endl;
        return -1;
    }
#endif
    
    return 0;
}

// 
int _hFileParser(std::string filename)
{
    std::ifstream file(filename.c_str());

    if (!file)
    {
        // TODO log
        std::cout << "can\'t open file" << std::endl;
        return -1;
    }
    
    while (!file.eof())
    {
        std::string temp = _findKeyword("struct",file);
        if (temp != "")
        {
            if (temp.find("inner") == std::string::npos)   // real struct
            {
                std::string structName = _getStructName(temp);
                if (!_doINeedThisStruct(structName))
                    continue;
                if (temp.find("{") != std::string::npos)    // same string
                {
                    ;  // TODO?
                }
                else if ((temp = _findKeyword("{",file)) != "")     // next string
                {
                    if (!_checkFreeMemory(sizeof(vecofOpShmRecord)))
                        return -1;
                    const opShmRecordAlloc alloc_inst (segment.get_segment_manager());
                    vecofOpShmRecord* vec = _constructAnonymousObject<vecofOpShmRecord>(alloc_inst);
                    shmpHandle vec_handle = segment.get_handle_from_address(vec);
                    
                    while (temp.find("}") == std::string::npos)
                    {
                        if (!_checkFreeMemory(sizeof(ShmRecord)))
                            return -1;
                        const ShmRecordAlloc alloc_inst (segment.get_segment_manager());
                        ShmRecord* d = _constructAnonymousObject<ShmRecord>(alloc_inst);
                        shmpHandle d_handle = segment.get_handle_from_address(d);
                        
                        getline(file,temp);
                        _parseString(temp,d_handle);
                        
                        vecofOpShmRecord* vec = static_cast<vecofOpShmRecord*>(segment.get_address_from_handle(vec_handle)); 
                        
                        if (!d->Name.empty())
                        {
                            mutex->unlock();
                            uint32_t offset = 0;
                            if (!vec->empty())
                                offset = vec->back().get()->Offset + _getTypeSize(vec->back())*vec->back().get()->NumOfItem;
                            d->Offset = offset;
                            fixOffset(d);
                            vec->push_back(d);
                            mutex->lock();
                        }
                    }
                    
                    // закончили структуру
                    ShmTable* tablePtr = static_cast<ShmTable*>(shmGetAddrFromHadnle(tableHandle));
                    opVecofOpShmRecord probably_new_vec = static_cast<vecofOpShmRecord*>(segment.get_address_from_handle(vec_handle)); 
                    vecofShmStruct::iterator it = std::find_if(tablePtr->VecOfStructs->begin(),tablePtr->VecOfStructs->end(),ShmStructFindTagNamePredicate(structName.c_str()));
                    while (it != tablePtr->VecOfStructs->end())
                    {
                        (*it).Description = probably_new_vec;
                        it = std::find_if(++it,tablePtr->VecOfStructs->end(),ShmStructFindTagNamePredicate(structName.c_str()));
                    }
                }
            }
            else    // inner struct
            {
                std::string innerStructName = _getStructName(temp);
                
                if (temp.find("{") != string::npos)    // same string
                {
                    ;  // TODO?
                }
                else if ((temp = _findKeyword("{",file)) != "")     // next string
                {
                    if (!_checkFreeMemory(sizeof(vecofOpShmRecord)))
                        return -1;
                    const opShmRecordAlloc alloc_inst (segment.get_segment_manager());
                    vecofOpShmRecord* innervec = _constructAnonymousObject<vecofOpShmRecord>(alloc_inst);
                    shmpHandle innervec_handle = segment.get_handle_from_address(innervec);
                    
                    while (temp.find("}") == std::string::npos)
                    {
                        if (!_checkFreeMemory(sizeof(ShmRecord)))
                            return -1;
                        const ShmRecordAlloc alloc_inst (segment.get_segment_manager());
                        ShmRecord* d = _constructAnonymousObject<ShmRecord>(alloc_inst);
                        shmpHandle d_handle = segment.get_handle_from_address(d);
                        
                        getline(file,temp);
                        _parseString(temp,d_handle);
                        
                        vecofOpShmRecord* invec = static_cast<vecofOpShmRecord*>(segment.get_address_from_handle(innervec_handle));
                        
                        if (!d->Name.empty())
                        {
                            mutex->unlock();
                            uint32_t offset = 0;
                            if (!invec->empty())
                                offset = invec->back()->Offset + _getTypeSize(invec->back())*invec->back()->NumOfItem;
                            d->Offset = offset;
                            invec->push_back(d);
                            mutex->lock();
                        }
                        else
                        {
                            mutex->unlock();
                            segment.destroy_ptr<ShmRecord>(d);
                            mutex->lock();
                        }
                    }
                    if (innerStructName != "")
                    {
                        mapOfInnerStructs.insert(make_pair(innerStructName,innervec_handle));
                    }
                }
            }   // else inner struct
        }   // if (temp != "")
    } // while !eof
    
    file.close();
    
    return 0;
}

std::string _findKeyword(const char* keyword, std::ifstream &file)
{
    std::string temp;
    
    while (getline(file,temp))
    {
        if (temp.find(keyword) != std::string::npos)
            return temp;
    }
    
    return "";
}

void _parseString(std::string s, shmpHandle shmd_handle)
{
    const charAlloc alloc_inst (segment.get_segment_manager());
    ShmRecord* shmd = static_cast<ShmRecord*>(segment.get_address_from_handle(shmd_handle)); 
    
    std::vector <std::string> splitString;
    boost::split(splitString, s, boost::is_any_of(";"),boost::token_compress_on);
    for (size_t i=0;i<splitString.size();i++)
        boost::trim(splitString[i]);
    
    std::vector <std::string> splitLeft;  // тип и имя
    boost::split(splitLeft, splitString[0], boost::is_any_of("\t "),boost::token_compress_on);
    if (2 <= splitLeft.size())
    {
        if (splitLeft[0].find("//") != std::string::npos)
            return;
        
        std::string name;
        
        if (splitLeft[1].find("[") != std::string::npos)   // массив
        {
            std::string::size_type begin = splitLeft[1].find_last_of('[');
            std::string::size_type end = splitLeft[1].find_last_of(']');
            std::string numberOfItemsString(splitLeft[1],begin+1,end-begin-1);
            name = std::string(splitLeft[1].begin(),splitLeft[1].begin()+begin);
            shmd->NumOfItem = atoi(numberOfItemsString.c_str());
        }
        else
        {
            name = std::string(splitLeft[1].begin(),splitLeft[1].end());
            shmd->NumOfItem = 1;
        }
        
        mutex->unlock();
        shmd->Type = _getTypeid(splitLeft[0]);
        shmd->Name = shmp_string(name.begin(),name.end(),alloc_inst);
        mutex->lock();
        
        if (mapOfInnerStructs.end() != mapOfInnerStructs.find(splitLeft[0]))    // есть описание описания
        {
            opVecofOpShmRecord innervec = static_cast<vecofOpShmRecord*>(segment.get_address_from_handle(mapOfInnerStructs[splitLeft[0]]));
            
            shmd->Descriptions = innervec;
        }
    }
    else
        return;
    
    if (2 == splitString.size())
    {
        boost::erase_first(splitString[1], "//");
        mutex->unlock();
        shmd->Comment = shmp_string(splitString[1].begin(),splitString[1].end(),alloc_inst);
        mutex->lock();
    }
}

bool _doINeedThisStruct(std::string s)
{
    return hStructs.end() != std::find(hStructs.begin(),hStructs.end(),s);
}

std::string _getStructName(std::string s)
{
    boost::trim(s);
    std::vector <std::string> splitString;
    boost::split(splitString, s, boost::is_any_of("\t "),boost::token_compress_on);
    if (2 <= splitString.size())
    {
        boost::trim(splitString[1]);
                
        return splitString[1];
    }
    
    return "";
}

uint32_t _getTypeid(std::string s)
{
    uint32_t ret = 0;
    
    if (s == "char")        ret = TYPE_CHAR;
    if (s == "short")       ret = TYPE_SHORT;
    if (s == "int")         ret = TYPE_INT;
    if (s == "long")        ret = TYPE_LONG;
    if (s == "float")       ret = TYPE_FLOAT;
    if (s == "double")      ret = TYPE_DOUBLE;
    if (s == "bool")        ret = TYPE_BOOL;
    if (s == "uint8_t")     ret = TYPE_UINT8;
    if (s == "uint16_t")    ret = TYPE_UINT16;
    if (s == "uint32_t")    ret = TYPE_UINT32;
    if (mapOfInnerStructs.end() != mapOfInnerStructs.find(s))
        ret = TYPE_STRUCT;
    
    return ret;
}

void fixOffset(ShmRecord* d)    // не подходит для многоразового использования внутренних структур
{
    if (d->Descriptions)
    {
        for (size_t i=0;i<d->Descriptions->size();i++)
        {
            d->Descriptions->at(i)->Offset += d->Offset;
            fixOffset(d->Descriptions->at(i).get());
        }
    }
}

size_t _getTypeSize(opShmRecord d)
{
    uint32_t ret = 0;    
    
    uint32_t t = d->Type;
    
    switch (t)
    {
    case TYPE_CHAR:
        ret = sizeof(char);
        break;
    case TYPE_SHORT:
        ret = sizeof(short);
        break;
    case TYPE_INT:
        ret = sizeof(int);
        break;
    case TYPE_LONG:
        ret = sizeof(long);
        break;
    case TYPE_FLOAT:
        ret = sizeof(float);
        break;
    case TYPE_DOUBLE:
        ret = sizeof(double);
        break;
    case TYPE_BOOL:
        ret = sizeof(bool);
        break;
    case TYPE_UINT8:
        ret = sizeof(uint8_t);
        break;
    case TYPE_UINT16:
        ret = sizeof(uint16_t);
        break;
    case TYPE_UINT32:
        ret = sizeof(uint32_t);
        break;
    case TYPE_STRUCT:
        for (managed_shared_memory::size_type i=0;i<d->Descriptions->size();i++)
            ret += _getTypeSize(d->Descriptions->at(i));
        break;
    default:    // ???
        ret = sizeof(char);
        break;
    }
    
    return ret;
}
