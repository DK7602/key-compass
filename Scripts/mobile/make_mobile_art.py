#!/usr/bin/env python3
"""Makes the artwork the Android app needs from the desktop page art (run once; the results are in Resources/mobile).

  panel.png  - a brass-framed parchment panel with a blank title scroll, cut from the Song Mapper page
               (1.5x design scale). The app draws it in nine slices at any size and writes the title on the scroll.
  map.jpg    - the old map from the edges of the compass page, for behind the panels.
"""
import os
import numpy as np
from PIL import Image

root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
res = os.path.join(root, 'Resources')
out = os.path.join(res, 'mobile')
os.makedirs(out, exist_ok=True)

# --- panel: the CHORD DIAGRAMS panel (design rect 10,250,308,610) with its title taken off the scroll
page = Image.open(os.path.join(res, 'mapper_bg.jpg')).convert('RGB')
s = 1.5
x, y, w, h = 10, 250, 308, 610
panel = np.asarray(page.crop((int(x * s), int(y * s), int((x + w) * s), int((y + h) * s)))).astype(float)
y0, y1, x0, x1 = 8, 33, 96, 377            # the lettering on the scroll (in the cut-out's pixels)
top, bottom = panel[y0 - 1, x0:x1], panel[y1, x0:x1]
rng = np.random.default_rng(7)
for yy in range(y0, y1):
    t = (yy - (y0 - 1)) / float(y1 - (y0 - 1))
    row = top * (1.0 - t) + bottom * t
    row += rng.normal(0.0, 2.2, row.shape)          # a little grain so it reads as paper
    panel[yy, x0:x1] = row
Image.fromarray(np.clip(panel, 0, 255).astype(np.uint8)).save(os.path.join(out, 'panel.png'), optimize=True)

# --- map: the left and right edges of the compass page, above the control plates
bg = Image.open(os.path.join(res, 'background.jpg')).convert('RGB')
left, right = bg.crop((0, 0, 200, 780)), bg.crop((890, 0, 1090, 780))
m = Image.new('RGB', (400, 780))
m.paste(left, (0, 0))
m.paste(right, (200, 0))
m.save(os.path.join(out, 'map.jpg'), quality=86, optimize=True)
# the panel as a JPEG is a third of the size and looks the same
Image.open(os.path.join(out, 'panel.png')).convert('RGB').save(os.path.join(out, 'panel.jpg'), quality=90, optimize=True)
os.remove(os.path.join(out, 'panel.png'))
print('wrote', os.listdir(out))
