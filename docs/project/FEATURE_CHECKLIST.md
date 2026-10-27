# Mathematical feature completion checklist

Apply to new mathematical capabilities/material contract changes. Record evidence
in the change; existing functions retain their guarantees until deliberately changed.

- **Contract:** Doxygen types/parameters, domains/branches, statuses, ownership,
  aliases, threading, budgets and unchanged outputs on failure.
- **Meaning:** exact/approximate classification; working versus output precision;
  rounding/error bounds, convergence/stopping and incomplete discovery.
- **Independent use:** typed math works without parsing, sessions or the web.
- **Examples:** working C plus supported adapters; label planned interfaces.
- **Correctness:** independent references, edge domains, high precision,
  cancellation, rounding, aliases, allocation/cancellation recovery.
- **Integration:** installed C/C++ consumers and affected adapter parity;
  preserve compatibility or version an intentional break.
- **Performance:** expensive algorithms/optimizations need reproducible Release
  measurements with validated results and resource/time failures retained.
  Thin wrappers can reuse existing evidence; they do not establish a speedup.
- **Documentation:** update the owning reference and availability; link to
  authoritative headers instead of copying contracts.

[Numerical testing](../testing/NUMERIC_TESTING.md) ·
[Measurement method](../testing/PERFORMANCE.md) · [Architecture](../design/ARCHITECTURE.md).
