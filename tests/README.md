# Test source map

Build/run commands and focused suite selection live in [Testing](../docs/TESTING.md).

```text
tests/
  CMakeLists.txt      registration, flags, shards and timeouts
  core/              numeric types and mathematical functions
  modules/           public mathematical modules
  engine/            expressions, parser and evaluator behavior
  application/       sessions, graphs and equation adapters
  http/              transport, request parsing and real server checks
  failures/          cross-layer allocation/deadline fault injection
  support/           shared fixture construction and canonical text assertions
  fixtures/          frozen independent references and their generators
  oracles/           independent reference drivers/checkers
  browser/, python/  adapter and user-interface suites
  package_consumer/  installed C/C++ consumers
  fuzz/              fuzz targets, replay and dictionary
```

Shared support builds inputs and compares values; expected mathematical answers
stay independent of production algorithms. Keep fixture generation outside timing
and fault-injection windows. Frozen data is regression evidence, not disposable
output. Local results, traces and screenshots belong under ignored `build/`.
