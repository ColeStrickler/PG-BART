#include "cache.hpp"


static inline bool isPowerOf2(uint64_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

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

uint32_t intLog2(uint64_t x) {
    uint32_t log = 0;

    while (x >>= 1) {
        ++log;
    }

    return log;
}



Cache::Cache(CacheConfig conf) : Cache(conf.line_size, conf.sets, conf.ways, conf.banks, conf.policy)
{
    
}


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
    //printf("LOAD! tag %lld, set %d, bank %d --> addr 0x%llx\n", tag, set, bank, addr);
    std::vector<CacheLineStore>& cache_set = m_CacheStore[bank][set];

    for (auto& line: cache_set) {
        if (line.m_Tag == tag && line.m_Valid)
            return ACCESS_RESULT::HIT;
    }
   
    return ACCESS_RESULT::MISS_ALLOCATE;
}

void Cache::InvalidateIfNecessary(uint64_t addr)
{
    uint64_t tag = BIT_EXTRACT(addr, m_TagBitsStart, m_TagBits);
    uint32_t set = BIT_EXTRACT(addr, m_SetBitsStart, m_SetBits);
    uint32_t bank = BIT_EXTRACT(addr, m_BankBitsStart, m_BankBits);
    std::vector<CacheLineStore>& cache_set = m_CacheStore[bank][set];

    for (auto& line: cache_set) {
        if (line.m_Tag == tag && line.m_Valid)
            line.m_Valid = false;
    }

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

