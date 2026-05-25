#include "online_analyzer.hpp"




#include <sstream>
#include <algorithm>

REPLACEMENT_POLICY StringToReplacementPolicy(const std::string& str)
 {
    static std::unordered_map<std::string, REPLACEMENT_POLICY> mapping = {
        {"RANDOM", REPLACEMENT_POLICY::RANDOM},
        {"PLRU", REPLACEMENT_POLICY::PLRU},
        {"LRU", REPLACEMENT_POLICY::LRU},
        {"FIFO", REPLACEMENT_POLICY::FIFO},
    };

    assert(mapping.find(str) != mapping.end());

    return mapping.at(str);
};


OnlineAnalyzer::OnlineAnalyzer() : m_L2Reads(0), m_L2Writes(0), m_DRAMReads(0), m_DRAMWrites(0)
{
    m_Config = OpenJSONFile("./MemorySimulationConfig.json");
    l1d = new Cache(GetCacheConfig("l1d"));
    l1i = new Cache(GetCacheConfig("l1i"));
    l2 = new Cache(GetCacheConfig("l2"));
    //m_CSVReader = new CSVLineReader("./output_artifacts/MemoryAnalysis.csv");




    m_ElfBinaryFunctionInfo = loadFunctions("./test/a.out");
    std::sort(
        m_ElfBinaryFunctionInfo.begin(),
        m_ElfBinaryFunctionInfo.end(),
        [](const FunctionInfo& a, const FunctionInfo& b)
        {
            return a.start < b.start;
        }
    );



}



OnlineAnalyzer::~OnlineAnalyzer()
{
    uint64_t sumDRAM = 0, sumL2 = 0;
for (auto& p : m_FuncDRAMInfo) sumDRAM += p.second;
for (auto& p : m_FuncL2Info)   sumL2   += p.second;

std::cout << "\n=== SUM CHECK ===\n";
std::cout << "Sum of all functions DRAM = " << sumDRAM << "  (global DRAM = " << (m_DRAMReads + m_DRAMWrites) << ")\n";
std::cout << "Sum of all functions L2   = " << sumL2   << "  (global L2   = " << (m_L2Reads + m_L2Writes) << ")\n";


    std::cout << PrintStats() << "\n";
    for (auto& m: m_FunctionResults)
    {
        std::cout << m.first << ": " << m.second << std::endl;
    }

}





void OnlineAnalyzer::RunEvent(EVENT type, void *pc, void *addr)
{
    m_PC = pc;
    m_CurrMemAddr = addr;
    m_CurrentFunc = findFunction(m_ElfBinaryFunctionInfo, (uint64_t)pc);
  
    switch(type)
    {
        case EVENT::READ:    DispatchRead(); break;
        case EVENT::WRITE:   DispatchWrite(); break;
        case EVENT::INST:    DispatchInstructionFetch(); break;
        default:
        {
            printf("OnlineAnalyzer::RunEvent() unrecognized event %d\n", type);
            exit(-1);
        }
    }
}

void OnlineAnalyzer::PushContext(EVENT type, void *pc)
{
    const FunctionInfo* fInfo = findFunction(m_ElfBinaryFunctionInfo, (uint64_t)pc);
    if (!fInfo)
        return;
    
    m_ContextStack.push({0, 0, 0, 0, 0, fInfo->name});
}

void OnlineAnalyzer::PopContext(EVENT type, void *pc)
{
    const FunctionInfo* fInfo = findFunction(m_ElfBinaryFunctionInfo, (uint64_t)pc);
    if (!fInfo)
        return;
    
    LogContextResults(GetCurrentContext());
    m_ContextStack.pop();
}

void OnlineAnalyzer::LogContextResults(FuncContext &ctx)
{
    if (ctx.m_InstructionsExecuted == 0)
        return;  // avoid division by zero

    auto& results = m_FunctionResults[ctx.m_FuncName];

    float inst = static_cast<float>(ctx.m_InstructionsExecuted);

    results.m_DRAMReadPerInst   = std::max(results.m_DRAMReadPerInst,
                                           static_cast<float>(ctx.m_DRAMReads) / inst);
    results.m_DRAMWritePerInst  = std::max(results.m_DRAMWritePerInst,
                                           static_cast<float>(ctx.m_DRAMWrites) / inst);
    results.m_L2ReadPerInst     = std::max(results.m_L2ReadPerInst,
                                           static_cast<float>(ctx.m_L2Reads) / inst);
    results.m_L2WritesPerInst   = std::max(results.m_L2WritesPerInst,
                                           static_cast<float>(ctx.m_L2Writes) / inst);
}


FuncContext &OnlineAnalyzer::GetCurrentContext()
{
    static FuncContext dummy;
    if (!m_ContextStack.size())
        return dummy;
    return m_ContextStack.top();
}

std::string OnlineAnalyzer::PrintStats() const
{
    std::string stats = "=== Offline Analyzer Statistics ===\n";
    
    stats += "L2 Reads     : " + std::to_string(m_L2Reads) + "\n";
    stats += "L2 Writes    : " + std::to_string(m_L2Writes) + "\n";
    stats += "DRAM Reads   : " + std::to_string(m_DRAMReads) + "\n";
    stats += "DRAM Writes  : " + std::to_string(m_DRAMWrites) + "\n";
    
    // Optional: Add totals
    uint64_t total_accesses = m_L2Reads + m_L2Writes + m_DRAMReads + m_DRAMWrites;
    stats += "Total Accesses : " + std::to_string(total_accesses) + "\n";
    
    return stats;
}


CacheConfig OnlineAnalyzer::GetCacheConfig(const std::string &cache) const
{
    return CacheConfig{
        .line_size =    m_Config["cache_hierarchy"][cache]["line_size"],
        .sets =         m_Config["cache_hierarchy"][cache]["sets"],
        .ways =         m_Config["cache_hierarchy"][cache]["ways"],
        .banks =        m_Config["cache_hierarchy"][cache]["banks"],
        .policy = StringToReplacementPolicy(m_Config["cache_hierarchy"][cache]["policy"])
    };
}



/*
We need to set up a policy to maintain inclusivity,


And to make sure we do not evict from L2 if a line is in L1, or to at least handle it properly
*/





void OnlineAnalyzer::DispatchRead()
{
#define L1D_LOAD(read_acc) l1d->Load(read_acc)
#define L2_LOAD(read_acc) l2->Load(read_acc); m_L2Reads++; GetCurrentContext().m_L2Reads++;
#define L2_WRITE(store_acc) l2->Store(store_acc); m_L2Writes++; GetCurrentContext().m_L2Writes++;
#define DRAM_LOAD()  GetCurrentContext().m_DRAMReads++;
#define DRAM_WRITE()  GetCurrentContext().m_DRAMWrites++;
#define EVICTED_2_WB(evicted) WriteAccess{.m_Addr=evicted.m_Addr, .m_StoreSize=64}


    /*
        Read from l1d
    */
    uint64_t load_addr = (uint64_t)(m_CurrMemAddr);



    ReadAccess load_acc = {.m_Addr=load_addr, .m_LoadSize=8};
    auto l1d_res = L1D_LOAD(load_acc);
    uint64_t pc_addr = (uint64_t)(m_PC);
    auto func = findFunction(m_ElfBinaryFunctionInfo, pc_addr);
    if (l1d_res.m_Result == ACCESS_RESULT::HIT) // If we hit, there is no evictions
    {

        return;
    }
    //printf("l1 miss\n");
    auto& evicted = l1d_res.m_Evicted.m_Evicted;
    

    /*
        If we had to allocate in l1d, we need to:
        1. check if we evicted a dirty line
            1a) if so, write back
            1b) if write back evicts line, write back the l2 evicted line to DRAM
        2. issue a read for the original request to the l2
    */
    if (evicted.m_Dirty && evicted.m_Valid)
    {
        auto l2_res1 = L2_WRITE(EVICTED_2_WB(evicted));
        if (l2_res1.m_Result == ACCESS_RESULT::MISS_ALLOCATE)
            assert(false);
        

        auto& l2_evicted_1 = l2_res1.m_Evicted.m_Evicted;
        if (l2_evicted_1.m_Dirty && l2_evicted_1.m_Valid)
            DRAM_WRITE();
        InvalidateIfNecessary(l2_evicted_1.m_Addr);
        
    }

    ReadAccess l2_load_acc = {.m_Addr=load_addr, .m_LoadSize=64};
   
    auto l2_load_res = L2_LOAD(l2_load_acc);

    if (l2_load_res.m_Result == ACCESS_RESULT::HIT)
        return;
    

 //   printf("L2 MISS\n");
    /*
        Now handle misses/evictions at the l2 from the original load
    */

    auto& evicted_l2 = l2_load_res.m_Evicted.m_Evicted;
    if (evicted_l2.m_Dirty && evicted_l2.m_Valid)
        DRAM_WRITE();

    InvalidateIfNecessary(evicted_l2.m_Addr);
    // Since we missed in the L2, the original read now must go to DRAM

    DRAM_LOAD();

#undef L1D_LOAD
#undef L2_LOAD
#undef L2_WRITE
#undef DRAM_LOAD
#undef DRAM_WRITE
#undef EVICTED_2_WB
}




void OnlineAnalyzer::DispatchWrite()
{
#define L1D_WRITE(store_acc) l1d->Store(store_acc)
#define L2_LOAD(read_acc) l2->Load(read_acc); m_L2Reads++; GetCurrentContext().m_L2Reads++;
#define L2_WRITE(store_acc) l2->Store(store_acc); m_L2Writes++; GetCurrentContext().m_L2Writes++;
#define DRAM_LOAD()  GetCurrentContext().m_DRAMReads++;
#define DRAM_WRITE()  GetCurrentContext().m_DRAMWrites++;
#define EVICTED_2_WB(evicted) WriteAccess{.m_Addr=evicted.m_Addr, .m_StoreSize=64}

   // printf("write()\n");
    /*
        Read from l1d
    */
    uint64_t load_addr =  (uint64_t)(m_CurrMemAddr);
    uint64_t pc_addr = (uint64_t)(m_PC);
    auto func = findFunction(m_ElfBinaryFunctionInfo, pc_addr);


    WriteAccess load_acc = {.m_Addr=load_addr, .m_StoreSize=8};
    auto l1d_res = L1D_WRITE(load_acc);
    if (l1d_res.m_Result == ACCESS_RESULT::HIT) // If we hit, there is no evictions
        return;

    auto& evicted = l1d_res.m_Evicted.m_Evicted;
    

    /*
        If we had to allocate in l1d, we need to:
        1. check if we evicted a dirty line
            1a) if so, write back
            1b) if write back evicts line, write back the l2 evicted line to DRAM
        2. issue a read for the original request to the l2
    */

    if (evicted.m_Dirty && evicted.m_Valid)
    {
        auto l2_res1 = L2_WRITE(EVICTED_2_WB(evicted));
        if (l2_res1.m_Result == ACCESS_RESULT::MISS_ALLOCATE)
            assert(false);
        
        auto& l2_evicted_1 = l2_res1.m_Evicted.m_Evicted;
        if (l2_evicted_1.m_Dirty && l2_evicted_1.m_Valid)
            DRAM_WRITE();
        InvalidateIfNecessary(l2_evicted_1.m_Addr);
    }

    ReadAccess l2_load_acc = {.m_Addr=load_addr, .m_LoadSize=64};
    auto l2_load_res = L2_LOAD(l2_load_acc);

    if (l2_load_res.m_Result == ACCESS_RESULT::HIT)
        return;


    /*
        Now handle misses/evictions at the l2 from the original load
    */


    auto& evicted_l2 = l2_load_res.m_Evicted.m_Evicted;
    if (evicted_l2.m_Dirty && evicted_l2.m_Valid)
        DRAM_WRITE();
    InvalidateIfNecessary(evicted_l2.m_Addr);

    // Since we missed in the L2, the original read now must go to DRAM
    DRAM_LOAD();


#undef L1D_WRITE
#undef L2_LOAD
#undef L2_WRITE
#undef DRAM_LOAD
#undef DRAM_WRITE
#undef EVICTED_2_WB
}


void OnlineAnalyzer::DispatchInstructionFetch()
{
#define L1I_LOAD(read_acc) l1i->Load(read_acc)
#define L2_LOAD(read_acc) l2->Load(read_acc); m_L2Reads++; GetCurrentContext().m_L2Reads++;
#define L2_WRITE(store_acc) l2->Store(store_acc); m_L2Writes++; GetCurrentContext().m_L2Writes++;
#define DRAM_LOAD()  GetCurrentContext().m_DRAMReads++;
#define DRAM_WRITE()  GetCurrentContext().m_DRAMWrites++;
#define EVICTED_2_WB(evicted) WriteAccess{.m_Addr=evicted.m_Addr, .m_StoreSize=64}


    /*
        For inst fetch, we store the basic block inst count in the m_CurrADdr

    */
    GetCurrentContext().m_InstructionsExecuted += (uint64_t)m_CurrMemAddr;
    
    /*
        Read from l1i
    */
    uint64_t load_addr = (uint64_t)(m_PC); // we use PC as the fetch addr
    auto func = findFunction(m_ElfBinaryFunctionInfo, load_addr);


    ReadAccess load_acc = {.m_Addr=load_addr, .m_LoadSize=8};
    auto l1d_res = L1I_LOAD(load_acc);
    
    if (l1d_res.m_Result == ACCESS_RESULT::HIT) // If we hit, there is no evictions
    {
        return;
    }
    auto& evicted = l1d_res.m_Evicted.m_Evicted;
    
    assert(!evicted.m_Dirty);

    /*
        If we had to allocate in l1d, we need to:
        1. check if we evicted a dirty line
            1a) if so, write back
            1b) if write back evicts line, write back the l2 evicted line to DRAM
        2. issue a read for the original request to the l2
    */

    if (evicted.m_Dirty && evicted.m_Valid)
    {
        auto l2_res1 = L2_WRITE(EVICTED_2_WB(evicted));
        assert(l2_res1.m_Result == ACCESS_RESULT::HIT);


        auto& l2_evicted_1 = l2_res1.m_Evicted.m_Evicted;
        if (l2_evicted_1.m_Dirty && l2_evicted_1.m_Valid)
            DRAM_WRITE();
        InvalidateIfNecessary(l2_evicted_1.m_Addr);
    }

    ReadAccess l2_load_acc = {.m_Addr=load_addr, .m_LoadSize=64};
    auto l2_load_res = L2_LOAD(l2_load_acc);
    
    if (l2_load_res.m_Result == ACCESS_RESULT::HIT)
    {

        return;
    }

    /*
        Now handle misses/evictions at the l2 from the original load
    */

    auto& evicted_l2 = l2_load_res.m_Evicted.m_Evicted;
    if (evicted_l2.m_Dirty && evicted_l2.m_Valid)
        DRAM_WRITE();
    InvalidateIfNecessary(evicted_l2.m_Addr);

    // Since we missed in the L2, the original read now must go to DRAM
    DRAM_LOAD();


#undef L1D_LOAD
#undef L1D_WRITE
#undef L2_LOAD
#undef L2_WRITE
#undef DRAM_LOAD
#undef DRAM_WRITE
#undef EVICTED_2_WB
}

void OnlineAnalyzer::InvalidateIfNecessary(uint64_t addr)
{
    l1d->InvalidateIfNecessary(addr);
    l1i->InvalidateIfNecessary(addr);
}
