#!/usr/bin/env python3
import time, json, random, os
UP = "data/updates.json"
if not os.path.exists(UP):
    print("Run fetch_place_and_route.py first to create data/updates.json")
    raise SystemExit(1)
for t in range(50):
    j = json.load(open(UP))
    # randomly pick some edges to spike traffic
    for _ in range(3):
        eid = random.choice(list(j.keys()))
        j[eid]["traffic_multiplier"] = round(random.uniform(1.0, 3.0),2)
        j[eid]["road_quality_adjust"] = round(random.uniform(-2.0, 1.0),2)
        # random block
        if random.random() < 0.05:
            j[eid]["blocked"] = True
        else:
            j[eid]["blocked"] = False
        # random rain
        j[eid]["rain_mm_hr"] = round(random.uniform(0.0, 12.0),2)
    json.dump(j, open(UP,"w"), indent=2)
    print("Wrote updates.json (tick)", t)
    time.sleep(3)
