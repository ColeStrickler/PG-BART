#include <stdio.h>

int factorial(int x) {
    if (x <= 0)
        return 1;
    return x * factorial(x-1);
}

int main() {
    int* data = new int[16384];

    int res = 0;
    for (int i = 0; i < 40; i++)
    {
        for (int j = 0; j < 32; j++)
        {
            for (int k = 0; k < 16; k++)
                res += data[i*2 + j*100 + k];
        }

       // printf("addr data 0x%llx\n", &data[i]);
    }

    return res;
}