---
status: accepted
date: 2026-08-29
---

# Subsystem SIL via posix deployments and behavioral port models

We need hardware-free integration testing, and Zephyr `native_sim` can run the full ReferenceDeployment topology with modest effort. We nevertheless chose **subsystem-scoped SIL first**: small posix-built F Prime deployments (radio first, startup/persistence second) that wire the subsystem's real components to Behavioral Models at the F Prime port boundary, driven by the existing fprime-gds pytest idiom over TCP. The full-topology `native_sim` SITL tier is adopted as a later phase (see the testing roadmap), not rejected.

## Why subsystem-first

- **Fault injection is the point.** SIL exists to script what HWIL cannot (corrupted LoRa frames, radio-busy, ALARM storms). A Behavioral Model at the port boundary makes faults first-class F Prime commands, scriptable with the same `IntegrationTestAPI` toolkit the HWIL suite already uses.
- **macOS local dev.** `native_sim` is hard Linux-only (POSIX arch fatals on Apple hosts); posix subsystem builds run on developer Macs and ubuntu CI alike.
- **Small blast radius.** A subsystem deployment compiles only portable components; the ~17 Zephyr-header-bound components stay out of the build instead of needing shims.

## Injection-boundary discipline

Each tier injects at exactly one canonical boundary: SIL injects at F Prime ports (Behavioral Models); the SITL tier injects at the Zephyr driver API (Reverse Drivers). Fakes from one tier must not leak into the other, so the pytest corpus stays portable across tiers via markers.

## Considered options

- **Full-topology native_sim as the primary SIL lane** — highest fidelity (~days to stand up), but Linux-only, and injecting scripted faults per-subsystem requires Reverse Drivers anyway; adopted as the Phase 4 SITL tier instead.
- **Register/SPI-level chip emulation** — targets driver code that lives in `lib/` submodules, outside this repo's test scope; rejected.
- **Loopback stubs without fault injection** — discards the main value of SIL over HWIL; rejected.
