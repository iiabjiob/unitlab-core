# IEC 61850 Debug

Standalone read-only SCD inspector for parser, fixture, and protocol regression work.

The package is intentionally independent from UnitLab UI state. It starts as a local XML/SCD inspector; backend/MMS controls will be added behind an explicit adapter when the package contract is stable.

```bash
pnpm install
pnpm dev
```
