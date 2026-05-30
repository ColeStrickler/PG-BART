#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>


#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <cstdint>
#include <cstdio>
#include <sys/ioctl.h>     // ← ADD THIS for ioctl()
#include <stdint.h>
#include <x86intrin.h>   // for __rdtsc() and __rdtscp()

// ============== CYCLES ==============
static inline uint64_t read_cycle(void)
{
    uint64_t cycles;
#if defined(__x86_64__)
    // Best practice: serialize with lfence
    __asm__ __volatile__("lfence" ::: "memory");
    cycles = __rdtsc();
    __asm__ __volatile__("lfence" ::: "memory");
#else
#error "Unsupported architecture"
#endif
    return cycles;
}

// ====================== PERF EVENT SETUP ======================
static int perf_fd = -1;

static void init_perf_instret()
{
    struct perf_event_attr pe = {};
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.exclude_kernel = 0;   // include kernel if you want
    pe.exclude_hv = 1;

    perf_fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    if (perf_fd < 0) {
        perror("perf_event_open failed");
        printf("Try: sudo sysctl kernel.perf_event_paranoid=-1\n");
    }
}

static inline uint64_t read_instret(void)
{
    if (perf_fd < 0) return 0;

    uint64_t count = 0;
    if (read(perf_fd, &count, sizeof(count)) == sizeof(count)) {
        return count;
    }
    return 0;
}

static inline void start_instret()
{
    if (perf_fd >= 0) ioctl(perf_fd, PERF_EVENT_IOC_RESET, 0);
    if (perf_fd >= 0) ioctl(perf_fd, PERF_EVENT_IOC_ENABLE, 0);
}

static inline void stop_instret()
{
    if (perf_fd >= 0) ioctl(perf_fd, PERF_EVENT_IOC_DISABLE, 0);
}




using Matrix = std::vector<std::vector<double>>;

// Generate random matrix
void init_matrix(Matrix& A, int n) {


    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            A[i][j] = dist(rng);
        }
    }


}

// Naive matrix multiplication (for reference)
void matmul_naive(const Matrix& A, const Matrix& B, Matrix& C, int n) {


    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            for (int k = 0; k < n; ++k) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }


}

// Tiled (Blocked) Matrix Multiplication
void matmul_tiled(const Matrix& A, const Matrix& B, Matrix& C, int n, int tile_size) {

    for (int i = 0; i < n; i += tile_size) {
        for (int j = 0; j < n; j += tile_size) {
            for (int k = 0; k < n; k += tile_size) {
                
                // Tile computation
                for (int ii = i; ii < std::min(i + tile_size, n); ++ii) {
                    for (int jj = j; jj < std::min(j + tile_size, n); ++jj) {
                        for (int kk = k; kk < std::min(k + tile_size, n); ++kk) {
                            C[ii][jj] += A[ii][kk] * B[kk][jj];
                        }
                    }
                }
            }
        }
    }


}

// ====================== MATRIX TRANSPOSE ======================
void transpose_naive(const std::vector<std::vector<double>>& A,
                     std::vector<std::vector<double>>& B,
                     int N)
{
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            B[j][i] = A[i][j];        // Bad access pattern on A
        }
    }
}

void transpose_tiled(const std::vector<std::vector<double>>& A,
                     std::vector<std::vector<double>>& B,
                     int N, int tile_size = 32)
{
    for (int i = 0; i < N; i += tile_size) {
        for (int j = 0; j < N; j += tile_size) {
            for (int ii = i; ii < std::min(i + tile_size, N); ++ii) {
                for (int jj = j; jj < std::min(j + tile_size, N); ++jj) {
                    B[jj][ii] = A[ii][jj];
                }
            }
        }
    }
}

void matmul_transposed_naive(const std::vector<std::vector<double>>& A,
                             const std::vector<std::vector<double>>& B,
                             std::vector<std::vector<double>>& C,
                             int N)
{
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            double sum = 0.0;
            for (int k = 0; k < N; ++k) {
                sum += A[i][k] * B[j][k];   // B is accessed transposed
            }
            C[i][j] = sum;
        }
    }
}

// Tiled + Transposed (usually best cache behavior)
void matmul_transposed_tiled(const std::vector<std::vector<double>>& A,
                             const std::vector<std::vector<double>>& B,
                             std::vector<std::vector<double>>& C,
                             int N, int tile_size = 32)
{
    for (int i = 0; i < N; i += tile_size) {
        for (int j = 0; j < N; j += tile_size) {
            for (int k = 0; k < N; k += tile_size) {

                for (int ii = i; ii < std::min(i + tile_size, N); ++ii) {
                    for (int jj = j; jj < std::min(j + tile_size, N); ++jj) {
                        double sum = C[ii][jj];
                        for (int kk = k; kk < std::min(k + tile_size, N); ++kk) {
                            sum += A[ii][kk] * B[jj][kk];   // Good spatial locality on both
                        }
                        C[ii][jj] = sum;
                    }
                }
            }
        }
    }
}

#define BENCH(func, ...) \
    do { \
        uint64_t s_c = read_cycle(); \
        start_instret(); \
        func(__VA_ARGS__); \
        uint64_t insts = read_instret(); \
        uint64_t e_c = read_cycle(); \
        uint64_t cycles = e_c - s_c; \
        double ipc = (cycles > 0) ? (double)insts / cycles : 0.0; \
        printf("%-28s : %12lu cycles, %12lu insts, IPC = %.3f\n", \
               #func, cycles, insts, ipc); \
    } while(0)
/*
We can attribute total # of instructions to a function

and get accesses/inst


(accesses/inst) * (inst/cycle) = accesses/cycle
*/

int main() {

    init_perf_instret();          // ← Call this once
    const int N = 256;           // Matrix size (N x N)
    const int TILE_SIZE = 128;     // <<< Change this to test different blocking factors

    std::cout << "Matrix size: " << N << " x " << N << "\n";
    std::cout << "Tile size: " << TILE_SIZE << "\n\n";

    Matrix A(N, std::vector<double>(N));
    Matrix B(N, std::vector<double>(N));
    Matrix Bt(N, std::vector<double>(N));
    Matrix C(N, std::vector<double>(N));

    //init_matrix(A, N);
    //init_matrix(B, N);

    auto start = std::chrono::high_resolution_clock::now();
    
    BENCH(matmul_tiled, A, B, C, N, TILE_SIZE);
    BENCH(matmul_naive, A,B,C,N);
    BENCH(transpose_naive, B, Bt, N);
    BENCH(matmul_transposed_naive, A, Bt, C, N);
    BENCH(matmul_transposed_tiled, A, Bt, C, N, TILE_SIZE);


    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Tiled Matmul completed in " << duration.count() << " ms\n";

    // Optional: compute one value to verify
    std::cout << "C[0][0] = " << C[0][0] << "\n";

    return 0;

}