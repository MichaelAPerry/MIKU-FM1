#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Browser regression; serve repository root on localhost:8765 before running."""
from playwright.sync_api import sync_playwright
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with sync_playwright() as p:
 browser=p.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox'])
 page=browser.new_page(viewport={'width':1000,'height':1100})
 errors=[];page.on('pageerror',lambda e: errors.append(str(e)))
 page.goto('http://127.0.0.1:8765/companion/index.html')
 page.locator('#state').set_input_files(str(root/'build/evidence/performance.json'))
 page.locator('#wave').set_input_files(str(root/'build/evidence/performance.wav'))
 page.wait_for_function("document.querySelector('#audio').readyState>=1")
 page.evaluate("document.querySelector('#audio').currentTime=2")
 page.wait_for_function("document.querySelector('#status').textContent.includes('sample')")
 page.screenshot(path=str(root/'build/evidence/companion.png'))
 print(page.locator('#status').inner_text())
 assert page.evaluate("document.querySelector('#stage').getContext('2d').getImageData(0,0,240,240).data.some(x=>x>100)")
 assert page.evaluate('atlas.naturalWidth')==488
 page.locator('#state').set_input_files({'name':'invalid.json','mimeType':'application/json','buffer':b'{"format":"wrong"}'})
 page.wait_for_function("document.querySelector('#status').textContent.includes('Unsupported')")
 assert not errors,errors
 browser.close()
