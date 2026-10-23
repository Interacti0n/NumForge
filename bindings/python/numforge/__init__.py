"""A thin, lossless Python interface to the NumForge C mathematics engine."""
from collections.abc import Mapping
from dataclasses import dataclass
from decimal import Decimal
from fractions import Fraction
import sys as _sys

from . import _native

__version__ = "0.1.0.dev0"
core_version = _native.core_version
Error = _native.Error
__all__ = ["Error", "Result", "Expression", "PolynomialRoot", "PolynomialSolution",
           "number", "evaluate", "prepare", "sqrt", "solve_quadratic", "Matrix", "LinearSolution",
           "describe", "covariance", "StatisticsSummary", "linear_regression", "LinearRegression", "find_root", "RootResult", "find_roots", "RootScanResult",
           "integrate", "IntegralResult", "grid", "visit_grid", "core_version"]

_ROUNDING = {"toward_zero": 0, "away_from_zero": 1, "floor": 2,
             "ceiling": 3, "half_up": 4, "half_even": 5}


def _options(precision=34, rounding="half_even", time_limit_ms=10000,
             allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
    if type(precision) is not int:
        raise TypeError("precision must be an integer")
    if not 1 <= precision <= 10000:
        raise ValueError("precision must be between 1 and 10000 working significant digits")
    if not isinstance(rounding, str) or rounding not in _ROUNDING:
        raise ValueError("unknown rounding mode")
    if type(time_limit_ms) is not int:
        raise TypeError("time_limit_ms must be an integer")
    if not 0 <= time_limit_ms <= 2**63 - 1:
        raise ValueError("time_limit_ms must fit a nonnegative signed 64-bit integer")
    limits = []
    for name, value in (("allocation_bytes", allocation_bytes), ("single_allocation", single_allocation)):
        if value is None:
            value = _native.MAX_ALLOCATION
        if type(value) is not int:
            raise TypeError(f"{name} must be an integer or None")
        if not 0 <= value <= _native.MAX_ALLOCATION:
            raise ValueError(f"{name} is outside the platform size range")
        limits.append(value)
    return precision, _ROUNDING[rounding], time_limit_ms, *limits


def _fraction(pair):
    return None if pair is None else Fraction(int(pair[0]), int(pair[1]))


class Result:
    """Owned C value. Formatting never replaces it with a rounded Python float.

    Use number(), evaluate(), sqrt() or Expression.evaluate() to create results.
    kind is the stored C type; exact reports a proven exact path. Decimal and
    decimal-complex paths are conservatively approximate. Native memory is freed
    when the last Python reference to the handle is released.
    """
    __slots__ = ("_handle",)

    def __init__(self, handle):
        _native.info(handle)  # Validate the private capsule before owning its reference.
        self._handle = handle

    @property
    def kind(self):
        return _native.info(self._handle)[0]

    @property
    def exact(self):
        return _native.info(self._handle)[1]

    def as_fraction(self):
        """Copy an exact real result into Fraction; TypeError for approximations/complex values."""
        return _fraction(_native.fraction(self._handle))

    def __str__(self):
        return _native.text(self._handle)

    def __repr__(self):
        return f"Result({str(self)!r}, kind={self.kind!r}, exact={self.exact})"


def number(value):
    """Copy int, Fraction, finite Decimal or a numeric literal string losslessly.

    Literal strings and Decimal values are interpreted as exact finite input.
    A Result retains its original C value and provenance without copying. Floats,
    complex floats, booleans and arbitrary coercible objects are rejected. Text
    accepts decimal/exponent syntax, not calculator expressions or fraction syntax.
    Use Fraction(1, 3) or evaluate('1/3') for recurring fractions.
    """
    if isinstance(value, Result):
        return value
    if type(value) is int:
        return Result(_native.number("integer", str(value)))
    if isinstance(value, Fraction):
        return Result(_native.number("rational", str(value.numerator), str(value.denominator)))
    if isinstance(value, (str, Decimal)):
        return Result(_native.number("decimal", str(value)))
    raise TypeError("use int, Fraction, Decimal, numeric text or Result; binary floats are not accepted")


class Expression:
    """Prepared C expression with explicit ordered parameters; no persistent session state."""
    __slots__ = ("_handle", "_parameters")

    def __init__(self, handle):
        self._handle = handle
        self._parameters = _native.parameters(handle)

    @property
    def parameters(self):
        return self._parameters

    def evaluate(self, arguments=(), *, precision=34, rounding="half_even", time_limit_ms=10000,
                 allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
        """Evaluate borrowed arguments by declaration order or by an exact name mapping.

        Arguments are converted before the C evaluation budget starts. No input
        is retained after the call; returned values outlive the expression. Higher
        working precision does not recover digits lost in approximate arguments.
        """
        options = _options(precision, rounding, time_limit_ms, allocation_bytes, single_allocation)
        if isinstance(arguments, Mapping):
            if set(arguments) != set(self.parameters):
                raise ValueError("argument names must exactly match declared parameters")
            arguments = tuple(arguments[name] for name in self.parameters)
        elif isinstance(arguments, (str, bytes)):
            raise TypeError("arguments must be a sequence of values or a name mapping")
        else:
            arguments = tuple(arguments)
        # Keep all converted values alive until native evaluation has unbound them.
        values = tuple(number(value) for value in arguments)
        return Result(_native.evaluate_prepared(self._handle, tuple(value._handle for value in values), options))


def prepare(expression, parameters=()):
    """Parse once in C; copy up to 32 distinct case-sensitive ASCII parameter names.

    Names are 1..31 letters and cannot shadow C built-ins. Unsupported capabilities
    and undeclared names raise Error, with a zero-based UTF-8 byte offset. Preparation
    has its own default C resource budget; evaluation takes an independent policy.
    """
    if not isinstance(expression, str):
        raise TypeError("expression must be text")
    if isinstance(parameters, (str, bytes)):
        raise TypeError("parameters must be a sequence of names")
    return Expression(_native.prepare(expression, tuple(parameters)))


def evaluate(expression, *, variables=None, precision=34, rounding="half_even", time_limit_ms=10000,
             allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
    """Evaluate using the shared C engine; precision is working significant digits.

    Exact operations remain exact. Approximate composed functions do not promise
    certified final error bounds or general correct rounding. variables is an
    optional explicit name mapping, never a persistent calculator session.
    """
    if not isinstance(expression, str):
        raise TypeError("expression must be text")
    options = _options(precision, rounding, time_limit_ms, allocation_bytes, single_allocation)
    if variables is None:
        return Result(_native.evaluate(expression, options))
    if not isinstance(variables, Mapping):
        raise TypeError("variables must be a mapping")
    return prepare(expression, tuple(variables)).evaluate(variables, precision=precision, rounding=rounding,
        time_limit_ms=time_limit_ms, allocation_bytes=allocation_bytes, single_allocation=single_allocation)


def sqrt(value, *, precision=34, rounding="half_even", time_limit_ms=10000,
         allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
    """Principal square root through the existing C typed evaluator.

    Exact rational roots stay exact; negative inputs can produce principal complex
    values. Working precision and approximation guarantees match evaluate().
    """
    options = _options(precision, rounding, time_limit_ms, allocation_bytes, single_allocation)
    value = number(value)
    expression = prepare("sqrt(x)", ("x",))
    return Result(_native.evaluate_prepared(expression._handle, (value._handle,), options))


@dataclass(frozen=True, slots=True)
class RootResult:
    """Bounded search diagnostics, not a certified root enclosure or completeness proof.

    x is an exact sampled Fraction coordinate only when kind='found'; its root
    status may be approximate. exact_zero records a proven zero of the callback's
    returned value. lower/upper are final search bounds, residual is Decimal/None.
    """
    kind: str
    x: Fraction | None
    lower: Fraction
    upper: Fraction
    residual: Decimal | None
    exact_zero: bool
    iterations: int
    evaluations: int


@dataclass(frozen=True, slots=True)
class RootScanResult:
    """Bounded sampled roots and diagnostics; finished never proves completeness.

    roots is a sorted immutable tuple of independently owned RootResult values.
    Explicit root/evaluation limits return partial lists; fatal errors return none.
    """
    kind: str
    roots: tuple[RootResult, ...]
    evaluations: int
    iterations: int
    scanned_segments: int
    domain_gaps: int
    rejected_brackets: int
    candidate_samples: int
    candidate_roots: int
    candidate_gaps: int

    @property
    def complete(self):
        return False

    @property
    def count(self):
        return len(self.roots)


def _integer_range(name, value, minimum, maximum):
    if type(value) is not int:
        raise TypeError(f"{name} must be an integer")
    if not minimum <= value <= maximum:
        raise ValueError(f"{name} must be between {minimum} and {maximum}")


def _root_arguments(function, lower, upper, *, continuous=False, decimal_places=34,
              x_tolerance="1E-12", residual_tolerance="1E-12", max_iterations=96,
              max_evaluations=192, time_limit_ms=10000,
              allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
    """Shared conversion and callback bridge; no Python search or root discovery."""
    if not callable(function):
        raise TypeError("function must be callable as function(x, decimal_places)")
    if type(continuous) is not bool:
        raise TypeError("continuous must be a bool")
    for name, value, minimum, maximum in (("decimal_places", decimal_places, 1, 10000),
            ("max_iterations", max_iterations, 1, _native.MAX_ALLOCATION),
            ("max_evaluations", max_evaluations, 4, _native.MAX_ALLOCATION)):
        _integer_range(name, value, minimum, maximum)
    if type(residual_tolerance) is not int and not isinstance(residual_tolerance, (str, Decimal)):
        raise TypeError("residual_tolerance must be a finite decimal literal, Decimal or int")
    options = _options(time_limit_ms=time_limit_ms, allocation_bytes=allocation_bytes,
                       single_allocation=single_allocation)
    values = tuple(number(value) for value in (lower, upper, x_tolerance))

    def sample(pair, places):
        value = function(_fraction(pair), places)
        return None if value is None else number(value)._handle

    return (sample, tuple(value._handle for value in values), str(residual_tolerance),
            (decimal_places, max_iterations, max_evaluations, continuous), options)


def _root_result(data):
    return RootResult(data["kind"], _fraction(data["x"]), _fraction(data["lower"]),
                      _fraction(data["upper"]), None if data["residual"] is None else Decimal(data["residual"]),
                      data["exact_zero"], data["iterations"], data["evaluations"])


@dataclass(frozen=True, slots=True)
class IntegralResult:
    """Approximate Simpson value/error as Fractions; error is not a certified bound."""
    kind: str
    value: Fraction | None
    error: Fraction | None
    evaluations: int
    refinements: int

    @property
    def exact(self):
        return False


def integrate(function, lower, upper, *, decimal_places=34, absolute_tolerance="1E-12",
              max_refinements=12, max_evaluations=4097, time_limit_ms=10000,
              allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
    """Finite lower<upper integration through the public C composite Simpson kernel.

    function(Fraction, decimal_places) follows find_root's sample conversion and
    exception contract. Return None for undefined samples (domain_gap, no value).
    Supply accurate deterministic samples of a smooth function. Successive-grid
    error excludes callback error and can miss oscillations/discontinuities;
    kind='estimated' means only error<=absolute_tolerance, never proven accuracy.
    Limits retain the latest estimate, with unused evaluation slots if a complete
    next grid will not fit. All results are approximate, including zero error.
    Budgets cover cooperative native work, not Python allocations/preemption.
    """
    if not callable(function):
        raise TypeError("function must be callable as function(x, decimal_places)")
    for name, value, minimum, maximum in (("decimal_places", decimal_places, 1, 10000),
            ("max_refinements", max_refinements, 2, _native.MAX_ALLOCATION),
            ("max_evaluations", max_evaluations, 5, _native.MAX_ALLOCATION)):
        _integer_range(name, value, minimum, maximum)
    options = _options(time_limit_ms=time_limit_ms, allocation_bytes=allocation_bytes,
                       single_allocation=single_allocation)
    values = tuple(number(value) for value in (lower, upper, absolute_tolerance))

    def sample(pair, places):
        value = function(_fraction(pair), places)
        return None if value is None else number(value)._handle

    data = _native.integrate(sample, tuple(value._handle for value in values),
        (decimal_places, max_refinements, max_evaluations), options)
    return IntegralResult(data["kind"], _fraction(data["value"]), _fraction(data["error"]),
                          data["evaluations"], data["refinements"])


def find_root(function, lower, upper, *, continuous=False, decimal_places=34,
              x_tolerance="1E-12", residual_tolerance="1E-12", max_iterations=96,
              max_evaluations=192, time_limit_ms=10000,
              allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
    """Search a bracket with the public C bisection kernel.

    function(x, decimal_places) receives an exact Fraction and working fractional
    scale (including a doubled-scale verification call). Return a real number()/
    Result input, or None for a non-real/undefined sample. Decimal inputs denote
    exact finite values; use an approximate Result for unproven computed decimals.
    Supply accurate deterministic samples; callback exceptions propagate unchanged.

    continuous=True is YOUR assertion of continuity on the interval, not an engine
    proof. Without it only proven sampled zeros can be accepted. Width/residual
    thresholds are separate positive absolute tolerances, never certified errors.
    Native work shares one cooperative budget; Python work is not preempted or
    counted as C allocation. No session, expression parser or Python root algorithm.
    """
    return _root_result(_native.find_root(*_root_arguments(function, lower, upper,
        continuous=continuous, decimal_places=decimal_places, x_tolerance=x_tolerance,
        residual_tolerance=residual_tolerance, max_iterations=max_iterations,
        max_evaluations=max_evaluations, time_limit_ms=time_limit_ms,
        allocation_bytes=allocation_bytes, single_allocation=single_allocation)))


def find_roots(function, lower, upper, *, continuous=False, decimal_places=34,
               x_tolerance="1E-12", residual_tolerance="1E-12", max_iterations=96,
               max_root_evaluations=192, segments=128, max_roots=32, max_evaluations=4096,
               time_limit_ms=10000, allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
    """Scan an exact rational grid using the public C multiple-root kernel.

    Callback, precision, continuity, exceptions and budgets follow find_root().
    max_root_evaluations limits one bracket; max_evaluations limits the whole scan.
    Results are sorted, with exact-coordinate deduplication, not tolerance merging.
    kind='finished' means the grid was processed, never that all roots were found.
    Root/evaluation limits return partial diagnostic results; fatal errors do not.
    Exact callback ordinates also supply advisory C quadratic contact proposals.
    Unseen tangencies, domain gaps and several roots inside a cell may be missed.
    """
    for name, value, maximum in (("segments", segments, _native.MAX_ALLOCATION-1),
            ("max_roots", max_roots, _native.MAX_ALLOCATION),
            ("max_evaluations", max_evaluations, _native.MAX_ALLOCATION)):
        _integer_range(name, value, 1, maximum)
    _integer_range("max_root_evaluations", max_root_evaluations, 4, _native.MAX_ALLOCATION)
    args = list(_root_arguments(function, lower, upper, continuous=continuous,
        decimal_places=decimal_places, x_tolerance=x_tolerance, residual_tolerance=residual_tolerance,
        max_iterations=max_iterations, max_evaluations=max_root_evaluations,
        time_limit_ms=time_limit_ms, allocation_bytes=allocation_bytes, single_allocation=single_allocation))
    args[3] += (segments, max_roots, max_evaluations)
    data = _native.find_roots(*args)
    return RootScanResult(data["kind"], tuple(_root_result(root) for root in data["roots"]),
        data["evaluations"], data["iterations"], data["scanned_segments"], data["domain_gaps"],
        data["rejected_brackets"], data["candidate_samples"], data["candidate_roots"], data["candidate_gaps"])


def visit_grid(lower, upper, count, visit, *, time_limit_ms=10000,
               allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
    """Visit count exact Fraction coordinates through the C uniform-grid kernel.

    Both endpoints are included, lower < upper, count >= 2. visit(x, index) is
    synchronous and receives independent Fraction values and zero-based indices.
    Return values are ignored; exceptions stop traversal and propagate unchanged.
    Earlier callback effects remain on failure. No point array is retained by C.
    Native work shares a cooperative budget, including nested NumForge calls;
    Python allocation/work is not a hard native memory/time limit. No precision
    option: coordinate exactness does not establish sampled function accuracy.
    """
    if not callable(visit):
        raise TypeError("visit must be callable as visit(x, index)")
    _integer_range("count", count, 2, _native.MAX_ALLOCATION)
    options = _options(time_limit_ms=time_limit_ms, allocation_bytes=allocation_bytes,
                       single_allocation=single_allocation)
    values = tuple(number(value) for value in (lower, upper))

    def callback(pair, index):
        return visit(_fraction(pair), index)

    return _native.visit_grid(tuple(value._handle for value in values), count, callback, options)


def grid(lower, upper, count, *, time_limit_ms=10000,
         allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
    """Return an immutable tuple of exact Fraction grid coordinates from C.

    Bounds/count follow visit_grid(), with count additionally fitting sys.maxsize.
    This convenience collects all points in Python memory; use visit_grid() to
    process coordinates without retaining a point list. No partial tuple is
    returned on failure. Python object allocation is outside C budget accounting.
    """
    _integer_range("count", count, 2, _sys.maxsize)
    points = []
    visit_grid(lower, upper, count, lambda x, index: points.append(x), time_limit_ms=time_limit_ms,
               allocation_bytes=allocation_bytes, single_allocation=single_allocation)
    return tuple(points)


@dataclass(frozen=True, slots=True)
class PolynomialRoot:
    """Exact center +/- sqrt(radicand), with optional i and rational-complex value.

    value is None for an irrational offset; the descriptor remains exact.
    sign is -1/+1 for a pair, zero for a single root. multiplicity is 1 or 2.
    """
    center: Fraction
    radicand: Fraction
    imaginary: bool
    sign: int
    multiplicity: int
    value: Result | None

    @property
    def exact(self):
        return True

    def __str__(self):
        if self.value is not None:
            return str(self.value)
        operator = "-" if self.sign < 0 else "+"
        return f"{self.center} {operator} {'i*' if self.imaginary else ''}sqrt({self.radicand})"


@dataclass(frozen=True, slots=True)
class PolynomialSolution:
    """Complete exact degree <=2 solution: kind 'finite', 'none' or 'all'."""
    kind: str
    degree: int
    roots: tuple[PolynomialRoot, ...]
    discriminant: Fraction | None


def solve_quadratic(a, b, c, *, time_limit_ms=10000,
                    allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
    """Solve a*x*x+b*x+c=0 in C with exact real coefficients.

    Zero leading coefficients reduce the degree. Irrational/complex descriptors
    and multiplicities are retained; no decimal preview or numerical root search
    is implied. Approximate or complex coefficients raise TypeError. Input copies
    precede the solver's cooperative C budget; descriptors are copied before its
    scope ends. The result no longer borrows C solution/coefficient objects.
    """
    options = _options(34, "half_even", time_limit_ms, allocation_bytes, single_allocation)
    coefficients = tuple(number(value) for value in (a, b, c))
    data = _native.solve(*(value._handle for value in coefficients), options)
    center, radicand = _fraction(data["center"]), _fraction(data["radicand"])
    count = len(data["values"])
    roots = tuple(PolynomialRoot(center, radicand, data["imaginary"], 0 if count == 1 else (-1 if index == 0 else 1),
        data["multiplicities"][index], None if handle is None else Result(handle))
        for index, handle in enumerate(data["values"]))
    return PolynomialSolution(data["kind"], data["degree"], roots, _fraction(data["discriminant"]))


@dataclass(frozen=True, slots=True)
class StatisticsSummary:
    """Exact descriptive statistics copied from C, with no borrowed input state.

    Variance denominators are n (population) and n-1 (sample). A singleton has
    population variance zero and sample variance None, because it is undefined.
    All numeric fields are Fraction; no uncertainty estimate or inference is implied.
    """
    count: int
    sum: Fraction
    minimum: Fraction
    maximum: Fraction
    mean: Fraction
    variance_population: Fraction
    variance_sample: Fraction | None

    @property
    def exact(self):
        return True


@dataclass(frozen=True, slots=True)
class LinearRegression:
    """Exact least-squares coefficients for y=slope*x+intercept, copied from C.

    Exact arithmetic on samples does not establish predictive/measurement accuracy.
    """
    count: int
    slope: Fraction
    intercept: Fraction

    @property
    def exact(self):
        return True


def _paired_inputs(left, right, minimum):
    if isinstance(left, (str, bytes)) or isinstance(right, (str, bytes)):
        raise TypeError("paired samples must be iterables of numeric values")
    x = tuple(number(value)._handle for value in left)
    y = tuple(number(value)._handle for value in right)
    if len(x) < minimum or len(x) != len(y):
        raise ValueError(f"paired datasets must have equal sizes and at least {minimum} observations")
    return x, y


def linear_regression(x, y, *, time_limit_ms=10000,
                      allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
    """Fit the unique unweighted least-squares line using exact C statistics.

    Inputs follow describe(); require two equally sized paired iterables with
    nonconstant x. Insufficient/mismatched samples raise ValueError; constant x
    raises Error('invalid_argument'). Coefficients are Fraction, with no precision
    option. Conversion precedes the C budget; native work/serialization share it.
    """
    options = _options(34, "half_even", time_limit_ms, allocation_bytes, single_allocation)
    x, y = _paired_inputs(x, y, 2)
    data = _native.linear_regression(x, y, options)
    return LinearRegression(data["count"], _fraction(data["slope"]), _fraction(data["intercept"]))


def covariance(left, right, *, sample=False, time_limit_ms=10000,
               allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
    """Exact Fraction covariance of equally sized paired real inputs, through C.

    Inputs follow describe(). Population covariance divides by n; sample=True
    divides by n-1 and requires two pairs. Conversion precedes the C budget;
    native copies, arithmetic and serialization share it. No rounding is used.
    """
    options = _options(34, "half_even", time_limit_ms, allocation_bytes, single_allocation)
    if type(sample) is not bool:
        raise TypeError("sample must be a bool")
    x, y = _paired_inputs(left, right, 2 if sample else 1)
    return _fraction(_native.covariance(x, y, sample, options))


def describe(values, *, time_limit_ms=10000,
             allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
    """Exact summary of a nonempty iterable of real number() inputs, through C.

    Accepts integers, Fraction, finite Decimal/numeric text and exact real Result.
    Binary floats, booleans, complex/approximate values and missing values are
    rejected. Repeated observations count separately. Inputs are converted before
    the summary's C scope; native copies/computation/field serialization share it.
    Python object memory and Fraction conversion are outside C accounting. No
    precision option is needed; native calls retain the GIL.
    """
    options = _options(34, "half_even", time_limit_ms, allocation_bytes, single_allocation)
    if isinstance(values, (str, bytes)):
        raise TypeError("samples must be an iterable of numeric values")
    samples = tuple(number(value) for value in values)
    if not samples:
        raise ValueError("statistics requires at least one sample")
    data = _native.describe(tuple(value._handle for value in samples), options)
    return StatisticsSummary(data["count"], _fraction(data["sum"]), _fraction(data["minimum"]), _fraction(data["maximum"]),
        _fraction(data["mean"]), _fraction(data["variance_population"]), _fraction(data["variance_sample"]))


class Matrix:
    """Owned immutable dense rational matrix, with all arithmetic performed in C.

    Rows must be nonempty, rectangular and contain exact real number() inputs.
    Binary floats, complex values and approximate Result inputs are rejected.
    Input number conversions happen before the matrix call's C budget begins.
    No precision option is needed: all entries/results are exact fractions.
    """
    __slots__ = ("_handle",)

    def __init__(self, rows, *, time_limit_ms=10000,
                 allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
        options = _options(34, "half_even", time_limit_ms, allocation_bytes, single_allocation)
        if isinstance(rows, (str, bytes)):
            raise TypeError("matrix rows must be a sequence of sequences")
        rows = tuple(rows)
        if any(isinstance(row, (str, bytes)) for row in rows):
            raise TypeError("each matrix row must be a sequence of values")
        rows = tuple(tuple(row) for row in rows)
        if not rows or not rows[0] or any(len(row) != len(rows[0]) for row in rows):
            raise ValueError("matrix rows must be nonempty and rectangular")
        values = tuple(number(value) for row in rows for value in row)
        self._handle = _native.matrix_create(len(rows), len(rows[0]), tuple(value._handle for value in values), options)

    @classmethod
    def _from_handle(cls, handle):
        _native.matrix_shape(handle)
        matrix = cls.__new__(cls)
        matrix._handle = handle
        return matrix

    @property
    def shape(self):
        return _native.matrix_shape(self._handle)

    @property
    def exact(self):
        return True

    def _operation(self, name, other=None, *, time_limit_ms=10000,
                   allocation_bytes=128 * 1024 * 1024, single_allocation=8 * 1024 * 1024):
        options = _options(34, "half_even", time_limit_ms, allocation_bytes, single_allocation)
        return _native.matrix_operation(name, self._handle, None if other is None else other._handle, options)

    def to_rows(self, **limits):
        """Copy entries into immutable tuples of Fraction, with optional C limits."""
        return tuple(tuple(_fraction(pair) for pair in row) for row in self._operation("rows", **limits))

    def determinant(self, **limits):
        """Exact Fraction determinant; requires a square matrix, zero if singular."""
        return _fraction(self._operation("determinant", **limits))

    def rank(self, **limits):
        """Exact coefficient rank of a rectangular matrix, with optional C limits."""
        return self._operation("rank", **limits)

    def multiply(self, other, **limits):
        """Exact matrix product; optional time_limit_ms/allocation_bytes/single_allocation."""
        if not isinstance(other, Matrix):
            raise TypeError("matrix multiplication requires another Matrix")
        return self._from_handle(self._operation("multiply", other, **limits))

    def __matmul__(self, other):
        if not isinstance(other, Matrix):
            return NotImplemented
        return self.multiply(other)

    def solve(self, right, **limits):
        """Solve self*x=right for one or more RHS columns, returning a Matrix.

        Requires square self with matching right rows. A singular coefficient
        matrix raises Error(status='singular') for both inconsistent systems and
        infinitely many solutions. Neither input is modified; no partial result.
        """
        if not isinstance(right, Matrix):
            raise TypeError("right-hand sides must be a Matrix")
        return self._from_handle(self._operation("solve", right, **limits))

    def solve_general(self, right, **limits):
        """Complete exact rectangular solve for ONE RHS column, returning LinearSolution.

        kind='none' is a successful inconsistent-system result, not an exception.
        Otherwise all solutions are particular + nullspace*t. If nullity==0,
        nullspace is None and the particular solution is unique. Native matrices
        retain shared ownership of the analysis and outlive inputs/the wrapper.
        """
        if not isinstance(right, Matrix):
            raise TypeError("right-hand side must be a Matrix with one column")
        data = self._operation("solve_general", right, **limits)
        return LinearSolution(data["kind"], data["rank"], data["nullity"], data["pivot_columns"], data["free_columns"],
            None if data["particular"] is None else self._from_handle(data["particular"]),
            None if data["nullspace"] is None else self._from_handle(data["nullspace"]))


@dataclass(frozen=True, slots=True)
class LinearSolution:
    """Complete exact a*x=b analysis. kind is 'none', 'unique' or 'infinite'.

    particular has zero free coordinates, or is None for inconsistent systems.
    nullspace columns form ker(a), including inconsistent systems; None if nullity
    is zero. Pivot/free coefficient columns are zero-based ascending tuples.
    For a consistent system nullspace's free rows form an identity matrix, so its
    columns parameterize all solutions without redundancy. No least-squares result.
    """
    kind: str
    rank: int
    nullity: int
    pivot_columns: tuple[int, ...]
    free_columns: tuple[int, ...]
    particular: Matrix | None
    nullspace: Matrix | None

    @property
    def exact(self):
        return True
