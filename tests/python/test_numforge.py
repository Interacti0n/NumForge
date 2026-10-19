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


if __name__ == "__main__":
    unittest.main()
