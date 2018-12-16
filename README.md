# progressive-filling

# Running benchmarks

Benchmarks are located in tests/benchmarks/ folder.  Executing the benchmarks
is easy (and possibly not done in the smartest way possible):

```
make bench BENCHMARK_NAME && ./bin/BENCHMARK_NAME
```

This command looks for a BENCHMARK_NAME.c file in the tests/benchmarks folder
and tries to compile it against the rest of the source code.

