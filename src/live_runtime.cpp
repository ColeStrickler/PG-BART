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





nlohmann::json OpenJSONFile(const std::string& filename) {
    nlohmann::json data;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Failed to open file\n";
        return {};
    }
    file >> data;
    return data;
}

static inline bool isPowerOf2(uint64_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

uint32_t intLog2(uint64_t x) {
    uint32_t log = 0;

    while (x >>= 1) {
        ++log;
    }

    return log;
}

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



class Cache
{
public:
    Cache(uint8_t line_size, uint32_t sets, uint32_t ways, uint16_t banks, REPLACEMENT_POLICY policy, uint8_t max_addr_bit=47);
    ~Cache();

    CacheAccessInfo Load(ReadAccess load);



    CacheAccessInfo Store(WriteAccess store);
    
private:
    void InitStore();
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

/*
    Sets = sets per bank
    Ways = ways per bank
*/
Cache::Cache(uint8_t line_size, uint32_t sets, uint32_t ways, uint16_t banks, REPLACEMENT_POLICY policy, uint8_t max_addr_bit)
{
    m_MaxAddrBit = max_addr_bit;
    m_SetCount = sets;
    m_WayCount = ways;
    m_Banks = banks;
    assert(isPowerOf2(m_SetCount));
    assert(isPowerOf2(m_WayCount));
    assert(isPowerOf2(m_Banks));

    m_LineSize = line_size;
    m_NumLines = m_SetCount*m_WayCount*m_Banks;
    m_Capacity = m_NumLines*m_LineSize;

    m_Policy = policy;
    m_OffsetBits = intLog2(m_LineSize);
    m_SetBits = intLog2(m_SetCount);
    m_SetBitsStart = m_OffsetBits;
    m_BankBits = intLog2(m_Banks);
    m_BankBitsStart = m_SetBitsStart + m_SetBits;
    m_TagBits = (max_addr_bit+1) - m_BankBits - m_SetBits - m_OffsetBits;
    m_TagBitsStart = m_BankBitsStart + m_BankBits;


    InitStore();

    // Statistics Init

    m_HitCount = 0;
    m_MissCount = 0;

}

Cache::~Cache()
{
}

CacheAccessInfo Cache::Load(ReadAccess load)
{
    printf("LOAD()\n");
    CacheAccessInfo access_info;

    ACCESS_RESULT access_res = CheckHit(load.m_Addr); // not dealing with any un-aligned loads for now.
    access_info.m_Result = access_res;
    if (access_res == ACCESS_RESULT::HIT) // probably want to insert an "UpdatePolicyHit()" type function
        access_info.m_Effects = ACCESS_EFFECTS::NONE; 
    else
    {
        ReplacementResult result =  AllocateLine(load.m_Addr, false);
        access_info.m_Evicted = result;
    }

    return access_info;

}

CacheAccessInfo Cache::Store(WriteAccess store)
{
    CacheAccessInfo access_info;

    ACCESS_RESULT access_res = CheckHit(store.m_Addr); // not dealing with any un-aligned loads for now.
    access_info.m_Result = access_res;
    if (access_res == ACCESS_RESULT::HIT) // probably want to insert an "UpdatePolicyHit()" type function
        access_info.m_Effects = ACCESS_EFFECTS::NONE; 
    else
    {
        ReplacementResult result =  AllocateLine(store.m_Addr, true);
        access_info.m_Evicted = result;
    }

    return access_info;

}

void Cache::InitStore()
{
    m_CacheStore.resize(m_Banks);
    for (uint32_t b = 0; b < m_Banks; ++b)
    {
        m_CacheStore[b].resize(m_SetCount);

        for (uint32_t s = 0; s < m_SetCount; ++s)
        {
            m_CacheStore[b][s].resize(m_WayCount);
        }
    }
}


/*
    May want to return the whole line in a copy,
    depending on what we do later
*/
ACCESS_RESULT Cache::CheckHit(uint64_t addr)
{
    uint64_t tag = BIT_EXTRACT(addr, m_TagBitsStart, m_TagBits);
    uint32_t set = BIT_EXTRACT(addr, m_SetBitsStart, m_SetBits);
    uint32_t bank = BIT_EXTRACT(addr, m_BankBitsStart, m_BankBits);

    std::vector<CacheLineStore>& cache_set = m_CacheStore[bank][set];

    for (auto& line: cache_set) {
        if (line.m_Tag == tag && line.m_Valid)
            return ACCESS_RESULT::HIT;
    }

    return ACCESS_RESULT::MISS_ALLOCATE;
}

ReplacementResult Cache::AllocateLine(uint64_t addr, bool write)
{
    ReplacementResult result;
    switch (m_Policy)
    {
        case REPLACEMENT_POLICY::RANDOM: result = ReplaceRandom(addr, write); break;
        case REPLACEMENT_POLICY::LRU:
        default:
        {
            printf("Policy %d not yet implemented.\n", m_Policy);
            exit(-1);
        }
    }
    return result;
}

ReplacementResult Cache::ReplaceRandom(uint64_t addr, bool write)
{
    ReplacementResult  result;
    uint64_t tag = BIT_EXTRACT(addr, m_TagBitsStart, m_TagBits);
    uint32_t set = BIT_EXTRACT(addr, m_SetBitsStart, m_SetBits);
    uint32_t bank = BIT_EXTRACT(addr, m_BankBitsStart, m_BankBits);

    std::vector<CacheLineStore>& cache_set = m_CacheStore[bank][set];
    uint32_t way = rand() % cache_set.size();
    
    auto& EvictLine = cache_set[way];

    result.m_Evicted = EvictLine; // maybe only need to copy the address here
    result.m_Effect = EvictLine.m_Dirty ? ACCESS_EFFECTS::EVICT_DIRTY : ACCESS_EFFECTS::EVICT_CLEAN;

    cache_set[way].m_Tag = tag;
    cache_set[way].m_Dirty = write;
    cache_set[way].m_Valid = true;
    cache_set[way].m_Region = 0;
    cache_set[way].m_Addr = BIT_ZEROBOTTOM_N(addr, m_OffsetBits);

    return result;
}

class TLB
{

};

class Prefetcher // abstract interface
{
public:
    Prefetcher();
    ~Prefetcher();
    virtual void Snoop() = 0;
    virtual ReadAccess IssueRequest() = 0;
private:

};

class MultiNextLinePrefetcher : public Prefetcher
{

};

class MemoryModel
{
public:
    MemoryModel();
    MemoryModel(const std::vector<Cache> cache_hierarchy, std::vector<std::pair<int, Prefetcher>> prefetchers, TLB tlb_model);
    ~MemoryModel();

    void Load(uint64_t addr, uint64_t size, int level);
    void Store(uint64_t addr, uint64_t size, int level);
private:
    std::vector<Cache*> m_Cache;

};
/*
    We also want to insert pre-fetcher models, TLB model
*/


MemoryModel* g_MemoryModel;

MemoryModel::MemoryModel()
{
    printf("MemoryModel()\n");
    for (int i = 0; i < 2; i++)
        m_Cache.push_back(new Cache(64, 256*(i+1), 4, 1, REPLACEMENT_POLICY::RANDOM));
}

MemoryModel::MemoryModel(const std::vector<Cache> cache_hierarchy, std::vector<std::pair<int, Prefetcher>> prefetchers, TLB tlb_model)
{
    
}

void MemoryModel::Load(uint64_t addr, uint64_t size, int level)
{
    assert(level < m_Cache.size());
    bool last_level = m_Cache.size()-1 == level;
    auto res = m_Cache[level]->Load({addr, size});
    if (res.m_Result == ACCESS_RESULT::HIT)
    {
        printf("[Cache%d] LOAD HIT 0x%llx\n",level, addr);
    }
    else
    {
        printf("[Cache%d] LOAD MISS 0x%llx\n", level, addr);
        if (res.m_Effects == ACCESS_EFFECTS::EVICT_DIRTY && !last_level)
            Store(res.m_Evicted.m_Evicted.m_Addr, 64, level+1);
        if (!last_level)
            Load(addr, 64, level+1); // we maintain inclusivity. 
    }
}

void MemoryModel::Store(uint64_t addr, uint64_t size, int level)
{
    assert(level < m_Cache.size());
    bool last_level = m_Cache.size()-1 == level;
    auto res = m_Cache[level]->Store({addr, size});
    if (res.m_Result == ACCESS_RESULT::HIT)
    {
        printf("[Cache%d] STORE HIT 0x%llx\n", level, addr);
    }
    else
    {
        printf("[Cache%d] STORE MISS 0x%llx\n", level, addr);
        if (res.m_Effects == ACCESS_EFFECTS::EVICT_DIRTY && !last_level)
            Store(res.m_Evicted.m_Evicted.m_Addr, 64, level+1);
        if (!last_level)
            Load(addr, 64, level+1); // we maintain inclusivity. 
    }
}

extern "C"
void record_load(void* addr, uint64_t size)
{
    g_MemoryModel->Load((uint64_t)addr, size, 0);
}

extern "C"
void record_store(void* addr, uint64_t size)
{
    g_MemoryModel->Store((uint64_t)addr, size, 0);
}



extern "C"
void runtime_init()
{
    g_MemoryModel = new MemoryModel();
}

