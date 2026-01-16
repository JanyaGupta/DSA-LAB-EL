#!/usr/bin/env python3
"""
fetch_place_and_route.py
- Usage: python fetch_place_and_route.py "MG Road, Bangalore" "Koramangala, Bangalore"
- Produces data/nodes.csv, data/edges.csv and data/updates.json
Requires: pip install requests
"""
import sys, os, requests, json, math, time

OUT_DIR = "data"
os.makedirs(OUT_DIR, exist_ok=True)

NOMINATIM = "https://nominatim.openstreetmap.org/search"
OSRM_ROUTE = "https://router.project-osrm.org/route/v1/driving/{lon1},{lat1};{lon2},{lat2}?overview=full&steps=true"

OPEN_METEO = "https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}&hourly=rain"

HEADERS = {'User-Agent': 'SafePathDemo/1.0 (+https://example.com)'}

def geocode(place):
    params = {"format":"json","q":place,"limit":1}
    r = requests.get(NOMINATIM, params=params, headers=HEADERS, timeout=15)
    r.raise_for_status()
    j = r.json()
    if not j:
        raise RuntimeError("No geocode for " + place)
    return float(j[0]['lat']), float(j[0]['lon']), j[0].get('display_name','')

def fetch_route(lat1,lon1,lat2,lon2):
    url = OSRM_ROUTE.format(lat1=lat1,lon1=lon1,lat2=lat2,lon2=lon2)
    # OSRM expects lon,lat; careful with formatting
    url = OSRM_ROUTE.format(lon1=lon1,lat1=lat1,lon2=lon2,lat2=lat2)
    r = requests.get(url, headers=HEADERS, timeout=20)
    r.raise_for_status()
    return r.json()

def get_hourly_rain(lat, lon):
    url = OPEN_METEO.format(lat=lat, lon=lon)
    r = requests.get(url, timeout=15)
    r.raise_for_status()
    j = r.json()
    # get next hour's rainfall if available
    try:
        if 'hourly' in j and 'rain' in j['hourly']:
            arr = j['hourly']['rain']
            if len(arr)>0:
                # take max recent value
                return float(max(arr))
    except:
        pass
    return 0.0

def haversine(lat1,lon1,lat2,lon2):
    R=6371000.0
    phi1=math.radians(lat1); phi2=math.radians(lat2)
    dphi=math.radians(lat2-lat1); dlambda=math.radians(lon2-lon1)
    a=math.sin(dphi/2)**2 + math.cos(phi1)*math.cos(phi2)*math.sin(dlambda/2)**2
    return R*2*math.atan2(math.sqrt(a), math.sqrt(1-a))

def main():
    if len(sys.argv) < 3:
        print("Usage: python fetch_place_and_route.py \"Start place\" \"Destination place\"")
        sys.exit(1)
    start = sys.argv[1]
    dest = sys.argv[2]
    print("Geocoding:", start, "->", dest)
    s_lat,s_lon,s_name = geocode(start)
    d_lat,d_lon,d_name = geocode(dest)
    print("Start coords:", s_lat, s_lon)
    print("Dest coords:", d_lat, d_lon)

    print("Requesting route from OSRM...")
    rjson = fetch_route(s_lat,s_lon,d_lat,d_lon)
    if rjson.get("code") != "Ok":
        raise RuntimeError("OSRM error: " + str(rjson.get("code")))

    # Build nodes/edges from steps. We'll create node per step start location
    nodes = []
    edges = []
    node_id = 0
    # start node
    nodes.append({"id": node_id, "name": s_name, "lat": s_lat, "lon": s_lon})
    prev_id = node_id
    node_id += 1

    # parse legs->steps
    legs = rjson['routes'][0]['legs']
    total_idx = 0
    for leg in legs:
        for step in leg['steps']:
            # each step has maneuver.location [lon,lat] sometimes; use geometry for end
            # take step['maneuver']['location'] as start, step.geometry as polyline for full coordinates
            # OSRM returns geometry with coordinates array inside route geometry, but step geometry may be encoded.
            # For simplicity we'll compute end as the step's maneuver location of the next step or use step['geometry']'s last point
            # We'll just use the step's maneuver location as node point (lon,lat)
            if 'maneuver' in step and 'location' in step['maneuver']:
                lon, lat = step['maneuver']['location'][0], step['maneuver']['location'][1]
            else:
                # fallback to route geometry
                coords = rjson['routes'][0]['geometry']['coordinates']
                lon, lat = coords[min(total_idx, len(coords)-1)][0], coords[min(total_idx, len(coords)-1)][1]
            name = step.get('name', '')
            nodes.append({"id": node_id, "name": name if name else f"seg{node_id}", "lat": lat, "lon": lon})
            # approximate distance: use step.distance if available else haversine to previous
            dist = step.get('distance', None)
            if dist is None:
                dist = haversine(nodes[prev_id]['lat'], nodes[prev_id]['lon'], lat, lon)
            # time:
            duration = step.get('duration', max(1.0, dist / 13.88))  # default 50 km/h => 13.88 m/s
            # default road quality/safety guess (we'll refine by fetching other data)
            road_quality = 7.0
            safety_index = 7.0
            edges.append({
                "u": prev_id,
                "v": node_id,
                "distance_m": float(dist),
                "freeflow_time_s": float(duration),
                "road_quality": road_quality,
                "safety_index": safety_index,
                "edge_id": len(edges)
            })
            prev_id = node_id
            node_id += 1
            total_idx += 1

    # final node (destination)
    nodes.append({"id": node_id, "name": d_name, "lat": d_lat, "lon": d_lon})
    edges.append({"u": prev_id, "v": node_id, "distance_m": haversine(nodes[prev_id]['lat'], nodes[prev_id]['lon'], d_lat, d_lon),
                  "freeflow_time_s": haversine(nodes[prev_id]['lat'], nodes[prev_id]['lon'], d_lat, d_lon)/13.88,
                  "road_quality": 7.0,
                  "safety_index": 7.0,
                  "edge_id": len(edges)})
    node_id += 1

    # Write nodes.csv and edges.csv
    nodes_path = os.path.join(OUT_DIR, "nodes.csv")
    edges_path = os.path.join(OUT_DIR, "edges.csv")
    updates_path = os.path.join(OUT_DIR, "updates.json")

    with open(nodes_path, "w", encoding="utf-8") as f:
        f.write("id,name,lat,lon\n")
        for n in nodes:
            f.write(f"{n['id']},\"{n['name'].replace('\"','')}\",{n['lat']},{n['lon']}\n")
    with open(edges_path, "w", encoding="utf-8") as f:
        f.write("u,v,distance_m,freeflow_time_s,road_quality,safety_index,edge_id\n")
        for e in edges:
            f.write(f"{e['u']},{e['v']},{e['distance_m']},{e['freeflow_time_s']},{e['road_quality']},{e['safety_index']},{e['edge_id']}\n")

    # Build updates.json initial (traffic=1.0 multiplier, rain=0.0)
    updates = {}
    for e in edges:
        # fetch per-edge weather estimate by sampling midpoint
        mid_lat = (nodes[e['u']]['lat'] + nodes[e['v']]['lat']) / 2.0
        mid_lon = (nodes[e['u']]['lon'] + nodes[e['v']]['lon']) / 2.0
        rain = get_hourly_rain(mid_lat, mid_lon)
        # traffic multiplier initial 1.0 (no congestion)
        updates[str(e['edge_id'])] = {
            "traffic_multiplier": 1.0,
            "rain_mm_hr": rain,
            "road_quality_adjust": 0.0,
            "blocked": False
        }
        time.sleep(0.2)  # be polite to API
    with open(updates_path, "w", encoding="utf-8") as f:
        json.dump(updates, f, indent=2)

    print("Wrote:", nodes_path, edges_path, updates_path)
    print("Now run C++ core to compute routes, then run viewer.py to visualize.")

if __name__ == "__main__":
    main()
