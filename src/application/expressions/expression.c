#include "src/application/expressions/expression.h"
#include "src/application/expressions/fast_expression_internal.h"
#include "src/engine/parser/expression_internal.h"
#include "src/application/expressions/expression_domain_internal.h"
#include "src/engine/api/status.h"
#include "src/engine/evaluator/evaluator.h"
#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(APPLICATION_EXPRESSION_MAX_SAMPLES <=
    CALCULATOR_SINGLE_ALLOCATION / sizeof(ApplicationExpressionSample),
    "Sample array must fit the application single-allocation budget");

struct ApplicationExpression
{
    CalculatorExpression *tree;
    CalculatorContext context;
    char parameter_name[CALCULATOR_VARIABLE_NAME_BYTES + 1U];
    CalculatorValue parameter; /* borrowed for one sample only */
    CalculatorValue answer;
    CalculatorVariable variables[CALCULATOR_VARIABLE_CAPACITY];
    size_t variable_count;
    bool uses_variables;
    bool graph_phases, phase_overflow;
    const CalculatorExpression *phase_nodes[APPLICATION_EXPRESSION_MAX_PHASES];
    int phase_kinds[APPLICATION_EXPRESSION_MAX_PHASES];
    size_t phase_count;
};

void application_expression_destroy(ApplicationExpression *expression)
{
    if (expression == NULL) return;
    calculator_expression_destroy(expression->tree);
    calculator_value_destroy(&expression->answer);
    for (size_t i = 0; i < expression->variable_count; ++i)
        calculator_value_destroy(&expression->variables[i].value);
    free(expression);
}

static bool valid_parameter(const char *name)
{
    size_t length = 0U;
    while (length <= CALCULATOR_VARIABLE_NAME_BYTES && name[length] != '\0') ++length;
    if (length == 0U || length > CALCULATOR_VARIABLE_NAME_BYTES) return false;
    for (size_t i = 0; i < length; ++i)
        if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= 'A' && name[i] <= 'Z')))
            return false;
    CalculatorConstant constant;
    return strcmp(name, "i") != 0 && strcmp(name, "ans") != 0 &&
        calculator_function_find(name, length) == NULL &&
        !calculator_constant_from_text(name, length, &constant);
}

static CalculatorStatus bind_snapshot(ApplicationExpression *prepared,
    CalculatorExpression *node, const CalculatorSession *session, CalculatorError *error)
{
    CalculatorStatus status = CALCULATOR_OK;
    if (!numforge_budget_check()) status = calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
    else switch (node->type)
    {
        case CALCULATOR_EXPRESSION_VARIABLE:
        {
            prepared->uses_variables = true;
            const char *name = node->data.variable.name;
            if (strcmp(name, prepared->parameter_name) == 0)
            {
                node->data.variable.value = &prepared->parameter;
                break;
            }
            for (size_t i = 0; i < prepared->variable_count; ++i)
                if (strcmp(name, prepared->variables[i].name) == 0)
                {
                    node->data.variable.value = &prepared->variables[i].value;
                    return CALCULATOR_OK;
                }
            status = CALCULATOR_UNDEFINED_VARIABLE;
            if (session != NULL)
                for (size_t i = 0; i < session->variable_count; ++i)
                    if (strcmp(name, session->variables[i].name) == 0)
                    {
                        CalculatorVariable *snapshot = &prepared->variables[prepared->variable_count];
                        status = calculator_value_copy(&snapshot->value, &session->variables[i].value);
                        if (status == CALCULATOR_OK)
                        {
                            memcpy(snapshot->name, name, strlen(name) + 1U);
                            node->data.variable.value = &snapshot->value;
                            ++prepared->variable_count;
                        }
                        break;
                    }
            break;
        }
        case CALCULATOR_EXPRESSION_ANSWER:
            if (prepared->answer.number == NULL)
            {
                const CalculatorValue *answer = calculator_session_answer(session);
                status = answer == NULL ? CALCULATOR_UNDEFINED_ANSWER :
                    calculator_value_copy(&prepared->answer, answer);
            }
            break;
        case CALCULATOR_EXPRESSION_UNARY:
            return bind_snapshot(prepared, node->data.unary.operand, session, error);
        case CALCULATOR_EXPRESSION_POSTFIX:
            return bind_snapshot(prepared, node->data.postfix.operand, session, error);
        case CALCULATOR_EXPRESSION_BINARY:
            status = bind_snapshot(prepared, node->data.binary.left, session, error);
            if (status != CALCULATOR_OK) return status;
            return bind_snapshot(prepared, node->data.binary.right, session, error);
        case CALCULATOR_EXPRESSION_CALL:
            if (node->data.call.function->implementation == CALCULATOR_FUNCTION_RANDOM)
                status = CALCULATOR_INVALID_ARGUMENT;
            else
                for (size_t i = 0; i < node->data.call.count; ++i)
                {
                    status = bind_snapshot(prepared, node->data.call.arguments[i], session, error);
                    if (status != CALCULATOR_OK) return status;
                }
            break;
        default: break;
    }
    if (status != CALCULATOR_OK) calculator_error_set(error, status, node->offset);
    return status;
}

static CalculatorStatus infer_parameter(const CalculatorExpression *node,
    const CalculatorSession *session, char *name, CalculatorError *error)
{
    if (!numforge_budget_check()) return calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
    switch (node->type) {
        case CALCULATOR_EXPRESSION_VARIABLE: {
            const char *candidate = node->data.variable.name;
            if (session != NULL) for (size_t i = 0; i < session->variable_count; ++i)
                if (strcmp(candidate, session->variables[i].name) == 0) return CALCULATOR_OK;
            if (*name == '\0') memcpy(name, candidate, strlen(candidate) + 1U);
            else if (strcmp(candidate, name) != 0) {
                calculator_error_set(error, CALCULATOR_UNDEFINED_VARIABLE, node->offset);
                return CALCULATOR_UNDEFINED_VARIABLE;
            }
            break;
        }
        case CALCULATOR_EXPRESSION_UNARY:
            return infer_parameter(node->data.unary.operand, session, name, error);
        case CALCULATOR_EXPRESSION_POSTFIX:
            return infer_parameter(node->data.postfix.operand, session, name, error);
        case CALCULATOR_EXPRESSION_BINARY: {
            CalculatorStatus status = infer_parameter(node->data.binary.left, session, name, error);
            if (status != CALCULATOR_OK) return status;
            return infer_parameter(node->data.binary.right, session, name, error);
        }
        case CALCULATOR_EXPRESSION_CALL:
            for (size_t i = 0; i < node->data.call.count; ++i) {
                CalculatorStatus status = infer_parameter(node->data.call.arguments[i], session, name, error);
                if (status != CALCULATOR_OK) return status;
            }
            break;
        default: break;
    }
    return CALCULATOR_OK;
}

static CalculatorStatus prepare_expression(ApplicationExpression **result,
    const char *input, const char *parameter, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error, bool automatic)
{
    CalculatorStatus status;
    ApplicationExpression *prepared = NULL;
    calculator_error_clear(error);
    if (result == NULL || input == NULL || (!automatic && parameter == NULL) || context == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);
        return CALCULATOR_NULL_ARGUMENT;
    }
    status = calculator_context_validate(context, error);
    if (status != CALCULATOR_OK) return status;
    if ((!automatic && !valid_parameter(parameter)) || (session != NULL && session->variable_count > CALCULATOR_VARIABLE_CAPACITY))
        status = CALCULATOR_INVALID_ARGUMENT;
    else if (session != NULL && session->released) status = CALCULATOR_SESSION_EXPIRED;
    else status = CALCULATOR_OK;
    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, 0U);
        return status;
    }
    size_t length = 0U;
    while (length <= CALCULATOR_MAX_INPUT_BYTES && input[length] != '\0') ++length;
    if (length > CALCULATOR_MAX_INPUT_BYTES)
    {
        calculator_error_set(error, CALCULATOR_VALUE_TOO_LARGE, length);
        return CALCULATOR_VALUE_TOO_LARGE;
    }
    bool owner = numforge_budget_begin((uint64_t)context->time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    prepared = numforge_calloc(1U, sizeof(*prepared));
    status = prepared == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
    if (status == CALCULATOR_OK)
    {
        prepared->context = *context;
        status = calculator_parse_variables(input, &prepared->tree, error);
        if (status == CALCULATOR_OK && automatic) {
            status = infer_parameter(prepared->tree, session, prepared->parameter_name, error);
            if (status == CALCULATOR_OK && prepared->parameter_name[0] == '\0')
                memcpy(prepared->parameter_name, "x", 2U);
        } else if (status == CALCULATOR_OK)
            memcpy(prepared->parameter_name, parameter, strlen(parameter) + 1U);
        if (status == CALCULATOR_OK) status = bind_snapshot(prepared, prepared->tree, session, error);
    }
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) *result = prepared;
    else
    {
        application_expression_destroy(prepared);
        calculator_error_set(error, status, error == NULL ? 0U : error->offset);
    }
    if (owner) numforge_budget_end();
    return status;
}

CalculatorStatus application_expression_prepare(ApplicationExpression **result,
    const char *input, const char *parameter, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error)
{
    return prepare_expression(result, input, parameter, session, context, error, false);
}
CalculatorStatus application_expression_prepare_auto(ApplicationExpression **result,
    const char *input, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error)
{
    return prepare_expression(result, input, NULL, session, context, error, true);
}
const char *application_expression_parameter(const ApplicationExpression *expression)
{
    return expression == NULL ? NULL : expression->parameter_name;
}
CalculatorStatus application_expression_fast_compile(ApplicationFastExpression **result,
    const ApplicationExpression *expression,CalculatorError *error)
{
    if(expression==NULL){calculator_error_set(error,CALCULATOR_NULL_ARGUMENT,0U);return CALCULATOR_NULL_ARGUMENT;}
    return application_fast_compile_tree(result,expression->tree,&expression->parameter,
        &expression->answer,&expression->context,error);
}

static bool graph_smooth_node(const CalculatorExpression *node)
{
    switch (node->type) {
        case CALCULATOR_EXPRESSION_NUMBER:
        case CALCULATOR_EXPRESSION_CONSTANT:
        case CALCULATOR_EXPRESSION_ANSWER:
        case CALCULATOR_EXPRESSION_VARIABLE: return true;
        case CALCULATOR_EXPRESSION_UNARY: return graph_smooth_node(node->data.unary.operand);
        case CALCULATOR_EXPRESSION_POSTFIX:
            return node->data.postfix.operation != CALCULATOR_POSTFIX_FACTORIAL && graph_smooth_node(node->data.postfix.operand);
        case CALCULATOR_EXPRESSION_BINARY: {
            CalculatorBinaryOperator operation = node->data.binary.operation;
            if (operation == CALCULATOR_BINARY_DIVIDE && node->data.binary.right->type != CALCULATOR_EXPRESSION_NUMBER) return false;
            if (operation == CALCULATOR_BINARY_POWER) {
                if (node->data.binary.right->type != CALCULATOR_EXPRESSION_NUMBER) return false;
                const char *power = node->data.binary.right->data.number.text;
                for (size_t i=0;power[i];++i) if (power[i]<'0' || power[i]>'9') return false;
            }
            return graph_smooth_node(node->data.binary.left) && graph_smooth_node(node->data.binary.right);
        }
        case CALCULATOR_EXPRESSION_CALL:
            if (node->data.call.function->implementation != CALCULATOR_FUNCTION_SIN &&
                node->data.call.function->implementation != CALCULATOR_FUNCTION_COS &&
                node->data.call.function->implementation != CALCULATOR_FUNCTION_EXP &&
                node->data.call.function->implementation != CALCULATOR_FUNCTION_ABS) return false;
            for (size_t i=0;i<node->data.call.count;++i) if (!graph_smooth_node(node->data.call.arguments[i])) return false;
            return true;
        default: return false;
    }
}
bool application_expression_graph_smooth(const ApplicationExpression *expression)
{
    return expression != NULL && graph_smooth_node(expression->tree);
}
CalculatorStatus application_expression_domain(bool *verified, const ApplicationExpression *expression,
    const BigRational *lower, const BigRational *upper)
{
    if (!verified || !expression || !lower || !upper) return CALCULATOR_NULL_ARGUMENT;
    int order = 0;
    CalculatorStatus status = calculator_from_rational_status(bigrational_compare(&order, lower, upper));
    if (status != CALCULATOR_OK) return calculator_budget_status(status);
    if (order > 0) return CALCULATOR_INVALID_ARGUMENT;
    return application_expression_domain_tree(verified, expression->tree, expression->parameter_name, lower, upper);
}
static bool integer_domain_node(const CalculatorExpression *node,const char *parameter)
{
    switch(node->type){
        case CALCULATOR_EXPRESSION_POSTFIX:
            if(node->data.postfix.operation==CALCULATOR_POSTFIX_FACTORIAL&&
                node->data.postfix.operand->type==CALCULATOR_EXPRESSION_VARIABLE&&
                strcmp(node->data.postfix.operand->data.variable.name,parameter)==0)return true;
            return integer_domain_node(node->data.postfix.operand,parameter);
        case CALCULATOR_EXPRESSION_UNARY:return integer_domain_node(node->data.unary.operand,parameter);
        case CALCULATOR_EXPRESSION_BINARY:return integer_domain_node(node->data.binary.left,parameter)||integer_domain_node(node->data.binary.right,parameter);
        case CALCULATOR_EXPRESSION_CALL:{
            CalculatorFunctionImplementation kind=node->data.call.function->implementation;
            bool restricted=kind==CALCULATOR_FUNCTION_FACTORIAL||kind==CALCULATOR_FUNCTION_GCD||kind==CALCULATOR_FUNCTION_LCM||kind==CALCULATOR_FUNCTION_MOD||kind==CALCULATOR_FUNCTION_NPR||kind==CALCULATOR_FUNCTION_NCR||kind==CALCULATOR_FUNCTION_ISQRT;
            for(size_t i=0;i<node->data.call.count;++i){const CalculatorExpression *argument=node->data.call.arguments[i];
                if(restricted&&argument->type==CALCULATOR_EXPRESSION_VARIABLE&&strcmp(argument->data.variable.name,parameter)==0)return true;
                if(integer_domain_node(argument,parameter))return true;
            }return false;
        }
        default:return false;
    }
}
bool application_expression_graph_integer_domain(const ApplicationExpression *expression)
{
    return expression!=NULL&&integer_domain_node(expression->tree,expression->parameter_name);
}

static void collect_graph_phases(ApplicationExpression *expression, const CalculatorExpression *node)
{
    switch (node->type) {
        case CALCULATOR_EXPRESSION_UNARY: collect_graph_phases(expression,node->data.unary.operand);break;
        case CALCULATOR_EXPRESSION_POSTFIX: collect_graph_phases(expression,node->data.postfix.operand);break;
        case CALCULATOR_EXPRESSION_BINARY:
            collect_graph_phases(expression,node->data.binary.left);collect_graph_phases(expression,node->data.binary.right);break;
        case CALCULATOR_EXPRESSION_CALL: {
            CalculatorFunctionImplementation function=node->data.call.function->implementation;
            bool step=function==CALCULATOR_FUNCTION_FLOOR||function==CALCULATOR_FUNCTION_CEIL||
                function==CALCULATOR_FUNCTION_TRUNC||function==CALCULATOR_FUNCTION_ROUND;
            if (function == CALCULATOR_FUNCTION_SIN || function == CALCULATOR_FUNCTION_COS ||
                function == CALCULATOR_FUNCTION_TAN || step) {
                if (expression->phase_count < APPLICATION_EXPRESSION_MAX_PHASES) {
                    size_t i=expression->phase_count++;
                    expression->phase_nodes[i] = step?node:node->data.call.arguments[0];
                    expression->phase_kinds[i] = step?-2:function == CALCULATOR_FUNCTION_COS ? 0 :
                        function == CALCULATOR_FUNCTION_SIN ? 1 : -1;
                }
                else expression->phase_overflow = true;
            }
            for (size_t i=0;i<node->data.call.count;++i) collect_graph_phases(expression,node->data.call.arguments[i]);
            break;
        }
        default: break;
    }
}
void application_expression_enable_graph_phases(ApplicationExpression *expression)
{
    if (expression == NULL || expression->graph_phases) return;
    expression->graph_phases = true;collect_graph_phases(expression,expression->tree);
}
size_t application_expression_graph_phase_count(const ApplicationExpression *expression)
{
    return expression ? expression->phase_count : 0;
}
int application_expression_graph_peak_offset(const ApplicationExpression *expression,size_t phase)
{
    if(expression == NULL || phase >= expression->phase_count)return -1;
    /* Metadata follows the same collection order as the captured arguments. */
    return expression->phase_kinds[phase];
}
static CalculatorStatus sample_graph_phases(ApplicationExpression *expression,
    ApplicationExpressionSample *sample)
{
    sample->phase_unknown = expression->phase_overflow;
    if (expression->phase_count == 0U || sample->error.status != CALCULATOR_OK ||
        calculator_value_is_complex(&sample->value) || sample->value.quantity) return CALCULATOR_OK;
    sample->phase_count = expression->phase_count;
    sample->phases = numforge_calloc(sample->phase_count,sizeof(*sample->phases));
    if (sample->phases == NULL) return CALCULATOR_OUT_OF_MEMORY;
    for (size_t i=0;i<sample->phase_count;++i) {
        CalculatorValue phase = {0};CalculatorError error;
        CalculatorStatus status = calculator_evaluate_parsed_value(expression->phase_nodes[i],&expression->context,
            expression->answer.number == NULL ? NULL : &expression->answer,NULL,&phase,&error);
        if (status == CALCULATOR_OK && !calculator_value_is_complex(&phase) && !phase.quantity) {
            sample->phases[i] = phase.number;phase.number = NULL;
        } else if (status == CALCULATOR_OK || status == CALCULATOR_DIVISION_BY_ZERO ||
            status == CALCULATOR_INVALID_ARGUMENT || status == CALCULATOR_DIMENSION_ERROR || status == CALCULATOR_NOT_IMPLEMENTED) {
            sample->phase_unknown = true;status = CALCULATOR_OK;
        }
        calculator_value_destroy(&phase);
        if (status != CALCULATOR_OK) return status;
    }
    return CALCULATOR_OK;
}

void application_expression_samples_destroy(ApplicationExpressionSample *samples, size_t count)
{
    if (samples == NULL) return;
    for (size_t i = 0; i < count; ++i) {
        calculator_value_destroy(&samples[i].value);
        if (samples[i].phases != NULL) for (size_t j=0;j<samples[i].phase_count;++j) bigdecimal_destroy(samples[i].phases[j]);
        free(samples[i].phases);
    }
    free(samples);
}

CalculatorStatus application_expression_sample(ApplicationExpression *expression,
    const CalculatorValue *arguments, size_t count,
    ApplicationExpressionSample **result, CalculatorError *error)
{
    calculator_error_clear(error);
    CalculatorStatus status = CALCULATOR_OK;
    if (expression == NULL || arguments == NULL || result == NULL) status = CALCULATOR_NULL_ARGUMENT;
    else if (count == 0U || count > APPLICATION_EXPRESSION_MAX_SAMPLES) status = CALCULATOR_INVALID_ARGUMENT;
    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, 0U);
        return status;
    }
    bool owner = numforge_budget_begin((uint64_t)expression->context.time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    ApplicationExpressionSample *samples = numforge_calloc(count, sizeof(*samples));
    status = samples == NULL ? CALCULATOR_OUT_OF_MEMORY : CALCULATOR_OK;
    for (size_t i = 0; status == CALCULATOR_OK && i < count; ++i)
    {
        if (!numforge_budget_check()) { status = CALCULATOR_OUT_OF_MEMORY; break; }
        if (arguments[i].number == NULL) { status = CALCULATOR_INVALID_ARGUMENT; break; }
        expression->parameter = arguments[i];
        samples[i].value.uses_variables = expression->uses_variables;
        status = calculator_evaluate_parsed_value(expression->tree, &expression->context,
            expression->answer.number == NULL ? NULL : &expression->answer, NULL,
            &samples[i].value, &samples[i].error);
        if (status == CALCULATOR_OK && expression->graph_phases) status = sample_graph_phases(expression,&samples[i]);
        memset(&expression->parameter, 0, sizeof(expression->parameter));
        if (status == CALCULATOR_DIVISION_BY_ZERO || status == CALCULATOR_INVALID_ARGUMENT ||
            status == CALCULATOR_DIMENSION_ERROR || status == CALCULATOR_NOT_IMPLEMENTED)
            status = CALCULATOR_OK;
        else if (status != CALCULATOR_OK && error != NULL) *error = samples[i].error;
    }
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) *result = samples;
    else
    {
        application_expression_samples_destroy(samples, count);
        calculator_error_set(error, status, error == NULL ? 0U : error->offset);
    }
    if (owner) numforge_budget_end();
    return status;
}
