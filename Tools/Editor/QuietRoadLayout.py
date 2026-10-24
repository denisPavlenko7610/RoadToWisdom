"""Shared, fixed route in centimetres. Gameplay POIs remain authored."""
import math

ROAD_HALF_WIDTH = 450.0
ROAD_LENGTH = 24000.0


def road_y(x):
    t = max(0.0, min(1.0, x / ROAD_LENGTH))
    return 1200.0 * math.sin(2.0 * math.pi * t) + 350.0 * math.sin(4.0 * math.pi * t)


def distance_to_road(x, y):
    best = float("inf")
    for i in range(80):
        ax, bx = i * 300.0, (i + 1) * 300.0
        ay, by = road_y(ax), road_y(bx)
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)))
        best = min(best, math.hypot(x - ax - t * dx, y - ay - t * dy))
    return best