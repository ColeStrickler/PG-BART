#include "offline_analzyer.hpp"




#include <sstream>
#include <algorithm>

CSVLineReader::CSVLineReader(const std::string& filename) 
    : file_(filename) 
{
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    // Read header
    if (std::getline(file_, current_line_)) {
        line_number_ = 1;
        headers_ = split_csv_line(current_line_);
    }
}

CSVLineReader::~CSVLineReader() = default;

bool CSVLineReader::next() {
    if (!file_.good()) return false;
    
    if (std::getline(file_, current_line_)) {
        line_number_++;
        current_fields_ = split_csv_line(current_line_);
        return true;
    }
    return false;
}

std::string CSVLineReader::get(const std::string& column_name) const {
    auto it = std::find(headers_.begin(), headers_.end(), column_name);
    if (it == headers_.end()) {
        return "";  // or throw
    }
    size_t idx = std::distance(headers_.begin(), it);
    return idx < current_fields_.size() ? current_fields_[idx] : "";
}

std::string CSVLineReader::get(size_t column_index) const {
    return column_index < current_fields_.size() ? current_fields_[column_index] : "";
}

bool CSVLineReader::has_column(const std::string& column_name) const {
    return std::find(headers_.begin(), headers_.end(), column_name) != headers_.end();
}

std::vector<std::string> CSVLineReader::split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    bool in_quotes = false;
    
    for (char c : line) {
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ',' && !in_quotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    fields.push_back(field);  // last field
    return fields;
}







OfflineAnalyzer::OfflineAnalyzer() : m_L2Reads(0), m_L2Writes(0), m_DRAMReads(0), m_DRAMWrites(0)
{
    m_Config = OpenJSONFile("./MemorySimulationConfig.json");
    l1d = new Cache(GetCacheConfig("l1d"));
    l1i = new Cache(GetCacheConfig("l1i"));
    l2 = new Cache(GetCacheConfig("l2"));
    m_CSVReader = new CSVLineReader("./output_artifacts/MemoryAnalysis.csv");
    

}

OfflineAnalyzer::~OfflineAnalyzer()
{
}



#define EVENT2ENUM(event_type) \
    (event_type == "read" ? EVENT::READ : \
    (event_type == "inst" ? EVENT::INST : \
    (event_type == "write" ? EVENT::WRITE : \
    (assert(false && "EVENT2ENUM->Unknown event_type!"), EVENT::WRITE))))
bool OfflineAnalyzer::ConsumeEvent()
{
    if (!m_CSVReader->next())
        return false; // no more events to process
    std::string event_type = m_CSVReader->get("event");
    EVENT event = EVENT2ENUM(event_type);
    switch(event)
    {
        case EVENT::READ:   DispatchRead(); break;
        case EVENT::WRITE:  DispatchWrite(); break;
        case EVENT::INST:   DispatchInstructionFetch(); break;
        default:
        {
            printf("OfflineAnalyzer::ConsumeEvent() unrecognized event %s\n", event_type.c_str());
            exit(-1);
        }
    }

    return true;
}

std::string OfflineAnalyzer::PrintStats() const
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


CacheConfig OfflineAnalyzer::GetCacheConfig(const std::string &cache) const
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

void OfflineAnalyzer::DispatchRead()
{
#define L1D_LOAD(read_acc) l1d->Load(read_acc)
#define L2_LOAD(read_acc) l2->Load(read_acc); m_L2Reads++;
#define L2_WRITE(store_acc) l2->Store(store_acc); m_L2Writes++;
#define DRAM_LOAD() m_DRAMReads++
#define DRAM_WRITE() m_DRAMWrites++
#define EVICTED_2_WB(evicted) WriteAccess{.m_Addr=evicted.m_Addr, .m_StoreSize=64}


    /*
        Read from l1d
    */
    uint64_t load_addr = std::stoull(m_CSVReader->get("addr"));
    ReadAccess load_acc = {.m_Addr=load_addr, .m_LoadSize=8};
    auto l1d_res = L1D_LOAD(load_acc);
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
        auto& l2_evicted_1 = l2_res1.m_Evicted.m_Evicted;
        if (l2_evicted_1.m_Dirty && l2_evicted_1.m_Valid)
            DRAM_WRITE();
    }

    ReadAccess l2_load_acc = {.m_Addr=load_addr, .m_LoadSize=64};
    auto l2_load_res = L2_LOAD(l2_load_acc);

    if (l2_load_res.m_Effects == ACCESS_RESULT::HIT)
        return;


    /*
        Now handle misses/evictions at the l2 from the original load
    */

    auto& evicted_l2 = l2_load_res.m_Evicted.m_Evicted;
    if (evicted_l2.m_Dirty && evicted_l2.m_Valid)
    {
        auto l2_res2 = l2->Store(EVICTED_2_WB(evicted_l2));
        auto& l2_evicted_2 = l2_res2.m_Evicted.m_Evicted;
        if (l2_evicted_2.m_Dirty && l2_evicted_2.m_Valid)
            DRAM_WRITE();
    }

    // Since we missed in the L2, the original read now must go to DRAM
    DRAM_LOAD();

#undef L1D_LOAD
#undef L2_LOAD
#undef L2_WRITE
#undef DRAM_LOAD
#undef DRAM_WRITE
#undef EVICTED_2_WB
}

void OfflineAnalyzer::DispatchWrite()
{
#define L1D_WRITE(store_acc) l1d->Store(store_acc)
#define L2_LOAD(read_acc) l2->Load(read_acc); m_L2Reads++;
#define L2_WRITE(store_acc) l2->Store(store_acc); m_L2Writes++;
#define DRAM_LOAD() m_DRAMReads++
#define DRAM_WRITE() m_DRAMWrites++
#define EVICTED_2_WB(evicted) WriteAccess{.m_Addr=evicted.m_Addr, .m_StoreSize=64}


    /*
        Read from l1d
    */
    uint64_t load_addr = std::stoull(m_CSVReader->get("addr"));
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
        auto& l2_evicted_1 = l2_res1.m_Evicted.m_Evicted;
        if (l2_evicted_1.m_Dirty && l2_evicted_1.m_Valid)
            DRAM_WRITE();
    }

    ReadAccess l2_load_acc = {.m_Addr=load_addr, .m_LoadSize=64};
    auto l2_load_res = L2_LOAD(l2_load_acc);

    if (l2_load_res.m_Effects == ACCESS_RESULT::HIT)
        return;


    /*
        Now handle misses/evictions at the l2 from the original load
    */

    auto& evicted_l2 = l2_load_res.m_Evicted.m_Evicted;
    if (evicted_l2.m_Dirty && evicted_l2.m_Valid)
    {
        auto l2_res2 = l2->Store(EVICTED_2_WB(evicted_l2));
        auto& l2_evicted_2 = l2_res2.m_Evicted.m_Evicted;
        if (l2_evicted_2.m_Dirty && l2_evicted_2.m_Valid)
            DRAM_WRITE();
    }

    // Since we missed in the L2, the original read now must go to DRAM
    DRAM_LOAD();

#undef L1D_WRITE
#undef L2_LOAD
#undef L2_WRITE
#undef DRAM_LOAD
#undef DRAM_WRITE
#undef EVICTED_2_WB
}


void OfflineAnalyzer::DispatchInstructionFetch()
{
#define L1I_LOAD(read_acc) l1i->Load(read_acc)
#define L2_LOAD(read_acc) l2->Load(read_acc); m_L2Reads++;
#define L2_WRITE(store_acc) l2->Store(store_acc); m_L2Writes++;
#define DRAM_LOAD() m_DRAMReads++
#define DRAM_WRITE() m_DRAMWrites++
#define EVICTED_2_WB(evicted) WriteAccess{.m_Addr=evicted.m_Addr, .m_StoreSize=64}
    

    /*
        Read from l1d
    */
    uint64_t load_addr = std::stoull(m_CSVReader->get("pc")); // we use PC as the fetch addr
        
    ReadAccess load_acc = {.m_Addr=load_addr, .m_LoadSize=8};
    auto l1d_res = L1I_LOAD(load_acc);
    
    if (l1d_res.m_Result == ACCESS_RESULT::HIT) // If we hit, there is no evictions
        return;
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
        auto& l2_evicted_1 = l2_res1.m_Evicted.m_Evicted;
        if (l2_evicted_1.m_Dirty && l2_evicted_1.m_Valid)
            DRAM_WRITE();
    }

    ReadAccess l2_load_acc = {.m_Addr=load_addr, .m_LoadSize=64};
    auto l2_load_res = L2_LOAD(l2_load_acc);
    
    if (l2_load_res.m_Effects == ACCESS_RESULT::HIT)
        return;

    /*
        Now handle misses/evictions at the l2 from the original load
    */

    auto& evicted_l2 = l2_load_res.m_Evicted.m_Evicted;
    if (evicted_l2.m_Dirty && evicted_l2.m_Valid)
    {
        auto l2_res2 = l2->Store(EVICTED_2_WB(evicted_l2));
        auto& l2_evicted_2 = l2_res2.m_Evicted.m_Evicted;
        if (l2_evicted_2.m_Dirty && l2_evicted_2.m_Valid)
            DRAM_WRITE();
    }

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

int main()
{
    OfflineAnalyzer analysis;



    while(analysis.ConsumeEvent())
    {

    }


    std::cout << analysis.PrintStats() << std::endl;



    
}