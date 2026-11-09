# Curated DX7 factory bank

Eight original packed DX7 voices selected from `assets/builtin_pgm.zip` in
asb2m10/dexed at fea2f0f0f2c397799f59b27eb11c038cd34b22c5:
https://github.com/asb2m10/dexed/tree/fea2f0f0f2c397799f59b27eb11c038cd34b22c5/assets

The archive is distributed as part of that GPLv3 repository; retain its license
and patch-author credits. Dexed credits Jean-Marc Desprez (SynprezFM) for program
compilation and credits the DX7 program contributors in its README. No patch
bytes were invented or retuned for this selection. Source bank/program indices,
original names, algorithms and SHA-256 hashes are recorded in
`docs/evidence/neon-console/dx7-shortlist-verified.json`. `config/dx7/review_bank.json`
records the pinned shortlist and candidate musical roles. Final musical choices
still require listening review. Engine code has separate Apache-2.0 provenance.

Reproduce the exact 1024-byte `factory.packed` using
`tools/prepare_dx7_review_bank.py` on the pinned source ZIP, then copy its
`review-voices.bin`. `tools/gen_dx7_bank.py` converts those unchanged bytes into
the compiled header. The complete upstream ZIP remains available at the source
revision; `factory.packed` preserves complete packed data for all selected voices.
