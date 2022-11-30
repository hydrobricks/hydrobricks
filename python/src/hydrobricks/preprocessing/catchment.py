import hydrobricks as hb


class Catchment:
    """Creation of catchment-related data"""

    def __init__(self, outline=None):
        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")
        if not hb.has_geopandas:
            raise ImportError("geopandas is required to do this.")
        if not hb.has_shapely:
            raise ImportError("shapely is required to do this.")
        self.outline = None
        self.dem = None
        self._extract_outline(outline)

    def extract_dem(self, dem_path) -> bool:
        try:
            geoms = [hb.mapping(self.outline)]
            with hb.rasterio.open(dem_path) as src:
                self.dem, _ = hb.mask(src, geoms, crop=True)
            return True
        except ValueError as e:
            print(e)
            return False
        except Exception as e:
            print(e)
            return False

    def get_elevation_bands(self):
        pass

    def _extract_outline(self, outline):
        if not outline:
            return
        shapefile = hb.gpd.read_file(outline)
        geoms = shapefile.geometry.values
        self.outline = geoms[0]

