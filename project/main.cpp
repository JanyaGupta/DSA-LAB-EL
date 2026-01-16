// main.cpp
// SafePath C++ core (DSA + demo graph for Bangalore).
// Builds a 20-node graph, runs Dijkstra and simple K-shortest (Yen-lite),
// explains why alternatives are worse, and writes path.json for viewer.
//
// Compile: g++ -std=c++17 main.cpp -O2 -o safepath
// Run example: ./safepath Koramangala "MG Road" 3
//
// Arguments:
//   start_name (string) - node name (e.g., "Koramangala")
//   dest_name  (string) - node name (e.g., "MG Road")
//   k (optional int)    - number of top routes to output (default 3)

#include <bits/stdc++.h>
using namespace std;

struct NodeInfo {
    string name;
    double lat;
    double lon;
};

struct Edge {
    int to;
    double w; // weight (distance in meters)
    int id;
};

struct PathInfo {
    vector<int> nodes;
    double dist; // meters
};

static vector<NodeInfo> NODES;
static vector<vector<Edge>> G;
static int EDGE_COUNTER = 0;

// Helper: add undirected edge
void add_edge(int u, int v, double meters) {
    G[u].push_back({v, meters, EDGE_COUNTER++});
    G[v].push_back({u, meters, EDGE_COUNTER++});
}

// Dijkstra - returns distance and parent arrays (parent[v] = previous node)
double dijkstra(int s, int t, const unordered_set<int>& forbiddenEdgeIds, vector<int>& parent) {
    const double INF = 1e18;
    int n = (int)G.size();
    vector<double> dist(n, INF);
    parent.assign(n, -1);
    using P = pair<double,int>;
    priority_queue<P, vector<P>, greater<P>> pq;
    dist[s] = 0;
    pq.push({0, s});
    while (!pq.empty()) {
        auto top = pq.top();
        pq.pop();
    
        double d = top.first;
        int u = top.second;
    
        if (d > dist[u]) continue;
        if (u == t) break;
    
        for (auto &e : G[u]) {
            if (forbiddenEdgeIds.count(e.id)) continue;
    
            int v = e.to;
            double nd = d + e.w;
    
            if (nd + 1e-9 < dist[v]) {
                dist[v] = nd;
                parent[v] = u;
                pq.push(std::make_pair(nd, v));
            }
        }
    }
    
    return dist[t];
}

vector<int> build_path_from_parent(int t, const vector<int>& parent) {
    vector<int> path;
    int cur = t;
    while (cur != -1) {
        path.push_back(cur);
        if (parent[cur] == -1) break;
        cur = parent[cur];
    }
    reverse(path.begin(), path.end());
    return path;
}

PathInfo compute_path_info(const vector<int>& path) {
    double total = 0;
    for (size_t i=0;i+1<path.size();++i) {
        int u = path[i], v = path[i+1];
        // find edge weight
        double w = 1e18;
        for (auto &e : G[u]) if (e.to == v) { w = e.w; break; }
        if (w > 1e17) {
            // shouldn't happen
        } else total += w;
    }
    return {path, total};
}

// Yen-lite: simple candidate generation by removing one edge at a time from the best path
vector<PathInfo> yen_lite_k_shortest(int s, int t, int K) {
    vector<PathInfo> results;
    unordered_set<int> emptySet;
    vector<int> parent;
    double bestd = dijkstra(s,t,emptySet,parent);
    if (bestd >= 1e17) return results;
    vector<int> bestpath = build_path_from_parent(t,parent);
    results.push_back(compute_path_info(bestpath));
    // candidate set (dist -> path)
    set<pair<double, vector<int>>> candidates;
    // we'll iterate to produce K-1 alternatives
    for (int k=1; k<K; ++k) {
        // generate candidates by removing one edge from each path in results (or just from best)
        // We'll remove edges from the best path only and from subsequent ones generated as practical (simple method)
        vector<int> basePath = results.back().nodes; // start expanding from latest found path for variety
        for (size_t i=0;i+1<basePath.size();++i) {
            int a = basePath[i], b = basePath[i+1];
            // find edge ids for edge a->b (could be multiple; forbid all matching a->b)
            unordered_set<int> forb;
            for (auto &e : G[a]) if (e.to == b) forb.insert(e.id);
            vector<int> parent2;
            double d2 = dijkstra(s,t,forb,parent2);
            if (d2 < 1e17) {
                vector<int> p2 = build_path_from_parent(t,parent2);
                // avoid adding same as any already in results
                bool duplicate=false;
                for(auto &r: results) if (r.nodes==p2) { duplicate=true; break; }
                if (!duplicate) candidates.insert({d2,p2});
            }
        }
        if (candidates.empty()) break;
        auto it = candidates.begin();
        results.push_back(compute_path_info(it->second));
        candidates.erase(it);
    }
    return results;
}

// Human-friendly reason generator
string explain_reason(const PathInfo& best, const PathInfo& cand) {
    vector<string> reasons;
    double diff_m = cand.dist - best.dist;
    if (diff_m > 1.0) {
        char buf[200];
        sprintf(buf,"Longer than best by %.2f km", diff_m/1000.0);
        reasons.push_back(buf);
    }
    if (cand.nodes.size() > best.nodes.size()) {
        char buf[100];
        sprintf(buf,"More hops (%d edges vs %d)", (int)cand.nodes.size()-1, (int)best.nodes.size()-1);
        reasons.push_back(buf);
    }
    // detours: nodes in cand not in best
    vector<int> det;
    unordered_set<int> bestset(best.nodes.begin(), best.nodes.end());
    for (int x : cand.nodes) if (!bestset.count(x)) det.push_back(x);
    if (!det.empty()) {
        string s = "Detours via ";
        for (size_t i=0;i<det.size() && i<6; ++i) {
            if (i) s += ", ";
            s += NODES[det[i]].name;
        }
        if (det.size()>6) s += ", ...";
        reasons.push_back(s);
    }
    // heavy segments heuristic: any edge > 6 km?
    bool heavy=false; int heavyCount=0;
    for (size_t i=0;i+1<cand.nodes.size();++i) {
        int u=cand.nodes[i], v=cand.nodes[i+1];
        for (auto &e : G[u]) if (e.to==v) {
            if (e.w >= 6000.0) { heavy=true; heavyCount++; }
            break;
        }
    }
    if (heavy) {
        char buf[120];
        sprintf(buf,"Contains %d long segment(s) >= 6 km", heavyCount);
        reasons.push_back(buf);
    }
    if (reasons.empty()) return "Very similar to best; slight differences make it less optimal.";
    string out;
    for (size_t i=0;i<reasons.size();++i) {
        if (i) out += ". ";
        out += reasons[i];
    }
    return out;
}

// Utility: escape json string
string jstr(const string &s) {
    string r=""; for (char c : s) {
        if (c=='\\') r += "\\\\";
        else if (c=='\"') r += "\\\"";
        else if (c=='\n') r += "\\n";
        else r += c;
    }
    return r;
}

void write_path_json(const vector<PathInfo>& routes, const string& outfn) {
    ofstream fo(outfn);
    if (!fo) { cerr<<"Cannot write "<<outfn<<"\n"; return; }
    fo << "{\n  \"routes\": [\n";
    for (size_t i=0;i<routes.size();++i) {
        auto &rt = routes[i];
        fo << "    {\n";
        fo << "      \"id\": " << (int)i << ",\n";
        fo << "      \"distance_m\": " << rt.dist << ",\n";
        // crude duration estimate: assume avg 30 km/h -> 833.33 m/min => duration minutes = dist / 833.33
        double minutes = rt.dist / 833.3333;
        fo << "      \"duration_min\": " << (int)round(minutes) << ",\n";
        fo << "      \"hops\": " << (int)rt.nodes.size()-1 << ",\n";
        fo << "      \"points\": [\n";
        for (size_t j=0;j<rt.nodes.size();++j) {
            int nid = rt.nodes[j];
            fo << "        {\"name\":\"" << jstr(NODES[nid].name) << "\",\"lat\":" << fixed << setprecision(6) << NODES[nid].lat << ",\"lon\":" << fixed << setprecision(6) << NODES[nid].lon << "}";
            if (j+1<rt.nodes.size()) fo << ",";
            fo << "\n";
        }
        fo << "      ]\n";
        fo << "    }";
        if (i+1<routes.size()) fo << ",";
        fo << "\n";
    }
    fo << "  ]\n}\n";
    fo.close();
    cout << "Wrote " << outfn << " with " << routes.size() << " route(s).\n";
}

int find_node_by_name(const string &q) {
    for (size_t i=0;i<NODES.size();++i) if (NODES[i].name == q) return (int)i;
    return -1;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // 20 Bangalore locations (names and approximate lat/lon)
    NODES = {
        {"Koramangala", 12.9352, 77.6245},
        {"Indiranagar", 12.9719, 77.6412},
        {"MG Road", 12.9740, 77.6066},
        {"Whitefield", 12.9699, 77.7490},
        {"Silk Board", 12.9250, 77.6175},
        {"Electronic City", 12.8431, 77.6651},
        {"Jayanagar", 12.9250, 77.5938},
        {"JP Nagar", 12.9257, 77.5810},
        {"Hebbal", 13.0389, 77.5895},
        {"Yelahanka", 13.1076, 77.5936},
        {"KR Puram", 12.9844, 77.6845},
        {"Marathahalli", 12.9601, 77.7035},
        {"Banashankari", 12.9252, 77.5486},
        {"Rajajinagar", 13.0020, 77.5600},
        {"Majestic", 12.9763, 77.5713},
        {"Ulsoor", 12.9780, 77.6190},
        {"Bellandur", 12.9358, 77.6795},
        {"HSR Layout", 12.9131, 77.6400},
        {"Basavanagudi", 12.9353, 77.5685},
        {"BTM Layout", 12.9236, 77.6101}
    };

    int n = (int)NODES.size();
    G.assign(n, {});

    // Add realistic-ish connections (undirected), distances are approximate in meters
    // These connections are example edges — you can extend/modify them as needed
    auto m = [](double km){ return km * 1000.0; };
    add_edge(0, 1, m(5.0));   // Koramangala - Indiranagar
    add_edge(0, 6, m(3.1));   // Koramangala - Jayanagar
    add_edge(0, 20-1, m(2.7));// Koramangala - BTM Layout (index 19)
    add_edge(1, 2, m(3.2));   // Indiranagar - MG Road
    add_edge(2, 14, m(1.8));  // MG Road - Majestic
    add_edge(2, 15, m(1.1));  // MG Road - Ulsoor
    add_edge(3, 11, m(12.0)); // Whitefield - Marathahalli
    add_edge(11, 10, m(6.2)); // Marathahalli - KR Puram
    add_edge(10, 8, m(5.3));  // KR Puram - Hebbal
    add_edge(8, 9, m(12.0));  // Hebbal - Yelahanka
    add_edge(4, 0, m(7.2));   // Silk Board - Koramangala
    add_edge(4, 5, m(10.0));  // Silk Board - Electronic City
    add_edge(5, 16, m(6.4));  // Electronic City - Bellandur
    add_edge(16, 11, m(6.1)); // Bellandur - Marathahalli
    add_edge(6, 12, m(6.8));  // Jayanagar - Banashankari
    add_edge(6, 19, m(3.0));  // Jayanagar - BTM Layout
    add_edge(7, 6, m(3.5));   // JP Nagar - Jayanagar
    add_edge(7, 4, m(8.5));   // JP Nagar - Silk Board
    add_edge(12, 13, m(10.0)); // Banashankari - Rajajinagar
    add_edge(13, 14, m(6.5)); // Rajajinagar - Majestic
    add_edge(14, 15, m(2.3)); // Majestic - Ulsoor
    add_edge(15, 2, m(2.0));  // Ulsoor - MG Road
    add_edge(11, 16, m(4.5)); // Marathahalli - Bellandur
    add_edge(16, 17, m(7.5)); // Bellandur - HSR Layout
    add_edge(17, 0, m(6.2));  // HSR Layout - Koramangala
    add_edge(19, 0, m(2.7));  // BTM - Koramangala
    add_edge(18, 13, m(5.9)); // Basavanagudi - Rajajinagar
    add_edge(12, 19, m(7.0)); // Banashankari - BTM
    add_edge(9, 8, m(11.7));  // Yelahanka - Hebbal (already connected)
    add_edge(10, 2, m(9.8));  // KR Puram - MG Road via central
    add_edge(11, 3, m(12.0)); // Marathahalli - Whitefield (already connected in reverse)

    // A few more cross-connections to make graph richer
    add_edge(1, 17, m(6.0)); // Indiranagar - HSR
    add_edge(5, 4, m(10.5));// Electronic City - Silk Board (alt)
    add_edge(16, 0, m(4.8)); // Bellandur - Koramangala
    add_edge(2, 11, m(8.0)); // MG Road - Marathahalli (east link)
    add_edge(14, 13, m(3.2)); // Majestic - Rajajinagar (quick link)

    // Parse arguments
    if (argc < 3) {
        cout << "Usage: " << argv[0] << " <start_name> <dest_name> [K]\n";
        cout << "Available nodes:\n";
        for (size_t i=0;i<NODES.size();++i) cout << "  " << NODES[i].name << "\n";
        return 0;
    }

    string startName = argv[1];
    string destName = argv[2];
    int K = 3;
    if (argc >= 4) K = stoi(argv[3]);
    // If the name contains spaces, user should quote them. We accept exact matches only.
    int s = find_node_by_name(startName);
    int t = find_node_by_name(destName);
    if (s == -1 || t == -1) {
        cerr << "Start or destination not found (exact match required). Available nodes:\n";
        for (auto &nd : NODES) cerr << "  " << nd.name << "\n";
        return 1;
    }

    // compute up to K routes using Yen-lite
    auto routes = yen_lite_k_shortest(s,t,K);

    if (routes.empty()) {
        cout << "No path found from " << startName << " to " << destName << "\n";
        return 0;
    }

    // Print routes + reasons
    cout << fixed << setprecision(2);
    cout << "\nTop " << routes.size() << " routes from " << startName << " -> " << destName << ":\n";
    for (size_t i=0;i<routes.size();++i) {
        auto &r = routes[i];
        cout << i+1 << ") Distance: " << (r.dist/1000.0) << " km | Hops: " << (r.nodes.size()-1) << " | Path: ";
        for (size_t j=0;j<r.nodes.size();++j) {
            cout << NODES[r.nodes[j]].name;
            if (j+1<r.nodes.size()) cout << " -> ";
        }
        cout << "\n";
        if (i>0) {
            string reason = explain_reason(routes[0], r);
            cout << "   Why not preferred: " << reason << "\n";
        } else {
            cout << "   Chosen as BEST route.\n";
        }
    }

    // write path.json for viewer; include up to K (we'll write all computed routes)
    write_path_json(routes, "path.json");

    cout << "\nOpen viewer/index.html in your browser (or run a local server) to visualize path.json\n";
    return 0;
}
