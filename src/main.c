#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <numforge/bigint.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Small benchmark/demo for the public BigInt API. It is intentionally not
    the interactive calculator advertised by the project name.
------------------------------------------------------------------------------------------------------------------------------
*/
int main(void)
{
    BigInt *number1 = bigint_create();
    BigInt *number2 = bigint_create();
    BigInt *result = bigint_create();

    if (number1 == NULL || number2 == NULL || result == NULL)
    {
        printf("Failed to create BigInt instances\n");
        bigint_destroy(number1);
        bigint_destroy(number2);
        bigint_destroy(result);
        return 1;
    }

    BigIntStatus status = bigint_set_string(number1, "123456789");

    if (status != BIGINT_OK)
    {
        printf("Failed to set number1: %s\n", bigint_status_to_string(status));
        bigint_destroy(number1);
        bigint_destroy(number2);
        bigint_destroy(result);
        return 1;
    }

    status = bigint_set_string(number2, "10000");

    if (status != BIGINT_OK)
    {
        printf("Failed to set number2: %s\n", bigint_status_to_string(status));
        bigint_destroy(number1);
        bigint_destroy(number2);
        bigint_destroy(result);
        return 1;
    }

    clock_t start = clock();
    // clock() measures CPU time consumed by this process, not wall-clock time.
    for (int i = 0; i < 1; i++)
    {
        status = bigint_pow(result, number1, number2);

        if (status != BIGINT_OK)
        {
            printf("Failed to raise to power: %s\n", bigint_status_to_string(status));
            bigint_destroy(number1);
            bigint_destroy(number2);
            bigint_destroy(result);
            return 1;
        }
    }

    clock_t end = clock();

    double elapsed = (start == (clock_t)-1 || end == (clock_t)-1)
        ? -1.0
        : (double)(end - start) / CLOCKS_PER_SEC;

    char *string = bigint_to_string(result);

    if (string == NULL)
    {
        bigint_destroy(result);
        bigint_destroy(number1);
        bigint_destroy(number2);
        return 1;
    }

    printf("Number: %s\n", string);
    if (elapsed >= 0.0)
    {
        printf("CPU time: %.9f s\n", elapsed);
    }

    free(string);
    bigint_destroy(result);
    bigint_destroy(number1);
    bigint_destroy(number2);

    return 0;
}
