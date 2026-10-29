# Ops contract

`manifest.json` lists every dictionary item that mission ground tools depend on: the commands they send, the events they parse, the channels and Beacon members they read, and the parameter defaults they assume. Compiled sequences on board carry raw opcodes and argument layouts, so the manifest pins those too, not only names.

The keys are maintained by hand. The values are generated from the built dictionary by `tools/ops_contract.py`. Types are described by layout and values, so renaming a type with the same layout keeps the contract, and adding an enum value or resizing a string breaks it.

## The check

The `sitl` CI job runs `make ops-contract-check` against the sim dictionary on every pull request. The sim and the flight boards build the same topology. The check fails when the dictionary no longer produces the committed manifest.

Run it locally after a build:

```sh
make ops-contract-check
make ops-contract-check OPS_CONTRACT_DICTIONARY=build-artifacts-sim/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json
```

## Changing the contract

When a pull request changes an item in the manifest on purpose:

1. Run `make ops-contract` and commit the new `manifest.json`.
2. Open an issue in each mission ops repository that uses the item, so its ground tools migrate before a flight build picks up the change.
3. Link that issue in the pull request description and add the `contract-change` label.

CI fails a pull request that changes `manifest.json` without the label or without a link to a GitHub issue. After you add the label, re-run the `sitl` job.

To add an item, add its full name with `{}` as the value and run `make ops-contract`. A name that is not in the dictionary becomes `"MISSING"`, and the check fails until it is removed or the item exists.
