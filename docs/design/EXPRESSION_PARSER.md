# Expression parser design

The parser owns syntax and source locations. Mathematical domains and rounding
belong to evaluators and numeric APIs. The current grammar, precedence and
examples live in the [expression reference](../reference/CALCULATOR_EXPRESSIONS.md#calculator-expressions).

## Expression grammar

Recursive-descent precedence layers are additive, multiplicative, unary, power,
postfix and primary. Power is right-associative; postfix binds more tightly than
unary signs. Implicit multiplication uses ordinary multiplicative precedence,
except that two adjacent numeric tokens are rejected.

The tokenizer treats a sign as an operator, accepts calculator decimal commas,
and distinguishes uppercase scientific `E` from Euler's lowercase `e`.
Function names are scanned as whole identifiers and resolved through the shared
registry. Multi-argument calls use semicolons, so commas remain decimal separators.
See the reference for reserved constants, aliases and accepted variable names.

## AST ownership and failure

Nodes own child expressions, call argument arrays, numeric text and variable
names. Registry entries have static lifetime. Bound variable/ans values are
borrowed for evaluation; application snapshots own their backing values.
No value is substituted into source text.

Argument-array growth uses the fault-injectable allocator. Failed parsing frees
partial children and arrays and preserves the caller's output handle. Parser
recursion and constructed AST depth are limited independently to 256; calls
also have at most 256 arguments. Arity errors identify the function name.

## Special forms and typed evaluation

`convert(value; "from"; "to")` and `qty(value; "unit")` permit restricted
quoted catalogue identifiers only in their unit positions. These are not general
string values. Visitors still inspect their numeric children for variables,
ans and randomness. [Units](../reference/UNITS.md#calculator-expressions) and
[quantities](../reference/QUANTITIES.md) define their mathematical rules.

The parser builds syntax; the typed evaluator selects exact real, approximate,
complex or quantity operations. A noninteger exponent is therefore a numerical
domain question, not an unsupported grammar feature. Scalar functions delegate
to the C numeric APIs; their algorithms are documented with their owning types.

The private application expression adapters bind snapshots and inspect nodes for
graph domains or polynomial extraction. They are not a public CAS tree. The
[public engine](../reference/ENGINE_API.md) keeps its prepared expressions opaque.

## Verification

Grammar, operator/error locations, depth/arity boundaries, nested calls and
allocation cleanup are covered by calculator contract, function-call, parser
fuzz and allocation-failure tests. See [testing](../TESTING.md).
