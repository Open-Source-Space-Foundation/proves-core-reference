# Svc::Ccsds::SAManager

The `Svc::Ccsds::SAManager` is a passive reference implementation of the `Ccsds::ProcessSecurity` port. It owns the table of Security Associations and performs SPI lookup and anti-replay checks per CCSDS [355.0-B-2](https://ccsds.org/Pubs/355x0b2.pdf) §3.3. Header parsing and MAC verification are delegated to a project-supplied `Svc::Ccsds::ISecurityProvider` (a pure C++ interface declared in [`ISecurityProvider.hpp`](../ISecurityProvider.hpp)).

This split keeps the upstream library free of any specific crypto dependency. Projects implement `ISecurityProvider` against their own crypto library (mbedTLS, libsodium, hardware accelerator, etc.) and call `setProvider()` during initialization.

`SAManager` is intended for use as the downstream side of `Svc::Ccsds::TcDeframer::processSecurityOut`, but is not required — any component that calls `Ccsds::ProcessSecurity` may invoke it.

## Wiring

```cpp
ProvesSecurityProvider provider;     // implements ISecurityProvider
provider.importKey(...);              // project-specific setup

saManager.setProvider(&provider);
saManager.registerSa({
    .spi = 0x0001,
    .scope = { .tfvn = 0, .scid = 0x44, .vcid = 0 },
    .highestAcceptedSeq = 0,
    .antiReplayBitmap = 0,
    .windowSize = 64,
    .active = false,                  // set true by registerSa()
});
```

## Anti-Replay

`SAManager` maintains a 64-bit sliding-window bitmap per SA. Bit `i` set means `highestAcceptedSeq - i` has been accepted. Sequence-number wraparound is handled via signed-difference arithmetic on `U32`, so the window survives `0xFFFFFFFF → 0` rollover.

State is updated **after** MAC verification succeeds. An attacker forwarding spoofed frames with bad MACs cannot advance the window.

The first frame ever delivered to an SA (`antiReplayBitmap == 0 && highestAcceptedSeq == 0`) is unconditionally accepted; the window initializes on first receipt. Projects that need to resume from a persisted sequence number should set `highestAcceptedSeq` at `registerSa()` time.

## Port Descriptions

| Kind | Name | Port Type | Description |
|---|---|---|---|
| Input (guarded) | processSecurityIn | Ccsds.ProcessSecurity | Entry point invoked by TcSecurityDeframer (or equivalent) |

## Telemetry

| Name | Type | Description |
|---|---|---|
| SACount | U16 | Number of SAs currently registered |
| AcceptedTotal | U32 | Count of accepted frames across all SAs |
| RejectedTotal | U32 | Count of rejected frames across all SAs |

## Events

| Name | Severity | Description |
|---|---|---|
| SaLookupFailed | warning low | Incoming SPI not registered |
| AntiReplayReject | warning low | Sequence number rejected by anti-replay check |
| ProviderError | warning high | `ISecurityProvider::parseHeader` or `verifyMac` returned an error |
| SaRegistered | activity low | New SA installed via `registerSa()` |
| SaTableFull | warning high | `registerSa()` rejected because the table is full |

## Limitations

- Up to `MAX_SAS = 8` Security Associations. Adjust the compile-time constant if your mission needs more (no FPP parameter exposed in v1).
- 64-bit anti-replay bitmap (max window 64 frames). The `windowSize` field may be smaller per SA.
- The SA table lookup is a linear scan; fine for the expected small SA counts but not optimized for large tables.
