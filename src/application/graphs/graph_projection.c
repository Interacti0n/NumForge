#include "src/application/graphs/graph_projection.h"
#include "src/engine/api/status.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>

CalculatorStatus application_graph_project(const BigDecimal *number, double *value)
{
    char *text = NULL;
    CalculatorStatus status = calculator_from_decimal_status(bigdecimal_format_mode(number,
        16, BIGDECIMAL_ROUND_HALF_EVEN, BIGDECIMAL_FORMAT_SCIENTIFIC, 128U, &text));
    if (status == CALCULATOR_OK) {
        char *end = NULL;
        errno = 0;
        double projected = strtod(text, &end);
        *value = errno == ERANGE || !isfinite(projected) || end == text || *end ? NAN : projected;
    }
    free(text);
    return status;
}
