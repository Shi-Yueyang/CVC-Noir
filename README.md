# cvc-noir

cvc-noir is a host-side simulator for the 300C embedded system.

## Quick picture

- `Source/300C_SIMU`: simulator runtime and host-side adapters.
- Domain logic (e.g. `Source/ATP_CODE`): pure C embedded code, managed externally.
- Runtime entry: `Source/300C_SIMU/main.cpp`.

## Build

Requires **CMake 3.16+** and a C++17 compiler (GCC 11+, Clang 14+, or MSVC 2022).

### Domain logic setup

Place your domain logic folder under `Source/`. CMake auto-detects `ATP_CODE`, `ATO_CODE`, or `application`:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

To use a custom folder name:

```bash
cmake -B build -DDOMAIN_CODE_DIR=Source/MyDomain -DCMAKE_BUILD_TYPE=Debug
```

To build the simulator without domain logic (simulator-only mode), ensure none of the default folders exist or point to a non-existent path.

Other build variants:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The `--config` option works with both single-config generators (such as Makefiles or Ninja) and multi-config generators (such as Visual Studio).

### Build targets

Build all default targets, including the simulator and tests:

```bash
cmake --build build --config Debug
```

Build the main simulator only:

```bash
cmake --build build --target cvc-noir --config Debug
```

Build an individual test target:

```bash
cmake --build build --target test_a_train_session --config Debug
cmake --build build --target test_tcp_framing --config Debug
```

Run all registered tests after building:

```bash
ctest --test-dir build --build-config Debug --output-on-failure
```

Configure without test targets:

```bash
cmake -B build -DBUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target cvc-noir --config Debug
```

Output binaries are written to the `build/` directory.

## Run

Copy the template config and customize for your environment:

```bash
cp docs/simu_config.template.json simu_config.json
```

Edit `simu_config.json` to set your data paths, launchers, and session configs.

Launch the executable from the build directory:

```bash
cd build && ./cvc-noir
```

On Windows:

```powershell
cd build && .\Debug\cvc-noir.exe
```

By default the simulator loads `simu_config.json` from the current working directory. An optional positional argument selects a different config file (absolute or relative path):

```bash
./cvc-noir path/to/simu_config_test.json
```

## Documentation

Canonical docs are in `docs/`:

- `docs/doc-map.md`: index, ownership, precedence, and module-local README references.
- `docs/architecture.md`: architecture boundaries and dependency direction.
- `docs/configuration.md`: `simu_config.json` specification.
- `docs/testing.md`: test workflow and pass criteria.
- `docs/behavior-guidelines.md`: coding behavior and quality rules.
- `docs/workflow.md`: change lifecycle and reporting format.

Module-local docs:

- `Source/300C_SIMU/BSW_CODE/Pub_Comm/README.md`: Pub_Comm connection API.
- `Source/300C_SIMU/ASW_300C/README.md`: PDA storage API reference.
