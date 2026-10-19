"""A thin, lossless Python interface to the NumForge C mathematics engine."""
from collections.abc import Mapping
from dataclasses import dataclass
from decimal import Decimal
from fractions import Fraction

from . import _native

__version__ = "0.1.0.dev0"
core_version = _native.core_version
Error = _native.Error
__all__ = ["Error", "Result", "Expression", "PolynomialRoot", "PolynomialSolution",
           "number", "evaluate", "prepare", "sqrt", "solve_quadratic", "Matrix", "core_version"]

_ROUNDING = {"toward_zero": 0, "away_from_zero": 1, "floor": 2,
             "ceiling": 3, "half_up": 4, "half_even": 5}


def _options(precision=34, rounding="half_even", time_limit_ms=5000,
             allocation_bytes=64 * 1024 * 1024, single_allocation=512 * 1024):
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

    def evaluate(self, arguments=(), *, precision=34, rounding="half_even", time_limit_ms=5000,
                 allocation_bytes=64 * 1024 * 1024, single_allocation=512 * 1024):
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


def evaluate(expression, *, variables=None, precision=34, rounding="half_even", time_limit_ms=5000,
             allocation_bytes=64 * 1024 * 1024, single_allocation=512 * 1024):
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


def sqrt(value, *, precision=34, rounding="half_even", time_limit_ms=5000,
         allocation_bytes=64 * 1024 * 1024, single_allocation=512 * 1024):
    """Principal square root through the existing C typed evaluator.

    Exact rational roots stay exact; negative inputs can produce principal complex
    values. Working precision and approximation guarantees match evaluate().
    """
    options = _options(precision, rounding, time_limit_ms, allocation_bytes, single_allocation)
    value = number(value)
    expression = prepare("sqrt(x)", ("x",))
    return Result(_native.evaluate_prepared(expression._handle, (value._handle,), options))


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


def solve_quadratic(a, b, c, *, time_limit_ms=5000,
                    allocation_bytes=64 * 1024 * 1024, single_allocation=512 * 1024):
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


class Matrix:
    """Owned immutable dense rational matrix, with all arithmetic performed in C.

    Rows must be nonempty, rectangular and contain exact real number() inputs.
    Binary floats, complex values and approximate Result inputs are rejected.
    Input number conversions happen before the matrix call's C budget begins.
    No precision option is needed: all entries/results are exact fractions.
    """
    __slots__ = ("_handle",)

    def __init__(self, rows, *, time_limit_ms=5000,
                 allocation_bytes=64 * 1024 * 1024, single_allocation=512 * 1024):
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

    def _operation(self, name, other=None, *, time_limit_ms=5000,
                   allocation_bytes=64 * 1024 * 1024, single_allocation=512 * 1024):
        options = _options(34, "half_even", time_limit_ms, allocation_bytes, single_allocation)
        return _native.matrix_operation(name, self._handle, None if other is None else other._handle, options)

    def to_rows(self, **limits):
        """Copy entries into immutable tuples of Fraction, with optional C limits."""
        return tuple(tuple(_fraction(pair) for pair in row) for row in self._operation("rows", **limits))

    def determinant(self, **limits):
        """Exact Fraction determinant; requires a square matrix, zero if singular."""
        return _fraction(self._operation("determinant", **limits))

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
