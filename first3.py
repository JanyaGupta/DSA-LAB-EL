import networkx as nx
import matplotlib.pyplot as plt
from matplotlib.table import Table

#nx.DiGraph() for directed and nx.Graph() for undirected
G = nx.Graph()

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

def edge_score(u,v,attr):
    return attr['time']*0.4 + attr['traffic']*0.3 + (10 - attr['road_quality'])*0.2 + attr['weather']*0.1

# Store rows for table
table_data = []

# Insert edges + calculated score
for u,v,t,traffic,quality,weather in edges:
    score = round(edge_score(u, v, {
        'time': t,
        'traffic': traffic,
        'road_quality': quality,
        'weather': weather
    }), 2)
    
    G.add_edge(u, v,
               time=t, traffic=traffic,
               road_quality=quality, weather=weather,
               score=score)
    
    table_data.append([f"{u} → {v}", t, traffic, quality, weather, score])

start = input("Enter starting point: ").strip()
end = input("Enter destination point: ").strip()

best_path = nx.dijkstra_path(G, start, end, weight=lambda u,v,attr: attr['score'])
print(f"\nBest Path from {start} to {end}: {best_path}")

# -------- VISUALIZATION --------
fig, ax = plt.subplots(figsize=(12, 8))

pos = nx.spring_layout(G, seed=42)
nx.draw(G, pos, with_labels=True, node_color='lightblue', node_size=1200, arrows=True)

# Edge labels (score)
edge_labels = {(u, v): G[u][v]['score'] for u, v in G.edges()}
nx.draw_networkx_edge_labels(G, pos, edge_labels=edge_labels, font_color='red')

# Highlight best path
nx.draw_networkx_edges(G, pos, edgelist=list(zip(best_path, best_path[1:])),
                       edge_color='red', width=2)

# -------- TABLE IN CORNER --------
table_ax = fig.add_axes([0.70, 0.05, 0.28, 0.35])  # (left, bottom, width, height)
table_ax.axis("off")

# Create table
column_labels = ["Edge", "Time", "Traffic", "Quality", "Weather", "Score"]
table = table_ax.table(cellText=table_data,
                       colLabels=column_labels,
                       loc='center')

table.scale(1, 1.3)  # Increase row height
table.auto_set_font_size(False)
table.set_fontsize(8)

plt.title(f"Best Route: {start} → {end}")
plt.show()
