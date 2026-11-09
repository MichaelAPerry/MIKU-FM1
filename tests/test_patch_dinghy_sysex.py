# SPDX-License-Identifier: GPL-3.0-only
"""tools/patch_dinghy_sysex.py: applies to the pinned text (pristine, or wrapped by the video patch), refuses anything else."""
from pathlib import Path
import importlib.util, sys, tempfile
root = Path(sys.argv[1])
spec = importlib.util.spec_from_file_location('ps', root / 'tools/patch_dinghy_sysex.py')
ps = importlib.util.module_from_spec(spec); spec.loader.exec_module(ps)
OTA = ps.OTA_OLD + '    if (d == 7u && ota_dec[2] == 0x11)\n        ota_reply_identity();\n}\n'
USB = 'x\n' + ps.SX_OLD + '\ny\n'
def tree(tmp, usb, ota):
    d = Path(tmp) / 'firmware/src'; d.mkdir(parents=True)
    (d / 'usb.c').write_text(usb); (d / 'ota.c').write_text(ota); return d
with tempfile.TemporaryDirectory() as t:
    d = tree(t, USB, OTA); ps.patch(t)
    u = (d / 'usb.c').read_text(); o = (d / 'ota.c').read_text()
    assert 'sx_frame[4104]' in u and 'sx_frame[640]' in u and '#ifdef FELUCCA_LOADER' in u    # the loader keeps 640
    assert o.count('app_sysex(fp, fn)') == 1 and 'fp[0] == 0x43u' in o and 'd = ota_take();' in o and '#ifndef FELUCCA_LOADER' in o
    try: ps.patch(t); raise SystemExit('second application must refuse')
    except ValueError: pass
with tempfile.TemporaryDirectory() as t:                      # after the video patch both branches carry the buffer
    d = tree(t, USB + USB, OTA); ps.patch(t)
    assert (d / 'usb.c').read_text().count('sx_frame[4104]') == 2
for usb, ota in ((USB.replace('640', '512'), OTA), (USB, OTA.replace('ota_take', 'ota_grab'))):
    with tempfile.TemporaryDirectory() as t:
        tree(t, usb, ota)
        try: ps.patch(t); raise SystemExit('changed pinned text must refuse')
        except ValueError: pass
