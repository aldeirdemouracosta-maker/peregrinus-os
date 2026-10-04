# Purgatório 0.1 — Admission Gate

Scope: bounded in-memory component quarantine, identity keyed by component ID with SHA-256 digest retained for evidence.

## Guarantees
- no heap; 32-entry fixed registry;
- quarantined component IDs are denied;
- conflicting digest for an already-quarantined ID escalates reason to policy violation;
- registry saturation fails closed for new optional component admissions;
- no runtime release API;
- no disk writes, filesystem moves, antivirus signatures, process killing, or sandbox claims.

## Why this is intentionally small
The current Peregrinus kernel has no mature filesystem/process loader. A larger quarantine subsystem would be decorative and unsafe. This phase establishes the admission primitive that later loaders can call.

## 0.1 review fixes
- component ID `0` is invalid: `quarantine(0, …)` is rejected and counted (`rejected_invalid()`) instead of poisoning the whole registry; `admit(0, …)` returns `DENY-INVALID-ID`;
- a conflicting digest for an already-quarantined ID keeps the original digest and also records the conflicting one (`digest_conflict`, `conflict_digest`) as evidence.

## Superseded in 0.1.1
Admission became an allowlist (`trust()` + digest check); see `docs/PURGATORIO-0.1.1.md`.
