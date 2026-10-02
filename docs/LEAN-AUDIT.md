# Lean/Hardened audit results

Baseline Muro 0.2 SAFE (`size`): text 50,570; data 480; bss 20,832; total 71,882 bytes.

Muro 0.2.1 SAFE (`size`): text 23,869; data 336; bss 11,400; total 35,605 bytes.

That is about a 50.5% reduction in the in-memory total reported by `size`, while retaining the runtime integrity/recovery/firewall foundation. The embedded `.text` region sealed by Peregrinus Guard is 18,741 bytes after cleanup.

The reduction came from removing runtime self-tests, duplicate Boot Control/rollback code, unused requests, PCI BAR caching, and dead sections—not by weakening fail-closed checks.
