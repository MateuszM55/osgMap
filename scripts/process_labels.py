import sys
sys.stdout.reconfigure(encoding='utf-8')

import osmium
import geopandas as gpd
from shapely.geometry import Point

# --- Lista tagów do filtrowania ---
TAG_CATEGORIES = {
    "food": ["restaurant", "fast_food", "bar", "cafe", "pub"],
    "education": ["school", "university", "college", "kindergarten", "research"],
    "office": ["government", "public_building", "townhall"],
    "public_transport": ["bus_stop", "tram_stop", "station", "subway_entrance", "halt"]
}

# Mapowanie OSM tagów na kategorie
OSM_TAG_TO_SUBTYPE = {
    "amenity": TAG_CATEGORIES["food"] + TAG_CATEGORIES["education"] + ["townhall", "public_building"],
    "office": TAG_CATEGORIES["office"],
    "highway": ["bus_stop"],
    "public_transport": ["platform", "stop_position"],
    "railway": ["station", "tram_stop", "halt", "subway_entrance"]
}

class NodeExtractor(osmium.SimpleHandler):
    def __init__(self):
        super().__init__()
        self.points = []

    def node(self, n):
        tags = n.tags
        name = tags.get("name", None)
        osm_id = n.id

        # sprawdzenie czy którykolwiek tag odpowiada naszym kategoriom
        for k, values in OSM_TAG_TO_SUBTYPE.items():
            if k in tags:
                v = tags[k]
                if v in values:
                    # dopasowanie nadrzędnej kategorii
                    for category, subtypes in TAG_CATEGORIES.items():
                        if v in subtypes:
                            self.points.append({
                                "name": name,
                                "type": category,
                                "subtype": v,
                                "source_id": osm_id,
                                "geometry": Point(n.location.lon, n.location.lat)
                            })
                            return  # tylko jeden wpis na punkt

# --- Wczytanie pliku PBF ---
pbf_file = "latest.osm.pbf"  # <-- zmień na właściwy plik
handler = NodeExtractor()
handler.apply_file(pbf_file)

# --- Konwersja do GeoDataFrame ---
gdf = gpd.GeoDataFrame(handler.points, crs="EPSG:4326")

# --- Zapis do Shapefile ---
gdf.to_file("osm_points.shp", encoding="utf-8")
print(f"Zapisano {len(gdf)} punktów do osm_points.shp")