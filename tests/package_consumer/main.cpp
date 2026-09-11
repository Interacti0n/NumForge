#include <cstdlib>
#include <cstring>
#include <memory>
#include <numforge/bigint.h>
#include <numforge/bigdecimal.h>

// Public headers must provide C linkage without a consumer-side extern block.
int main()
{
    std::unique_ptr<BigInt, decltype(&bigint_destroy)> integer(bigint_create(), bigint_destroy);
    std::unique_ptr<BigDecimal, decltype(&bigdecimal_destroy)> decimal(bigdecimal_create(), bigdecimal_destroy);
    if (!integer || !decimal) return 1;
    if (bigint_set_string(integer.get(), "12345678901234567890") != BIGINT_OK ||
        bigdecimal_set_string(decimal.get(), "1.25") != BIGDECIMAL_OK) return 2;
    std::unique_ptr<char, decltype(&std::free)> integer_text(bigint_to_string(integer.get()), std::free);
    char *raw = nullptr;
    if (bigdecimal_to_string(decimal.get(), &raw) != BIGDECIMAL_OK) return 3;
    std::unique_ptr<char, decltype(&std::free)> decimal_text(raw, std::free);
    return !integer_text || !decimal_text ||
        std::strcmp(integer_text.get(), "12345678901234567890") != 0 ||
        std::strcmp(decimal_text.get(), "1.25") != 0;
}
