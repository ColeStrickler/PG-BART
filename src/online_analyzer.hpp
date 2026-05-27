#ifndef ONLINE_ANALYZER_HPP
#define ONLINE_ANALYZER_HPP


#include "cache.hpp"
#include "elf_info.hpp"
#include "base64.hpp"
#include <chrono>
#include <stack>
REPLACEMENT_POLICY StringToReplacementPolicy(const std::string& str);




class CSVLineReader {
public:
    explicit CSVLineReader(const std::string& filename);
    ~CSVLineReader();

    // Read next line and parse it
    bool next();

    // Get value by column name (after header is read)
    std::string get(const std::string& column_name) const;

    // Get value by column index
    std::string get(size_t column_index) const;

    // Check if column exists
    bool has_column(const std::string& column_name) const;

    // Get current raw line
    const std::string& current_line() const { return current_line_; }

    size_t line_number() const { return line_number_; }

private:
    std::ifstream file_;
    std::string current_line_;
    size_t line_number_ = 0;
    
    std::vector<std::string> headers_;
    std::vector<std::string> current_fields_;

    std::vector<std::string> split_csv_line(const std::string& line);
};



enum EVENT
{
    READ,
    WRITE,
    INST,
    FUNC_ENTRY,
    FUNC_EXIT
};


struct FuncResults
{
    float m_DRAMWritePerInst = 0.0f;
    float m_DRAMReadPerInst = 0.0f;
    float m_L2ReadPerInst = 0.0f;
    float m_L2WritesPerInst = 0.0f;

        // Pretty print
    friend std::ostream& operator<<(std::ostream& os, const FuncResults& r)
    {
        os << std::fixed << std::setprecision(4);
        os << "FuncResults {\n";
        os << "  DRAM Read / Inst   : " << r.m_DRAMReadPerInst   << "\n";
        os << "  DRAM Write / Inst  : " << r.m_DRAMWritePerInst  << "\n";
        os << "  L2 Read / Inst     : " << r.m_L2ReadPerInst     << "\n";
        os << "  L2 Write / Inst    : " << r.m_L2WritesPerInst   << "\n";
        os << "}";
        return os;
    }


    std::string Print(float ipc, float cpu_frequency_ghz)
    {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4);

        os << "FuncResults {\n";
        
        // Multiply each "Per Inst" metric by IPC
        os << "  DRAM Read / Inst   : " << (m_DRAMReadPerInst)   << "\n";
        os << "  DRAM Write / Inst  : " << (m_DRAMWritePerInst)  << "\n";
        os << "  L2 Read / Inst     : " << (m_L2ReadPerInst)     << "\n";
        os << "  L2 Write / Inst    : " << (m_L2WritesPerInst)   << "\n";
        // DRAM Read
        int bytes_per_access = 64;
        double dram_read_per_cycle = m_DRAMReadPerInst * ipc;
        double dram_read_bw = dram_read_per_cycle * bytes_per_access * cpu_frequency_ghz * 1000.0;
        os << "  DRAM Read / cycle   : " << dram_read_per_cycle << "\n";
        os << "  DRAM Read Bandwidth : " << dram_read_bw << " MB/s\n";

        // DRAM Write
        double dram_write_per_cycle = m_DRAMWritePerInst * ipc;
        double dram_write_bw = dram_write_per_cycle * bytes_per_access * cpu_frequency_ghz * 1000.0;
        os << "  DRAM Write / cycle  : " << dram_write_per_cycle << "\n";
        os << "  DRAM Write Bandwidth: " << dram_write_bw << " MB/s\n";

        // L2 Read
        double l2_read_per_cycle = m_L2ReadPerInst * ipc;
        double l2_read_bw = l2_read_per_cycle * bytes_per_access * cpu_frequency_ghz * 1000.0;
        os << "  L2 Read / cycle     : " << l2_read_per_cycle << "\n";
        os << "  L2 Read Bandwidth   : " << l2_read_bw << " MB/s\n";

        // L2 Write
        double l2_write_per_cycle = m_L2WritesPerInst * ipc;
        double l2_write_bw = l2_write_per_cycle * bytes_per_access * cpu_frequency_ghz * 1000.0;
        os << "  L2 Write / cycle    : " << l2_write_per_cycle << "\n";
        os << "  L2 Write Bandwidth  : " << l2_write_bw << " MB/s\n";
        os << "  (IPC = " << ipc << ")\n";
        os << "}";

        return os.str();
    }
};


struct FuncContext
{
    uint64_t m_DRAMWrites;
    uint64_t m_DRAMReads;
    uint64_t m_L2Reads;
    uint64_t m_L2Writes;
    uint64_t m_InstructionsExecuted;
    std::string m_FuncName;
};


class OnlineAnalyzer
{
public:
    OnlineAnalyzer();
    ~OnlineAnalyzer();

    /*
        ConsumeEvent() will read a single line from the offline data.csv
        parse it into an event, update the memoryh hierarchy model, and then
        update any relevant statistics

        ConsumeEvent() returns true if there are more lines to process

        ConsumeEvent returns false when there is no more lines to process
    */
    void RunEvent(EVENT type, void* pc, void* addr = 0x0);

    void PushContext(EVENT type, void* pc);
    void PopContext(EVENT type, void* pc);
    void LogContextResults(FuncContext& ctx);
    FuncContext& GetCurrentContext();

    




    std::unordered_map<std::string, uint64_t> m_FuncDRAMInfo;
    std::unordered_map<std::string, uint64_t> m_FuncL2Info;
private:
    void* m_PC;
    void* m_CurrMemAddr;
    const FunctionInfo* m_CurrentFunc;
    // Statistics
    std::vector<FunctionInfo> m_ElfBinaryFunctionInfo;
    std::stack<FuncContext> m_ContextStack;
    std::unordered_map<std::string, FuncResults> m_FunctionResults;


    //std::unordered_map<std::string, uint64_t> m_FuncMemoryCount;
    
    // Methods
    CacheConfig GetCacheConfig(const std::string& cache) const;
    void DispatchRead(); 
    void DispatchWrite(); 
    void DispatchInstructionFetch();
    void InvalidateIfNecessary(uint64_t addr);

    // Members
    nlohmann::json m_Config;
    Cache* l1i;
    Cache* l1d;
    Cache* l2;
};









#endif