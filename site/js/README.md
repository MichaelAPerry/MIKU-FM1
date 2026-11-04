# Vendored: the FM-1 web installer modules

`fm1ota.js` (the FM-1 USB-MIDI update protocol over Web MIDI) and `fm1pkg.js` (package identity, logical image, the
official V15 check) are copied unchanged from [Felucca](https://github.com/hugelton/Felucca) `web/` at commit
`743551712c36b3984392eea1dbe13a309907e17a`, by Leo Kuroshita (@kurogedelic), Hügelton Instruments, GPL-3.0-only (the same
licence as Miku-FM). They are the browser side of the protocol that Dinghy's `tools/fm1_install.py` implements in Python.
`site/install.html` uses them. Update them by copying the files again and recording the commit here.
