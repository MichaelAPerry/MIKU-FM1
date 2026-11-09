# Art and song pack: notice

Everything in `assets/` is **separate from the code** and is not covered by the code's GPL-3.0 / MIT licences.
Replace it with your own art to make a different singer: `tools/sprite_pack.py` rebuilds the firmware's sprites.

## The singer (`assets/singer/`)

- Made by Michael Perry with ChatGPT image generation (2026-11-03) from the prompts in `docs/ART_PROMPTS.md`:
  `reference.webp` (character sheet), `faces.webp` (face sprite sheet), `stage.webp` (full-body sheet),
  `banner.webp` (site banner), `icon.webp` (icon).
- The singer is **fan art of Hatsune Miku**, Crypton Future Media, INC.'s character, under the Piapro Character License
  (PCL). Credit: この作品はピアプロ・キャラクター・ライセンスに基づいてクリプトン・フューチャー・メディア株式会社の
  キャラクター「初音ミク」を描いたものです。 / This work depicts Crypton Future Media, INC.'s character "Hatsune Miku" under
  the Piapro Character License (PCL), https://piapro.jp/license/pcl/summary. © Crypton Future Media, INC. www.piapro.net
- Free and non-commercial only (no sales, no payment or donations for it, no ads), not official, not to be sublicensed:
  the art is **not** under the code's GPL / MIT licences. The full terms and the list of files that hold the art (also
  `firmware/ui/singer_sprites.h` and `site/media/`) are in [LICENSING.md](../LICENSING.md).

## The demo song (`assets/songs/`)

- 「おとのはこ」(The Little Sound Box): original lyrics written for Miku-FM with ChatGPT, provided by Michael Perry;
  melody original to Miku-FM. Free to use with the firmware.

## Wardrobe concepts (2026-11-08)

`assets/singer/wardrobe/concepts-v1.png`, `concepts-v2.png` and `concepts-v3.png` were made with ChatGPT image generation using the existing reference, face and dance sheets. It contains unapproved costume concepts, including matching portrait insets, for owner review. It is Hatsune Miku fan art under the same PCL terms and credit above, not GPL/MIT. The adjacent catalog is review metadata, not a production sprite atlas.

The lossless compact representation `firmware/ui/singer_compact.h` contains the same character pixels as `singer_sprites.h` and retains their PCL terms; compression does not relicense the artwork.

## Paired wardrobe and stage sheets

`assets/singer/outfits/*/sheet.png` are generated production source options with
12 portrait states and eight dance poses per outfit, based on the original
character art. All retain the singer's PCL notice above. They remain prototype
visual choices pending owner review. `firmware/ui/wardrobe_sprites.h` contains
the corresponding character pixels under the same terms.

`assets/stage/effects.png` contains separately generated generic fireworks,
sparkles, confetti and smoke; see `assets/stage/README.md`. It remains part of the
separate art pack, not the code's GPL/MIT grant.

The four selected public-domain factory tunes and transcription references are
listed in `assets/songs/factory/README.md`.
