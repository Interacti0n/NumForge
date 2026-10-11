"""Independent Decimal references for the experimental logarithm harness.

No third-party dependencies. Generate the checked-in smoke fixture with:
  python benchmarks/generate_logarithm_agm_cases.py --max-digits 250 --output benchmarks/data/logarithm_agm_smoke.tsv
Larger optional references belong under ignored build/ directories.
"""
import argparse
from decimal import Decimal, localcontext
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', required=True, type=Path)
parser.add_argument('--max-digits', type=int, choices=(250, 1000, 2000, 5000, 10000), default=250)
parser.add_argument('--conditioning-only', action='store_true', help='Generate only difficult rounding midpoint inputs')
args = parser.parse_args()
inputs = ('2', '0.5', '10', '1.01', '0.99', '1',
          '1.' + '0'*39 + '1', '1e1000', '1e-1000', '123.456',
          '0.999', '0.99899999999999999999', '0.99900000000000000001',
          '1.001', '1.00099999999999999999', '1.00100000000000000001')
if args.conditioning_only:
    inputs = ()
precisions = [p for p in (34,250,1000,2000,5000,10000) if p <= args.max_digits]
modes = ('ROUND_DOWN', 'ROUND_UP', 'ROUND_FLOOR', 'ROUND_CEILING',
         'ROUND_HALF_UP', 'ROUND_HALF_EVEN')
rows = ['input\tdigits\trounding\treference']
for text in inputs:
    references = []
    for extra in (80,160):
        with localcontext() as context:
            context.prec = args.max_digits + extra
            references.append(Decimal(text).ln())
    for digits in precisions:
        for mode, name in enumerate(modes):
            with localcontext() as context:
                context.prec, context.rounding = digits, name
                expected = +references[0]
                assert expected == +references[1], (text, digits, name)
            rows.append(f'{text}\t{digits}\t{mode}\t{expected}')

# Two exact decimal inputs whose logarithms sit on opposite sides of a
# 34-digit nearest-rounding midpoint, about 200 digits away from it. The old
# fixed-guard experiment cannot certify these; exercise interval retries only.
for digits in (34, 2000):
    if digits > args.max_digits:
        continue
    with localcontext() as context:
        context.prec = digits
        anchor = Decimal(2).ln()
    with localcontext() as context:
        context.prec = digits + 246
        midpoint = anchor + Decimal(f'5e-{digits+1}')
        centre = midpoint.exp()
        delta = Decimal(f'1e-{digits+166}')
        difficult = (str(centre - delta), str(centre + delta))
    for text in difficult:
        references = []
        for precision in (digits + 296, digits + 376):
            with localcontext() as context:
                context.prec = precision
                references.append(Decimal(text).ln())
        for mode, name in enumerate(modes):
            with localcontext() as context:
                context.prec, context.rounding = digits, name
                expected = +references[0]
                assert expected == +references[1], (text, mode)
            rows.append(f'{text}\t{digits}\t{mode}\t{expected}\tadaptive')
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text('\n'.join(rows)+'\n', encoding='ascii')
print(f'{len(rows)-1} exact references written to {args.output}')
