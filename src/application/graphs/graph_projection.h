#ifndef NUMFORGE_APPLICATION_GRAPH_PROJECTION_H
#define NUMFORGE_APPLICATION_GRAPH_PROJECTION_H
#include "src/engine/api/calculator_internal.h"

/* Lossy display projection only. Unrepresentable values become NaN on success;
 * allocation/formatting failures preserve the destination. Never use this
 * conversion to subtract exact values or establish an exact root. */
CalculatorStatus application_graph_project(const BigDecimal *number, double *value);
#endif
