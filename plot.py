import matplotlib.pyplot as plt
import os

def read_data(filename):
    x = []
    y = []
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split()
            if len(parts) >= 2:
                try:
                    dist = float(parts[0])
                    val = float(parts[1])
                    x.append(dist)
                    y.append(val)
                except ValueError:
                    pass
    return x, y

x1, y1 = read_data('hardware/2431080_data.txt')
x2, y2 = read_data('hardware/2431026_data.txt')

plt.figure(figsize=(10, 6))
plt.scatter(x1, y1, label='2431080_data', alpha=0.6)
plt.scatter(x2, y2, label='2431026_data', alpha=0.6)
plt.title('IR Sensor Value vs Distance')
plt.xlabel('Distance (mm)')
plt.ylabel('Sensor ADC Value')
plt.grid(True)
plt.legend()
plt.savefig('C:/Users/細川晃良/.gemini/antigravity/scratch/robotics-exercise/sensor_graph.png')
