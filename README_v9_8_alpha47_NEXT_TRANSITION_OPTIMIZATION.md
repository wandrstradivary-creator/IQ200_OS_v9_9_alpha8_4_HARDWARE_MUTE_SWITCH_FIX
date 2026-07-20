# IQ200 OS v9.8-alpha47 — Next Transition Optimization

- Prevents a cache hit on the already-current ART entry from incrementing the artwork generation.
- Removes the duplicate JPEG/PNG TFT decode during Next/Prev/Auto Next pre-play synchronization.
- Next and Prev no longer call the full `playerScreen()` renderer when Player is already open.
- Track transitions update artwork, title, metadata, progress and transport state through partial rendering.
- Audio pipeline, Web playlist, EQ, Resume and SD clock logic are unchanged.

Expected diagnostics:

```text
[ART] cache hit ... gen=53 changed=0
[ART][PREPLAY] gen=53 rendered=53 wait=0ms
```

The `full=` counter should not increase for a normal Next/Prev transition while Player remains open.
