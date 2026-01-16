#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#define NODES 7
#define EDGE_COUNT 10

typedef struct {
    char name[10];
} Node;

typedef struct {
    int from;
    int to;
    int time;
    int traffic;
    int road_quality;
    int weather;
    double score;
} Edge;

// Calculate score
double edge_score(int time, int traffic, int quality, int weather) {
    return time*0.4 + traffic*0.3 + (10 - quality)*0.2 + weather*0.1;
}

// Dijkstra implementation
void dijkstra(double graph[NODES][NODES], int src, int dest, int prev[NODES]) {
    int visited[NODES] = {0};
    double dist[NODES];
    for(int i=0;i<NODES;i++) dist[i] = DBL_MAX;
    dist[src] = 0;

    for(int i=0;i<NODES;i++) {
        double min = DBL_MAX;
        int u = -1;
        for(int j=0;j<NODES;j++)
            if(!visited[j] && dist[j] < min) { min = dist[j]; u = j; }

        if(u == -1) break;
        visited[u] = 1;

        for(int v=0;v<NODES;v++)
            if(graph[u][v] != -1 && dist[u]+graph[u][v] < dist[v]) {
                dist[v] = dist[u]+graph[u][v];
                prev[v] = u;
            }
    }
}

// Print path into array
void get_path(int prev[NODES], int dest, int path[], int *length) {
    if(prev[dest] != -1) get_path(prev, prev[dest], path, length);
    path[(*length)++] = dest;
}

int main() {
    Node nodes[NODES] = {{"Hospital"}, {"A"}, {"B"}, {"C"}, {"D"}, {"E"}, {"F"}};
    Edge edges[EDGE_COUNT] = {
        {0,1,5,2,8,1,0}, {0,2,7,3,7,2,0},
        {1,3,6,4,9,1,0}, {1,4,4,3,8,1,0},
        {2,3,5,5,6,2,0}, {2,5,7,4,7,2,0},
        {3,4,4,2,8,1,0}, {3,5,6,3,9,1,0},
        {4,6,5,3,8,1,0}, {5,6,4,2,9,1,0}
    };

    // Calculate scores
    for(int i=0;i<EDGE_COUNT;i++)
        edges[i].score = edge_score(edges[i].time, edges[i].traffic,
                                    edges[i].road_quality, edges[i].weather);

    // Build adjacency matrix
    double graph[NODES][NODES];
    for(int i=0;i<NODES;i++)
        for(int j=0;j<NODES;j++) graph[i][j] = -1;

    for(int i=0;i<EDGE_COUNT;i++)
        graph[edges[i].from][edges[i].to] = edges[i].score;

    // Write edges and scores to file
    FILE *f = fopen("graph_data.csv", "w");
    fprintf(f, "from,to,time,traffic,quality,weather,score\n");
    for(int i=0;i<EDGE_COUNT;i++)
        fprintf(f, "%s,%s,%d,%d,%d,%d,%.2f\n",
                nodes[edges[i].from].name,
                nodes[edges[i].to].name,
                edges[i].time,
                edges[i].traffic,
                edges[i].road_quality,
                edges[i].weather,
                edges[i].score);
    fclose(f);

    // Input start and end nodes
    char start_name[10], end_name[10];
    printf("Enter starting point: "); scanf("%s", start_name);
    printf("Enter destination point: "); scanf("%s", end_name);

    int start=-1, end=-1;
    for(int i=0;i<NODES;i++) {
        if(strcmp(start_name, nodes[i].name)==0) start=i;
        if(strcmp(end_name, nodes[i].name)==0) end=i;
    }

    int prev[NODES];
    for(int i=0;i<NODES;i++) prev[i] = -1;

    dijkstra(graph, start, end, prev);

    int path[NODES], length=0;
    get_path(prev, end, path, &length);

    // Write path to file
    FILE *pf = fopen("best_path.csv", "w");
    for(int i=0;i<length;i++) {
        fprintf(pf, "%s\n", nodes[path[i]].name);
    }
    fclose(pf);

    printf("Best path computed and saved to best_path.csv\n");
    return 0;
}
