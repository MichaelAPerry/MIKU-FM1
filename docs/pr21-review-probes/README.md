# PR21 review probes

Small host programs written for the independent PR21 review (`docs/PR21_CLAUDE_REVIEW.md`).
They are not part of the build or CTest. Run them against a checkout of the PR21 branch
(`feature/miku-nextgen-performance` at `33d2115`), with `PR` set to that checkout:

```bash
PR=/path/to/pr21-checkout
LIBS="$PR/firmware/audio/fmband/fmband_mx.c $PR/firmware/audio/fmband/fmband_tables.c"
cc -std=c11 -O1 -I"$PR" p_lfo.c $LIBS -o p_lfo && ./p_lfo        # includes dx7.c directly
cc -std=c11 -O1 -I"$PR" p_lfo2.c $LIBS -o p_lfo2 && ./p_lfo2
for p in p_ext p_steal p_rel p_cost; do
  cc -std=c11 -O1 -g -fsanitize=address,undefined -I"$PR" $p.c "$PR/firmware/audio/dx7/dx7.c" $LIBS -o $p &&
  ASAN_OPTIONS=detect_leaks=0 ./$p
done
```

- `p_lfo.c`, `p_lfo2.c`: LFO-delay accumulator wrap (finding F2).
- `p_ext.c`: extreme and random valid patches under ASan/UBSan, peak output.
- `p_steal.c`: release tails hard-cut by four-voice stealing; carrier L4 > 0 termination (F5, F6).
- `p_rel.c`: time from PANIC to voice inactive, which gates SAVE (F3).
- `p_cost.c`: host-relative render cost per algorithm (not a device timing).
