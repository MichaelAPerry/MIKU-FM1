# JPEG encoder provenance

`mjpeg.c` and `vjpeg_tables.h` derive from Synthesthetic's `firmware/src/vjpeg.c`
and `firmware/src/vjpeg_tables.h`, pinned revision
`589bd0f30b68b66f78dab84debb42909131f4cad`:
https://github.com/michaeljsmalley/synthesthetic/tree/589bd0f30b68b66f78dab84debb42909131f4cad

Copyright (C) 2026 the Synthesthetic contributors; GPL-3.0-only, as stated in its
LICENSING.md. The repository's LICENSE contains GPL version 3.

The transform is the integer Loeffler/Ligtenberg/Moschytz DCT design described
in IJG `jfdctint`; constants use 13 fractional bits. The tables are the standard
JPEG T.81 Annex K quantisation and Huffman tables, reproduced by Synthesthetic
from the public-domain stb_image_write.h v1.16 table data. Original explanatory
comments and table provenance are retained.

MIKU modifications replace indexed/framebuffer input with a RGB565 callback,
move all mutable scratch into a caller-owned encoder, remove zoom-specific
paths, bound dimensions/capacity, and expose begin/step/completed-size calls.
No Synthesthetic graphics, samples, or visual assets are copied.

`performance_frame.c` samples MIKU's existing original character atlas and stage
geometry. The character pixels remain Piapro Character License art, outside the
code GPL; see [LICENSING.md](../../../LICENSING.md).
© Crypton Future Media, INC. www.piapro.net. The generated images are unofficial,
free, non-commercial fan artwork; preserve the repository's character credit.
