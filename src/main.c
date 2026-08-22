#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <mysciencecalc/bigint.h>

int main(void)
{
    BigInt *number1 = bigint_create();
    BigInt *number2 = bigint_create();
    BigInt *result = bigint_create();

    if (number1 == NULL || number2 == NULL || result == NULL)
    {
        printf("Failed to create BigInt instances\n");
        return 1;
    }

    BigIntStatus status = bigint_set_string(number1, "900");

    if (status != BIGINT_OK)
    {
        printf("Failed to set number1: %s\n", bigint_status_to_string(status));
        return 1;
    }

    status = bigint_set_string(number2, "900");

    if (status != BIGINT_OK)
    {
        printf("Failed to set number2: %s\n", bigint_status_to_string(status));
        return 1;
    }

    LARGE_INTEGER start;
    LARGE_INTEGER end;
    LARGE_INTEGER frequency;

    QueryPerformanceFrequency(&frequency);

    QueryPerformanceCounter(&start);
    /* code to benchmark */
    for(int i = 0; i < 1; i++)
    {
        status = bigint_pow(result, number1, number2);

        if (status != BIGINT_OK)
        {
            printf("Failed to raise to power: %s\n", bigint_status_to_string(status));
            return 1;
        }
    }

    QueryPerformanceCounter(&end);

    double elapsed =
        (double)(end.QuadPart - start.QuadPart) /
        (double)frequency.QuadPart; 

    char *string = bigint_to_string(result);

    if (string == NULL)
    {
        bigint_destroy(result);
        bigint_destroy(number1);
        bigint_destroy(number2);
        return 1;
    }

    printf("Number: %s\n", string);
    printf("Time: %.9f s\n", elapsed);

    free(string);
    bigint_destroy(result);
    bigint_destroy(number1);
    bigint_destroy(number2);  

    return 0;
}