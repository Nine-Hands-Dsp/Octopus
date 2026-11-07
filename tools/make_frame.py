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
BX0, BY0, BX1, BY1, BR = 0.5, 0.5, W - 0.5, H - 0.5, 24.0     # boîtier (plan du 07/11)
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

# arête du boîtier : arrondi en quart de cercle (rayon 14) qui donne du volume : la pente est forte au bord et s'adoucit
# vers la façade, la lumière (haut gauche) glisse donc progressivement sur l'arrondi au lieu d'un biseau plat
RR = 14.0
t = np.clip(-d_box, 0.0, RR)
hgt = RR * np.sqrt(np.clip(1.0 - ((RR - t) / RR) ** 2, 0.0, 1.0))
gy_, gx_ = np.gradient(cv2.GaussianBlur(hgt, (0, 0), 0.6 * AS), 1.0 / AS)
gx_ = np.clip(gx_, -6, 6); gy_ = np.clip(gy_, -6, 6)
nrm = np.dstack([-gx_, -gy_, np.ones_like(hgt)]); nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
Lv = np.array([-0.5, -0.62, 0.6]); Lv /= np.linalg.norm(Lv)
Hv = Lv + np.array([0, 0, 1.0]); Hv /= np.linalg.norm(Hv)
ndl = np.clip((nrm * Lv).sum(2), 0, 1)
spec = np.clip((nrm * Hv).sum(2), 0, 1)
on_round = 1.0 - smooth(RR - 3.0, RR, t)                                  # 1 sur l'arrondi, 0 sur la façade plate
base = tex + (on_round * 7.2)[..., None]                                 # métal un peu plus clair sur l'arrondi (sinon noyé dans le noir)
shade = 0.30 + 0.70 * ndl / Lv[2]
sheen = (spec ** 14) * 40.5 + (spec ** 80) * 58.5                       # reflet large + reflet vif (discrets, 07/11)
img = base * shade[..., None] + (sheen * on_round * smooth(0, 1.2, -d_box))[..., None] * np.array([0.97, 1.0, 1.05])

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
# centrés en hauteur entre le bord extérieur du boîtier (0) et le bord extérieur de la lunette (GY0 - 13) ;
# OCTOPUS réduit à 80 % (132 x 34,4), bord gauche à 35,1 ; NINE HANDS | DSP, bord droit à 1204,9, en bas du logo OCTOPUS
MID = (GY0 - 13.0) / 2.0 + 3.8                 # + 3,8 (10 px à 100 %) vers le bas (07/11)
OW, OH = 164.9 * 0.80, 43.0 * 0.80
OY = 0.75                                        # OCTOPUS seul : encore 2 px plus bas (à 100 %)
logo("logo_octopus_v003.png", 35.1, MID - OH / 2 + OY, 35.1 + OW, MID + OH / 2 + OY)
NW, NH = 145.3, 12.4
logo("logo_ninehands.png", 1204.9 - NW, MID + OH / 2 - NH, 1204.9, MID + OH / 2)      # bas aligné sur celui du logo OCTOPUS
out.save(os.path.join(ROOT, "background.png"), optimize=True)
print("background.png", out.size)
