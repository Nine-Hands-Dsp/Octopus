# OCTOPUS (NINE HANDS | DSP) - footswitchs (fs_off.png / fs_on.png, 2x) : photo de l'écrou hexagonal inox
# (design/footswitch_hex_source.png, 07/11/2026) détourée, 93 unités entre sommets, ombre portée.
# fs_on = léger reflet clair sur le haut de l'écrou (la lettre lumineuse est au-dessus). Sprite de 150 unités (Layout.h fsSprite).
import os
import numpy as np, cv2
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AS = 2.0
FS_W = 150.0                       # côté du sprite (unités)
HEX_W = 93.0                       # écrou : largeur entre sommets (plan de l'utilisateur)
SRC_C, SRC_W = (625.5, 618.0), 813.0   # centre et largeur entre sommets de l'écrou sur la photo

src = np.asarray(Image.open(os.path.join(ROOT, "design", "footswitch_hex_source.png")).convert("RGBA")).astype(np.float64)
n = int(round(FS_W * AS))
sc = HEX_W * AS / SRC_W
M = np.float32([[sc, 0, n / 2 - SRC_C[0] * sc], [0, sc, n / 2 - SRC_C[1] * sc]])
rgba = cv2.warpAffine(src, M, (n, n), flags=cv2.INTER_AREA, borderMode=cv2.BORDER_CONSTANT, borderValue=(0, 0, 0, 0))
a = rgba[..., 3] / 255.0
rgb = np.where(a[..., None] > 1e-4, rgba[..., :3], 0)

def shift(img_, dx, dy): return cv2.warpAffine(img_, np.float32([[1, 0, dx], [0, 1, dy]]), (n, n))
cast = cv2.GaussianBlur(shift(a, 4.5 * AS, 6.5 * AS), (0, 0), 4.5 * AS) * 0.55
contact = cv2.GaussianBlur(shift(a, 1.0 * AS, 1.5 * AS), (0, 0), 1.6 * AS) * 0.45
for led_on in (False, True):
    c = rgb.copy()
    if led_on:
        yy_, xx_ = np.mgrid[0:n, 0:n].astype(np.float64)
        uy_ = -(yy_ - n / 2) / (AS * 50.0)
        w = np.clip(uy_, 0, 1) ** 2 * (c.mean(2) / 255.0)
        c = c + (w * 46.0)[..., None] * np.array([1.0, 0.97, 0.92])
    sh = np.clip(cast + contact, 0, 0.8) * (1 - a)
    al = np.clip(a + sh, 0, 1)
    col = np.where(al[..., None] > 1e-4, c * (a / np.maximum(al, 1e-4))[..., None], 0)
    out = np.dstack([np.clip(col, 0, 255), al * 255]).astype(np.uint8)
    name = "fs_on.png" if led_on else "fs_off.png"
    Image.fromarray(out, "RGBA").save(os.path.join(ROOT, name), optimize=True)
    print(name, out.shape[:2])
