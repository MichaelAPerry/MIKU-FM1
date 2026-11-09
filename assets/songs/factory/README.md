# Factory song library sources

The owner selected these four songs. `songs.json` contains independent melody
transcriptions and first-verse Japanese mora assignments, with complete melody
forms, rests and endings. These are ordinary editable songs, not audio samples.
The accompaniment is an original restrained firmware arrangement. No downloaded
recording, MIDI accompaniment, modern arrangement, or score image is bundled.

1. **Sakura Sakura** — traditional; 1888 text beginning “sakura sakura / yayoi no
   sora wa”. Melody checked against John Chambers' ABC collection:
   https://abcnotation.com/tunePage?a=trillian.mit.edu%2F~jc%2Fmusic%2Fabc%2FJapan%2FSakura_Am.utf8%2F0000
2. **Tōryanse** — traditional complete 19-bar tune and text, checked against the
   LilyPond melody in https://en.wikipedia.org/wiki/T%C5%8Dryanse
3. **Furusato** — Teiichi Okano (1914), words Tatsuyuki Takano. First verse,
   complete 16-bar melody. Melody/first-verse underlay checked against the vocal
   line at https://www.senzoku.ac.jp/music/career/pdf/score_furusato.pdf .
   Its modern piano accompaniment is not copied.
4. **Kōjō no Tsuki** — Rentarō Taki (1901), words Bansui Doi. First verse,
   complete 16-bar melody in D minor. Melody checked against Cdang's CC0
   LilyPond transcription:
   https://commons.wikimedia.org/wiki/File:Kojo_no_tsuki_re_mineur.svg
   Japanese text checked against https://www.mu-tech.org/Traditional/koujyo.html .

The underlying old melodies and lyrics are public domain in Japan and the US.
The factual melody transcription is separate from any source's page prose,
score typography, accompaniment or recording. Existing code/asset licence rules
continue to apply; this does not relicense character artwork.

## Grid and articulation

A source quarter note occupies two existing grid steps; the stored tempo is
half the source quarter tempo. This fits complete melodies into 6–10 vocal
patterns without changing the four-track architecture or MKS2 layout. Furusato
keeps continuous 3/4 timing: its bar accents repeat every six steps and do not
insert extra silence to fill 4/4 rows. An instrumental ending adds one row.

`_` sustains the preceding vowel; `/` is an explicit rest. Multiple morae on one
pitch use `+` and share that note's duration. `sha` and `ja` use existing SH/J
consonants with A. `rya` currently uses the existing R→A approximation: the engine
has no palatalized R consonant. This limitation needs listening review, not a
claim of native-like pronunciation. The complete first verses were selected by
the owner; additional verses are not silently substituted or truncated.

`python3 tools/gen_factory_songs.py --check` verifies the generated sparse tables.
`factory_export` exports exactly the same native songs as editable `.mks` files.
Factory reconstruction never writes flash: saved slots override factories.
