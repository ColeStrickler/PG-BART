#ifndef PG_CACHE_HPP
#define PG_CACHE_HPP


#include <ostream>
#include <sstream>
#include <string.h>
#include <list>
#include <unordered_map>
#include <cassert>
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <iostream>
#include <random>
#include <cassert>




nlohmann::json OpenJSONFile(const std::string& filename);
void WriteJSONFile(const std::string& filename, nlohmann::json& json);


uint32_t intLog2(uint64_t x);
#define BIT_SET(val, n) (val | (1 << n))
#define BIT_EXTRACT(val, offset, num_bits) ((val >> offset) & ((1ULL << num_bits) - 1))
#define BIT_ZEROBOTTOM_N(val, n) (val & ~((1ULL << n) - 1ULL))

enum REPLACEMENT_POLICY
{
    LRU,
    PLRU,
    RANDOM,
    FIFO,
};



enum ACCESS_RESULT
{
    MISS_ALLOCATE,
    HIT,
};

enum ACCESS_EFFECTS
{
    NONE,
    EVICT_DIRTY,
    EVICT_CLEAN,
};

struct ReadAccess
{
    uint64_t m_Addr;
    uint64_t m_LoadSize;
};

struct WriteAccess
{
    uint64_t m_Addr;
    uint64_t m_StoreSize;
};




struct CacheLineStore
{
    uint64_t m_Tag = 0;
    uint64_t m_Addr = 0;
    uint32_t m_Region = 0;
    bool m_Valid = false;
    bool m_Dirty = false;
};


struct ReplacementResult
{
    CacheLineStore m_Evicted;
    ACCESS_EFFECTS m_Effect;
};


struct CacheAccessInfo
{
    ACCESS_RESULT m_Result;
    ACCESS_EFFECTS m_Effects;
    ReplacementResult m_Evicted;
};

struct CacheConfig
{
    int line_size;
    int sets;
    int ways;
    int banks;
    REPLACEMENT_POLICY policy;
};

class Cache
{
public:
    Cache(uint8_t line_size, uint32_t sets, uint32_t ways, uint16_t banks, REPLACEMENT_POLICY policy, uint8_t max_addr_bit=47);
    Cache(CacheConfig conf);
    ~Cache();

    CacheAccessInfo Load(ReadAccess load);
    CacheAccessInfo Store(WriteAccess store);
    void InvalidateIfNecessary(uint64_t addr);

private:
    void InitStore();
    void SetDirty(uint64_t& addr);
    ACCESS_RESULT CheckHit(uint64_t addr);
    ReplacementResult AllocateLine(uint64_t addr, bool write);


    // Replacement functions
    ReplacementResult ReplaceRandom(uint64_t addr, bool write);


    // Statistics
    uint64_t m_HitCount;
    uint64_t m_MissCount;



    // Configuration
    uint32_t m_Capacity;
    uint32_t m_NumLines;
    uint32_t m_WayCount;
    uint32_t m_SetCount;
    uint8_t m_LineSize;
    uint16_t m_Banks;
    uint8_t m_TagBits;
    uint8_t m_TagBitsStart;
    uint8_t m_BankBits;
    uint8_t m_BankBitsStart;
    uint8_t m_SetBits;
    uint8_t m_SetBitsStart;
    uint8_t m_OffsetBits;
    uint8_t m_MaxAddrBit;
    REPLACEMENT_POLICY m_Policy;
    std::vector<std::vector<std::vector<CacheLineStore>>> m_CacheStore;
};



#endif