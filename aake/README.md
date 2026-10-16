# Aake: ARK-OS Native Build Orchestrator

## Overview

**Aake** is the official build orchestrator for ARK-OS, designed according to [ADR-0005: Host Tooling and Build Orchestration](file:///home/aarav/repo/arkos/docs/decisions/ADR-0005-host-tooling-and-build-orchestration.md). Inspired by the developer experience of Android's Soong build system and backed by [Ninja](https://ninja-build.org/) for fast parallel and incremental execution, Aake serves as the primary developer entry point for building, testing, and verifying ARK-OS components and target disk images.

Aake coordinates the high-level build concerns:
- Manifest resolution (`manifests/components.toml` and `manifests/targets/*.toml`)
- Target selection and dependency validation
- Generating deterministic Ninja build definitions (`out/build.ninja`)
- Live, formatted progress rendering with elapsed timings and failure summaries
- Host unit testing and UEFI VM execution orchestration

---

## Architecture

```
                          Developer Command
                                 │
                        ┌────────▼────────┐
                        │   aake [cmd]    │
                        └────────┬────────┘
                                 │
         ┌───────────────────────┼───────────────────────┐
         │                       │                       │
┌────────▼────────┐     ┌────────▼────────┐     ┌────────▼────────┐
│   aake doctor   │     │   aake build    │     │    aake test    │
│  (env check)    │     │  (Ninja gen &   │     │  (unit tests &  │
│                 │     │   execution)    │     │   QEMU/OVMF)    │
└─────────────────┘     └────────┬────────┘     └─────────────────┘
                                 │
                        ┌────────▼────────┐
                        │ out/build.ninja │
                        └────────┬────────┘
                                 │
                   ┌─────────────┴─────────────┐
                   ▼                           ▼
          Parallel Toolchains          Incremental Outputs
         (swiftc, gcc, objcopy)         (out/, components/)
```

Aake separates high-level orchestration from execution scheduling:
1. **Manifest Layer**: Parses project metadata in `manifests/components.toml` and target configurations in `manifests/targets/x86_64.toml`.
2. **Generator Layer**: Synthesizes a deterministic `out/build.ninja` declaring rules, compiler flags, dependencies, and outputs.
3. **Execution Layer**: Invokes Ninja in parallel using the host's CPU core capacity, tracking progress via Ninja's status output adapter.
4. **UI Adapter**: Intercepts Ninja status tokens `[%f/%t]` and formats them into a deterministic Soong-inspired console interface with live step numbers and action summaries.

---

## Bootstrap Compilation

Aake is implemented in ANSI C (C99 / POSIX.1-2008) without third-party library dependencies. It is compiled using a small, dedicated bootstrap Makefile located at `host-tools/aake/Makefile`.

### Building Aake

```bash
# Compile Aake into bin/aake
make -C host-tools/aake

# Compile with a specific compiler or flags
make -C host-tools/aake CC=clang CFLAGS="-O3 -Wall"

# Clean Aake bootstrap artifacts
make -C host-tools/aake clean
```

### Installation and Removal

To install Aake system-wide or to a custom prefix:

```bash
# Install to default prefix (/usr/local/bin)
sudo make -C host-tools/aake install

# Install to custom prefix
make -C host-tools/aake PREFIX=$HOME/.local install

# Uninstall from prefix
make -C host-tools/aake PREFIX=$HOME/.local uninstall
```

---

## Environment Setup (`envsetup.sh`)

ARK-OS provides an environment setup script at the repository root:

```bash
source envsetup.sh
```

### Why `envsetup.sh` Must Be Sourced

Executing a script (`./envsetup.sh` or `bash envsetup.sh`) executes within an isolated subshell child process. Any modifications to environment variables (`ARKOS_ROOT`, `AAKE_DIR`, or `PATH`) are immediately lost when that subshell terminates.

Sourcing (`source envsetup.sh` or `. envsetup.sh`) runs the script in the **current interactive shell**, ensuring:
- `ARKOS_ROOT` is set to the repository root.
- `AAKE_DIR` points to `host-tools/aake`.
- `$AAKE_DIR/bin` is prepended to `PATH` (without duplicate entries).
- Aake is automatically compiled via its bootstrap Makefile if missing or out of date.

---

## Command-Line Interface

```text
aake help                  Show usage and command summary
aake version               Display Aake version and backend
aake doctor                Inspect environment, toolchain, and dependencies
aake build [target]        Orchestrate and compile targets via Ninja
aake test [target]         Execute host unit tests or target verification
aake test --uefi           Launch QEMU with UEFI firmware and ESP image
aake test --bios           Inspect legacy BIOS boot support
aake clean [target]        Clean generated artifacts and build outputs
```

### Options

- `-v`, `--verbose`: Print raw compiler invocations and detailed tool commands.
- `--debug`: Select debug compilation configuration.
- `--release`: Select release compilation configuration (default).
- `--out-dir <path>`: Specify build output directory (default: `out`).

---

## Build Targets

| Target | Description |
| :--- | :--- |
| `all` | Compiles runtime, services, bootloader, and assembles host initramfs ESP. |
| `all-target` | Compiles static-musl runtime, services, and static initramfs ESP. |
| `runtime` / `arkrt` | Compiles host PID 1 starter (`starter`), C signals, and Swift runtime (`arkrt-swift`). |
| `runtime-target` | Compiles static-musl PID 1 starter and static Swift runtime (`arkrt-swift-target`). |
| `services` / `arkdiag` | Compiles diagnostic service daemon (`arkdiag`). |
| `services-target` | Compiles static-musl diagnostic service daemon (`arkdiag.target`). |
| `boot` | Compiles the UEFI bootloader (`ARKBOOT.EFI`). |
| `initramfs` | Packs the CPIO initramfs archive with host glibc libraries. |
| `initramfs-target` | Packs the CPIO initramfs archive with static musl binaries. |
| `esp` | Assembles UEFI ESP directory with kernel, host initramfs, and bootloader. |
| `esp-target` | Assembles UEFI ESP directory with kernel, static musl initramfs, and bootloader. |
| `test-supervisor` | Compiles host unit test binary for service supervisor. |
| `test-supervisor-target` | Compiles static musl unit test binary for service supervisor. |

---

## Test Workflows

### Host Unit Tests

```bash
# Run host service supervisor unit tests
aake test
```

Aake automatically ensures the required test target is built via Ninja, executes the test suite, and outputs test case outcomes.

### UEFI Boot Testing

```bash
# Verify and launch UEFI VM in QEMU
aake test --uefi
```

Prerequisites for `--uefi`:
1. ESP boot image built (`aake build esp` or `aake build esp-target`).
2. `qemu-system-x86_64` installed.
3. OVMF UEFI firmware installed (e.g., `/usr/share/edk2/x64/OVMF.4m.fd` or `/usr/share/OVMF/OVMF_CODE.fd`).

### BIOS Boot Testing

```bash
# Inspect legacy BIOS boot support
aake test --bios
```

ARK-OS currently implements UEFI booting via `ARKBOOT.EFI` in `components/boot/loader/`. Legacy BIOS (MBR/multiboot) is not implemented; `aake test --bios` factually reports this limitation rather than simulating a fake pass.

---

## Output Directories and Artifacts

- `out/build.ninja`: The generated deterministic Ninja build graph.
- `out/`: Build directory for intermediate build outputs.
- `components/runtime/arkrt/`: Host and target runtime binaries (`starter`, `arkrt-swift`, `test_supervisor`).
- `components/services/arkdiag/`: Diagnostic service binaries (`arkdiag`, `arkdiag.target`).
- `components/boot/build/`:
  - `esp/`: UEFI System Partition tree containing `EFI/BOOT/BOOTX64.EFI`, `boot/bzImage`, `boot/initramfs.cpio.gz`.
  - `initramfs/`: Staging directory for root filesystem.

---

## Troubleshooting

### `mimalloc.o` Missing in `aake doctor`

If `aake doctor` reports `build/sysroot/lib/mimalloc.o missing`:
- Run the sysbuilts sync script (`components/boot/scripts/sync.sh`).
- Or export the location of a pre-built object:
  ```bash
  export MIMALLOC_OBJ=/path/to/mimalloc.o
  ```

### Swift Static Linux SDK Not Found

Static target builds (`runtime-target`, `esp-target`) require the Swift Static Linux SDK:
```bash
swift sdk install https://download.swift.org/swift-6.4-release/static-sdk/swift-6.4-RELEASE/swift-6.4-RELEASE_static-linux-0.1.0.artifactbundle.tar.gz
```

---

## Current Status and Migration

- Aake provides the top-level orchestration layer replacing manual invocations of `components/Makefile`.
- Component Makefiles (`components/boot/Makefile`, `components/runtime/arkrt/Makefile`, `components/services/arkdiag/Makefile`) are preserved for independent component bring-up and upstream tooling compatibility.
