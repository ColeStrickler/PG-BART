#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>

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



/*
We can attribute total # of instructions to a function

and get accesses/inst


(accesses/inst) * (inst/cycle) = accesses/cycle
*/

int main() {


    const int N = 512;           // Matrix size (N x N)
    const int TILE_SIZE = 32;     // <<< Change this to test different blocking factors

    std::cout << "Matrix size: " << N << " x " << N << "\n";
    std::cout << "Tile size: " << TILE_SIZE << "\n\n";

    Matrix A(N, std::vector<double>(N));
    Matrix B(N, std::vector<double>(N));
    Matrix Bt(N, std::vector<double>(N));
    Matrix C(N, std::vector<double>(N));

    //init_matrix(A, N);
    //init_matrix(B, N);

    auto start = std::chrono::high_resolution_clock::now();
    
    matmul_tiled(A, B, C, N, TILE_SIZE);
    matmul_naive(A,B,C,N);
    transpose_naive(B, Bt, N);
    matmul_transposed_naive(A, Bt, C, N);
    matmul_transposed_tiled(A, Bt, C, N, TILE_SIZE);


    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Tiled Matmul completed in " << duration.count() << " ms\n";

    // Optional: compute one value to verify
    std::cout << "C[0][0] = " << C[0][0] << "\n";

    return 0;

}