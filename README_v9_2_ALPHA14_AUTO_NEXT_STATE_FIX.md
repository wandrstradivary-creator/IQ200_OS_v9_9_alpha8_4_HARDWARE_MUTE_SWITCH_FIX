# IQ200 OS v9.2-alpha14 AUTO NEXT STATE FIX

- Auto Next is queued after confirmed natural EOF.
- The next decoder starts on the following Core0 loop after the previous decoder state is fully settled.
- Manual next/prev, segmented VU and FLAC guards are unchanged.

Expected log:

```text
[AUTONEXT] EOF queued current=1/280
[HANDOFF] NEXT selected: ... autoplay=1 source=EOF
[HANDOFF] start OK codec=FLAC path=...
```
