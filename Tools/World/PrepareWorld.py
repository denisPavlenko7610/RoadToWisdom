"""Deterministic terrain/placement data for native UE Landscape. Needs numpy.

python Tools/World/PrepareWorld.py [--size 1008] [--seed 123]
Output is temporary Saved/WorldGeneration; UE assets are saved by GenerateWorld.py.
"""
import argparse
from collections import defaultdict
import hashlib
import json
import math
from pathlib import Path
import random
import numpy as np

ROOT = Path(__file__).resolve().parents[2]


def smooth(t): return t * t * (3 - 2 * t)


def value_noise(x, y, seed, period):
    x = x / period;
    y = y / period
    ix = np.floor(x).astype(np.int64);
    iy = np.floor(y).astype(np.int64)
    fx = smooth(x - ix);
    fy = smooth(y - iy)

    def h(a, b):
        v = np.sin(a * 127.1 + b * 311.7 + seed * 0.73) * 43758.5453
        return (v - np.floor(v)) * 2 - 1

    return (h(ix, iy) * (1 - fx) + h(ix + 1, iy) * fx) * (1 - fy) + (
                h(ix, iy + 1) * (1 - fx) + h(ix + 1, iy + 1) * fx) * fy


def base_height(x, y, scale):
    return 35 + scale * (
                18 * np.sin(x / (800 * scale)) + 14 * np.cos(y / (1000 * scale)) + 9 * np.sin((x + y) / (600 * scale)))


def catmull(knots, scale):
    points = []
    for i in range(len(knots) - 1):
        p0 = np.array(knots[max(0, i - 1)]) * scale
        p1 = np.array(knots[i]) * scale;
        p2 = np.array(knots[i + 1]) * scale
        p3 = np.array(knots[min(len(knots) - 1, i + 2)]) * scale
        steps = max(3, int(np.linalg.norm(p2 - p1) / 10))
        for j in range(steps):
            t = j / steps
            p = .5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (
                        -p0 + 3 * p1 - 3 * p2 + p3) * t * t * t)
            points.append([float(p[0]), float(p[1]), float(base_height(p[0], p[1], scale))])
    x, y = np.array(knots[-1]) * scale
    points.append([float(x), float(y), float(base_height(x, y, scale))])
    return points


def sample_grid(grid, x, y, half, spacing):
    u = np.clip((x + half) / spacing, 0, grid.shape[1] - 1.00001)
    v = np.clip((y + half) / spacing, 0, grid.shape[0] - 1.00001)
    i = int(u);
    j = int(v);
    a = u - i;
    b = v - j
    return float(grid[j, i] * (1 - a) * (1 - b) + grid[j, i + 1] * a * (1 - b) + grid[j + 1, i] * (1 - a) * b + grid[
        j + 1, i + 1] * a * b)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int)
    parser.add_argument("--seed", type=int)
    args = parser.parse_args()
    settings = json.loads((ROOT / "Config/WorldGeneration.json").read_text(encoding="utf-8-sig"))
    if args.size: settings["WorldSize"] = args.size
    if args.seed is not None: settings["Seed"] = args.seed
    size = settings["WorldSize"]
    if size not in (1008, 2016, 4032): raise ValueError(
        "WorldSize must be 1008, 2016 or 4032 m (native 126-quad component layout)")
    if size == 1008: settings["Map"] = "/Game/QuietRoadWorld/Maps/L_QuietRoad_WorldPilot"
    scale = size / 4032
    seed = int(settings["Seed"]);
    rng = random.Random(seed)
    resolution = size // 2 + 1;
    half = size / 2;
    spacing = 2
    axis = np.linspace(-half, half, resolution, dtype=np.float32)
    x, y = np.meshgrid(axis, axis)
    terrain = base_height(x, y, scale).astype(np.float32)
    variation = float(settings["TerrainVariation"])
    # Broad geographic masses, ridges and foothills, then gentle detail. The road
    # follows a low valley bed; no geology simulation or runtime terrain creation.
    masses = [(-1350, 1280, 320, 570, 430), (1500, -1000, 350, 470, 650), (1300, 1500, 260, 600, 360),
              (-1720, -600, 170, 400, 650), (350, 1650, 210, 600, 300), (-200, -1600, 110, 800, 360)]
    for mx, my, h, sx, sy in masses:
        distance = ((x - mx * scale) / (sx * scale)) ** 2 + ((y - my * scale) / (sy * scale)) ** 2
        terrain += h * scale * variation * np.exp(-distance * 1.5)
    terrain += variation * scale * (
                22 * value_noise(x, y, seed, 450 * scale) + 9 * value_noise(x, y, seed + 5, 180 * scale)
                + 3 * value_noise(x, y, seed + 11, 65 * scale))
    poi_specs = [
        ("IronHollow", "Settlement", (-400, -250), 135, 8),
        ("DustCrossing", "Village", (1050, 350), 95, 3),
        ("DryCreekFarm", "Farm", (-1200, 700), 65, 2),
        ("SouthHomestead", "Lone", (-1270, -830), 45, 1),
        ("RidgeCabin", "Lone", (700, -800), 45, 1),
        ("EastDepot", "Outpost", (1450, -500), 65, 2),
        ("NorthOverlook", "Overlook", (-350, 1450), 45, 0),
        ("OldCamp", "Camp", (-900, 500), 45, 0),
        ("NorthernHouse", "Lone", (1650, 1550), 45, 1)]
    if size == 1008:
        poi_specs = [("PilotVillage", "Village", (-400, -250), 65, 3),
                     ("PilotFarm", "Farm", (-1200, 700), 48, 2),
                     ("PilotHouse", "Lone", (1650, 1550), 32, 1)]
    pois = []
    for name, kind, center, radius, count in poi_specs[
        :max(2, math.ceil(len(poi_specs) * min(1, float(settings["POIDensity"]))))]:
        cx, cy = [v * scale for v in center]
        radius = min(radius, size * .08)
        z = sample_grid(terrain, cx, cy, half, spacing)
        dist = np.hypot(x - cx, y - cy)
        weight = 1 - smooth(np.clip((dist - radius) / (radius * .55), 0, 1))
        terrain = terrain * (1 - weight) + z * weight
        pois.append(dict(name=name, kind=kind, center=[cx, cy, z], radius=radius, buildings=count))
    definitions = [
        ("Main",
         [(-1700, -1400), (-1400, -1000), (-1100, -400), (-800, -200), (-400, -250), (0, -200), (500, 150), (1050, 350),
          (1450, 900), (1650, 1550)], 4.5),
        ("ValleyLoop", [(-800, -200), (-900, 500), (-300, 950), (400, 850), (750, 400), (500, 150)], 3.8),
        ("FarmSpur", [(-900, 500), (-1100, 550), (-1200, 700)], 3.5),
        ("SouthSpur", [(-1400, -1000), (-1270, -830)], 3.5),
        ("CabinSpur", [(0, -200), (250, -600), (700, -800)], 3.5),
        ("DepotSpur", [(1050, 350), (1250, -50), (1450, -500)], 3.8),
        ("OverlookSpur", [(-300, 950), (-400, 1200), (-350, 1450)], 3.5)]
    if float(settings["RoadDensity"]) >= .75 and size > 1008:
        definitions.append(("RidgeLink", [(700, -800), (1100, -1000), (1450, -500)], 3.5))
    roads = [dict(name=name, points=catmull(knots, scale), half_width=width) for name, knots, width in definitions]
    # Fit the route to natural elevations, with shared junction heights and a
    # bounded longitudinal grade. Projection adjusts only the sampled road bed.
    junctions = {}
    nodes = []
    edges = []
    road_nodes = []
    node_xy = []
    for road in roads:
        ids = []
        for point in road["points"]:
            key = (round(point[0], 3), round(point[1], 3))
            if key not in junctions:
                junctions[key] = len(nodes)
                nodes.append(sample_grid(terrain, point[0], point[1], half, spacing))
                node_xy.append(point[:2])
            ids.append(junctions[key])
        road_nodes.append(ids)
        for i, (a, b) in enumerate(zip(road["points"], road["points"][1:])):
            edges.append((ids[i], ids[i+1], math.hypot(b[0]-a[0], b[1]-a[1])*.085))
    coordinates = np.array(node_xy)
    for ia in range(len(nodes)):
        delta = coordinates[ia+1:]-coordinates[ia]
        lengths = np.sqrt(np.sum(delta*delta,axis=1))
        for offset in np.flatnonzero(lengths<25*max(.4,scale)):
            edges.append((ia,ia+1+int(offset),max(.001,float(lengths[offset])*.06)))
    for iteration in range(6500):
        maximum = 0.0
        for ia, ib, limit in edges:
            delta = nodes[ib]-nodes[ia]
            excess = max(0.0, abs(delta)-limit)
            maximum = max(maximum, excess)
            if excess:
                correction = math.copysign(excess*.5, delta)
                nodes[ia] += correction
                nodes[ib] -= correction
        if maximum < .0001: break
    for road, ids in zip(roads, road_nodes):
        for point, index in zip(road["points"], ids): point[2] = nodes[index]
    for poi in pois:
        cx, cy, _ = poi["center"]
        point = min((p for road in roads for p in road["points"]),key=lambda p: math.hypot(p[0]-cx,p[1]-cy))
        z = point[2]
        poi["center"][2] = z
        dist = np.hypot(x-cx, y-cy)
        weight = 1-smooth(np.clip((dist-poi["radius"])/(poi["radius"]*.8),0,1))
        terrain = terrain*(1-weight)+z*weight
    distance = np.full_like(terrain, 1e7);
    bed = np.zeros_like(terrain);
    road_width = np.zeros_like(terrain)
    # Local bounding rectangles avoid a world-sized allocation per road segment.
    for road in roads:
        for a, b in zip(road["points"], road["points"][1:]):
            ax, ay, az = a;
            bx, by, bz = b
            radius = 420 * max(.4, scale)
            imin = max(0, int((min(ax, bx) - radius + half) / spacing));
            imax = min(resolution, int((max(ax, bx) + radius + half) / spacing) + 1)
            jmin = max(0, int((min(ay, by) - radius + half) / spacing));
            jmax = min(resolution, int((max(ay, by) + radius + half) / spacing) + 1)
            xx = x[jmin:jmax, imin:imax];
            yy = y[jmin:jmax, imin:imax]
            dx = bx - ax;
            dy = by - ay;
            den = dx * dx + dy * dy
            t = np.clip(((xx - ax) * dx + (yy - ay) * dy) / max(den, .001), 0, 1)
            d = np.hypot(xx - ax - t * dx, yy - ay - t * dy)
            target = distance[jmin:jmax, imin:imax];
            mask = d < target
            target[mask] = d[mask]
            bed[jmin:jmax, imin:imax][mask] = (az + (bz - az) * t)[mask]
            road_width[jmin:jmax, imin:imax][mask] = road["half_width"]
    shoulder = np.maximum(90 * max(.35, scale), np.abs(terrain-bed)*4.0)
    shoulder = np.minimum(shoulder, 400*max(.4,scale))
    falloff = 1 - smooth(np.clip((distance - (road_width + 3)) / shoulder, 0, 1))
    terrain = terrain * (1 - falloff) + bed * falloff
    # Serialization quantization exactly matches Landscape z scale 256 (2 cm).
    raw = np.clip(np.rint(32768 + terrain * 50), 0, 65535).astype("<u2")
    terrain = (raw.astype(np.float32) - 32768) * .02
    # Bed samples use the actual quantized Landscape for tight ribbon contact.
    for road in roads:
        for point in road["points"]:
            point[2] = sample_grid(terrain, point[0], point[1], half, spacing) + .025
    cells = defaultdict(lambda: defaultdict(list))

    def location_height(px, py):
        return sample_grid(terrain, px, py, half, spacing)

    def road_distance(px, py):
        return sample_grid(distance, px, py, half, spacing)

    def excluded(px, py, margin):
        if abs(px) > half - 12 or abs(py) > half - 12 or road_distance(px, py) < margin: return True
        return any(math.hypot(px - p["center"][0], py - p["center"][1]) < p["radius"] + 12 for p in pois)

    area_scale = (size / 4032) ** 2
    for category, count in (("Shrubs", int(28000 * area_scale * settings["VegetationDensity"])),
                            ("Grass", int(64000 * area_scale * settings["VegetationDensity"])),
                            ("RockSmall", int(8500 * area_scale * settings["RockDensity"])),
                            ("RockLarge", int(1500 * area_scale * settings["RockDensity"]))):
        for i in range(count):
            px = rng.uniform(-half, half);
            py = rng.uniform(-half, half)
            large = category == "RockLarge"
            if excluded(px, py, 32 if large else 11): continue
            z = location_height(px, py)
            elevation = float(z - base_height(px, py, scale))
            patch = .5 + .5 * math.sin(px / (90 * max(.5, scale)) + seed) * math.cos(py / (110 * max(.5, scale)))
            accept = (.24 + .48 * patch) * (
                        1 - min(.8, max(0, elevation) / (160 * scale))) if "Rock" not in category else (
                        .18 + min(.7, max(0, elevation) / (120 * scale)))
            if rng.random() > accept: continue
            sx = rng.uniform(.7, 1.8)
            if large: sx = rng.uniform(3, 11)
            if category == "RockSmall": sx = rng.uniform(.4, 2.1)
            transform = [round(px * 100, 3), round(py * 100, 3), round(z * 100, 3), rng.uniform(0, 360),
                         sx, sx * rng.uniform(.65, 1.3), sx * rng.uniform(.6, 1.35)]
            cell = (int((px + half) // 252), int((py + half) // 252))
            cells[cell][category].append(transform)
    metrics = {}
    lengths = [];
    max_grade = 0;
    min_radius = 1e9
    for road in roads:
        pts = np.array(road["points"]);
        delta = np.diff(pts, axis=0)
        horizontal = np.linalg.norm(delta[:, :2], axis=1)
        lengths.append(float(np.linalg.norm(delta, axis=1).sum()))
        max_grade = max(max_grade, float(np.max(np.abs(delta[:, 2]) / np.maximum(horizontal, .001))))
        for a, b, c in zip(pts, pts[1:], pts[2:]):
            ab = np.linalg.norm(b[:2] - a[:2]);
            bc = np.linalg.norm(c[:2] - b[:2]);
            ac = np.linalg.norm(c[:2] - a[:2])
            vab, vac = b[:2] - a[:2], c[:2] - a[:2]
            cross = abs(float(vab[0]*vac[1] - vab[1]*vac[0]))
            if cross > 1e-5: min_radius = min(min_radius, ab * bc * ac / (2 * cross))
    metrics = dict(road_length_m=sum(lengths), max_road_grade=max_grade, min_turn_radius_m=min_radius,
                   terrain_min_m=float(terrain.min()), terrain_max_m=float(terrain.max()),
                   instances=dict((cat, sum(len(v.get(cat, [])) for v in cells.values())) for cat in
                                  ("Shrubs", "Grass", "RockSmall", "RockLarge")))
    assert max_grade < .12, metrics
    output = ROOT / "Saved/WorldGeneration";
    output.mkdir(parents=True, exist_ok=True)
    (output / "Terrain.r16").write_bytes(raw.tobytes())
    manifest = dict(settings=settings, resolution=resolution, spacing_cm=200, roads=roads, pois=pois,
                    cells=[dict(cell=list(k), categories=v) for k, v in sorted(cells.items())], metrics=metrics,
                    height_sha256=hashlib.sha256(raw.tobytes()).hexdigest())
    serialized = json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode()
    (output / "World.json").write_bytes(serialized)
    print(json.dumps(dict(size=size, resolution=resolution, metrics=metrics,
                          height_sha256=manifest["height_sha256"],
                          manifest_sha256=hashlib.sha256(serialized).hexdigest()), indent=2))


if __name__ == "__main__": main()