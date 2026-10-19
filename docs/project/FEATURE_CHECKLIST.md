# Mathematical feature completion checklist

Apply this to a new public mathematical capability or a material contract change.
Record the evidence in its PR or development report. An existing function keeps
its documented guarantees until deliberately changed; migrate older documentation
incrementally when working on that area.

- **Public contract:** describe types, parameters, domains, return values, error
  statuses, ownership/lifetimes, output preservation on failure, supported aliasing,
  threading and resource limits. Use Doxygen comments for new C APIs.
- **Numerical meaning:** state exact versus approximate behavior, precision units,
  rounding, branches, assumptions and actual accuracy guarantees. Solvers document
  convergence, residual/error meaning, stopping rules and incomplete coverage.
  Do not infer a rigorous error bound from a guard-digit count or a small residual.
- **Independent use:** the mathematical operation is callable without the web UI
  or an application session. Expression parsing is optional for typed numeric calls.
- **Examples and availability:** include a working C example and examples for
  supported adapters where applicable. Explicitly mark unavailable/planned adapters;
  a hypothetical Python/REST/WASM example must not be presented as an existing API.
- **Correctness evidence:** independent references or exact properties appropriate
  to the mathematics; relevant zero/sign/domain boundaries, cancellation, magnitude,
  rounding modes, high precision, aliasing and unchanged-output-on-failure cases.
  Test allocation failures and cancellation for new owned/public paths.
- **Integration evidence:** installed C/C++ consumers for public API changes and
  parity checks for adapters affected by the change. Keep existing API compatibility
  unless a versioned breaking change is intentional and documented.
- **Performance evidence:** computationally expensive new algorithms and
  optimizations need reproducible Release benchmarks, validated results, relevant
  precision/input shapes, timeouts and allocation/memory observations. Reusing an
  existing algorithm can reuse its benchmark with an explicit reference; a thin
  API wrapper does not imply a new algorithm or a speedup.
- **Documentation:** update references, supported-interface coverage and Unreleased
  notes. Keep the public C declaration's detailed contract authoritative rather than
  maintaining conflicting copies. Link examples to verification where practical.

See [numerical testing](../testing/NUMERIC_TESTING.md),
[performance methodology](../testing/PERFORMANCE.md) and
[the platform architecture](../design/ARCHITECTURE.md).
