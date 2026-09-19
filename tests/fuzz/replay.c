#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

/* Portable harness smoke; real coverage-guided mutation is a separate Clang job. */
int main(void)
{
    static const char *const seeds[] = {
        "", "0", "-0.00", "1e-9223372036854775807", "1E999999999999999999999",
        "123456789012345678901234567890.125", "0.0000000000000000001",
        "(2+3)*4", "πe", "1e3", "1.2.3", "2 3", "2^3^2", "5!", "1/0",
        "pow(2;factorial(3))", "sqrt(abs(-4))", "log(8;2)", "min(1;2;3;4;5)",
        "sum(1;2;3)", "product(1;2;3)", "mean(1;2;3)",
        "variance(1;2;3)", "stdevp(1;2;3)", "stdev(1;2;3)",
        "atan(1;2)", "exp(1)", "log(1;;2)", "√(4)", "e(2)", "log2(8)",
        "GET / HTTP/1.1\r\n\r\n", "POST /api/evaluate HTTP/1.1\r\nContent-Length: 3\r\n\r\n2+2",
        "POST / HTTP/1.1\r\nContent-Length: 4\r\nContent-Length: 4\r\n\r\n",
        "GET / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"
    };
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); i++)
        LLVMFuzzerTestOneInput((const uint8_t *)seeds[i], strlen(seeds[i]));
    uint32_t state = UINT32_C(0x6e756d66);
    uint8_t bytes[128];
    for (size_t trial = 0; trial < 1000; trial++)
    {
        for (size_t i = 0; i < sizeof(bytes); i++)
        {
            state = state * UINT32_C(1664525) + UINT32_C(1013904223);
            bytes[i] = (uint8_t)(state >> 24);
        }
        LLVMFuzzerTestOneInput(bytes, trial % sizeof(bytes));
    }
    return 0;
}
