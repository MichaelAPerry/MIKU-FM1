#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Make the site's trailer and page screenshots from an fm1-emu capture (host tool; needs Pillow and ffmpeg).

  tools/site_trailer.py CAPTURE_DIR OUT_DIR

CAPTURE_DIR is where fm1-emu ran with the key script and snapshot times printed by `--script` (see below): work/lcd_<ms>.bmp
every 83 ms and audio.wav. Writes OUT_DIR/trailer.mp4 (1280x720, H.264 + AAC), OUT_DIR/trailer.jpg (poster) and
OUT_DIR/screens/{stage,dance,song,pattern,mixer,gen}.png (240x240, the native screen).

  tools/site_trailer.py --script      prints the fm1-emu arguments of the capture
Every picture of the device in the trailer is the firmware's own screen, every sound its own audio; only the captions,
the title and the end card are drawn here."""
import subprocess
import sys
from pathlib import Path

KEYS = ("400:PLAY,7000:SELECT:+3,7600:FX,8000:K3:+20,8800:K1:+15,9600:K2:+12,10400-11000:LFO,11300:K1:+4,"
        "12600:HOME,18500:HOME,19300:SELECT:+2,20500:HOME,23000:HOME,24200:HOME,25000:SEQ,25400:K1:+2,25800:K4:+3,"
        "26200:REC,26800:PLAY,27600:HOME,31500:HOME")
T0, DT, T1 = 300, 83, 37000
# (from ms, to ms, label, caption); None = a card instead of the screen
SEG = [(300, 3300, None, "title"), (3300, 7000, "STAGE", "She sings.\nIn Japanese."), (7000, 10400, "STAGE", "Shape her\nvoice."),
       (10400, 12600, "HARMONY", "Give her\na harmony."), (12600, 18500, "DANCE", "She dances\nto your song."),
       (18500, 20500, "SONG", "Arrange it."), (20500, 23000, "PATTERN", "Write it,\nnote by note."), (23000, 24200, "MIXER", "Mix it."),
       (24200, 27600, "GEN", "Or let her\ndream one up."), (27600, 31500, "STAGE", "A new song.\nOne press."),
       (31500, 34000, "DANCE", "Every song\nis a stage."), (34000, 37000, None, "end")]
SHOTS = {"stage": 5000, "voice": 8600, "dance": 15000, "song": 19900, "pattern": 22000, "mixer": 23600, "gen": 26500}
FONT = "/usr/share/fonts/opentype/inter/Inter-{}.otf"
JP = "/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf"
W, H, NAVY, TEAL, PINK, DIM = 1280, 720, (10, 18, 48), (63, 224, 208), (255, 79, 168), (169, 184, 224)


def main():
    from PIL import Image, ImageDraw, ImageFont
    cap, out = Path(sys.argv[1]), Path(sys.argv[2])
    root = Path(__file__).resolve().parent.parent
    (out / "screens").mkdir(parents=True, exist_ok=True)
    f = lambda w, s: ImageFont.truetype(FONT.format(w), s)
    big, mid, small, jp = f("ExtraBold", 64), f("Bold", 30), f("Bold", 20), ImageFont.truetype(JP, 34)
    sheet = Image.open(root / "site/media/dancer.png").convert("RGBA")
    dancer = [sheet.crop((i * 61, 0, i * 61 + 61, 71)).resize((61 * 5, 71 * 5), Image.NEAREST) for i in range(8)]
    grid = Image.new("RGB", (W, H), NAVY)
    g = ImageDraw.Draw(grid)
    for x in range(0, W, 44): g.line([(x, 0), (x, H)], fill=(22, 34, 84))
    for y in range(0, H, 44): g.line([(0, y), (W, y)], fill=(22, 34, 84))
    times = list(range(T0, T1, DT))
    lcd = lambda t: Image.open(cap / f"work/lcd_{t}.bmp").convert("RGB")
    for name, t in SHOTS.items():
        lcd(min(times, key=lambda x: abs(x - t))).save(out / "screens" / f"{name}.png", optimize=True)
    ff = subprocess.Popen(["ffmpeg", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}",
                           "-framerate", f"1000/{DT}", "-i", "-", "-ss", f"{T0 / 1000}", "-i", str(cap / "audio.wav"),
                           "-t", f"{(T1 - T0) / 1000}", "-af", f"afade=t=out:st={(T1 - T0) / 1000 - 1.5}:d=1.5",
                           "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "23", "-preset", "slow",
                           "-c:a", "aac", "-b:a", "128k", "-movflags", "+faststart", str(out / "trailer.mp4")], stdin=subprocess.PIPE)
    poster = None
    for k, t in enumerate(times):
        a, b, label, cap_txt = next(s for s in SEG if s[0] <= t < s[1])
        fr = grid.copy()
        d = ImageDraw.Draw(fr)
        if label is None:
            frame = dancer[(t // 400) % 4 if cap_txt == "title" else 6 if (t // 400) % 2 else 4]
            fr.paste(frame, (W - 420, (H - frame.size[1]) // 2), frame)
            if cap_txt == "title":
                if poster is None: poster = fr
                d.text((90, 210), "Miku", font=f("Black", 120), fill=(238, 246, 255))
                d.text((90 + d.textlength("Miku", font=f("Black", 120)), 210), "-FM", font=f("Black", 120), fill=TEAL)
                d.text((94, 360), "a singer in your FM-1", font=mid, fill=DIM)
                d.text((94, 420), "おとのはこ", font=jp, fill=PINK)
            else:
                d.text((90, 200), "Play her in your browser.", font=f("Bold", 48), fill=(238, 246, 255))
                d.text((90, 270), "Install her over USB.", font=f("Bold", 48), fill=TEAL)
                d.text((94, 370), "michaelaperry.github.io/MIKU-FM1", font=mid, fill=PINK)
                d.text((94, 420), "Unofficial, non-commercial fan firmware for the M-VAVE FM-1", font=small, fill=DIM)
                d.text((94, 452), "Hatsune Miku: fan work under the Piapro Character License (PCL).", font=small, fill=DIM)
                d.text((94, 478), "\u00a9 Crypton Future Media, INC. www.piapro.net", font=small, fill=DIM)
        else:
            img = lcd(t).resize((630, 630), Image.NEAREST)
            d.rounded_rectangle((W - 700 - 12, 33, W - 70 + 12, 33 + 630 + 24), 28, fill=(5, 9, 20), outline=(42, 58, 122), width=3)
            fr.paste(img, (W - 700, 45))
            d.text((90, 230), label, font=small, fill=TEAL)
            d.multiline_text((88, 264), cap_txt, font=big, fill=(238, 246, 255), spacing=8)
            d.text((90, 640), "Miku-FM", font=small, fill=DIM)
        ff.stdin.write(fr.tobytes())
    ff.stdin.close()
    ff.wait()
    (poster or fr).save(out / "trailer.jpg", quality=85, optimize=True)
    print(f"trailer: {out / 'trailer.mp4'} ({len(times)} frames), screens: {', '.join(SHOTS)}")


if __name__ == "__main__":
    if sys.argv[1:] == ["--script"]:
        print(f"--ms {T1} --keys '{KEYS}' --snap {','.join(str(t) for t in range(T0, T1, DT))} --wav audio.wav")
    elif len(sys.argv) == 3:
        main()
    else:
        sys.exit(__doc__)
