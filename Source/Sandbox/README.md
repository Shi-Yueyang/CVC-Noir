# Sandbox Domain

This is a casual, standalone C domain template. Its source and public header use
only standard C types and have no dependency on simulator headers or internals.

The module defines the `SRV_*` lifecycle functions expected by the runtime.
Local primitive typedefs preserve their C ABI without importing simulator
headers. Initialization reports version `0x00010000`; teach, learn, and
maintenance callbacks return status `205` until domain behavior is added.

CMake uses this folder as a fallback after the formal domain folder names; set
`DOMAIN_CODE_DIR` explicitly to select another implementation.