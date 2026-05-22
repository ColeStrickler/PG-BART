/*
    In this runtime we will dump the data for later analysis
*/


#include <fstream>
#include <vector>
#include <string>
#include <stdint.h>

class CSVWriter
{
public:
    CSVWriter(const std::string& file, const std::vector<std::string>& labels);
    ~CSVWriter();


    void WriteOut(const std::string& str);
private:
    std::ofstream m_File;
};


CSVWriter::CSVWriter(const std::string& file,
                     const std::vector<std::string>& labels)
    : m_File(file, std::ios::app)
{
    //printf("here\n");
    if (!m_File.is_open())
    {
        throw std::runtime_error("Failed to open CSV file");
    }

    for (size_t i = 0; i < labels.size(); i++)
    {
        m_File << labels[i];

        if (i + 1 != labels.size())
            m_File << ",";
    }

    m_File << "\n";
}

CSVWriter::~CSVWriter()
{
}

void CSVWriter::WriteOut(const std::string &str)
{
    m_File << str << "\n";
}



CSVWriter* g_CSV_Writer;



inline std::string load_to_csv(void* addr, void* pc) {
    return "load," + std::to_string(reinterpret_cast<uint64_t>(addr)) + "," + std::to_string(reinterpret_cast<uint64_t>(pc));
}

inline std::string store_to_csv(void* addr, void* pc) {
    return "store," + std::to_string(reinterpret_cast<uint64_t>(addr)) + "," + std::to_string(reinterpret_cast<uint64_t>(pc));
}

inline std::string instfetch_to_csv(void* addr, void* pc) {
    return "inst," + std::to_string(reinterpret_cast<uint64_t>(addr)) + "," + std::to_string(reinterpret_cast<uint64_t>(pc));
}


extern "C"
void record_load(void* addr, void* pc)
{
        g_CSV_Writer->WriteOut(load_to_csv(addr, pc));
}

extern "C"
void record_store(void* addr, void* pc)
{
    g_CSV_Writer->WriteOut(store_to_csv(addr, pc));
}


/*
    For now this i think is okay - if this is not accurate enough, we can pass BB_IDs and resolve later

    Our instrumentation shifts the addresses a bit, but I don't think this will matter???
*/

static uint64_t last_pc = 0x00;

bool new_instfetch_block(void* pc) {
    uint64_t pc_int = reinterpret_cast<uint64_t>(pc);
    pc_int -= (pc_int%0x40); // cache line boundary
    uint64_t diff = last_pc > pc_int ? last_pc - pc_int : pc_int - last_pc;
   
    if (diff >= 0x40)
    {
        last_pc = pc_int;
        return true;
    }
    return false;
}


extern "C"
void bb_entry_callback(uint64_t bb_id)
{
    void* pc = __builtin_return_address(0);
    if (new_instfetch_block(pc))
        g_CSV_Writer->WriteOut(instfetch_to_csv(0x00, pc));

}

__attribute__((constructor))
extern "C"
void runtime_init()
{
    printf("CSV INIT\n");
    g_CSV_Writer = new CSVWriter("./output_artifacts/MemoryAnalysis.csv", {"event", "addr", "pc"});
}

