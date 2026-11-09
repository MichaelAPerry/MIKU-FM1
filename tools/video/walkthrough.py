#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Record a captioned Miku-FM walkthrough in fm1-emu: the real firmware package, real LCD frames and audio.

    tools/video/walkthrough.py PACKAGE.fwsc FM1_EMU_BIN OUT_DIR

Writes OUT_DIR/trailer.mp4 (1280x720, captions beside the enlarged 240x240 LCD), OUT_DIR/trailer.jpg (poster) and
OUT_DIR/screens/*.png (240x240 stills of each page). Needs ffmpeg and Pillow. The LCD frames are the emulator's own,
nearest-neighbour enlarged; nothing on the display is drawn or edited afterwards. Captions sit outside the display."""
import subprocess, sys, shutil
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

FPS = 15
STEP = 1000 // FPS           # ms between LCD snapshots

def tap(t, k): return [f"{t}:{k}"]
# (time ms, panel action). Pages: STAGE DANCE SONG PATTERN MIXER GEN LYRICS LOOK LIBRARY BANK (HOME steps forward).
SCRIPT = []
SCRIPT += tap(1500, "PLAY")                                   # the boot song (factory slot 1) sings on STAGE
SCRIPT += tap(9000, "HOME")                                   # DANCE
SCRIPT += tap(14000, "EDIT")                                  # DANCE -> LOOK
SCRIPT += [f"{15000 + 900 * i}:enc0:+1" for i in range(5)]   # SELECT: outfits
SCRIPT += [f"{19800}:K3:+1", "20600:REC"]                     # stage effect, trigger it
SCRIPT += tap(22000, "EDIT")                                  # back to DANCE in the new outfit
SCRIPT += tap(27000, "SEQ")                                   # stop
SCRIPT += [f"{27300 + 300 * i}:HOME" for i in range(4)]       # SONG PATTERN MIXER GEN
SCRIPT += tap(30000, "REC")                                   # GEN: a new HYPERPOP song (DX7 chords and bass)
SCRIPT += tap(31500, "PLAY")
SCRIPT += tap(35500, "HOME")                                  # LYRICS
SCRIPT += tap(38500, "HOME")                                  # LOOK
SCRIPT += tap(41000, "HOME")                                  # LIBRARY
SCRIPT += tap(43500, "HOME")                                  # BANK (browse)
SCRIPT += [f"{45000 + 1000 * i}:enc0:+1" for i in range(3)]   # browse silently
SCRIPT += tap(48500, "EDIT")                                  # GLOBAL
SCRIPT += tap(50500, "EDIT")                                  # OP FREQUENCY
SCRIPT += [f"{51300}:K1:-2"]                                  # edit OP1 output level
SCRIPT += tap(52800, "EDIT")                                  # ENV RATES
SCRIPT += tap(54300, "EDIT")                                  # ENV LEVELS
SCRIPT += tap(56000, "HOME")                                  # STAGE, singing the new song
SCRIPT += tap(63000, "HOME")                                  # DANCE
END = 70000

CAPTIONS = [
    (0, 9000, "Miku-FM 0.7", "She sings on the M-VAVE FM-1."),
    (9000, 14000, "DANCE", "Full-body dancer, in time with the song."),
    (14000, 22000, "LOOK", "12 outfits, backgrounds, frames and stage effects, per song."),
    (22000, 27300, "DANCE", "The new outfit, on stage."),
    (27300, 35500, "GEN", "HYPERPOP, FUTURE BASS, JERSEY CLUB, DIGICORE, IDOL POP: a new song with DX7 chords and bass."),
    (35500, 38500, "LYRICS", "Write and edit what she sings."),
    (38500, 41000, "LOOK", "Choose her look for each song."),
    (41000, 43500, "LIBRARY", "10 song slots, four Japanese folk songs included."),
    (43500, 48500, "DX7 BANK", "Ten original six-operator voices, plus 32 of your own from Dexed."),
    (48500, 56000, "DX7 EDIT", "Every operator editable on the device; edits are saved in the song."),
    (56000, END, "Miku-FM 0.7", "Free fan firmware. Install it from your browser."),
]
STILLS = {"stage.png": 7000, "dance.png": 26000, "look.png": 21000, "gen.png": 34000, "lyrics.png": 37500,
          "library.png": 42800, "bank.png": 47800, "bank-edit.png": 55500}

def main():
    pkg, emu, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve(), Path(sys.argv[3]).resolve()
    run = out / "run"; (run / "work").mkdir(parents=True, exist_ok=True)
    snaps = list(range(0, END, STEP))
    subprocess.run([str(emu), "--fwsc", str(pkg), "--ms", str(END + 500), "--keys", ",".join(SCRIPT),
                    "--snap", ",".join(map(str, snaps)), "--wav", "audio.wav"], cwd=run, check=True,
                   stdout=open(run / "emu.log", "w"), stderr=subprocess.STDOUT)
    font = lambda s: ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", s)
    big, small = font(46), font(26)
    frames = out / "frames"; frames.mkdir(exist_ok=True)
    last = None
    for n, t in enumerate(snaps):
        p = run / "work" / f"lcd_{t}.bmp"
        if p.exists(): last = Image.open(p).convert("RGB")
        canvas = Image.new("RGB", (1280, 720), (16, 14, 24))
        if last is not None: canvas.paste(last.resize((648, 648), Image.NEAREST), (36, 36))
        d = ImageDraw.Draw(canvas)
        for a, b, title, text in CAPTIONS:
            if a <= t < b:
                d.text((730, 250), title, font=big, fill=(255, 120, 200))
                y, line = 330, ""
                for w in text.split():
                    if d.textlength(line + " " + w, font=small) > 510: d.text((730, y), line.strip(), font=small, fill=(220, 225, 240)); y += 38; line = ""
                    line += " " + w
                d.text((730, y), line.strip(), font=small, fill=(220, 225, 240))
        d.text((730, 660), "Real firmware in the fm1-emu emulator", font=font(18), fill=(120, 125, 150))
        canvas.save(frames / f"f{n:05d}.png")
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-framerate", str(FPS), "-i", str(frames / "f%05d.png"),
                    "-i", str(run / "audio.wav"), "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "26", "-preset", "slow",
                    "-c:a", "aac", "-b:a", "128k", "-shortest", "-movflags", "+faststart", str(out / "trailer.mp4")], check=True)
    shutil.copy(frames / f"f{int(STILLS['dance.png'] / STEP):05d}.png", out / "poster.png")
    Image.open(out / "poster.png").save(out / "trailer.jpg", quality=88)
    (out / "screens").mkdir(exist_ok=True)
    for name, t in STILLS.items():
        k = max(s for s in snaps if s <= t)
        while k > 0 and not (run / "work" / f"lcd_{k}.bmp").exists(): k -= STEP
        Image.open(run / "work" / f"lcd_{k}.bmp").convert("RGB").save(out / "screens" / name)
    print("walkthrough:", out / "trailer.mp4", (out / "trailer.mp4").stat().st_size, "bytes")

if __name__ == "__main__":
    main()
