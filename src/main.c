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
        return 1;
    }

    if (!bigint_set_string(number1, "1234567890123456789012345678901234567890"))
    {
        return 1;
    }

    if (!bigint_set_string(number2, "9876543210987654321098765432109876543210"))
    {
        return 1;
    }

    LARGE_INTEGER start;
    LARGE_INTEGER end;
    LARGE_INTEGER frequency;

    QueryPerformanceFrequency(&frequency);

    QueryPerformanceCounter(&start);
    /* code to benchmark */
    for(int i = 0; i < 100000; i++)
        if (!bigint_add(result, number1, number2))
        {
            return 1;
        }

    QueryPerformanceCounter(&end);

    double elapsed =
        (double)(end.QuadPart - start.QuadPart) /
        (double)frequency.QuadPart; 

    /*char *string = bigint_to_string(result);

    if (string == NULL)
    {
        bigint_destroy(result);
        bigint_destroy(number1);
        bigint_destroy(number2);
        return 1;
    }*/

    //printf("Number: %s\n", string);
    printf("Time: %.9f s\n", elapsed);

   // free(string);
   /* bigint_destroy(result);
    bigint_destroy(number1);
    bigint_destroy(number2);  */  

    return 0;
}