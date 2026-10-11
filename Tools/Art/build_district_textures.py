"""Generate the tiling surface textures of the 1v1 map with numpy, inside Blender (for its numpy and PNG writer).

    ART_DIR = r"<repo>/Tools/Art"; exec(open(ART_DIR + "/build_district_textures.py").read())

Everything is synthesised from periodic noise, so every map tiles without a seam, and nothing is taken
from anywhere. Per surface: T_District_<Name>_BaseColor (sRGB), _Normal (green up, as Blender bakes; the
importer flips it) and _RM (red roughness, green metallic, blue cavity). Plus T_District_Grunge, a large
soft mask the materials use to break up repetition. Output: Tools/Art/District/*.png, 2048 px
(the mask 1024 px). The sizes in metres each texture is meant to cover are in SURFACES.
"""
import os

import bpy
import numpy as np

N = 2048
# Name: metres one repeat covers. The materials map the textures in world space with these sizes.
SURFACES = {'Concrete': 4.0, 'Floor': 4.0, 'Plaster': 2.0, 'Tread': 1.0, 'Deck': 2.0, 'Steel': 1.5, 'Wood': 1.2}


def value_noise(n, cells, rng):
    grid = rng.random((cells, cells))
    x = np.linspace(0.0, cells, n, endpoint=False)
    i = x.astype(int)
    f = x - i
    f = f * f * (3.0 - 2.0 * f)
    j = (i + 1) % cells
    fx, fy = f[None, :], f[:, None]
    return (grid[np.ix_(i, i)] * (1 - fx) + grid[np.ix_(i, j)] * fx) * (1 - fy) + (grid[np.ix_(j, i)] * (1 - fx) + grid[np.ix_(j, j)] * fx) * fy


def fbm(n, cells, octaves, rng, gain=0.5):
    total, amplitude, norm = np.zeros((n, n)), 1.0, 0.0
    for octave in range(octaves):
        c = min(cells * 2 ** octave, n // 2)
        total += value_noise(n, c, rng) * amplitude
        norm += amplitude
        amplitude *= gain
    return total / norm


def stretched(n, cells_x, cells_y, rng):
    """Noise with different detail along the two axes: streaks."""
    grid = rng.random((cells_y, cells_x))

    def axis(cells):
        x = np.linspace(0.0, cells, n, endpoint=False)
        i = x.astype(int)
        f = x - i
        return i, (i + 1) % cells, f * f * (3.0 - 2.0 * f)
    ix, jx, fx = axis(cells_x)
    iy, jy, fy = axis(cells_y)
    fx, fy = fx[None, :], fy[:, None]
    return (grid[np.ix_(iy, ix)] * (1 - fx) + grid[np.ix_(iy, jx)] * fx) * (1 - fy) + (grid[np.ix_(jy, ix)] * (1 - fx) + grid[np.ix_(jy, jx)] * fx) * fy


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def grid_lines(n, period, width):
    """1 on the lines of a grid with this period in pixels, falling off over the width."""
    x = np.arange(n)
    d = np.minimum(x % period, period - x % period).astype(float)
    line = 1.0 - smoothstep(width * 0.5, width, d)
    return np.maximum(line[None, :], line[:, None]), line[None, :] * np.ones((n, 1)), line[:, None] * np.ones((1, n))


def dots(n, period, offsets, radius):
    """Round marks repeated on a grid, at the given offsets inside each cell."""
    y, x = np.mgrid[0:n, 0:n]
    out = np.zeros((n, n))
    for ox, oy in offsets:
        dx = (x - ox * period) % period
        dy = (y - oy * period) % period
        dx = np.minimum(dx, period - dx)
        dy = np.minimum(dy, period - dy)
        out = np.maximum(out, 1.0 - smoothstep(radius * 0.6, radius, np.sqrt(dx * dx + dy * dy)))
    return out


def cracks(n, count, steps, rng):
    """Thin wandering lines, wrapped at the edges."""
    mask = np.zeros((n, n))
    for _ in range(count):
        x, y = rng.random() * n, rng.random() * n
        angle = rng.random() * 2.0 * np.pi
        for _ in range(steps):
            angle += (rng.random() - 0.5) * 0.5
            x = (x + np.cos(angle) * 1.5) % n
            y = (y + np.sin(angle) * 1.5) % n
            xi, yi = int(x), int(y)
            mask[yi, xi] = 1.0
            mask[yi, (xi + 1) % n] = max(mask[yi, (xi + 1) % n], 0.6)
            mask[(yi + 1) % n, xi] = max(mask[(yi + 1) % n, xi], 0.6)
    return mask


def normal_from_height(height, strength):
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * strength
    length = np.sqrt(dx * dx + dy * dy + 1.0)
    return np.stack((-dx / length * 0.5 + 0.5, -dy / length * 0.5 + 0.5, 1.0 / length * 0.5 + 0.5), axis=-1)


def save(folder, name, rgb, srgb):
    rgb = np.clip(rgb, 0.0, 1.0)
    n = rgb.shape[0]
    if name in bpy.data.images:
        bpy.data.images.remove(bpy.data.images[name])
    image = bpy.data.images.new(name, n, n, alpha=False)
    image.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
    rgba = np.concatenate((rgb, np.ones((n, n, 1))), axis=-1).astype(np.float32)
    # The values are written to the file as they are: for a colour map they are meant as what is seen (sRGB).
    image.pixels.foreach_set(rgba.ravel())
    image.filepath_raw = os.path.join(folder, name + '.png')
    image.file_format = 'PNG'
    image.save()
    bpy.data.images.remove(image)


def write_set(folder, name, color, height, rough, metal, strength):
    cavity = np.clip(1.0 - (np.clip(-(height - np.mean(height)), 0.0, None) * 4.0), 0.4, 1.0)
    save(folder, 'T_District_%s_BaseColor' % name, color, True)
    save(folder, 'T_District_%s_Normal' % name, normal_from_height(height, strength), False)
    save(folder, 'T_District_%s_RM' % name, np.stack((rough, metal, cavity), axis=-1), False)


def tint(value, rgb):
    return value[..., None] * np.array(rgb)[None, None, :]


def concrete(folder):
    """Cast wall concrete: two metre formwork panels with tie holes, pores, blotches and run-off streaks."""
    rng = np.random.default_rng(11)
    seams, _, _ = grid_lines(N, N // 2, 7)
    holes = dots(N, N // 2, ((0.25, 0.25), (0.75, 0.25), (0.25, 0.75), (0.75, 0.75)), 13)
    fine = fbm(N, 96, 5, rng)
    blotch = fbm(N, 5, 4, rng)
    pores = smoothstep(0.80, 0.86, fbm(N, 400, 2, rng)) * smoothstep(0.45, 0.6, fbm(N, 24, 2, rng))
    streaks = stretched(N, 180, 5, rng) * smoothstep(0.35, 0.75, fbm(N, 6, 2, rng))
    height = fine * 0.10 + blotch * 0.05 - seams * 0.5 - holes * 0.7 - pores * 0.35
    value = 0.47 * (0.84 + 0.30 * blotch) * (0.94 + 0.12 * fine) * (1.0 - 0.10 * streaks) * (1.0 - 0.35 * seams) * (1.0 - 0.55 * holes) * (1.0 - 0.30 * pores)
    color = tint(value, (1.0, 0.985, 0.955))
    rough = np.clip(0.80 + 0.10 * (fine - 0.5) - 0.10 * (blotch - 0.5) + 0.08 * seams, 0.0, 1.0)
    write_set(folder, 'Concrete', color, height, rough, np.zeros((N, N)), 9.0)


def floor(folder):
    """Sealed floor concrete: two metre slabs with saw cuts, polished and dull patches, hairline cracks, scuffs."""
    rng = np.random.default_rng(23)
    joints, _, _ = grid_lines(N, N // 2, 4)
    fine = fbm(N, 160, 4, rng)
    blotch = fbm(N, 6, 5, rng)
    polish = smoothstep(0.35, 0.7, fbm(N, 9, 4, rng))
    crack = cracks(N, 9, 900, rng)
    speck = smoothstep(0.78, 0.84, fbm(N, 500, 1, rng))
    scuff = stretched(N, 6, 140, rng) * smoothstep(0.55, 0.8, fbm(N, 5, 3, rng))
    height = fine * 0.035 - joints * 0.55 - crack * 0.30
    value = 0.34 * (0.80 + 0.36 * blotch) * (0.96 + 0.08 * fine) * (1.0 - 0.5 * joints) * (1.0 - 0.45 * crack) * (1.0 + 0.10 * speck) * (1.0 - 0.12 * scuff)
    color = tint(value, (0.97, 0.99, 1.03))
    rough = np.clip(0.66 - 0.30 * polish + 0.12 * (fine - 0.5) + 0.2 * joints + 0.15 * crack + 0.10 * scuff, 0.18, 1.0)
    write_set(folder, 'Floor', color, height, rough, np.zeros((N, N)), 7.0)


def plaster(folder):
    """Painted render: a fine sandy grain with soft trowel marks. Near white, the material tints it."""
    rng = np.random.default_rng(37)
    grain = fbm(N, 260, 3, rng)
    trowel = stretched(N, 14, 60, rng)
    blotch = fbm(N, 5, 4, rng)
    height = grain * 0.16 + trowel * 0.05
    value = 0.80 * (0.93 + 0.12 * blotch) * (0.96 + 0.08 * grain)
    rough = np.clip(0.86 + 0.08 * (grain - 0.5), 0.0, 1.0)
    write_set(folder, 'Plaster', tint(value, (1.0, 1.0, 1.0)), height, rough, np.zeros((N, N)), 10.0)


def tread(folder):
    """Painted steel tread plate: raised lozenges in alternating directions, the paint worn bright on their tops."""
    rng = np.random.default_rng(41)
    period = N // 16
    y, x = np.mgrid[0:N, 0:N]
    cx, cy = (x % period) - period * 0.5, (y % period) - period * 0.5
    flip = (((x // period) + (y // period)) % 2) * 2.0 - 1.0
    u, v = (cx + cy * flip) * 0.7071, (cx - cy * flip) * 0.7071
    bump = 1.0 - smoothstep(0.75, 1.0, np.sqrt((u / (period * 0.40)) ** 2 + (v / (period * 0.11)) ** 2))
    fine = fbm(N, 200, 3, rng)
    wear = bump * smoothstep(0.40, 0.65, fbm(N, 10, 4, rng))
    dirt = (1.0 - bump) * smoothstep(0.45, 0.75, fbm(N, 14, 3, rng))
    height = bump * 0.6 + fine * 0.03
    paint = tint(0.20 * (0.9 + 0.2 * fine) * (1.0 - 0.35 * dirt), (0.95, 1.0, 1.08))
    steel = tint(0.62 * (0.9 + 0.2 * fine), (1.0, 1.0, 1.02))
    color = paint * (1.0 - wear[..., None]) + steel * wear[..., None]
    rough = np.clip(0.52 + 0.2 * dirt - 0.22 * wear + 0.08 * (fine - 0.5), 0.0, 1.0)
    metal = np.clip(0.25 + 0.75 * wear, 0.0, 1.0)
    write_set(folder, 'Tread', color, height, rough, metal, 6.0)


def deck(folder):
    """Trapezoidal steel roof deck seen from below, painted light and dusty."""
    rng = np.random.default_rng(53)
    period = N // 8
    x = np.arange(N) % period / period
    rib = np.clip(np.minimum(smoothstep(0.10, 0.22, x), 1.0 - smoothstep(0.58, 0.70, x)), 0.0, 1.0)
    rib = rib[None, :] * np.ones((N, 1))
    fine = fbm(N, 120, 4, rng)
    dust = fbm(N, 7, 4, rng)
    seam = grid_lines(N, N // 2, 3)[2]
    height = rib * 0.8 + fine * 0.02 - seam * 0.15
    value = 0.50 * (0.86 + 0.22 * dust) * (0.80 + 0.20 * rib) * (1.0 - 0.3 * seam)
    rough = np.clip(0.62 + 0.14 * (dust - 0.5), 0.0, 1.0)
    write_set(folder, 'Deck', tint(value, (0.98, 1.0, 1.03)), height, rough, np.full((N, N), 0.15), 5.0)


def steel(folder):
    """Painted structural steel: a faint orange peel, scratches along one direction, chips. The material tints it."""
    rng = np.random.default_rng(67)
    peel = fbm(N, 300, 2, rng)
    scratch = smoothstep(0.72, 0.80, stretched(N, 500, 6, rng)) * smoothstep(0.4, 0.7, fbm(N, 8, 3, rng))
    chips = smoothstep(0.70, 0.74, fbm(N, 40, 3, rng)) * smoothstep(0.55, 0.7, fbm(N, 6, 2, rng))
    blotch = fbm(N, 6, 3, rng)
    wear = np.clip(scratch * 0.7 + chips, 0.0, 1.0)
    height = peel * 0.04 - wear * 0.10
    value = 0.78 * (0.92 + 0.14 * blotch)
    color = tint(value, (1.0, 1.0, 1.0)) * (1.0 - wear[..., None]) + tint(np.full((N, N), 0.55), (1.0, 0.98, 0.95)) * wear[..., None]
    rough = np.clip(0.48 + 0.12 * (blotch - 0.5) - 0.15 * wear, 0.0, 1.0)
    metal = np.clip(0.30 + 0.70 * wear, 0.0, 1.0)
    write_set(folder, 'Steel', color, height, rough, metal, 5.0)


def wood(folder):
    """Sawn timber: eight boards across, each with its own tone, a long grain, a few knots and dark gaps between."""
    rng = np.random.default_rng(83)
    boards = 8
    period = N // boards
    y = np.arange(N)
    board = (y // period).astype(int)
    tone = (0.82 + 0.36 * rng.random(boards))[board][:, None] * np.ones((1, N))
    edge = np.minimum(y % period, period - y % period).astype(float)
    gap = (1.0 - smoothstep(1.5, 4.0, edge))[:, None] * np.ones((1, N))
    grain = stretched(N, 14, 520, rng)
    wave = stretched(N, 5, 60, rng)
    fine = fbm(N, 220, 3, rng)
    knots = smoothstep(0.80, 0.86, fbm(N, 14, 2, rng)) * smoothstep(0.5, 0.7, stretched(N, 4, 40, rng))
    height = grain * 0.10 + fine * 0.03 - gap * 0.6 - knots * 0.1
    value = 0.52 * tone * (0.80 + 0.30 * grain) * (0.92 + 0.14 * wave) * (1.0 - 0.75 * gap) * (1.0 - 0.45 * knots)
    color = tint(value, (1.0, 0.62, 0.30))
    rough = np.clip(0.62 + 0.16 * (grain - 0.5) + 0.2 * gap, 0.0, 1.0)
    write_set(folder, 'Wood', color, height, rough, np.zeros((N, N)), 7.0)


def grunge(folder):
    rng = np.random.default_rng(79)
    n = 1024
    soft = fbm(n, 3, 5, rng)
    streak = stretched(n, 40, 3, rng)
    mask = np.clip(0.5 + (soft - 0.5) * 1.6 + (streak - 0.5) * 0.25, 0.0, 1.0)
    save(folder, 'T_District_Grunge', np.repeat(mask[..., None], 3, axis=-1), False)


def build_all(art_dir):
    folder = os.path.join(art_dir, 'District')
    os.makedirs(folder, exist_ok=True)
    for make in (concrete, floor, plaster, tread, deck, steel, wood, grunge):
        make(folder)
    print('DISTRICT TEXTURES', sorted(os.listdir(folder)))


if not globals().get('DEFER'):
    build_all(ART_DIR)
