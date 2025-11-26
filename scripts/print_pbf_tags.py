import sys
sys.stdout.reconfigure(encoding='utf-8')

import osmium
from collections import defaultdict

class NodeTagIndex(osmium.SimpleHandler):
    def __init__(self):
        super().__init__()
        self.keys = defaultdict(set)  # klucz → zbiór wartości

    def node(self, n):
        for k, v in n.tags:
            self.keys[k].add(v)

# --- użycie ---
pbf_file = "latest.osm.pbf"   # <-- zmień na właściwy plik

h = NodeTagIndex()
h.apply_file(pbf_file)

# wypisz wyniki
for key in sorted(h.keys):
    print(f"{key} {{")
    for value in sorted(h.keys[key]):
        print(f"  {value}")
    print("}")
