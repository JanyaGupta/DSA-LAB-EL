# first.py
import networkx as nx
import matplotlib.pyplot as plt
import random
from itertools import islice

# ----------------------------
# Build a clean 45-node directed city-like graph
# ----------------------------
random.seed(42)

G = nx.DiGraph()
num_nodes = 45
nodes = list(range(1, num_nodes + 1))
G.add_nodes_from(nodes)

# Grid-like positions (7x7 grid trimmed to 45)
rows, cols = 7, 7
positions = {}
i = 0
for r in range(rows):
    for c in range(cols):
        if i >= num_nodes:
            break
        x = c + random.uniform(-0.12, 0.12)
        y = r + random.uniform(-0.12, 0.12)
        positions[nodes[i]] = (x, y)
        i += 1

# Connect each node to its ~3 nearest neighbors (directed edges both ways optionally)
for node in nodes:
    # calculate euclidean distances to others
    distances = []
    x1, y1 = positions[node]
    for other in nodes:
        if node == other:
            continue
        x2, y2 = positions[other]
        dist_euclid = ((x1-x2)**2 + (y1-y2)**2)**0.5
        distances.append((dist_euclid, other))
    distances.sort()

    # connect to 3 nearest neighbors; add edges in both directions with possibly different weights
    for _, neighbor in distances[:3]:
        w = round(random.uniform(1.0, 12.0), 1)
        if not G.has_edge(node, neighbor):
            G.add_edge(node, neighbor, weight=w)
        # also sometimes add the reverse with different weight to simulate one-way differences
        if not G.has_edge(neighbor, node) and random.random() < 0.8:
            wrev = round(max(0.7, w + random.uniform(-2.0, 2.0)), 1)
            G.add_edge(neighbor, node, weight=wrev)

# Add extra random connections for realism (avoid self-loops and duplicates)
extra_edges = 80
for _ in range(extra_edges):
    a, b = random.sample(nodes, 2)
    if a != b and not G.has_edge(a, b):
        G.add_edge(a, b, weight=round(random.uniform(1.0, 12.0), 1))

# ----------------------------
# Utility: get path stats and reasoner
# ----------------------------
def path_distance(G, path):
    total = 0.0
    weights = []
    for u, v in zip(path, path[1:]):
        w = G[u][v]['weight']
        total += w
        weights.append(w)
    return round(total, 2), weights

def explain_reason(best_path, best_dist, cand_path, cand_dist, cand_weights):
    reasons = []
    # longer than best?
    diff = round(cand_dist - best_dist, 2)
    if diff > 0:
        reasons.append(f"Longer than best by {diff} units.")
    # more hops?
    if len(cand_path) > len(best_path):
        reasons.append(f"More hops ({len(cand_path)-1} edges vs {len(best_path)-1}).")
    # detour nodes (nodes present in candidate but not in best)
    detour_nodes = [n for n in cand_path if n not in best_path]
    if detour_nodes:
        # only list up to 6 nodes to keep it readable
        limited = detour_nodes if len(detour_nodes) <= 6 else detour_nodes[:6] + ['...']
        reasons.append(f"Detours via nodes {limited}.")
    # heavy segments
    heavy_threshold = max(8.0, max(cand_weights) * 0.75)
    heavy_edges = [w for w in cand_weights if w >= heavy_threshold]
    if heavy_edges:
        reasons.append(f"Contains high-cost road segments (weights >= {round(heavy_threshold,1)}).")
    # more turns heuristic: more direction changes (approximate by node turning count)
    # (simple heuristic: number of intermediate nodes)
    if len(cand_path)-1 > len(best_path)-1 and (len(cand_path) - len(best_path)) >= 2:
        reasons.append("Likely more turns/complex routing (more intermediate stops).")
    if not reasons:
        reasons.append("Mostly similar to best; small variations cause slightly higher cost.")
    return " ".join(reasons)

# ----------------------------
# Ask user for start & end
# ----------------------------
print("Available nodes: 1 to", num_nodes)
print("Example input: 5  or  23\n")
try:
    s = int(input("Enter starting node (integer): ").strip())
    t = int(input("Enter destination node (integer): ").strip())
except Exception as e:
    print("Invalid input. Please enter integer node ids (1..45).")
    raise SystemExit

if s not in nodes or t not in nodes:
    print("Start or destination not in node list.")
    raise SystemExit

# ----------------------------
# Generate top-K simple paths (weighted) using networkx shortest_simple_paths
# ----------------------------
K_A = 5   # Option A: top 5 routes
K_D = 3   # Option D: best + 2 alternatives (top 3)

# generator of simple paths by increasing total weight
try:
    gen = nx.shortest_simple_paths(G, s, t, weight='weight')
except nx.NetworkXNoPath:
    print(f"No path between {s} and {t}.")
    raise SystemExit

# collect up to K_A paths
paths = list(islice(gen, K_A))
if not paths:
    print("No simple paths found.")
    raise SystemExit

# compute distances and weights
path_infos = []
for p in paths:
    dist, weights = path_distance(G, p)
    path_infos.append({'path': p, 'dist': dist, 'weights': weights, 'hops': len(p)-1})

# Best path is first
best = path_infos[0]
print("\n=== Option D (Best + 2 alternatives) ===\n")
for idx, info in enumerate(path_infos[:K_D]):
    tag = "BEST" if idx == 0 else f"ALTERNATIVE #{idx}"
    print(f"{tag}: Path {info['path']}  |  Distance = {info['dist']}  | Hops = {info['hops']}")
    if idx != 0:
        reason = explain_reason(best['path'], best['dist'], info['path'], info['dist'], info['weights'])
        print("  Why not preferred:", reason)
    print()

print("\n=== Option A (Top 5 routes with reasons) ===\n")
for idx, info in enumerate(path_infos):
    label = f"{idx+1}. Path {info['path']}  Distance={info['dist']}  Hops={info['hops']}"
    print(label)
    if idx == 0:
        print("   → Chosen as BEST route (lowest total weight).")
    else:
        reason = explain_reason(best['path'], best['dist'], info['path'], info['dist'], info['weights'])
        print("   → Why not preferred:", reason)
    print()

# ----------------------------
# Visualization: draw map, highlight best + 2 alternatives
# ----------------------------
plt.figure(figsize=(11, 11))

# draw baseline edges (light)
nx.draw_networkx_nodes(G, positions, node_size=260, node_color='lightblue')
nx.draw_networkx_labels(G, positions, font_size=8)

# draw all edges faint
nx.draw_networkx_edges(G, positions, edge_color='lightgray', width=1, arrows=False)

# prepare colored highlighting for top 3 (best + 2)
color_map = ['red', 'darkorange', 'gold']  # best, alt1, alt2
width_map = [3.5, 3, 2.5]
for i in range(min(K_D, len(path_infos))):
    p = path_infos[i]['path']
    pe = list(zip(p, p[1:]))
    nx.draw_networkx_edges(
        G, positions,
        edgelist=pe,
        edge_color=color_map[i],
        width=width_map[i],
        arrows=True,
        arrowstyle='-|>',
        arrowsize=16
    )
    # highlight nodes on that path (increase size)
    nx.draw_networkx_nodes(G, positions, nodelist=p, node_size=360, node_color=color_map[i], alpha=0.6)

# custom legend (patches)
import matplotlib.patches as mpatches
legend_handles = [
    mpatches.Patch(color='red', label='Best route'),
    mpatches.Patch(color='darkorange', label='Alternative #1'),
    mpatches.Patch(color='gold', label='Alternative #2'),
    mpatches.Patch(color='lightgray', label='Other roads')
]
plt.legend(handles=legend_handles, loc='upper right')

plt.title(f"City map: Best + 2 Alternatives from {s} → {t}", fontsize=14)
plt.axis('off')
plt.tight_layout()
plt.show()
