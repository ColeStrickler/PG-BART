/*
    In this runtime we will dump the data for later analysis
*/


#include <fstream>
#include <vector>
#include <string>
#include <stdint.h>
#include "elf_info.hpp"

#include "base64.hpp"
#include "online_analyzer.hpp"

#define MAX_ADDR_BIT

uint64_t m_LoadBias;
OnlineAnalyzer* m_OnlineAnalyzer;
IPCAnalyzer* m_IPCAnalyzer;

struct Record {
    uint8_t type; // 2 bits
    uint64_t addr; // 48 bytes
    uint64_t pc; // 48 bytes
};



class RecordWriter {
public:
    RecordWriter(const std::string& file);
    ~RecordWriter();

    void WriteOut(const Record& record);
private:

    std::ofstream m_File;
};


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
    : m_File(file, std::ios::trunc)
{
    printf("CSVWRITER()\n");
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
    m_File.close();
}

void CSVWriter::WriteOut(const std::string &str)
{
    m_File << str << "\n";
}


RecordWriter::RecordWriter(const std::string &file) : m_File(file, std::ios::trunc)
{
    if (!m_File.is_open())
    {
        throw std::runtime_error("Failed to open CSV file");
    }
}

RecordWriter::~RecordWriter()
{
    m_File.close();
}

void RecordWriter::WriteOut(const Record &record)
{

}



CSVWriter* g_CSV_Writer = nullptr;



inline std::string load_to_csv(void* addr, void* pc) {
    return "l," + encodeBase64(reinterpret_cast<uint64_t>(addr)) + "," + encodeBase64(reinterpret_cast<uint64_t>(pc));
}

inline std::string store_to_csv(void* addr, void* pc) {
    return "s," + encodeBase64(reinterpret_cast<uint64_t>(addr)) + "," + encodeBase64(reinterpret_cast<uint64_t>(pc));
}

inline std::string instfetch_to_csv(void* addr, void* pc) {
    return "i," + encodeBase64(reinterpret_cast<uint64_t>(addr)) + "," + encodeBase64(reinterpret_cast<uint64_t>(pc));
}


extern "C"
void record_load(void* addr, void* pc) {
    pc = (void*)((uint64_t)__builtin_return_address(0) - m_LoadBias);
    m_OnlineAnalyzer->RunEvent(EVENT::READ, pc, addr);
}

extern "C"
void record_store(void* addr, void* pc)
{
    pc = (void*)((uint64_t)__builtin_return_address(0) - m_LoadBias);

    m_OnlineAnalyzer->RunEvent(EVENT::WRITE, pc, addr);
}


/*
    For now this i think is okay - if this is not accurate enough, we can pass BB_IDs and resolve later

    Our instrumentation shifts the addresses a bit, but I don't think this will matter???
*/


extern "C"
void bb_entry_callback(uint64_t bb_inst_count)
{
    void* pc = (void*)((uint64_t)__builtin_return_address(0) - m_LoadBias);
    m_OnlineAnalyzer->RunEvent(EVENT::INST, pc, (void*)bb_inst_count);
}



extern "C"
void function_entry_callback()
{
    void* pc = (void*)((uint64_t)__builtin_return_address(0) - m_LoadBias);
    m_OnlineAnalyzer->PushContext(EVENT::FUNC_ENTRY, pc);
}

extern "C"
void function_exit_callback()
{
    void* pc = (void*)((uint64_t)__builtin_return_address(0) - m_LoadBias);
    m_OnlineAnalyzer->PopContext(EVENT::FUNC_EXIT, pc);
}



extern "C"
void ipcfunc_enter()
{
    uint64_t pc = ((uint64_t)__builtin_return_address(0) - m_LoadBias);
    m_IPCAnalyzer->FuncEnter(pc);
}
extern "C"
void ipcfunc_exit()
{
    m_IPCAnalyzer->FuncExit();
}



__attribute__((constructor))
extern "C"
void runtime_init()
{
    m_LoadBias = compute_load_bias("./test/a.out");
    m_OnlineAnalyzer = new OnlineAnalyzer();
    m_IPCAnalyzer = new IPCAnalyzer();
    printf("RUNTIME INIT swag\n");
}



__attribute__((destructor))
extern "C"
void runtime_destroy()
{
    delete m_OnlineAnalyzer;
    delete m_IPCAnalyzer;
}
