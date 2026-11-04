# Art prompts (for ChatGPT image generation)

> How the art was made. The prompts below asked for an "original character"; the finished art is presented and licensed
> as **fan art of Hatsune Miku under the Piapro Character License** (free and non-commercial only, not under the code's
> GPL): see [LICENSING.md](../LICENSING.md).

Use these to make Miku-FM's singer and site art. The FM-1 screen is 240 × 240 pixels, so the device art is small
pixel art with few colours; the site art can be larger.

**Keep the singer original.** She should feel like a 2007 virtual idol (synthetic, bright, cyan and pink, headset
mic), but she must not be Hatsune Miku or any existing character: no grey sleeveless top with a teal tie, no
"01" mark, no leek, no Crypton or VOCALOID logos, no copied outfit. Each prompt says so; keep that line in.

Send back the PNGs as they come out (don't resize them). They go into `assets/singer/` as a separate art pack with a
NOTICE saying they were made with ChatGPT, apart from the GPL code.

Start with prompt 1 and reuse its picture in the later chats ("use the character from this image") so she stays the
same person.

---

## 1. Character reference sheet (do this first)

```
Design an ORIGINAL virtual-idol character for a tiny handheld music synthesizer called "Miku-FM".
Mood: a 2007-era digital singing idol — youthful, cheerful, synthetic, a bit robotic, synth-pop.
Look: long twin-tails in bright cyan-teal with a few pink tips; large expressive eyes; a slim headset microphone;
a futuristic stage outfit of my own design (white and dark navy with cyan and hot-pink accents, a small
synthesizer-key motif on the collar, a waveform pattern on the skirt hem). Clean anime style, flat colours.
Show: a character sheet on a plain white background — front view full body, a 3/4 view, a close-up of the face,
and the colour palette as swatches with hex codes.
She must be an original character, NOT Hatsune Miku or any existing character: no grey sleeveless top, no teal
necktie, no "01" tattoo, no leek, no logos or text other than the palette hex codes.
```

## 2. Device sprite sheet: face and mouth shapes (the most important one)

```
Using the character from the attached image, make a PIXEL ART sprite sheet for a 240x240 colour LCD.
Strict rules:
- Head-and-shoulders portrait, each sprite exactly 96x96 game pixels, drawn so that every game pixel is a crisp
  4x4 block of screen pixels (each cell is 384x384 px). No anti-aliasing, no blur, no gradients, no dithering noise.
- At most 16 colours in the whole sheet, the same palette in every frame.
- Background of every cell: solid magenta #FF00FF (it will be made transparent). Do not use magenta in the character.
- A 4 x 3 grid of equal cells, no gaps, no borders, no labels, no text.
- The head, hair and shoulders are IDENTICAL in every cell; only the eyes and mouth change, at the same positions.
Cells, left to right, top to bottom:
1 neutral, mouth closed   2 blink (eyes closed)    3 singing "A" (open, tall)   4 singing "I" (wide, teeth)
5 singing "U" (small round) 6 singing "E" (half open, wide) 7 singing "O" (round, open) 8 humming "N/M" (lips pressed)
9 happy smile, eyes curved 10 surprised              11 head tilted left, smiling 12 head tilted right, smiling
Original character, NOT Hatsune Miku or any existing character.
```

If the frames don't line up, ask: "Redo it with the head in exactly the same pixels in every cell; only the eyes and
mouth may differ."

## 3. Small stage sprite: full body, dancing

```
Using the character from the attached image, make a PIXEL ART sprite sheet of her full body singing on stage.
- Each frame exactly 48x64 game pixels, every game pixel a crisp 4x4 block (each cell 192x256 px).
- Same 16-colour palette as before, solid magenta #FF00FF background, no anti-aliasing, no text.
- A 4 x 2 grid of equal cells, no gaps: 1-4 a looping idle sway to the beat (small bob, twin-tails swinging),
  5-6 holding the headset mic and singing, 7 arms up (chorus), 8 bow.
Original character, NOT Hatsune Miku or any existing character.
```

## 4. Site banner

```
Using the character from the attached image, make a wide banner (1600x600) for the website of "Miku-FM", a
pocket synthesizer firmware. She sings into her headset on a small neon stage made of synthesizer keys; behind her a
dark navy night sky with cyan and pink waveforms, pixel stars and a faint 240x240 LCD grid. Leave the left third
calm for a title. Anime style, flat colours, crisp. No text, no logos.
Original character, NOT Hatsune Miku or any existing character.
```

## 5. Hub card / app icon

```
Using the character from the attached image, make a square 1024x1024 icon: her face, smiling and singing, inside a
rounded cyan-to-pink gradient frame shaped like a tiny LCD screen with four piano keys under it. Bold and readable
at 64x64. Flat colours, no text, no logos.
Original character, NOT Hatsune Miku or any existing character.
```

(The Firmware Hub card itself will be a real screenshot of the firmware's stage page once the sprite is in it.)

## 6. Demo lyrics (text, not an image)

```
Write ORIGINAL Japanese lyrics for a short cheerful 2007-style synth-pop idol song about a tiny singer who lives
inside a pocket synthesizer and sings what you write for her. Not based on any existing song.
Structure: verse (4 lines), chorus (4 lines). 7 or 8 syllables (morae) per line.
Give each line three ways: kana (hiragana), romaji split into morae with hyphens (e.g. "ko-n-ni-chi-wa"),
and an English translation. Use only simple morae (vowels, k s t n h m y r w g z d b p rows, and "n").
```

Send the lyrics as text; they become the demo song's lyric track once the phoneme engine is in.
