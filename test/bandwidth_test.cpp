#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <chrono>

int main() {
    constexpr size_t ARRAY_SIZE_BYTES = 512ULL * 1024 * 1024; // 512 MB
    constexpr size_t ELEMENT_SIZE = sizeof(uint64_t);
    constexpr size_t NUM_ELEMENTS = ARRAY_SIZE_BYTES / ELEMENT_SIZE;

    std::vector<uint64_t> data(NUM_ELEMENTS, 1);

    constexpr size_t STRIDE_BYTES = 64;
    constexpr size_t STRIDE_ELEMENTS = STRIDE_BYTES / ELEMENT_SIZE;

    volatile uint64_t sum = 0;

    auto start = std::chrono::high_resolution_clock::now();


    int x = 0;

    while (x < 10)
    {
        for (size_t i = 0; i < NUM_ELEMENTS; i += STRIDE_ELEMENTS) {
            sum += data[i];
        }
        x++;
    }


    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Sum: " << sum << "\n";
    std::cout << "Time: " << elapsed.count() << " us\n";

    return 0;
}