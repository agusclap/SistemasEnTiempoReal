import matplotlib
matplotlib.use("TkAgg")

import sys
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

MAX_POINTS = 200

xs = deque(maxlen=MAX_POINTS)
ys = deque(maxlen=MAX_POINTS)
zs = deque(maxlen=MAX_POINTS)
ts = deque(maxlen=MAX_POINTS)

counter = 0

fig, ax = plt.subplots()
line_x, = ax.plot([], [], label="X")
line_y, = ax.plot([], [], label="Y")
line_z, = ax.plot([], [], label="Z")

ax.set_title("Aceleración filtrada MPU6050")
ax.set_xlabel("Muestra")
ax.set_ylabel("g")
ax.grid(True)
ax.legend()

def update(frame):
    global counter

    line = sys.stdin.readline()
    if not line:
        return line_x, line_y, line_z

    try:
        x, y, z = map(float, line.strip().split(","))
    except ValueError:
        return line_x, line_y, line_z

    ts.append(counter)
    xs.append(x)
    ys.append(y)
    zs.append(z)
    counter += 1

    line_x.set_data(ts, xs)
    line_y.set_data(ts, ys)
    line_z.set_data(ts, zs)

    if len(ts) > 1:
        ax.set_xlim(ts[0], ts[-1])

    values = list(xs) + list(ys) + list(zs)
    if values:
        ymin = min(values) - 0.2
        ymax = max(values) + 0.2
        if ymin == ymax:
            ymax = ymin + 1
        ax.set_ylim(ymin, ymax)

    return line_x, line_y, line_z

ani = animation.FuncAnimation(
    fig,
    update,
    interval=20,
    blit=False,
)

plt.show()