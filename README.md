# ARK-OS Host Tools

## Overview

The `host-tools/` repository contains utilities, build orchestrators, package signing infrastructure, and development automation intended to execute on the host development workstation.

Per [ADR-0005](file:///home/aarav/repo/arkos/docs/decisions/ADR-0005-host-tooling-and-build-orchestration.md), ARK-OS follows a staged tooling approach: early bring-up relies on lightweight, transparent host scripts, while the comprehensive build orchestrator (`aake`) is developed progressively.

---

## Directory Organization

- `aake/` - Design, implementation, and bootstrap Makefile for the ARK-OS Automated Kernel and Environment build system (`aake`). Coordinates manifest resolution, toolchain invocation, Ninja build file generation, and target image generation. See [Aake Documentation](file:///home/aarav/repo/arkos/host-tools/aake/README.md).
- `signing/` - Cryptographic signing tools, key management utilities, and artifact verification helpers for release builds.

---

## Engineering Guidelines

- All host tools must execute without polluting the host's global environment.
- Dependencies on host packages must be documented explicitly in target manifests.
- Host scripts must handle failures deterministically and output clear diagnostic errors.

---

## License

This repository is licensed under the Apache License, Version 2.0. See [LICENSE](file:///home/aarav/repo/arkos/host-tools/LICENSE) for the full license text.
