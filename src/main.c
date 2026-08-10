#include <stdio.h>
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

    bigint_set_string(number1, "1844674407370955164564498498495765465498489464984987987321321321189112918156191532985615");
    bigint_set_string(number2, "1844674407370955164564498498495765465498489464984987987321321321189112918156191532985615");

    if (!bigint_add(result, number1, number2))
    {
        return 1;
    }

    char *string = bigint_to_string(result);

    if (string == NULL)
    {
        bigint_destroy(result);
        bigint_destroy(number1);
        bigint_destroy(number2);
        return 1;
    }

    printf("Number: %s\n", string);

    bigint_string_free(string);
    bigint_destroy(result);
    bigint_destroy(number1);
    bigint_destroy(number2);    

    return 0;
}