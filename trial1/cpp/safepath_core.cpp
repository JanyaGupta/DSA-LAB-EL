// safepath_core.cpp
// Compile: g++ -std=c++17 safepath_core.cpp -O2 -o safepath_core
// Usage: ./safepath_core data/nodes.csv data/edges.csv data/updates.json start_node dest_node K
#include <bits/stdc++.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp> // need json single header; instructions below
using json = nlohmann::json;
using namespace std;

struct Node { int id; string name; double lat, lon; };
struct Edge { int u,v; double distance_m; double freeflow_time_s; double road_quality; double safety_index; int edge_id; };

vector<Node> nodes;
vector<vector<pair<int,int>>> adj; // adj[u] -> vector of (v, edge_index)
vector<Edge> edges;

unordered_map<int, json> updates_by_edge; // edge_id -> update object

// weights (configurable)
double W_TIME = 1.0;
double W_TRAFFIC = 300.0;    // scale to meters-equivalent
double W_WEATHER = 250.0;
double W_ROAD_QUAL = 200.0;
double W_SAFETY = 180.0;
double W_BLOCK = 1e7;

bool load_nodes(const string &path) {
    ifstream f(path);
    if(!f) return false;
    string line;
    getline(f,line);
    while(getline(f,line)){
        if(line.empty()) continue;
        stringstream ss(line);
        int id; string name; double lat, lon;
        // id,name,lat,lon
        string id_s, name_s, lat_s, lon_s;
        // naive CSV parse
        vector<string> parts;
        bool inq=false; string cur="";
        for(char c:line){
            if(c=='\"') inq=!inq;
            else if(c==',' && !inq){ parts.push_back(cur); cur=""; }
            else cur.push_back(c);
        }
        parts.push_back(cur);
        if(parts.size()<4) continue;
        id = stoi(parts[0]); name = parts[1]; lat = stod(parts[2]); lon = stod(parts[3]);
        nodes.push_back({id,name,lat,lon});
    }
    return true;
}

bool load_edges(const string &path) {
    ifstream f(path);
    if(!f) return false;
    string line;
    getline(f,line);
    int idx=0;
    while(getline(f,line)){
        if(line.empty()) continue;
        stringstream ss(line);
        int u,v; double dist, t, rq, si; int eid;
        char comma;
        ss>>u>>comma>>v>>comma>>dist>>comma>>t>>comma>>rq>>comma>>si>>comma>>eid;
        edges.push_back({u,v,dist,t,rq,si,eid});
        idx++;
    }
    // build adjacency sized by max node id
    int maxn=0;
    for(auto &n: nodes) maxn = max(maxn, n.id);
    adj.assign(maxn+1, {});
    for(size_t i=0;i<edges.size();++i){
        auto &e = edges[i];
        adj[e.u].push_back({e.v, (int)i});
        // also add reverse if needed (OSRM edges are directed but we will add reverse to allow path)
        adj[e.v].push_back({e.u, (int)i});
    }
    return true;
}

bool load_updates(const string &path) {
    ifstream f(path);
    if(!f) return false;
    json j; f>>j;
    updates_by_edge.clear();
    for(auto it = j.begin(); it!=j.end(); ++it){
        int eid = stoi(it.key());
        updates_by_edge[eid] = it.value();
    }
    return true;
}

// compute composite edge cost for an edge index
double edge_cost(int edge_index) {
    Edge e = edges[edge_index];
    // base travel time (seconds) -> scale convert to a baseline meters equivalent
    double base_time = e.freeflow_time_s;
    // get update multipliers
    double traffic_mul = 1.0;
    double rain_mm = 0.0;
    bool blocked=false;
    double road_adj = 0.0;
    if(updates_by_edge.count(e.edge_id)) {
        auto &obj = updates_by_edge[e.edge_id];
        if(obj.contains("traffic_multiplier")) traffic_mul = (double)obj["traffic_multiplier"];
        if(obj.contains("rain_mm_hr")) rain_mm = (double)obj["rain_mm_hr"];
        if(obj.contains("blocked")) blocked = (bool)obj["blocked"];
        if(obj.contains("road_quality_adjust")) road_adj = (double)obj["road_quality_adjust"];
    }
    if(blocked) return W_BLOCK;
    // metrics:
    double traffic_pen = (traffic_mul - 1.0) * e.distance_m; // extra delay ~ multiplier * distance
    double weather_pen = rain_mm * 100.0; // scale
    double road_quality_pen = (10.0 - (e.road_quality + road_adj)) * 50.0;
    double safety_pen = (10.0 - e.safety_index) * 40.0;
    // composite cost -> we add meters + scaled penalties to treat as unified cost
    double cost = e.distance_m + W_TRAFFIC * (traffic_mul - 1.0) + W_WEATHER * (rain_mm) + W_ROAD_QUAL * ((10.0 - e.road_quality - road_adj)/10.0) + W_SAFETY * ((10.0 - e.safety_index)/10.0);
    // include base_time converted to meters-equivalent roughly: time_seconds * 5 (scale)
    cost += W_TIME * base_time;
    return cost;
}

// Dijkstra to compute single shortest path using composite edge cost
struct Pred { double dist; int prev; int prev_edge_idx; };
vector<int> dijkstra_path(int src, int tgt) {
    int n = adj.size();
    const double INF = 1e18;
    vector<double> dist(n, INF);
    vector<int> prev(n, -1), prev_edge(n, -1);
    using P = pair<double,int>;
    priority_queue<P, vector<P>, greater<P>> pq;
    dist[src] = 0.0; pq.push({0.0, src});
    while(!pq.empty()){
        auto pr = pq.top(); pq.pop();
        double d = pr.first; int u = pr.second;
        if(d > dist[u]) continue;
        if(u == tgt) break;
        for(auto pr2: adj[u]){
            int v = pr2.first; int ei = pr2.second;
            double c = edge_cost(ei);
            if(c >= 1e6) continue; // blocked
            double nd = d + c;
            if(nd + 1e-9 < dist[v]) {
                dist[v] = nd; prev[v] = u; prev_edge[v] = ei;
                pq.push({nd, v});
            }
        }
    }
    if(dist[tgt] >= 1e17) return {};
    vector<int> path_nodes;
    int cur = tgt;
    while(cur != -1) {
        path_nodes.push_back(cur);
        cur = prev[cur];
    }
    reverse(path_nodes.begin(), path_nodes.end());
    return path_nodes;
}

// write path.json
void write_path_json(const vector<vector<int>>& routes, const string &outfn) {
    json j;
    j["routes"] = json::array();
    for(size_t i=0;i<routes.size();++i){
        json r;
        r["id"] = (int)i;
        double total_m = 0.0;
        double total_time = 0.0;
        vector<json> pts;
        for(size_t k=0;k<routes[i].size(); ++k){
            int nid = routes[i][k];
            json p;
            p["name"] = nodes[nid].name;
            p["lat"] = nodes[nid].lat;
            p["lon"] = nodes[nid].lon;
            pts.push_back(p);
            if(k+1<routes[i].size()){
                // find edge between k and k+1
                int u = routes[i][k], v=routes[i][k+1];
                for(auto pr: adj[u]){
                    if(pr.first == v){
                        Edge &ee = edges[pr.second];
                        total_m += ee.distance_m;
                        total_time += ee.freeflow_time_s;
                        break;
                    }
                }
            }
        }
        r["distance_m"] = total_m;
        r["duration_min"] = (int)round(total_time / 60.0);
        r["points"] = pts;
        j["routes"].push_back(r);
    }
    ofstream fo(outfn);
    fo<<setw(2)<<j;
    fo.close();
    cout<<"Wrote "<<outfn<<"\n";
}

// simple K-short: Yen-lite (remove one edge from best path to produce alternatives)
vector<vector<int>> k_short_simple(int src, int tgt, int K) {
    vector<vector<int>> result;
    vector<int> best = dijkstra_path(src,tgt);
    if(best.empty()) return result;
    result.push_back(best);
    set<pair<double, vector<int>>> candidate_set;
    for(int k=1;k<K;++k){
        // take last found path and remove each edge
        vector<int> base = result.back();
        for(size_t i=0;i+1<base.size();++i){
            int a = base[i], b = base[i+1];
            // forbid all edges connecting a->b temporarily by setting updates for their edge_id blocked
            vector<int> changed;
            unordered_map<int,json> orig;
            for(auto pr: adj[a]){
                if(pr.first == b){
                    int ei = pr.second;
                    if(updates_by_edge.count(edges[ei].edge_id)==0) continue;
                    orig[edges[ei].edge_id] = updates_by_edge[edges[ei].edge_id];
                    updates_by_edge[edges[ei].edge_id]["blocked"] = true;
                    changed.push_back(edges[ei].edge_id);
                }
            }
            // recompute path
            vector<int> np = dijkstra_path(src,tgt);
            // restore updates
            for(auto c: changed){
                updates_by_edge[c] = orig[c];
            }
            if(!np.empty()){
                double dist = 0.0;
                for(size_t z=0;z+1<np.size();++z){
                    // find edge
                    for(auto pr: adj[np[z]]){
                        if(pr.first == np[z+1]) { dist += edges[pr.second].distance_m; break; }
                    }
                }
                candidate_set.insert({dist, np});
            }
        }
        if(candidate_set.empty()) break;
        auto it = candidate_set.begin();
        result.push_back(it->second);
        candidate_set.erase(it);
    }
    return result;
}

int find_node_id_by_name(const string &q) {
    for(auto &n: nodes) if(n.name == q) return n.id;
    // try case-insensitive or substring match
    for(auto &n: nodes) {
        string s = n.name, t = q;
        transform(s.begin(), s.end(), s.begin(), ::tolower);
        transform(t.begin(), t.end(), t.begin(), ::tolower);
        if(s.find(t) != string::npos) return n.id;
    }
    return -1;
}

int main(int argc, char** argv) {
    if(argc < 6) {
        cerr<<"Usage: safepath_core nodes.csv edges.csv updates.json \"start_name\" \"dest_name\" [K]\n";
        return 1;
    }
    string nodes_file = argv[1], edges_file = argv[2], updates_file = argv[3];
    string start_name = argv[4], dest_name = argv[5];
    int K = 3;
    if(argc >= 7) K = stoi(argv[6]);

    if(!load_nodes(nodes_file)) { cerr<<"Cannot load nodes\n"; return 1; }
    if(!load_edges(edges_file)) { cerr<<"Cannot load edges\n"; return 1; }
    if(!load_updates(updates_file)) { cerr<<"Cannot load updates\n"; return 1; }

    int src = find_node_id_by_name(start_name);
    int tgt = find_node_id_by_name(dest_name);
    if(src==-1 || tgt==-1) {
        cerr<<"Start or dest node not found. Use exact name from nodes.csv\n";
        for(auto &n : nodes) cerr << n.name << "\n";
        return 1;
    }

    auto routes = k_short_simple(src,tgt,K);
    if(routes.empty()) { cerr<<"No routes found\n"; return 1; }

    // print reasons - compare to best
    vector<vector<int>> allroutes = routes;
    cout << fixed << setprecision(3);
    cout << "\nTop " << allroutes.size() << " routes from " << start_name << " -> " << dest_name << ":\n";
    for(size_t i=0;i<allroutes.size();++i) {
        double total_m = 0.0;
        for(size_t z=0; z+1<allroutes[i].size(); ++z){
            for(auto pr: adj[allroutes[i][z]]){
                if(pr.first == allroutes[i][z+1]){
                    total_m += edges[pr.second].distance_m;
                    break;
                }
            }
        }
        cout << i+1 << ") Distance = " << (total_m/1000.0) << " km | Hops = " << (allroutes[i].size()-1) << " | Path: ";
        for(size_t k=0;k<allroutes[i].size();++k){
            cout << nodes[allroutes[i][k]].name;
            if(k+1<allroutes[i].size()) cout << " -> ";
        }
        cout << "\n";
        if(i>0) {
            // simple reasoning
            double best_m = 0.0;
            for(size_t z=0; z+1<allroutes[0].size(); ++z){
                for(auto pr: adj[allroutes[0][z]]){
                    if(pr.first == allroutes[0][z+1]){
                        best_m += edges[pr.second].distance_m; break;
                    }
                }
            }
            double diff_km = (total_m - best_m) / 1000.0;
            cout << "  -> Why not preferred: Longer than best by " << diff_km << " km.";
            if(allroutes[i].size() > allroutes[0].size()) cout << " More hops (" << (allroutes[i].size()-1) << " vs " << (allroutes[0].size()-1) << ").";
            cout << "\n";
        } else {
            cout << "  -> Chosen as BEST route (composite score).\n";
        }
    }

    // write path.json for viewer
    write_path_json(allroutes, "path.json");
    cout << "Wrote path.json\n";
    return 0;
}
