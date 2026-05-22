#ifndef OFFLINE_ANALYZER_HPP
#define OFFLINE_ANALYZER_HPP


#include "cache.hpp"




REPLACEMENT_POLICY StringToReplacementPolicy(const std::string& str) {
    static std::unordered_map<std::string, REPLACEMENT_POLICY> mapping = {
        {"RANDOM", REPLACEMENT_POLICY::RANDOM},
        {"PLRU", REPLACEMENT_POLICY::PLRU},
        {"LRU", REPLACEMENT_POLICY::LRU},
        {"FIFO", REPLACEMENT_POLICY::FIFO},
    };

    assert(mapping.find(str) != mapping.end());

    return mapping.at(str);
};





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
    INST
};


class OfflineAnalyzer
{
public:
    OfflineAnalyzer();
    ~OfflineAnalyzer();

    /*
        ConsumeEvent() will read a single line from the offline data.csv
        parse it into an event, update the memoryh hierarchy model, and then
        update any relevant statistics

        ConsumeEvent() returns true if there are more lines to process

        ConsumeEvent returns false when there is no more lines to process
    */
    bool ConsumeEvent();


    std::string PrintStats() const;
private:
    // Statistics
    uint64_t m_DRAMWrites;
    uint64_t m_DRAMReads;
    uint64_t m_L2Reads;
    uint64_t m_L2Writes;

    
    // Methods
    CacheConfig GetCacheConfig(const std::string& cache) const;
    void DispatchRead(); 
    void DispatchWrite(); 
    void DispatchInstructionFetch();
    void InvalidateIfNecessary(uint64_t addr);

    // Members
    CSVLineReader* m_CSVReader;
    nlohmann::json m_Config;
    Cache* l1i;
    Cache* l1d;
    Cache* l2;
};









#endif