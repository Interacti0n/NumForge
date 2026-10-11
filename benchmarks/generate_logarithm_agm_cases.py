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
args = parser.parse_args()
inputs = ('2', '0.5', '10', '1.01', '0.99', '1',
          '1.' + '0'*39 + '1', '1e1000', '1e-1000', '123.456')
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
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text('\n'.join(rows)+'\n', encoding='ascii')
print(f'{len(rows)-1} exact references written to {args.output}')
