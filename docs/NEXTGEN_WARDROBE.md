# Wardrobe concept review — 2026-11-08

Current integration status: [NEXTGEN_NATIVE_INTEGRATION.md](NEXTGEN_NATIVE_INTEGRATION.md)
and [CLAUDE_HANDOFF.md](../CLAUDE_HANDOFF.md) supersede earlier original-only,
host-only, provisional source-grid and memory figures in this milestone document.

The owner requested at least ten outfits, maintaining the existing singer identity. All three references were visually inspected before generation: `reference.webp`, `stage.webp` and `faces.webp`. The portrait sheet is a required part of every outfit, not an optional follow-up. The original assets remain intact.

[Current concept sheet, revision3](../assets/singer/wardrobe/concepts-v3.png) contains eleven proposed outfits, each with a full-body view and coordinated portrait inset:

1. Encore — tailored stagewear (close to the original; can be made more distinct).
2. Off Duty — casual hoodie.
3. Retro Wave — retro sportswear.
4. Prism — geometric futuristic dress.
5. Neon Punk — jacket, skirt and chunky boots.
6. City Beat — varsity streetwear.
7. Signal Suit — cyan/navy sci-fi technical bodysuit, high collar and full-length sleeves/trousers (owner corrected the earlier terminology).
8. Summer Beat — sporty cyan/navy bikini with pink trim, matching portrait straps/neckline (replaces Rain Signal at owner request).
9. Snow Glow — winter jacket and scarf.
10. Festival — festival yukata.
11. Moon Circuit — witch dress, pointed hat and loosely braided twin-tails.

These are concepts for owner review, not approved native skins or final sprite atlases. `catalog.json` records all eleven as unapproved and has no placeholder production atlas paths. The generation preserves the same hair/headset silhouette and pairs body/portrait clothing. Several small details still need cleanup: e.g. City Beat legs differ from the opaque-leggings brief, and Encore is close to the default. Owner preferences take precedence over the proposed names and clothing directions.

## Matching portrait and dance delivery

For each approved outfit, produce a matched set: twelve portrait states (neutral, blink, A/I/U/E/O, closed/hum, happy, surprised, left tilt, right tilt) and eight existing dance poses (four idle, two singing, arms, bow). Face shape, eye placement, hair, headset, collar, shoulder/sleeve design, palette and outfit accessories must agree between both atlases. Preserve the real engine's mouth mapping and all existing poses. No silent substitution of generic Miku artwork.

The contact sheet is not an animation sheet and must not be passed directly into `sprite_pack.py`. First create aligned production sheets, then validate grid, anchoring, palette, transparency, mouth positions, native readability, dirty rectangles and emulator views on both STAGE and DANCE.

## Resource constraint

The current original portrait+dance pixel data uses65928 bytes. Ten additional complete uncompressed copies would add659280 bytes, exceeding the native image's remaining396356 bytes (581564 minus185208), even before code and tables. Therefore the firmware pack must share unchanged head/hair/expression pixels and use measured clothing overlays/deltas or compression. The computer view can retain larger artwork. Do not promise ten raw atlas copies fit flash; benchmark the packed sets before native inclusion. Decode/draw work belongs in the main loop, never the audio ISR.

## Provenance

Generated with ChatGPT image generation using all three existing repository images as references. New art remains Hatsune Miku fan art under the same Piapro Character License as the original assets; not GPL/MIT. © Crypton Future Media, INC. www.piapro.net. Free, non-commercial, unofficial. See [LICENSING.md](../LICENSING.md) and [assets notice](../assets/NOTICE.md).

Next: owner selects/revises concepts, then prepare matched production atlases and measure a shared-pixel pack before registering native outfits. None of these ten designs has been finalized or added to the runtime registry.

Revision2 replaces only concepts07/08 in response to owner direction; revision1 is retained as review history. These remain costume concepts, not production animation sheets.

Revision3 implements the corrected bodysuit direction and adds the witch concept. The owner authorized compact packing work and adjusting the count as necessary to retain USB video headroom. Final production art still requires review.
