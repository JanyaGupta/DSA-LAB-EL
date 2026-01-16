import networkx as nx
import matplotlib.pyplot as plt

# -----------------------------
# SafePath – Interactive Version
# -----------------------------

G = nx.DiGraph()  # Directed graph

# Roads: (from, to, time, traffic, road_quality, weather)
edges = [
    ("Hospital","A",5,2,8,1),
    ("Hospital","B",7,3,7,2),
    ("A","C",6,4,9,1),
    ("A","D",4,3,8,1),
    ("B","C",5,5,6,2),
    ("B","E",7,4,7,2),
    ("C","D",4,2,8,1),
    ("C","E",6,3,9,1),
    ("D","F",5,3,8,1),
    ("E","F",4,2,9,1),
    ("F","Patient",6,2,9,1)
]

# Add edges
for u,v,time,traffic,road_quality,weather in edges:
    G.add_edge(u,v, time=time, traffic=traffic, road_quality=road_quality, weather=weather)

# Scoring function
def edge_score(u,v,attr):
    return attr['time']*0.4 + attr['traffic']*0.3 + (10 - attr['road_quality'])*0.2 + attr['weather']*0.1

# --- User input ---
start = input("Enter starting point (e.g., Hospital, A, B, C, D, E, F): ").strip()
end = input("Enter destination point (e.g., A, B, C, D, E, F, Patient): ").strip()

# Compute best route
best_path = nx.dijkstra_path(G, start, end, weight=edge_score)
print(f"\nBest Path from {start} to {end}: {best_path}")

# Draw graph
pos = nx.spring_layout(G, seed=42)
nx.draw(G, pos, with_labels=True, node_color='lightblue', node_size=1200, arrows=True)
nx.draw_networkx_edges(G, pos, edgelist=list(zip(best_path, best_path[1:])), edge_color='red', width=2)
plt.title(f"Best Route: {start} → {end}")
plt.show()
