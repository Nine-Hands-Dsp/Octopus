# OCTOPUS (NINE HANDS | DSP) - cadre de la pédale (background.png, 2x) d'après le plan de l'utilisateur (07/11/2026) :
# boîtier 1249 x 934 à coins arrondis, façade en alu brossé noir (design/brushed_texture.png), arête éclairée en haut
# à gauche, lunette régulière autour de l'écran, logos OCTOPUS (haut gauche) et NINE HANDS | DSP (haut droite).
# Les footswitchs (fs_off.png, tools/make_footswitch.py), les lettres A-D et les marquages ] E [ / ] F [ / accordeur
# sont dessinés par le plugin (ils s'allument). Positions : Layout.h.
import os
import numpy as np, cv2
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AS = 2.0
W, H = 1249, 934
CW, CH = int(W * AS), int(H * AS)
BX0, BY0, BX1, BY1, BR = 0.5, 0.5, W - 0.5, H - 0.5, 24.0             # boîtier
GX0, GY0, GR = 35.85, 67.2, 7.5                                         # vitre (Layout.h : glass)
GW = 1176.95; GX1 = GX0 + GW; GY1 = GY0 + 651.0 * GW / 1064.0

def smooth(e0, e1, v):
    t = np.clip((v - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)
def sd_rr(x, y, x0, y0, x1, y1, r):
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    qx, qy = np.abs(x - cx) - ((x1 - x0) / 2 - r), np.abs(y - cy) - ((y1 - y0) / 2 - r)
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r

yy, xx = (np.mgrid[0:CH, 0:CW].astype(np.float64) + 0.5) / AS
d_box = sd_rr(xx, yy, BX0, BY0, BX1, BY1, BR)
alpha = np.clip(0.5 - d_box * AS, 0, 1)
tex = np.asarray(Image.open(os.path.join(ROOT, "design", "brushed_texture.png")).convert("RGB").resize((CW, CH), Image.LANCZOS)).astype(np.float64)

# arête du boîtier : biseau arrondi de 9 unités, éclairé en haut à gauche
hgt = 7.0 * smooth(0.0, 9.0, -d_box)
gy_, gx_ = np.gradient(cv2.GaussianBlur(hgt, (0, 0), 0.8 * AS), 1.0 / AS)
nrm = np.dstack([-gx_, -gy_, np.ones_like(hgt)]); nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
Lv = np.array([-0.5, -0.62, 0.6]); Lv /= np.linalg.norm(Lv)
Hv = Lv + np.array([0, 0, 1.0]); Hv /= np.linalg.norm(Hv)
ndl = np.clip((nrm * Lv).sum(2), 0, 1); spec = np.clip((nrm * Hv).sum(2), 0, 1) ** 60
img = tex * (0.35 + 0.65 * ndl / Lv[2])[..., None] + (spec * 210.0 * smooth(0, 1.5, -d_box))[..., None] * np.array([0.97, 1.0, 1.05])

# lunette de l'écran : fente sombre, flanc mat, fine gorge, petite lèvre claire, puis la façade
d_g = sd_rr(xx, yy, GX0, GY0, GX1, GY1, GR)
BEZ_OUT = 13.0
def bezel_profile(d):
    v = np.full_like(d, 8.0)
    v = np.where(d > 0.8, 8.0 + 14.0 * smooth(0.8, 2.2, d), v)
    v = np.where(d > 2.2, 22.0 + 4.0 * smooth(2.2, 9.0, d), v)
    v = v - 13.0 * np.exp(-((d - 10.0) / 0.55) ** 2)
    v = v + 30.0 * np.exp(-((d - 11.6) / 0.6) ** 2)
    return v
bez = smooth(-0.6, 0.0, d_g) * (1 - smooth(BEZ_OUT - 1.0, BEZ_OUT + 0.6, d_g))
img = img * (1 - bez[..., None]) + (bezel_profile(d_g)[..., None] * np.array([0.95, 0.98, 1.05])) * bez[..., None]
g = np.clip(0.5 - d_g * AS, 0, 1)[..., None]
img = img * (1 - g) + np.array([7.0, 8.0, 10.0]) * g
out = Image.fromarray(np.dstack([np.clip(img, 0, 255), alpha * 255]).astype(np.uint8), "RGBA")

# logos (tirés des SVG fournis, design/svg) : OCTOPUS en haut à gauche, NINE HANDS | DSP en haut à droite
def logo(name, x0, y0, x1, y1, op=0.96):
    im = Image.open(os.path.join(ROOT, "design", name)).convert("RGBA")
    im = im.resize((int(round((x1 - x0) * AS)), int(round((y1 - y0) * AS))), Image.LANCZOS)
    a = np.asarray(im).astype(np.float64); a[..., 3] *= op
    out.alpha_composite(Image.fromarray(a.astype(np.uint8), "RGBA"), (int(round(x0 * AS)), int(round(y0 * AS))))
logo("logo_octopus_v003.png", 35.1, 11.7, 200.0, 54.7)
logo("logo_ninehands.png", 1059.6, 43.8, 1204.9, 56.2)
out.save(os.path.join(ROOT, "background.png"), optimize=True)
print("background.png", out.size)
