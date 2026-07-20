# Windows / PlatformIO build recovery

If LovyanGFX reports missing `DataWrapper.hpp` or `pgmspace.h`, the downloaded
library is incomplete. Both files belong to LovyanGFX 1.1.16; do not create
placeholder headers.

Extract the project to a short path such as `C:\PIO\IQ200-radio`, then run:

```powershell
pio run -t clean
if (Test-Path .pio) { Remove-Item -Recurse -Force .pio }
pio system prune --cache -f
pio pkg install -e iq200-radio
pio run -e iq200-radio
```

Dependency check:

```powershell
Test-Path .pio\libdeps\iq200-radio\LovyanGFX\src\lgfx\v1\misc\DataWrapper.hpp
Test-Path .pio\libdeps\iq200-radio\LovyanGFX\src\lgfx\utility\pgmspace.h
```

Both commands must return `True` before compilation.
