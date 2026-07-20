# IQ200 OS v8.7.2 SEARCH RECOVERY FIX

Fixes verified from v8.7.1 hardware log:

- Library rebuild after background scan remains OK.
- Stability and burnin stop are allowed while ScanService is active.
- Search now applies UTF-8 Cyrillic case folding so `find цой` can match `Виктор Цой`.
- Existing `search.idx` can be searched without rebuild; new `libbuild` writes folded haystacks.

Test:

```
diag
stability
burnin start
scan
stability
burnin stop
find цой
find виктор
```
