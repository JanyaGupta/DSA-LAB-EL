import pandas as pd
import networkx as nx
import matplotlib.pyplot as plt

# Read data
edges_df = pd.read_csv("graph_data.csv")
with open("best_path.csv") as f:
    best_path = [line.strip() for line in f]

# Build graph
G = nx.DiGraph()
for _, row in edges_df.iterrows():
    G.add_edge(row['from'], row['to'], score=row['score'])

# Single figure
fig = plt.figure(figsize=(12, 8))
ax = fig.add_subplot(111)
ax.axis('off')  # Hide x/y axes completely

# Graph layout
pos = nx.spring_layout(G, seed=42)

# Draw nodes and edges
nx.draw(G, pos, with_labels=True, node_color='lightblue', node_size=1200, arrows=True, ax=ax)

# Edge labels
edge_labels = {(row['from'], row['to']): row['score'] for _, row in edges_df.iterrows()}
nx.draw_networkx_edge_labels(G, pos, edge_labels=edge_labels, font_color='red', ax=ax)

# Highlight best path
nx.draw_networkx_edges(G, pos, edgelist=list(zip(best_path, best_path[1:])),
                       edge_color='red', width=2, ax=ax)

# Table in same figure
table_ax = fig.add_axes([0.70, 0.05, 0.28, 0.35])
table_ax.axis("off")
column_labels = ["Edge", "Time", "Traffic", "Quality", "Weather", "Score"]
table_data = [[f"{row['from']}→{row['to']}", row['time'], row['traffic'],
               row['quality'], row['weather'], row['score']] for _, row in edges_df.iterrows()]
table = table_ax.table(cellText=table_data, colLabels=column_labels, loc='center')
table.scale(1, 1.3)
table.auto_set_font_size(False)
table.set_fontsize(8)

# Title
plt.title(f"Best Route: {' → '.join(best_path)}")
plt.show()
