## Dev
Build:
```
cmake -B build -DDEBUG_TRACE_EXECUTION=ON -DDEBUG_PRINT_CODE=ON
```

Compile and run:
```
cmake --build build
./bin/clox
```

Tests:
```
./bin/test
```

Release:
```
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```
