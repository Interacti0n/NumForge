"""Run against the installed wheel, without importing repository Python sources."""
import gc
import itertools
import random
import unittest
from dataclasses import FrozenInstanceError
from decimal import Decimal, localcontext
from fractions import Fraction
import sys

import numforge as nf


class PythonAdapterTests(unittest.TestCase):
    def test_shared_exact_inputs_preserve_values_and_result_lifetimes(self):
        expected = Fraction(10**1000 + 1, 3)
        value = nf.number(expected)
        summary = nf.describe([value, value])
        matrix = nf.Matrix([[value, value]])
        self.assertEqual(nf.covariance([value, value], [value, value]), 0)
        approximate = nf.sqrt(2)
        for operation in (lambda: nf.describe([value, approximate]),
                          lambda: nf.Matrix([[value, approximate]])):
            with self.assertRaises(TypeError):
                operation()
            self.assertEqual(value.as_fraction(), expected)
        del value, approximate
        gc.collect()
        self.assertEqual(summary.mean, expected)
        self.assertEqual(matrix.to_rows(), ((expected, expected),))

    def test_native_exact_inputs_survive_reentrant_policy_conversion(self):
        # __index__ can run Python after native adapters validate the input count.
        operations = (
            (lambda values, policy: nf._native.describe(values, policy)["sum"], ("1", "1")),
            (lambda values, policy: nf._native.covariance(values, values, False, policy), ("1", "36")),
            (lambda values, policy: nf.Matrix._from_handle(
                nf._native.matrix_create(1, 2, values, policy)).to_rows(), ((Fraction(1, 3), Fraction(2, 3)),)))
        for operation, expected in operations:
            values = [nf.number(Fraction(1, 3))._handle, nf.number(Fraction(2, 3))._handle]
            class Precision:
                def __index__(self):
                    values.clear()
                    gc.collect()
                    return 34
            result = operation(values, (Precision(), 0, 10000, 128*1024*1024, 8*1024*1024))
            self.assertEqual(values, [])
            self.assertEqual(result, expected)

    def test_lossless_inputs(self):
        for value, expected in ((2**128, Fraction(2**128)),
                                (Fraction(-7, 19), Fraction(-7, 19)),
                                (Decimal("0.10000000000000000000000000000000001"),
                                 Fraction(10000000000000000000000000000000001, 10**35)),
                                ("1E-1000", Fraction(1, 10**1000))):
            with self.subTest(value=type(value).__name__):
                result = nf.number(value)
                self.assertTrue(result.exact)
                self.assertEqual(result.as_fraction(), expected)
                self.assertIs(nf.number(result), result)

    def test_reject_binary_floats_and_coercions(self):
        for value in (0.1, float("nan"), complex(1, 2), True, False, None, [], object()):
            with self.subTest(value=type(value).__name__):
                with self.assertRaises(TypeError):
                    nf.number(value)

    def test_numeric_input_is_not_expression_text(self):
        for text in ("1/3", "sqrt(2)", "NaN", "Infinity", " 2 "):
            with self.subTest(text=text), self.assertRaises(nf.Error):
                nf.number(text)
        with self.assertRaises(ValueError):
            nf.number("2\0+3")
        with self.assertRaises(nf.Error):
            nf.number(Decimal("NaN"))

    def test_thousand_digit_root_independent_decimal_reference(self):
        with localcontext() as context:
            context.prec = 1000
            expected = Decimal(2).sqrt()
        root = nf.sqrt(2, precision=1000)
        self.assertFalse(root.exact)
        self.assertEqual(root.kind, "decimal")
        self.assertEqual(Decimal(str(root)), expected)
        self.assertEqual(len(expected.as_tuple().digits), 1000)

    def test_exact_roots_and_principal_complex_results(self):
        self.assertEqual(nf.sqrt(Fraction(4, 9)).as_fraction(), Fraction(2, 3))
        self.assertTrue(nf.sqrt(0).exact)
        imaginary = nf.sqrt(-1)
        self.assertTrue(imaginary.exact)
        self.assertEqual(imaginary.kind, "complex_rational")
        self.assertEqual(str(imaginary), "i")
        self.assertFalse(nf.sqrt(-2).exact)
        with self.assertRaises(TypeError):
            imaginary.as_fraction()

    def test_evaluate_preserves_exact_values(self):
        self.assertEqual(nf.evaluate("0.1+0.2").as_fraction(), Fraction(3, 10))
        self.assertEqual(nf.evaluate("1/3+1/6").as_fraction(), Fraction(1, 2))
        self.assertEqual(nf.evaluate("2^128").as_fraction(), Fraction(2**128))

    def test_six_rounding_modes_against_independent_reference(self):
        modes = {"toward_zero": "ROUND_DOWN", "away_from_zero": "ROUND_UP",
                 "floor": "ROUND_FLOOR", "ceiling": "ROUND_CEILING",
                 "half_up": "ROUND_HALF_UP", "half_even": "ROUND_HALF_EVEN"}
        with localcontext() as context:
            context.prec = 200
            reference = Decimal(2).sqrt()
            for mode, decimal_mode in modes.items():
                with self.subTest(rounding=mode):
                    expected = reference.quantize(Decimal("1E-33"), rounding=decimal_mode)
                    actual = nf.sqrt(2, precision=34, rounding=mode)
                    self.assertEqual(Decimal(str(actual)), expected)

    def test_prepared_order_mapping_and_lifetime(self):
        names = ["x", "Y"]
        expression = nf.prepare("x*x+Y", names)
        names[0] = "other"
        self.assertEqual(expression.parameters, ("x", "Y"))
        first = expression.evaluate((Fraction(1, 3), Fraction(1, 9)))
        self.assertEqual(first.as_fraction(), Fraction(2, 9))
        second = expression.evaluate({"Y": 4, "x": 3})
        del expression
        gc.collect()
        self.assertEqual(first.as_fraction(), Fraction(2, 9))
        self.assertEqual(second.as_fraction(), 13)

    def test_evaluate_explicit_variables(self):
        result = nf.evaluate("x+y", variables={"y": Fraction(1, 3), "x": Fraction(1, 6)})
        self.assertEqual(result.as_fraction(), Fraction(1, 2))
        expression = nf.prepare("precision+1", ("precision",))
        self.assertEqual(expression.evaluate({"precision": 7}, precision=50).as_fraction(), 8)

    def test_parameter_validation(self):
        for names in (("x", "x"), ("sin",), ("ans",), ("x1",), ("",), ("a"*32,)):
            with self.subTest(names=names), self.assertRaises(nf.Error):
                nf.prepare("1", names)
        with self.assertRaises(TypeError):
            nf.prepare("x", "x")
        with self.assertRaises(TypeError):
            nf.prepare("x", (3,))
        with self.assertRaises(ValueError):
            nf.prepare("x", ("x\0",))
        with self.assertRaises(nf.Error) as caught:
            nf.prepare("x+y", ("x",))
        self.assertEqual(caught.exception.status, "undefined_variable")
        self.assertEqual(caught.exception.offset, 2)

    def test_argument_validation_and_recovery(self):
        expression = nf.prepare("1/x", ("x",))
        with self.assertRaises(nf.Error) as caught:
            expression.evaluate(())
        self.assertEqual(caught.exception.status, "argument_count")
        with self.assertRaises(ValueError):
            expression.evaluate({"y": 2})
        with self.assertRaises(TypeError):
            expression.evaluate("2")
        with self.assertRaises(nf.Error) as caught:
            expression.evaluate((0,))
        self.assertEqual(caught.exception.status, "division_by_zero")
        self.assertEqual(expression.evaluate((2,)).as_fraction(), Fraction(1, 2))

    def test_alias_arguments_and_approximate_provenance(self):
        third = nf.number(Fraction(1, 3))
        expression = nf.prepare("x+y", ("x", "y"))
        self.assertEqual(expression.evaluate((third, third)).as_fraction(), Fraction(2, 3))
        self.assertEqual(third.as_fraction(), Fraction(1, 3))
        approximate = nf.sqrt(2)
        identity = nf.prepare("x", ("x",)).evaluate((approximate,), precision=1000)
        self.assertFalse(identity.exact)
        self.assertEqual(str(identity), str(approximate))
        with self.assertRaises(TypeError):
            identity.as_fraction()

    def test_python_policy_validation(self):
        for kwargs, error in (({"precision": 0}, ValueError), ({"precision": 10001}, ValueError),
                              ({"precision": True}, TypeError), ({"rounding": "unknown"}, ValueError),
                              ({"time_limit_ms": -1}, ValueError), ({"allocation_bytes": -1}, ValueError),
                              ({"single_allocation": 1.5}, TypeError)):
            with self.subTest(kwargs=kwargs), self.assertRaises(error):
                nf.evaluate("2+2", **kwargs)
        self.assertEqual(nf.evaluate("2+2", allocation_bytes=None, single_allocation=None).as_fraction(), 4)

    def test_cooperative_limits_and_recovery(self):
        for kwargs, status in (({"time_limit_ms": 0}, "time_limit"),
                               ({"allocation_bytes": 0}, "memory_limit"),
                               ({"single_allocation": 0}, "memory_limit")):
            with self.subTest(kwargs=kwargs), self.assertRaises(nf.Error) as caught:
                nf.evaluate("sqrt(2)", **kwargs)
            self.assertEqual(caught.exception.status, status)
            self.assertEqual(caught.exception.offset, 0)
        self.assertEqual(nf.evaluate("2+2").as_fraction(), 4)

    def test_utf8_offsets_and_unsupported_capabilities(self):
        with self.assertRaises(nf.Error) as caught:
            nf.evaluate("π+)")
        self.assertEqual(caught.exception.offset, 3)
        for expression in ("rand()", "ans+1", 'qty(1;"m")'):
            with self.subTest(expression=expression), self.assertRaises(nf.Error) as caught:
                nf.evaluate(expression)
            self.assertEqual(caught.exception.status, "unsupported")
        with self.assertRaises(ValueError):
            nf.evaluate("2\0+3")

    def test_rational_quadratic_coefficients(self):
        solution = nf.solve_quadratic(Fraction(1, 2), Fraction(-3, 2), 1)
        self.assertEqual(solution.kind, "finite")
        self.assertEqual(solution.degree, 2)
        self.assertEqual(tuple(root.value.as_fraction() for root in solution.roots), (1, 2))
        self.assertTrue(all(root.exact for root in solution.roots))
        with self.assertRaises(FrozenInstanceError):
            solution.roots[0].multiplicity = 7

    def test_irrational_and_complex_descriptors(self):
        solution = nf.solve_quadratic(1, 0, -2)
        self.assertEqual(solution.discriminant, 8)
        self.assertEqual(tuple(root.sign for root in solution.roots), (-1, 1))
        for root in solution.roots:
            self.assertEqual(root.center, 0)
            self.assertEqual(root.radicand, 2)
            self.assertIsNone(root.value)
            self.assertTrue(root.exact)
            self.assertIn("sqrt(2)", str(root))
        complex_solution = nf.solve_quadratic(1, 0, 1)
        self.assertEqual(tuple(str(root.value) for root in complex_solution.roots), ("-i", "i"))

    def test_degenerate_equations_and_multiplicity(self):
        repeated = nf.solve_quadratic(1, -2, 1)
        self.assertEqual(len(repeated.roots), 1)
        self.assertEqual(repeated.roots[0].multiplicity, 2)
        self.assertEqual(repeated.roots[0].value.as_fraction(), 1)
        linear = nf.solve_quadratic(0, 2, -1)
        self.assertEqual(linear.degree, 1)
        self.assertEqual(linear.roots[0].value.as_fraction(), Fraction(1, 2))
        self.assertEqual(nf.solve_quadratic(0, 0, 0).kind, "all")
        self.assertEqual(nf.solve_quadratic(0, 0, 1).kind, "none")

    def test_solver_rejects_approximate_and_complex_coefficients(self):
        for coefficient in (nf.sqrt(2), nf.sqrt(-1), 0.5):
            with self.subTest(kind=type(coefficient).__name__), self.assertRaises(TypeError):
                nf.solve_quadratic(coefficient, 0, 1)
        self.assertEqual(nf.solve_quadratic("0.5", "-1.5", 1).roots[0].value.as_fraction(), 1)

    def test_solver_limits_recover(self):
        for kwargs, status in (({"time_limit_ms": 0}, "time_limit"), ({"allocation_bytes": 0}, "memory_limit")):
            with self.subTest(kwargs=kwargs), self.assertRaises(nf.Error) as caught:
                nf.solve_quadratic(1, 0, -2, **kwargs)
            self.assertEqual(caught.exception.status, status)
        self.assertEqual(nf.solve_quadratic(1, -2, 1).roots[0].multiplicity, 2)

    def test_repeated_native_lifetime_cleanup(self):
        for index in range(200):
            expression = nf.prepare("x*x+1", ("x",))
            result = expression.evaluate((index,))
            self.assertEqual(result.as_fraction(), index*index+1)
            nf.solve_quadratic(1, -2, 1)
        del expression
        gc.collect()
        self.assertEqual(result.as_fraction(), 199*199+1)

    def test_python_integer_limits_are_not_mutated(self):
        before = sys.get_int_max_str_digits() if hasattr(sys, "get_int_max_str_digits") else None
        nf.sqrt(2, precision=1000)
        if before is not None:
            self.assertEqual(sys.get_int_max_str_digits(), before)


class MatrixTests(unittest.TestCase):
    def test_lossless_rectangular_inputs_and_ownership(self):
        rows = [[Decimal("0.1"), Fraction(1, 3)], [2**128, "1E-1000"]]
        matrix = nf.Matrix(rows)
        rows[0][0] = 9
        self.assertEqual(matrix.shape, (2, 2))
        self.assertTrue(matrix.exact)
        self.assertEqual(matrix.to_rows(), ((Fraction(1, 10), Fraction(1, 3)),
                                           (Fraction(2**128), Fraction(1, 10**1000))))
        with self.assertRaises(AttributeError):
            matrix.shape = (3, 3)

    def test_exact_product_and_swapped_pivot_solve(self):
        a = nf.Matrix([[0, 2], [3, 4]])
        b = nf.Matrix([[7, -4], [15, 7]])
        x = a.solve(b)
        self.assertEqual(a.determinant(), -6)
        self.assertEqual(x.to_rows(), ((Fraction(1, 3), Fraction(5)), (Fraction(7, 2), Fraction(-2))))
        self.assertEqual((a @ x).to_rows(), b.to_rows())
        self.assertEqual(a.solve(a).to_rows(), ((1, 0), (0, 1)))
        del a, b
        gc.collect()
        self.assertEqual(x.to_rows()[0][0], Fraction(1, 3))

    def test_fraction_oracle_for_determinants_and_products(self):
        rng = random.Random(3419)
        for size in (1, 2, 3, 4):
            for _ in range(5):
                rows = [[Fraction(rng.randrange(-7, 8), rng.randrange(1, 8)) for _ in range(size)] for _ in range(size)]
                reference = Fraction(0)
                for permutation in itertools.permutations(range(size)):
                    inversions = sum(permutation[i] > permutation[j] for i in range(size) for j in range(i + 1, size))
                    term = Fraction((-1)**inversions)
                    for i in range(size):
                        term *= rows[i][permutation[i]]
                    reference += term
                matrix = nf.Matrix(rows)
                self.assertEqual(matrix.determinant(), reference)
                expected = tuple(tuple(sum((rows[i][k] * rows[k][j] for k in range(size)), Fraction(0))
                                       for j in range(size)) for i in range(size))
                self.assertEqual((matrix @ matrix).to_rows(), expected)
                if reference:
                    # Independently calculate b=A*x with Python Fraction; C must recover x.
                    x = [[Fraction(i + 1, j + 2) for j in range(2)] for i in range(size)]
                    b = [[sum((rows[i][k] * x[k][j] for k in range(size)), Fraction(0))
                          for j in range(2)] for i in range(size)]
                    self.assertEqual(matrix.solve(nf.Matrix(b)).to_rows(), tuple(map(tuple, x)))

    def test_thousand_digit_cancellation_is_exact(self):
        n = 10**1000
        a = nf.Matrix([[n, n + 1], [n - 1, n]])
        self.assertEqual(a.determinant(), 1)
        self.assertEqual(a.solve(nf.Matrix([[-1], [-1]])).to_rows(), ((1,), (-1,)))

    def test_singular_and_dimension_diagnostics(self):
        singular = nf.Matrix([[1, 2], [2, 4]])
        self.assertEqual(singular.determinant(), 0)
        for right in ([[3], [6]], [[3], [7]]):
            with self.assertRaises(nf.Error) as caught:
                singular.solve(nf.Matrix(right))
            self.assertEqual(caught.exception.status, "singular")
            self.assertEqual(caught.exception.offset, 0)
        rectangular = nf.Matrix([[1, 2, 3]])
        for call in (rectangular.determinant, lambda: rectangular @ rectangular,
                     lambda: rectangular.solve(nf.Matrix([[1]]))):
            with self.assertRaises(nf.Error) as caught:
                call()
            self.assertEqual(caught.exception.status, "dimension_mismatch")

    def test_reject_shapes_and_approximate_entries(self):
        for rows in ([], [[]], [[1], [1, 2]]):
            with self.subTest(rows=rows), self.assertRaises(ValueError):
                nf.Matrix(rows)
        for rows in ("12", ["12"], [[0.1]], [[True]], [[nf.sqrt(2)]], [[nf.sqrt(-1)]]):
            with self.subTest(rows=rows), self.assertRaises(TypeError):
                nf.Matrix(rows)
        a = nf.Matrix([[1]])
        for call in (lambda: a @ 2, lambda: a.multiply([[1]]), lambda: a.solve([[1]])):
            with self.assertRaises(TypeError):
                call()

    def test_matrix_limits_and_recovery(self):
        a = nf.Matrix([[0, 2], [3, 4]])
        for call in (lambda **limits: nf.Matrix([[1]], **limits), a.determinant, a.to_rows,
                     lambda **limits: a.multiply(a, **limits), lambda **limits: a.solve(a, **limits)):
            for limits, status in (({"allocation_bytes": 0}, "memory_limit"), ({"time_limit_ms": 0}, "time_limit")):
                with self.assertRaises(nf.Error) as caught:
                    call(**limits)
                self.assertEqual(caught.exception.status, status)
        self.assertEqual(a.determinant(), -6)
        self.assertEqual(a.solve(a).to_rows(), ((1, 0), (0, 1)))
        with self.assertRaises(TypeError):
            a.determinant(precision=1000)

    def test_partial_matrix_input_allocation_and_recovery(self):
        values = [[Fraction(-1, 3), 2], [5, Fraction(7, 11)]]
        failures = successes = 0
        # Exercise failure after some entries have already been converted, as
        # well as exhaustion inside matrix construction; the next call must work.
        for allocation_bytes in range(256, 32769, 256):
            try:
                matrix = nf.Matrix(values, allocation_bytes=allocation_bytes)
            except nf.Error as error:
                self.assertEqual(error.status, "memory_limit")
                failures += 1
            else:
                self.assertEqual(matrix.to_rows(), tuple(map(tuple, values)))
                successes += 1
        self.assertGreater(failures, 0)
        self.assertGreater(successes, 0)
        self.assertEqual(nf.Matrix(values).determinant(), Fraction(-337, 33))


class LinearSystemTests(unittest.TestCase):
    @staticmethod
    def reference_rank(rows):
        """Independent rank by nonzero minors, not row elimination."""
        for size in range(min(len(rows), len(rows[0])), 0, -1):
            for chosen_rows in itertools.combinations(range(len(rows)), size):
                for chosen_columns in itertools.combinations(range(len(rows[0])), size):
                    determinant = Fraction(0)
                    for permutation in itertools.permutations(range(size)):
                        inversions = sum(permutation[i] > permutation[j] for i in range(size) for j in range(i + 1, size))
                        term = Fraction((-1)**inversions)
                        for i in range(size):
                            term *= rows[chosen_rows[i]][chosen_columns[permutation[i]]]
                        determinant += term
                    if determinant:
                        return size
        return 0

    def test_complete_family_and_canonical_free_coordinates(self):
        a = nf.Matrix([[0, 2, 4], [0, 3, 6]])
        analysis = a.solve_general(nf.Matrix([[10], [15]]))
        self.assertIsInstance(analysis, nf.LinearSolution)
        self.assertTrue(analysis.exact)
        self.assertEqual((analysis.kind, analysis.rank, analysis.nullity), ("infinite", 1, 2))
        self.assertEqual(a.rank(), 1)
        self.assertEqual(analysis.pivot_columns, (1,))
        self.assertEqual(analysis.free_columns, (0, 2))
        self.assertEqual(analysis.particular.to_rows(), ((0,), (5,), (0,)))
        self.assertEqual(analysis.nullspace.to_rows(), ((1, 0), (0, -2), (0, 1)))
        self.assertEqual((a @ analysis.nullspace).to_rows(), ((0, 0), (0, 0)))
        with self.assertRaises(FrozenInstanceError):
            analysis.rank = 2

    def test_overdetermined_unique_and_inconsistent_outcomes(self):
        a = nf.Matrix([[0, 2], [3, 4], [3, 6]])
        unique = a.solve_general(nf.Matrix([[7], [15], [22]]))
        self.assertEqual((unique.kind, unique.rank, unique.nullity), ("unique", 2, 0))
        self.assertEqual(unique.particular.to_rows(), ((Fraction(1, 3),), (Fraction(7, 2),)))
        self.assertEqual(unique.free_columns, ())
        self.assertIsNone(unique.nullspace)
        none = a.solve_general(nf.Matrix([[7], [15], [23]]))
        self.assertEqual((none.kind, none.rank, none.nullity), ("none", 2, 0))
        self.assertIsNone(none.particular)
        self.assertIsNone(none.nullspace)

    def test_zero_matrix_and_inconsistent_kernel(self):
        a = nf.Matrix([[0, 0], [0, 0], [0, 0]])
        infinite = a.solve_general(nf.Matrix([[0], [0], [0]]))
        self.assertEqual((infinite.rank, infinite.nullity, infinite.kind), (0, 2, "infinite"))
        self.assertEqual(infinite.pivot_columns, ())
        self.assertEqual(infinite.free_columns, (0, 1))
        self.assertEqual(infinite.particular.to_rows(), ((0,), (0,)))
        self.assertEqual(infinite.nullspace.to_rows(), ((1, 0), (0, 1)))
        none = a.solve_general(nf.Matrix([[0], [0], [1]]))
        self.assertEqual(none.kind, "none")
        self.assertIsNone(none.particular)
        self.assertEqual(none.nullspace.to_rows(), ((1, 0), (0, 1)))

    def test_independent_minor_rank_and_augmented_consistency_oracle(self):
        rng = random.Random(171019)
        for row_count, columns in ((1, 3), (3, 1), (2, 4), (4, 2), (3, 3), (3, 4)):
            for fixture in range(4):
                rows = [[Fraction(rng.randrange(-5, 6), rng.randrange(1, 6)) for _ in range(columns)] for _ in range(row_count)]
                if fixture == 1 and row_count > 1:
                    rows[-1] = [3 * value for value in rows[0]]
                if fixture == 2:
                    for row in rows:
                        row[0] = Fraction(0)
                if fixture % 2:
                    rhs = [[sum((row[j] * Fraction(j + 1, j + 2) for j in range(columns)), Fraction(0))] for row in rows]
                else:
                    rhs = [[Fraction(rng.randrange(-4, 5))] for _ in rows]
                rank = self.reference_rank(rows)
                augmented_rank = self.reference_rank([row + rhs[i] for i, row in enumerate(rows)])
                expected_kind = "none" if augmented_rank > rank else "unique" if rank == columns else "infinite"
                a = nf.Matrix(rows)
                result = a.solve_general(nf.Matrix(rhs))
                with self.subTest(shape=(row_count, columns), fixture=fixture):
                    self.assertEqual(a.rank(), rank)
                    self.assertEqual((result.kind, result.rank, result.nullity), (expected_kind, rank, columns - rank))
                    prefix_rank = 0
                    pivots = []
                    for j in range(columns):
                        current = self.reference_rank([row[:j + 1] for row in rows])
                        if current > prefix_rank:
                            pivots.append(j)
                        prefix_rank = current
                    self.assertEqual(result.pivot_columns, tuple(pivots))
                    free = tuple(j for j in range(columns) if j not in pivots)
                    self.assertEqual(result.free_columns, free)
                    if expected_kind == "none":
                        self.assertIsNone(result.particular)
                    else:
                        x = result.particular.to_rows()
                        for i, row in enumerate(rows):
                            self.assertEqual(sum((row[j] * x[j][0] for j in range(columns)), Fraction(0)), rhs[i][0])
                        for j in free:
                            self.assertEqual(x[j][0], 0)
                    if not free:
                        self.assertIsNone(result.nullspace)
                    else:
                        basis = result.nullspace.to_rows()
                        self.assertEqual(result.nullspace.shape, (columns, len(free)))
                        self.assertEqual(self.reference_rank(basis), len(free))
                        for row in rows:
                            for c in range(len(free)):
                                self.assertEqual(sum((row[j] * basis[j][c] for j in range(columns)), Fraction(0)), 0)
                        for i, j in enumerate(free):
                            self.assertEqual(basis[j], tuple(Fraction(i == c) for c in range(len(free))))

    def test_thousand_digit_rank_and_solution(self):
        n = 10**1000
        a = nf.Matrix([[n, n + 1], [n - 1, n]])
        result = a.solve_general(nf.Matrix([[-1], [-1]]))
        self.assertEqual(a.rank(), 2)
        self.assertEqual(result.kind, "unique")
        self.assertEqual(result.particular.to_rows(), ((1,), (-1,)))
        self.assertIsNone(result.nullspace)
        self.assertEqual(a.solve(nf.Matrix([[-1], [-1]])).to_rows(), result.particular.to_rows())

    def test_owned_matrix_views_outlive_inputs_and_analysis(self):
        for _ in range(100):
            a = nf.Matrix([[0, 2, 4], [0, 3, 6]])
            analysis = a.solve_general(nf.Matrix([[10], [15]]))
            particular, basis = analysis.particular, analysis.nullspace
            del a, analysis
            self.assertEqual(particular.to_rows(), ((0,), (5,), (0,)))
            del particular
            self.assertEqual(basis.to_rows(), ((1, 0), (0, -2), (0, 1)))
            # Reuse a borrowed matrix as input to every native matrix path.
            self.assertEqual(basis.rank(), 2)
            self.assertEqual(basis.solve_general(basis.multiply(nf.Matrix([[1], [2]]))).kind, "unique")
        a = nf.Matrix([[1, 2], [2, 4]])
        none = a.solve_general(nf.Matrix([[1], [3]]))
        basis = none.nullspace
        del none, a
        gc.collect()
        self.assertEqual(basis.to_rows(), ((-2,), (1,)))

    def test_validation_budgets_and_legacy_solve_compatibility(self):
        a = nf.Matrix([[1, 2], [2, 4]])
        for rhs in (nf.Matrix([[1, 2], [2, 4]]), nf.Matrix([[1]])):
            with self.assertRaises(nf.Error) as caught:
                a.solve_general(rhs)
            self.assertEqual(caught.exception.status, "dimension_mismatch")
        with self.assertRaises(TypeError):
            a.solve_general([[1], [2]])
        with self.assertRaises(TypeError):
            a.rank(precision=100)
        b = nf.Matrix([[1], [2]])
        for call in (a.rank, lambda **limits: a.solve_general(b, **limits)):
            for limits, status in (({"time_limit_ms": 0}, "time_limit"), ({"allocation_bytes": 0}, "memory_limit"),
                                   ({"single_allocation": 0}, "memory_limit")):
                with self.assertRaises(nf.Error) as caught:
                    call(**limits)
                self.assertEqual((caught.exception.status, caught.exception.offset), (status, 0))
        self.assertEqual(a.rank(), 1)
        self.assertEqual(a.solve_general(b).kind, "infinite")
        with self.assertRaises(nf.Error) as caught:
            a.solve(b)
        self.assertEqual(caught.exception.status, "singular")


class StatisticsTests(unittest.TestCase):
    def assert_summary(self, report, values):
        values = tuple(map(Fraction, values))
        count = len(values)
        total = sum(values, Fraction(0))
        # Independent pairwise identity, unlike the C centered-moment algorithm.
        pairwise = sum(((values[i] - values[j])**2 for i in range(count) for j in range(i)), Fraction(0))
        self.assertIsInstance(report, nf.StatisticsSummary)
        self.assertTrue(report.exact)
        self.assertEqual(report.count, count)
        self.assertEqual((report.sum, report.minimum, report.maximum, report.mean),
                         (total, min(values), max(values), total / count))
        self.assertEqual(report.variance_population, pairwise / (count * count))
        self.assertEqual(report.variance_sample, None if count == 1 else pairwise / (count * (count - 1)))
        for field in ("sum", "minimum", "maximum", "mean", "variance_population"):
            self.assertIsInstance(getattr(report, field), Fraction)

    def test_known_population_and_sample_reference(self):
        report = nf.describe([2, 4, 4, 4, 5, 5, 7, 9])
        self.assertEqual((report.count, report.sum, report.minimum, report.maximum, report.mean), (8, 40, 2, 9, 5))
        self.assertEqual((report.variance_population, report.variance_sample), (4, Fraction(32, 7)))
        with self.assertRaises(FrozenInstanceError):
            report.mean = Fraction(0)

    def test_singleton_duplicates_and_nonterminating_moments(self):
        single = nf.describe([Fraction(-9, 5)])
        self.assert_summary(single, [Fraction(-9, 5)])
        self.assertIsNone(single.variance_sample)
        value = nf.number(Fraction(7, 11))
        repeated = nf.describe([value, value, value])
        self.assert_summary(repeated, [Fraction(7, 11)] * 3)
        self.assertEqual(repeated.variance_sample, 0)
        report = nf.describe([0, 0, 1])
        self.assertEqual((report.mean, report.variance_population, report.variance_sample),
                         (Fraction(1, 3), Fraction(2, 9), Fraction(1, 3)))

    def test_lossless_iterable_inputs_and_independent_lifetime(self):
        values = [2**128, Decimal("0.1"), Fraction(-7, 19), "1E-1000", nf.number(Fraction(1, 3))]
        report = nf.describe(value for value in values)
        values.clear()
        gc.collect()
        self.assert_summary(report, [2**128, Fraction(1, 10), Fraction(-7, 19), Fraction(1, 10**1000), Fraction(1, 3)])

    def test_random_rational_pairwise_reference(self):
        rng = random.Random(191023)
        for count in (1, 2, 3, 8, 16):
            for fixture in range(5):
                values = [Fraction(rng.randrange(-50, 51), rng.randrange(1, 16)) for _ in range(count)]
                with self.subTest(count=count, fixture=fixture):
                    self.assert_summary(nf.describe(values), values)

    def test_thousand_digit_offset_and_tiny_spread(self):
        n = 10**1000
        report = nf.describe([n, n + 1, n + 2])
        self.assertEqual((report.sum, report.mean, report.minimum, report.maximum), (3*n+3, n+1, n, n+2))
        self.assertEqual((report.variance_population, report.variance_sample), (Fraction(2, 3), 1))
        tiny = nf.describe([Fraction(i, n) for i in (1, 2, 3)])
        self.assertEqual(tiny.mean, Fraction(2, n))
        self.assertEqual((tiny.variance_population, tiny.variance_sample), (Fraction(2, 3*n*n), Fraction(1, n*n)))

    def test_translation_scaling_and_permutation_identities(self):
        values = [Fraction(-2, 3), Fraction(1, 5), Fraction(7, 11), Fraction(2)]
        base = nf.describe(values)
        self.assertEqual(base, nf.describe(reversed(values)))
        shift, scale = Fraction(123, 5), Fraction(-7, 11)
        translated = nf.describe([value + shift for value in values])
        self.assertEqual(translated.mean, base.mean + shift)
        self.assertEqual((translated.variance_population, translated.variance_sample), (base.variance_population, base.variance_sample))
        scaled = nf.describe([value * scale for value in values])
        self.assertEqual(scaled.mean, base.mean * scale)
        self.assertEqual((scaled.minimum, scaled.maximum), (base.maximum * scale, base.minimum * scale))
        self.assertEqual((scaled.variance_population, scaled.variance_sample),
                         (base.variance_population * scale**2, base.variance_sample * scale**2))

    def test_validation_limits_and_failure_recovery(self):
        with self.assertRaises(ValueError):
            nf.describe([])
        for values in ("123", b"123", [True], [0.1], [complex(1, 2)], [None], [nf.sqrt(2)], [nf.sqrt(-1)]):
            with self.subTest(values=values), self.assertRaises(TypeError):
                nf.describe(values)
        for value in (Decimal("NaN"), Decimal("Infinity"), "NaN"):
            with self.assertRaises(nf.Error):
                nf.describe([value])
        with self.assertRaises(TypeError):
            nf.describe([1], precision=100)
        for limits, status in (({"time_limit_ms": 0}, "time_limit"), ({"allocation_bytes": 0}, "memory_limit"),
                               ({"single_allocation": 0}, "memory_limit")):
            with self.assertRaises(nf.Error) as caught:
                nf.describe([1, 2, 3], **limits)
            self.assertEqual((caught.exception.status, caught.exception.offset), (status, 0))
        # Exercise failures during copying, computation and field serialization.
        complete = False
        for allocation_bytes in range(1024, 65537, 1024):
            try:
                report = nf.describe([1, 2, 3], allocation_bytes=allocation_bytes)
            except nf.Error as error:
                self.assertEqual(error.status, "memory_limit")
            else:
                self.assert_summary(report, [1, 2, 3])
                complete = True
                break
        self.assertTrue(complete)
        self.assert_summary(nf.describe([Fraction(-1, 3), Fraction(1, 6), Fraction(2, 3)]),
                            [Fraction(-1, 3), Fraction(1, 6), Fraction(2, 3)])


class CovarianceTests(unittest.TestCase):
    @staticmethod
    def reference(x, y, sample=False):
        # Independent pairwise identity, without centered moment accumulation.
        n = len(x)
        return sum(((x[i] - x[j]) * (y[i] - y[j])
                    for i in range(n) for j in range(i)), Fraction(0)) / (n * (n - 1 if sample else n))

    def test_known_pairing_sign_and_singletons(self):
        self.assertEqual(nf.covariance([1, 2, 3], [3, 1, -1]), Fraction(-4, 3))
        self.assertEqual(nf.covariance([1, 2, 3], [3, 1, -1], sample=True), -2)
        self.assertEqual(nf.covariance([7], [-9]), 0)
        self.assertEqual(nf.covariance([7, 7, 7], [1, -2, 8], sample=True), 0)
        self.assertEqual(nf.covariance([1, 2, 3], [1, -2, 1]), 0)
        self.assertIsInstance(nf.covariance([1], [1]), Fraction)

    def test_fraction_reference_and_identities(self):
        rng = random.Random(201026)
        for n in (2, 3, 8, 16):
            for fixture in range(5):
                x = [Fraction(rng.randrange(-50, 51), rng.randrange(1, 16)) for _ in range(n)]
                y = [Fraction(rng.randrange(-50, 51), rng.randrange(1, 16)) for _ in range(n)]
                for sample in (False, True):
                    with self.subTest(n=n, fixture=fixture, sample=sample):
                        expected = self.reference(x, y, sample)
                        self.assertEqual(nf.covariance(x, y, sample=sample), expected)
                        self.assertEqual(nf.covariance(y, x, sample=sample), expected)
                        self.assertEqual(nf.covariance(reversed(x), reversed(y), sample=sample), expected)
                        self.assertEqual(nf.covariance([3*v+17 for v in x], [-2*v-9 for v in y], sample=sample), -6*expected)
                        report = nf.describe(x)
                        self.assertEqual(nf.covariance(x, x, sample=sample),
                                         report.variance_sample if sample else report.variance_population)

    def test_thousand_digit_offsets_spreads_and_lossless_iterables(self):
        n = 10**1000
        x, y = [n, n+1, n+2], [Fraction(3, n), Fraction(2, n), Fraction(1, n)]
        self.assertEqual(nf.covariance(x, y, sample=True), Fraction(-1, n))
        self.assertEqual(nf.covariance(y, y), Fraction(2, 3*n*n))
        x = [Decimal("0.1"), "0.25", nf.number(7)]
        result = nf.covariance((v for v in x), [1, Fraction(2, 3), "-2"])
        x.clear(); gc.collect()
        self.assertEqual(result, self.reference([Fraction(1, 10), Fraction(1, 4), Fraction(7)],
                                               [Fraction(1), Fraction(2, 3), Fraction(-2)]))

    def test_validation_limits_serialization_and_recovery(self):
        for x, y, options in (([], [], {}), ([1], [1, 2], {}), ([1], [1], {"sample": True})):
            with self.assertRaises(ValueError):
                nf.covariance(x, y, **options)
        for bad in ("123", b"123", [True], [0.1], [complex(1, 2)], [None], [nf.sqrt(2)], [nf.sqrt(-1)]):
            for x, y in ((bad, [1]), ([1], bad)):
                with self.assertRaises(TypeError):
                    nf.covariance(x, y)
        for sample in (0, 1, None, "sample"):
            with self.assertRaises(TypeError):
                nf.covariance([1, 2], [1, 2], sample=sample)
        with self.assertRaises(TypeError):
            nf.covariance([1], [1], precision=100)
        for options, expected in (({"time_limit_ms": 0}, "time_limit"),
                                  ({"allocation_bytes": 0}, "memory_limit"),
                                  ({"single_allocation": 0}, "memory_limit")):
            with self.assertRaises(nf.Error) as caught:
                nf.covariance([1, 2, 3], [3, 1, -1], **options)
            self.assertEqual((caught.exception.status, caught.exception.offset), (expected, 0))
        complete = False
        for allocation_bytes in range(1024, 131073, 1024):
            try:
                value = nf.covariance([1, 2, 3], [3, 1, -1], allocation_bytes=allocation_bytes)
            except nf.Error as error:
                self.assertEqual(error.status, "memory_limit")
            else:
                self.assertEqual(value, Fraction(-4, 3)); complete = True; break
        self.assertTrue(complete)
        self.assertEqual(nf.covariance([1, 2, 3], [3, 1, -1]), Fraction(-4, 3))


class NumericalRootTests(unittest.TestCase):
    @staticmethod
    def square(x, places):
        return x*x-2

    def test_exact_coordinates_and_owned_frozen_diagnostics(self):
        calls = []
        def linear(x, places):
            self.assertIsInstance(x, Fraction)
            calls.append(places)
            return x-Fraction(1, 2)
        report = nf.find_root(linear, 0, 1)
        self.assertEqual((report.kind, report.x, report.residual, report.exact_zero),
                         ("found", Fraction(1, 2), Decimal(0), True))
        self.assertEqual((report.iterations, report.evaluations), (1, len(calls)))
        with self.assertRaises(FrozenInstanceError):
            report.x = 0
        del linear
        gc.collect()
        self.assertEqual(report.x, Fraction(1, 2))
        endpoint = nf.find_root(lambda x, p: x-1, 1, 2)
        self.assertEqual((endpoint.x, endpoint.iterations, endpoint.evaluations), (1, 0, 1))

    def test_independent_high_precision_reference_and_verification_scale(self):
        scales = []
        def square(x, places):
            scales.append(places)
            return x*x-2
        report = nf.find_root(square, 1, 2, continuous=True, decimal_places=100,
            x_tolerance="1E-70", residual_tolerance="1E-70", max_iterations=260,
            max_evaluations=300, time_limit_ms=10000, allocation_bytes=256*1024*1024)
        with localcontext() as context:
            context.prec = 110
            reference = Fraction(Decimal(2).sqrt())
        self.assertEqual(report.kind, "found")
        self.assertFalse(report.exact_zero)
        self.assertLess(abs(report.x-reference), Fraction(1, 10**70))
        self.assertLessEqual(report.residual, Decimal("1E-70"))
        self.assertEqual(scales[-1], 200)
        self.assertEqual(len(scales), report.evaluations)
        self.assertLessEqual(report.iterations, 260)
        self.assertLessEqual(report.evaluations, 300)

    def test_thousand_digit_offset_and_tiny_span_are_lossless(self):
        for lower, step in ((Fraction(10**1000), Fraction(1)), (Fraction(0), Fraction(1, 10**1000))):
            target = lower+step/2
            result = nf.find_root(lambda x, p: x-target, lower, lower+step, decimal_places=1010)
            self.assertEqual((result.kind, result.x, result.exact_zero), ("found", target, True))

    def test_outcomes_are_not_exceptions_or_no_root_proofs(self):
        self.assertEqual(nf.find_root(self.square, 1, 2).kind, "domain_unverified")
        self.assertEqual(nf.find_root(lambda x, p: x*x+1, 0, 1).kind, "not_bracketed")
        self.assertEqual(nf.find_root(lambda x, p: None, 0, 1).kind, "domain_gap")
        limited = nf.find_root(self.square, 1, 2, continuous=True, max_iterations=1)
        self.assertEqual((limited.kind, limited.x, limited.residual), ("iteration_limit", None, None))
        self.assertFalse(limited.exact_zero)
        unstable = nf.find_root(lambda x, p: self.square(x, p) if p == 34 else 1,
                                1, 2, continuous=True)
        self.assertEqual(unstable.kind, "precision_limit")

    def test_sample_provenance_and_callback_exceptions_are_preserved(self):
        approximate = nf.sqrt(2)
        result = nf.find_root(lambda x, p: approximate, 0, 1)
        self.assertEqual(result.kind, "not_bracketed")
        rounded_zero = nf.evaluate("sqrt(2)-sqrt(2)")
        self.assertFalse(rounded_zero.exact)
        result = nf.find_root(lambda x, p: rounded_zero, 0, 1, continuous=True)
        self.assertEqual((result.kind, result.x, result.exact_zero), ("not_bracketed", None, False))
        for bad in (0.5, True, [], nf.sqrt(-1)):
            with self.subTest(bad=bad), self.assertRaises(TypeError):
                nf.find_root(lambda x, p: bad, 0, 1)
        for error in (ValueError("callback failed"), KeyboardInterrupt()):
            def failing(x, p):
                raise error
            with self.assertRaises(type(error)) as caught:
                nf.find_root(failing, 0, 1)
            self.assertIs(caught.exception, error)
        # A callback may invoke other NumForge operations under the same scope.
        def nested(x, p):
            inner = nf.find_root(lambda t, q: t, 0, 1)
            return nf.evaluate("x-0.5", variables={"x": x-inner.x}, precision=p)
        self.assertTrue(nf.find_root(nested, 0, 1).exact_zero)

    def test_prepared_engine_callback_shares_the_search_scope(self):
        expression = nf.prepare("cos(x)-x", ("x",))
        def sample(x, places):
            # Additional significant digits accommodate cancellation in cos(x)-x.
            return expression.evaluate((x,), precision=places+24)
        result = nf.find_root(sample, 0, 1, continuous=True)
        self.assertEqual(result.kind, "found")
        self.assertFalse(result.exact_zero)
        reference = Fraction(Decimal("0.7390851332151606416553120876738734040134117589007574649656806357732846548835475945993761069317665318"))
        self.assertLess(abs(result.x-reference), Fraction(1, 10**12))
        self.assertLessEqual(result.residual, Decimal("1E-12"))
        # A transcendental path retains approximate provenance even at cos(0)=1.
        self.assertEqual(Decimal(str(expression.evaluate((0,)))), 1)

    def test_validation_and_resource_failure_recovery(self):
        for kwargs, error in (({"continuous": 1}, TypeError), ({"decimal_places": True}, TypeError),
                ({"decimal_places": 0}, ValueError), ({"decimal_places": 10001}, ValueError),
                ({"max_iterations": 0}, ValueError), ({"max_evaluations": 3}, ValueError),
                ({"residual_tolerance": Fraction(1, 3)}, TypeError), ({"x_tolerance": 0.1}, TypeError)):
            with self.subTest(kwargs=kwargs), self.assertRaises(error):
                nf.find_root(self.square, 1, 2, **kwargs)
        for lower, upper in ((1, 1), (2, 1)):
            with self.assertRaises(nf.Error) as caught:
                nf.find_root(self.square, lower, upper)
            self.assertEqual(caught.exception.status, "invalid_argument")
        for bounds in ((0.1, 1), (nf.sqrt(2), 2), (nf.sqrt(-1), 2)):
            with self.subTest(bounds=bounds), self.assertRaises(TypeError):
                nf.find_root(self.square, *bounds)
        with self.assertRaises(TypeError):
            nf.find_root("x*x-2", 1, 2)
        for kwargs in ({"x_tolerance": 0}, {"residual_tolerance": "-1"}, {"residual_tolerance": "NaN"}):
            with self.assertRaises(nf.Error):
                nf.find_root(self.square, 1, 2, **kwargs)
        for limits, status in (({"time_limit_ms": 0}, "time_limit"), ({"allocation_bytes": 0}, "memory_limit"),
                               ({"single_allocation": 0}, "memory_limit")):
            calls = []
            with self.assertRaises(nf.Error) as caught:
                nf.find_root(lambda x, p: calls.append(x), 0, 1, **limits)
            self.assertEqual((caught.exception.status, caught.exception.offset), (status, 0))
            self.assertFalse(calls)
        success = False
        for size in range(1024, 65537, 1024):
            try:
                result = nf.find_root(lambda x, p: x, 0, 1, allocation_bytes=size)
            except nf.Error as error:
                self.assertEqual(error.status, "memory_limit")
            else:
                self.assertEqual((result.x, result.exact_zero), (0, True))
                success = True
                break
        self.assertTrue(success)
        self.assertTrue(nf.find_root(lambda x, p: x, 0, 1).exact_zero)


class NumericalScanTests(unittest.TestCase):
    def test_sorted_unique_exact_roots_and_owned_immutable_values(self):
        calls = []
        def cubic(x, places):
            self.assertIsInstance(x, Fraction)
            calls.append((x, places))
            return x*(x-2)*(x+3)
        report = nf.find_roots(cubic, -4, 4, segments=8)
        self.assertEqual((report.kind, report.count, report.complete), ("finished", 3, False))
        self.assertEqual(tuple(root.x for root in report.roots), (-3, 0, 2))
        self.assertTrue(all(root.kind == "found" and root.exact_zero and root.residual == 0 for root in report.roots))
        self.assertEqual(report.evaluations, len(calls))
        self.assertEqual(report.scanned_segments, 8)
        self.assertIsInstance(report.roots, tuple)
        with self.assertRaises(FrozenInstanceError):
            report.kind = "complete"
        root = report.roots[0]
        del report, cubic
        gc.collect()
        self.assertEqual(root.x, -3)
        with self.assertRaises(FrozenInstanceError):
            root.x = 0

    def test_exact_contact_proposals_and_no_tolerance_merging(self):
        target = Fraction(1, 3)
        contact = nf.find_roots(lambda x, p: (x-target)**2, 0, 1, segments=1)
        self.assertEqual(tuple(root.x for root in contact.roots), (target,))
        self.assertTrue(contact.roots[0].exact_zero)
        self.assertEqual(contact.candidate_roots, 1)
        other = target+Fraction(1, 10**14)
        # Both are sampled exact zeros, closer than the default width tolerance.
        close = nf.find_roots(lambda x, p: (x-target)*(x-other), target, other, segments=1)
        self.assertEqual(tuple(root.x for root in close.roots), (target, other))
        self.assertTrue(all(root.exact_zero for root in close.roots))
        minimum = nf.find_roots(lambda x, p: (x-target)**2+Fraction(1, 10**80), 0, 1, segments=1)
        self.assertEqual((minimum.kind, minimum.count), ("finished", 0))

    def test_independent_high_precision_pair_and_work_counts(self):
        calls = []
        def square(x, places):
            calls.append(places)
            return x*x-2
        report = nf.find_roots(square, -2, 2, continuous=True, segments=4,
            decimal_places=50, x_tolerance="1E-35", residual_tolerance="1E-35",
            max_iterations=140, max_root_evaluations=150, max_evaluations=1000)
        with localcontext() as context:
            context.prec = 80
            reference = Fraction(Decimal(2).sqrt())
        self.assertEqual((report.kind, report.count), ("finished", 2))
        for root, expected in zip(report.roots, (-reference, reference)):
            self.assertFalse(root.exact_zero)
            self.assertLess(abs(root.x-expected), Fraction(1, 10**35))
            self.assertLessEqual(root.residual, Decimal("1E-35"))
        self.assertIn(100, calls)
        self.assertEqual(report.evaluations, len(calls))
        self.assertLessEqual(report.evaluations, 1000)
        self.assertGreaterEqual(report.iterations, sum(root.iterations for root in report.roots))
        expression = nf.prepare("cos(x)-x", ("x",))
        approximate = nf.find_roots(lambda x, p: expression.evaluate((x,), precision=p+24),
                                     0, 1, continuous=True, segments=1)
        self.assertEqual(approximate.count, 1)
        self.assertFalse(approximate.roots[0].exact_zero)
        self.assertLess(abs(approximate.roots[0].x-Fraction(Decimal("0.7390851332151606416553120876738734"))),
                        Fraction(1, 10**12))

    def test_thousand_digit_contacts_preserve_original_ordinates(self):
        for low, step in ((Fraction(10**1000), Fraction(1)), (Fraction(0), Fraction(1, 10**1000))):
            target = low+step/3
            report = nf.find_roots(lambda x, p: (x-target)**2, low, low+step, segments=1, decimal_places=1010)
            self.assertEqual(tuple(root.x for root in report.roots), (target,))
            self.assertTrue(report.roots[0].exact_zero)

    def test_explicit_partial_limits_domain_gaps_and_incomplete_discovery(self):
        capped = nf.find_roots(lambda x, p: 0, 0, 1, segments=10, max_roots=3)
        self.assertEqual((capped.kind, capped.count, capped.complete), ("root_limit", 3, False))
        exhausted = nf.find_roots(lambda x, p: x*x-2, 0, 2, max_evaluations=2)
        self.assertEqual(exhausted.kind, "evaluation_limit")
        self.assertLessEqual(exhausted.evaluations, 2)
        self.assertFalse(exhausted.complete)
        gaps = nf.find_roots(lambda x, p: None if x == 0 else 1/x, -1, 1, segments=2)
        self.assertEqual((gaps.count, gaps.domain_gaps), (0, 2))
        unverified = nf.find_roots(lambda x, p: x*x-2, 1, 2, segments=1)
        self.assertEqual(unverified.count, 0)
        self.assertGreater(unverified.rejected_brackets, 0)
        missed = nf.find_roots(lambda x, p: (x-Fraction(1, 7))**4, 0, 1, segments=1)
        self.assertEqual((missed.kind, missed.count, missed.complete), ("finished", 0, False))

    def test_callback_failures_after_discovery_discard_partial_results(self):
        original = nf.find_roots(lambda x, p: x, 0, 1, segments=1)
        for error in (ValueError("after first root"), KeyboardInterrupt()):
            calls = []
            def failing(x, p):
                calls.append(x)
                if len(calls) > 1:
                    raise error
                return 0
            with self.assertRaises(type(error)) as caught:
                nf.find_roots(failing, 0, 1, segments=1)
            self.assertIs(caught.exception, error)
            self.assertEqual(len(calls), 2)
        self.assertEqual(original.roots[0].x, 0)
        with self.assertRaises(TypeError):
            nf.find_roots(lambda x, p: nf.sqrt(-1), 0, 1, segments=1)
        rounded_zero = nf.evaluate("sqrt(2)-sqrt(2)")
        approximate = nf.find_roots(lambda x, p: rounded_zero, 0, 1, segments=1, continuous=True)
        self.assertEqual(approximate.count, 0)

    def test_validation_budget_failures_and_recovery(self):
        for kwargs, error in (({"segments": True}, TypeError), ({"segments": 0}, ValueError),
                ({"max_roots": 0}, ValueError), ({"max_evaluations": 0}, ValueError),
                ({"max_root_evaluations": 3}, ValueError), ({"continuous": "yes"}, TypeError)):
            with self.subTest(kwargs=kwargs), self.assertRaises(error):
                nf.find_roots(lambda x, p: x, 0, 1, **kwargs)
        for kwargs in ({"x_tolerance": 0}, {"residual_tolerance": "0"}):
            with self.assertRaises(nf.Error):
                nf.find_roots(lambda x, p: x, 0, 1, **kwargs)
        for limits, status in (({"time_limit_ms": 0}, "time_limit"), ({"allocation_bytes": 0}, "memory_limit"),
                               ({"single_allocation": 0}, "memory_limit")):
            calls = []
            with self.assertRaises(nf.Error) as caught:
                nf.find_roots(lambda x, p: calls.append(x), 0, 1, **limits)
            self.assertEqual((caught.exception.status, caught.exception.offset), (status, 0))
            self.assertFalse(calls)
        recovered = False
        for size in range(4096, 262145, 4096):
            try:
                result = nf.find_roots(lambda x, p: x*(x-1), 0, 1, segments=1, allocation_bytes=size)
            except nf.Error as error:
                self.assertEqual(error.status, "memory_limit")
            else:
                self.assertEqual(tuple(root.x for root in result.roots), (0, 1))
                recovered = True
                break
        self.assertTrue(recovered)
        self.assertEqual(nf.find_roots(lambda x, p: x, 0, 1, segments=1).count, 1)


class NumericalGridTests(unittest.TestCase):
    def test_exact_recurring_coordinates_and_visit_order(self):
        expected = (Fraction(0), Fraction(1, 3), Fraction(2, 3), Fraction(1))
        coordinates = nf.grid(0, 1, 4)
        self.assertEqual(coordinates, expected)
        self.assertIsInstance(coordinates, tuple)
        self.assertTrue(all(type(value) is Fraction for value in coordinates))
        visits = []
        class Ignored:
            def __bool__(self):
                raise AssertionError("visit return values must not control traversal")
        def visit(x, index):
            visits.append((x, index))
            return Ignored()
        self.assertIsNone(nf.visit_grid(0, 1, 4, visit))
        self.assertEqual(visits, list(zip(expected, range(4))))
        self.assertEqual(sum(coordinates), 2)

    def test_independent_exact_spacing_at_thousand_digit_offsets_and_scales(self):
        cases = [(Fraction(-7, 13), Fraction(11, 17)),
                 (Fraction(10**1000), Fraction(10**1000)+1),
                 (Fraction(10**1000), Fraction(10**1000)+Fraction(1, 10**1000)),
                 (Fraction(0), Fraction(1, 10**1000))]
        for lower, upper in cases:
            for count in (2, 3, 7, 17):
                with self.subTest(count=count, magnitude=len(str(lower.numerator))):
                    values = nf.grid(lower, upper, count)
                    self.assertEqual((len(values), values[0], values[-1]), (count, lower, upper))
                    for i, x in enumerate(values):
                        self.assertEqual((x-lower)*(count-1), i*(upper-lower))
                    self.assertEqual(2*sum(values), count*(lower+upper))

    def test_input_policy_nested_calls_and_retained_coordinate_lifetime(self):
        lower, upper = nf.number(Decimal("0.1")), nf.evaluate("13/10")
        expected = tuple(Fraction(x, 10) for x in (1, 5, 9, 13))
        values = nf.grid(lower, upper, 4)
        self.assertEqual(values, expected)
        visits = []
        def nested(x, index):
            self.assertEqual(nf.grid(0, 1, 2), (0, 1))
            self.assertEqual(nf.evaluate("x+1", variables={"x": x}).as_fraction(), x+1)
            visits.append(x)
        nf.visit_grid(lower, upper, 4, nested)
        del lower, upper
        gc.collect()
        self.assertEqual(tuple(visits), expected)
        self.assertEqual(values, expected)

    def test_callback_exceptions_preserve_identity_and_earlier_effects(self):
        for error in (ValueError("stop after two coordinates"), KeyboardInterrupt()):
            visits = []
            def visit(x, index):
                visits.append((x, index))
                if index == 1:
                    raise error
            with self.assertRaises(type(error)) as caught:
                nf.visit_grid(0, 1, 4, visit)
            self.assertIs(caught.exception, error)
            self.assertEqual(visits, [(0, 0), (Fraction(1, 3), 1)])
        # Streaming counts can exceed a Python container's signed length range.
        calls = []
        sentinel = ValueError("bounded early stop")
        def stop(x, index):
            calls.append(index)
            raise sentinel
        with self.assertRaises(ValueError) as caught:
            nf.visit_grid(0, 1, sys.maxsize+1, stop)
        self.assertIs(caught.exception, sentinel)
        self.assertEqual(calls, [0])
        self.assertEqual(nf.grid(0, 1, 2), (0, 1))

    def test_validation_and_lossless_bound_requirements(self):
        for count, error in ((True, TypeError), (2.0, TypeError), ("4", TypeError),
                             (1, ValueError), (-2, ValueError), (2**130, ValueError)):
            with self.subTest(count=count), self.assertRaises(error):
                nf.grid(0, 1, count)
        with self.assertRaises(ValueError):
            nf.grid(0, 1, sys.maxsize+1)
        with self.assertRaises(TypeError):
            nf.visit_grid(0, 1, 4, None)
        with self.assertRaises(TypeError):
            nf.grid(0, 1, 4, precision=100)
        for lower, upper in ((0.1, 1), (nf.sqrt(2), 2), (nf.sqrt(-1), 2)):
            with self.assertRaises(TypeError):
                nf.grid(lower, upper, 4)
        for lower, upper in ((1, 1), (2, 1)):
            calls = []
            with self.assertRaises(nf.Error) as caught:
                nf.visit_grid(lower, upper, 4, lambda x, i: calls.append(x))
            self.assertEqual((caught.exception.status, caught.exception.offset), ("invalid_argument", 0))
            self.assertFalse(calls)

    def test_budget_failure_partial_visits_and_recovery(self):
        for limits, status in (({"time_limit_ms": 0}, "time_limit"), ({"allocation_bytes": 0}, "memory_limit"),
                               ({"single_allocation": 0}, "memory_limit")):
            calls = []
            with self.assertRaises(nf.Error) as caught:
                nf.visit_grid(0, 1, 4, lambda x, i: calls.append(x), **limits)
            self.assertEqual((caught.exception.status, caught.exception.offset), (status, 0))
            self.assertFalse(calls)
            with self.assertRaises(nf.Error):
                nf.grid(0, 1, 4, **limits)
        partial = False
        recovered = False
        for size in range(1024, 65537, 1024):
            calls = []
            try:
                nf.visit_grid(0, 1, 20, lambda x, i: calls.append((x, i)), allocation_bytes=size)
            except nf.Error as error:
                self.assertEqual(error.status, "memory_limit")
                partial |= bool(calls)
            else:
                self.assertEqual(len(calls), 20)
                recovered = True
                break
            for x, i in calls:
                self.assertEqual(19*x, i)
        self.assertTrue(partial)
        self.assertTrue(recovered)
        self.assertEqual(nf.grid(0, 1, 2), (0, 1))


class LinearRegressionTests(unittest.TestCase):
    @staticmethod
    def reference(x, y):
        # Pairwise differences provide an independent Fraction oracle.
        cross = sum(((x[i]-x[j])*(y[i]-y[j]) for i in range(len(x)) for j in range(i)), Fraction(0))
        square = sum(((x[i]-x[j])**2 for i in range(len(x)) for j in range(i)), Fraction(0))
        slope = cross / square
        return slope, (sum(y, Fraction(0)) - slope*sum(x, Fraction(0))) / len(x)

    def test_known_lines_and_owned_frozen_results(self):
        for x, y, expected in (([1, 2, 3], [1, 2, 2], (Fraction(1, 2), Fraction(2, 3))),
                               ([1, 2, 3], [3, 1, -1], (Fraction(-2), Fraction(5))),
                               ([1, 1, 2], [7, 7, 7], (Fraction(0), Fraction(7))),
                               ([Fraction(1, 3), Fraction(2, 3)], [Fraction(3, 7), Fraction(1, 7)],
                                (Fraction(-6, 7), Fraction(5, 7)))):
            result = nf.linear_regression(iter(x), iter(y))
            self.assertIsInstance(result, nf.LinearRegression)
            self.assertEqual((result.count, result.slope, result.intercept, result.exact), (len(x), *expected, True))
            self.assertIsInstance(result.slope, Fraction)
            self.assertIsInstance(result.intercept, Fraction)
            with self.assertRaises(FrozenInstanceError):
                result.slope = Fraction(9)
        x = [Decimal(".1"), "0.25", nf.number(7)]
        result = nf.linear_regression(x, [1, 2, 3]); x.clear(); gc.collect()
        self.assertEqual((result.slope, result.intercept), self.reference(
            [Fraction(1, 10), Fraction(1, 4), Fraction(7)], [Fraction(1), Fraction(2), Fraction(3)]))

    def test_random_fraction_references_and_residual_normal_equations(self):
        rng = random.Random(20261020)
        for n in (2, 3, 8, 16):
            for fixture in range(5):
                x = [Fraction(rng.randrange(-50, 51), rng.randrange(1, 16)) for _ in range(n)]
                y = [Fraction(rng.randrange(-50, 51), rng.randrange(1, 16)) for _ in range(n)]
                if len(set(x)) == 1:
                    continue
                with self.subTest(n=n, fixture=fixture):
                    expected = self.reference(x, y)
                    result = nf.linear_regression(x, y)
                    self.assertEqual((result.slope, result.intercept), expected)
                    residuals = [b-result.slope*a-result.intercept for a, b in zip(x, y)]
                    self.assertEqual(sum(residuals), 0)
                    self.assertEqual(sum(a*r for a, r in zip(x, residuals)), 0)
                    reverse = nf.linear_regression(reversed(x), reversed(y))
                    self.assertEqual((reverse.slope, reverse.intercept), expected)
                    transformed = nf.linear_regression([3*a+17 for a in x], [-2*b-9 for b in y])
                    self.assertEqual(transformed.slope, -2*expected[0]/3)
                    self.assertEqual(transformed.intercept, -2*expected[1]-9-transformed.slope*17)

    def test_thousand_digit_offsets_and_tiny_differences(self):
        n = 10**1000
        for x, y, expected in (([n, n+1, n+2], [Fraction(3, n), Fraction(2, n), Fraction(1, n)],
                                (Fraction(-1, n), Fraction(n+3, n))),
                               ([Fraction(3, n), Fraction(2, n), Fraction(1, n)], [n, n+1, n+2],
                                (Fraction(-n), Fraction(n+3))),
                               ([n, n+Fraction(1, n), n+Fraction(2, n)], [1, 2, 3],
                                (Fraction(n), Fraction(1-n*n)))):
            result = nf.linear_regression(x, y)
            self.assertEqual((result.slope, result.intercept), expected)

    def test_validation_and_constant_x(self):
        for x, y in (([], []), ([1], [2]), ([1, 2], [1]), ([1], [1, 2])):
            with self.assertRaises(ValueError):
                nf.linear_regression(x, y)
        for x, y in (([1, 1], [1, 2]), ([0, 0], [0, 0])):
            with self.assertRaises(nf.Error) as caught:
                nf.linear_regression(x, y)
            self.assertEqual((caught.exception.status, caught.exception.offset), ("invalid_argument", 0))
        for bad in ("12", b"12", [True, 1], [0.1, 1], [None, 1], [complex(1, 2), 1],
                    [nf.sqrt(2), 1], [nf.sqrt(-1), 1]):
            for x, y in ((bad, [1, 2]), ([1, 2], bad)):
                with self.assertRaises(TypeError):
                    nf.linear_regression(x, y)
        with self.assertRaises(TypeError):
            nf.linear_regression([1, 2], [1, 2], precision=100)

    def test_budget_failure_through_serialization_and_recovery(self):
        for options, expected in (({"time_limit_ms": 0}, "time_limit"),
                                  ({"allocation_bytes": 0}, "memory_limit"),
                                  ({"single_allocation": 0}, "memory_limit")):
            with self.assertRaises(nf.Error) as caught:
                nf.linear_regression([1, 2, 3], [1, 2, 2], **options)
            self.assertEqual((caught.exception.status, caught.exception.offset), (expected, 0))
        recovered = False
        for allocation_bytes in range(2048, 262145, 2048):
            try:
                result = nf.linear_regression([1, 2, 3], [1, 2, 2], allocation_bytes=allocation_bytes)
            except nf.Error as error:
                self.assertEqual(error.status, "memory_limit")
            else:
                self.assertEqual((result.slope, result.intercept), (Fraction(1, 2), Fraction(2, 3)))
                recovered = True
                break
        self.assertTrue(recovered)
        self.assertEqual(nf.linear_regression([0, 1], [1, 0]).slope, -1)


class IntegralTests(unittest.TestCase):
    def test_polynomial_antiderivatives_and_approximate_result(self):
        rng = random.Random(8107)
        for _ in range(12):
            coefficients = [rng.randrange(-5, 6) for _ in range(4)]
            lower, upper = rng.randrange(-4, 0), rng.randrange(1, 5)
            expected = sum(Fraction(c * (upper**(i+1) - lower**(i+1)), i+1)
                           for i, c in enumerate(coefficients))
            result = nf.integrate(lambda x, places: sum(c*x**i for i, c in enumerate(coefficients)), lower, upper)
            self.assertEqual((result.kind, result.value, result.error), ("estimated", expected, 0))
            self.assertEqual((result.evaluations, result.refinements), (5, 2))
            self.assertFalse(result.exact)
        with self.assertRaises(FrozenInstanceError):
            result.value = 1
        # Alias blindness is a documented limitation, even for smooth polynomials.
        alias = nf.integrate(lambda x, p: (x*(x-Fraction(1, 4))*(x-Fraction(1, 2))*(x-Fraction(3, 4))*(x-1))**2, 0, 1)
        self.assertEqual((alias.kind, alias.value, alias.error), ("estimated", 0, 0))
        self.assertFalse(alias.exact)

    def test_quartic_error_and_nonpolynomial_reference(self):
        result = nf.integrate(lambda x, p: x**4, 0, 1, absolute_tolerance="1E-6")
        self.assertEqual((result.kind, result.evaluations), ("estimated", 33))
        self.assertEqual(result.error, Fraction(1, 7864320))
        self.assertEqual(result.value - Fraction(1, 5), result.error)
        result = nf.integrate(lambda x, p: 1/(1+x), 0, 1, absolute_tolerance="1E-7")
        with localcontext() as ctx:
            ctx.prec = 60
            actual_error = abs(Fraction(Decimal(2).ln()) - result.value)
        self.assertEqual(result.kind, "estimated")
        self.assertLess(actual_error, Fraction(1, 10**7))

    def test_limits_domain_gaps_and_callback_exceptions(self):
        for options, kind in (({"max_evaluations": 8}, "evaluation_limit"),
                              ({"max_refinements": 2}, "refinement_limit")):
            result = nf.integrate(lambda x, p: x**4, 0, 1, **options)
            self.assertEqual((result.kind, result.evaluations, result.value, result.error),
                             (kind, 5, Fraction(77, 384), Fraction(1, 1920)))
        gap = nf.integrate(lambda x, p: None if x == Fraction(1, 4) else x, 0, 1)
        self.assertEqual((gap.kind, gap.value, gap.error, gap.evaluations), ("domain_gap", None, None, 4))
        for exception in (ValueError("sample failed"), KeyboardInterrupt()):
            def fail(x, p):
                raise exception
            with self.assertRaises(type(exception)) as caught:
                nf.integrate(fail, 0, 1)
            self.assertIs(caught.exception, exception)

    def test_high_precision_input_lifetime_and_callback_precision(self):
        seen = []
        def sample(x, places):
            self.assertIsInstance(x, Fraction)
            seen.append(places)
            return Fraction(1, 10**1000)
        result = nf.integrate(sample, 0, "1E-1000", decimal_places=1000)
        gc.collect()
        self.assertEqual(result.value, Fraction(1, 10**2000))
        self.assertEqual(seen, [1000]*5)
        n = 10**1000
        result = nf.integrate(lambda x, p: x-n, n, n+1)
        self.assertEqual((result.value, result.error), (Fraction(1, 2), 0))

    def test_validation_budgets_and_recovery(self):
        for options in ({"max_refinements": 1}, {"max_evaluations": 4}, {"decimal_places": 0}):
            with self.assertRaises(ValueError):
                nf.integrate(lambda x, p: x, 0, 1, **options)
        for options in ({"max_refinements": True}, {"max_evaluations": 5.0}, {"decimal_places": False}):
            with self.assertRaises(TypeError):
                nf.integrate(lambda x, p: x, 0, 1, **options)
        with self.assertRaises(TypeError):
            nf.integrate(None, 0, 1)
        for lower, upper, tolerance in ((1, 0, 1), (1, 1, 1), (0, 1, 0), (0, 1, -1)):
            with self.assertRaises(nf.Error) as caught:
                nf.integrate(lambda x, p: x, lower, upper, absolute_tolerance=tolerance)
            self.assertEqual(caught.exception.status, "invalid_argument")
        for value in (0.1, True, 1j):
            with self.assertRaises(TypeError):
                nf.integrate(lambda x, p: value, 0, 1)
        for options, status in (({"time_limit_ms": 0}, "time_limit"),
                                ({"allocation_bytes": 0}, "memory_limit"),
                                ({"single_allocation": 0}, "memory_limit")):
            with self.assertRaises(nf.Error) as caught:
                nf.integrate(lambda x, p: x, 0, 1, **options)
            self.assertEqual(caught.exception.status, status)
        recovered = False
        for size in range(2048, 262145, 2048):
            try:
                result = nf.integrate(lambda x, p: x*x, 0, 1, allocation_bytes=size)
            except nf.Error as error:
                self.assertEqual(error.status, "memory_limit")
            else:
                self.assertEqual(result.value, Fraction(1, 3))
                recovered = True
                break
        self.assertTrue(recovered)


if __name__ == "__main__":
    unittest.main()
