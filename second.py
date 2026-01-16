import networkx as nx
import matplotlib.pyplot as plt
import random

# ----------------------------
# Generate 45-node city layout
# ----------------------------
G = nx.DiGraph()   # IMPORTANT: Use directed graph to draw arrows

num_nodes = 45
nodes = list(range(1, num_nodes+1))
G.add_nodes_from(nodes)

# Grid-like city structure with randomness
rows, cols = 7, 7
positions = {}

i = 0
for r in range(rows):
    for c in range(cols):
        if i >= num_nodes:
            break
        x = c + random.uniform(-0.15, 0.15)
        y = r + random.uniform(-0.15, 0.15)
        positions[nodes[i]] = (x, y)
        i += 1

# Connect nearest neighbors (city roads)
for node in nodes:
    distances = []
    for other in nodes:
        if node != other:
            x1, y1 = positions[node]
            x2, y2 = positions[other]
            dist = ((x1-x2)**2 + (y1-y2)**2)**0.5
            distances.append((dist, other))
    distances.sort()

    # connect to 3 nearest neighbors
    for _, neighbor in distances[:3]:
        weight = round(random.uniform(1, 10), 1)
        G.add_edge(node, neighbor, weight=weight)

# ----------------------------
# User input
# ----------------------------
print("Available nodes: 1 to 45\n")
start = int(input("Enter starting point: "))
end = int(input("Enter destination point: "))

# ----------------------------
# Shortest path using Dijkstra
# ----------------------------
path = nx.shortest_path(G, source=start, target=end, weight='weight')
distance = nx.shortest_path_length(G, source=start, target=end, weight='weight')

print("\nShortest path:", path)
print("Total distance:", distance)

# ----------------------------
# Draw city map
# ----------------------------
plt.figure(figsize=(11, 11))

# Normal roads
nx.draw(
    G,
    positions,
    node_size=300,
    node_color="skyblue",
    edge_color="lightgray",
    width=1,
    arrows=False
)

# Highlight path edges in red + thick + arrows
path_edges = list(zip(path, path[1:]))

nx.draw_networkx_edges(
    G,
    positions,
    edgelist=path_edges,
    edge_color="red",
    width=3,
    arrows=True,
    arrowstyle='-|>',
    arrowsize=20
)

# Show node labels
nx.draw_networkx_labels(G, positions, font_size=8)

plt.title(f"City Map with Shortest Path {start} → {end}", fontsize=15)
plt.axis('off')
plt.show()
