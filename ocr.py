import easyocr
import sys
import json
import numpy as np
from PIL import Image

img = np.array(Image.open(sys.argv[1]))

reader = easyocr.Reader(['ch_sim', 'en'], gpu=False)
results = reader.readtext(img)

def clean_text(t):
    # Keep only printable ASCII + CJK
    return ''.join(
        c for c in t
        if (32 <= ord(c) < 127) or (0x4E00 <= ord(c) <= 0x9FFF)
    )

out = []
for (bbox, text, conf) in results:
    cleaned = clean_text(text)
    if not cleaned:
        continue
    x1, y1 = bbox[0]
    x2, y2 = bbox[2]
    out.append({
        "text": cleaned,
        "x": int((x1 + x2) / 2),
        "y": int((y1 + y2) / 2),
        "conf": float(conf)
    })

# Force ASCII output
s = json.dumps(out, ensure_ascii=True)
sys.stdout.buffer.write(s.encode('ascii', errors='ignore'))
sys.stdout.buffer.write(b'\n')
sys.stdout.flush()