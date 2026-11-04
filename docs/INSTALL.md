# Installing Miku-FM

**Miku-FM has not been tested on a physical FM-1 yet.** Until it has, install it only if you are ready to use the
USB rescue (below).

## Before you start

- Back up what you care about. Miku-FM does not read other firmwares' projects or samples, and saving replaces the
  settings other Felucca-family firmwares keep in the same flash sector (see the README).
- Use Chrome or Edge for the web installers (Web MIDI).
- Leave any "bootloader" option of an installer **off**.

## In the browser (the site's Install button)

Open https://michaelaperry.github.io/MIKU-FM1/install.html in Chrome or Edge, connect the FM-1 by USB (a data cable,
directly), press **Install Miku-FM** and allow MIDI when asked. The page checks the package's identity before it starts,
shows the progress, and checks the identity the FM-1 reports after the restart. If it stops, replug and press Install
again: an interrupted write resumes. The same page returns the FM-1 to the official V15 (select the unmodified
`FM-1.fwsc` from M-VAVE; the browser checks its SHA-256). The installer code is Felucca's web installer (`site/js/`,
GPL-3.0-only), the browser side of the protocol Dinghy's `fm1_install.py` speaks. Until a release is tagged, the page
carries the development build of main.

## With the FM-1 Firmware Hub (once listed)

Open https://thefirmwarehub.com, press **Connect FM-1**, find Miku-FM and press **Install**.

## From a release, with Dinghy's installer

Download `miku-fm-X.Y.fwsc` from the [releases](https://github.com/MichaelAPerry/MIKU-FM1/releases) and check its
SHA-256 against `SHA256SUMS`. Then, with Python 3 and `pip install mido python-rtmidi`:

```sh
git clone https://github.com/zvenson/dinghy && cd dinghy
python3 tools/fm1_install.py --info                 # what runs on the connected FM-1
python3 tools/fm1_install.py miku-fm-X.Y.fwsc       # asks before it writes, then restarts into Miku-FM
```

## Going back

Install another firmware the same way (the Hub, or `fm1_install.py` with its package). Miku-FM carries Dinghy's
update loader, so the Hub reaches it like any other firmware.

## If it does not start

Hold **OCT−** while switching on: the USB rescue, no sound, only the updater. Install a working firmware from there.
After two crashes at boot the FM-1 goes there by itself.

Miku-FM's identity is `FM-1_39XXYYZZ` (version digits). The Hub resumes an interrupted install only for
`FM-1_9NN` identities; if an install of Miku-FM is cut off, install again, and if the FM-1 does not start, use
the USB rescue.
