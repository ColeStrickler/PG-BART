#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

struct Record
{
    int id;
    int category;
    double value;
    double weight;
};

class DataGenerator
{
public:
    DataGenerator(size_t count)
        : m_Count(count),
          m_Rng(std::random_device{}()),
          m_ValueDist(0.0, 1000.0),
          m_CategoryDist(0, 15)
    {
    }

    std::vector<Record> Generate()
    {
        std::vector<Record> data;
        data.reserve(m_Count);

        for (size_t i = 0; i < m_Count; i++)
        {
            Record r;
            r.id = static_cast<int>(i);
            r.category = m_CategoryDist(m_Rng);
            r.value = m_ValueDist(m_Rng);
            r.weight = std::sin(r.value) + std::cos(r.value / 10.0);

            data.push_back(r);
        }

        return data;
    }

private:
    size_t m_Count;

    std::mt19937 m_Rng;
    std::uniform_real_distribution<double> m_ValueDist;
    std::uniform_int_distribution<int> m_CategoryDist;
};

class Statistics
{
public:
    static double Mean(const std::vector<double>& values)
    {
        double sum = std::accumulate(values.begin(),
                                     values.end(),
                                     0.0);

        return sum / static_cast<double>(values.size());
    }

    static double StdDev(const std::vector<double>& values)
    {
        double mean = Mean(values);

        double accum = 0.0;

        for (double v : values)
        {
            double diff = v - mean;
            accum += diff * diff;
        }

        return std::sqrt(accum / values.size());
    }
};

class Analyzer
{
public:
    void Analyze(std::vector<Record>& data)
    {
        Normalize(data);
        HeavyTransform(data);
        SortByCategory(data);
        Aggregate(data);
    }

private:
    void Normalize(std::vector<Record>& data)
    {
        double maxVal = 0.0;

        for (const auto& r : data)
        {
            maxVal = std::max(maxVal, r.value);
        }

        for (auto& r : data)
        {
            r.value /= maxVal;
        }
    }

    void HeavyTransform(std::vector<Record>& data)
    {
        for (size_t iter = 0; iter < 10; iter++)
        {
            for (auto& r : data)
            {
                double x = r.value;

                for (int k = 0; k < 20; k++)
                {
                    x = std::sin(x * 3.14159)
                      + std::cos(x * 0.25)
                      + std::sqrt(std::abs(x) + 1.0);
                }

                r.weight = x;
            }
        }
    }

    void SortByCategory(std::vector<Record>& data)
    {
        std::sort(data.begin(),
                  data.end(),
                  [](const Record& a, const Record& b)
                  {
                      if (a.category == b.category)
                          return a.weight < b.weight;

                      return a.category < b.category;
                  });
    }

    void Aggregate(const std::vector<Record>& data)
    {
        std::unordered_map<int, std::vector<double>> groups;

        for (const auto& r : data)
        {
            groups[r.category].push_back(r.weight);
        }

        for (const auto& [category, values] : groups)
        {
            double mean = Statistics::Mean(values);
            double stddev = Statistics::StdDev(values);

            m_CategoryMeans[category] = mean;
            m_CategoryStddevs[category] = stddev;
        }
    }

public:
    void PrintResults() const
    {
        for (const auto& [category, mean] : m_CategoryMeans)
        {
            auto it = m_CategoryStddevs.find(category);

            std::cout
                << "Category: " << category
                << " Mean: " << mean
                << " StdDev: " << it->second
                << "\n";
        }
    }

private:
    std::unordered_map<int, double> m_CategoryMeans;
    std::unordered_map<int, double> m_CategoryStddevs;
};

class FileWriter
{
public:
    explicit FileWriter(const std::string& path)
        : m_Out(path)
    {
    }

    void Write(const std::vector<Record>& data)
    {
        for (const auto& r : data)
        {
            m_Out
                << r.id << ","
                << r.category << ","
                << r.value << ","
                << r.weight << "\n";
        }
    }

private:
    std::ofstream m_Out;
};

int main()
{
    printf("MAIN()\n");
    constexpr size_t RecordCount = 500000;

    auto start =
        std::chrono::high_resolution_clock::now();

    DataGenerator generator(RecordCount);

    std::vector<Record> data =
        generator.Generate();

    Analyzer analyzer;

    analyzer.Analyze(data);

    analyzer.PrintResults();

    FileWriter writer("results.csv");

    writer.Write(data);

    auto end =
        std::chrono::high_resolution_clock::now();

    auto ms =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(end - start);

    std::cout
        << "Completed in "
        << ms.count()
        << " ms\n";

    return 0;
}