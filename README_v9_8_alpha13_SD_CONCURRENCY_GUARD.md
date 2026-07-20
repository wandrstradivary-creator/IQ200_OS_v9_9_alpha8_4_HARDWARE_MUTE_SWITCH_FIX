# IQ200 OS v9.8-alpha13 SD Concurrency Guard

This build addresses the two crashes captured in the alpha11 runtime log:

1. `ArtworkCache::prepareForTrack()` blocked Core0 in a large SD read until Task WDT fired.
2. `ResumeEngine::save()` opened a file while SD recovery was remounting FAT/VFS, causing LoadProhibited.

## Runtime policy

- Audio stream owns SD while active.
- Recovery locks out all background SD clients.
- Artwork uses 4 KB cooperative reads and rejects images larger than 512 KB.
- Resume/Queue saves are deferred until streaming and recovery are both inactive.

## Expected diagnostics

```text
[ART] deferred: SD busy/recovering ...
[SMARTRESUME] save deferred: SD stream/recovery active
[SDM][BRIDGE] enter 8 MHz recovery bridge (background locked)
```

These messages are normal safety deferrals, not errors.
