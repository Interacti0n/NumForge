#include <stdio.h>
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

    if (!bigint_set_string(number1, "25600000000000000000512000000000000000007680000000000000000102400000000000000001280000000000000000015360000000000000000179200000000000000002048000000000000000023040000000000000000204800000000000000001792000000000000000015360000000000000000128000000000000000001024000000000000000007680000000000000000051200000000000000000256000000000000000000000000000000000000"))
    {
        printf("Failed to set number1\n");
        return 1;
    }

    if (!bigint_set_string(number2, "25600000000000000000512000000000000000007680000000000000000102400000000000000001280000000000000000015360000000000000000179200000000000000002048000000000000000023040000000000000000204800000000000000001792000000000000000015360000000000000000128000000000000000001024000000000000000007680000000000000000051200000000000000000256000000000000000000000000000000000000"))
    {
        printf("Failed to set number2\n");
        return 1;
    }

    LARGE_INTEGER start;
    LARGE_INTEGER end;
    LARGE_INTEGER frequency;

    QueryPerformanceFrequency(&frequency);

    QueryPerformanceCounter(&start);
    /* code to benchmark */
    for(int i = 0; i < 1; i++)
        if (!bigint_mul(result, number1, number2))
        {
            return 1;
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