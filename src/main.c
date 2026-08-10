#include <stdio.h>
#include <mysciencecalc/bigint.h>

int main(void)
{
    BigInt *number = bigint_create();

    if (number == NULL)
    {
        return 1;
    }

    bigint_set_string(number, "1844674407370955164564498498495765465498489464984987987321321321189112918156191532985615");

    char *string = bigint_to_string(number);

    if (string == NULL)
    {
        bigint_destroy(number);
        return 1;
    }

    printf("Number: %s\n", string);

    bigint_string_free(string);
    bigint_destroy(number);

    return 0;
}