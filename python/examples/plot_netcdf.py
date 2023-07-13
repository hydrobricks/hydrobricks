# https://joehamman.com/2013/10/12/plotting-netCDF-data-with-Python/
# https://www2.atmos.umd.edu/~cmartin/python/examples/netcdf_example1.html
from datetime import datetime, timedelta

import geopandas as gpd
import matplotlib.pyplot as plt
import numpy as np
import shapefile as shp
import shapely
import shapely.vectorized
import xarray as xa
from descartes import PolygonPatch  # https://pypi.org/project/descartes/
from matplotlib.collections import PatchCollection
from matplotlib.colorbar import ColorbarBase
from matplotlib.colors import LightSource
from matplotlib.patches import PathPatch, Polygon
from matplotlib.path import Path
from mpl_toolkits.basemap import Basemap
from netCDF4 import Dataset as NetCDFFile
from osgeo import gdal

# WARNING: This library had to be updated BY HAND
# https://stackoverflow.com/questions/75287534/indexerror-descartes-polygonpatch-wtih-shapely
# https://stackoverflow.com/questions/21824157/how-to-extract-interior-polygon-coordinates-using-shapely

def plot_Alps(output_filename):
    #bmap = Basemap(llcrnrlon=4.5, llcrnrlat=43.5, urcrnrlon=17., urcrnrlat=48.5, epsg=5520) # WHOLE ALPS
    bmap = Basemap(llcrnrlon=6.0, llcrnrlat=45.0, urcrnrlon=13., urcrnrlat=47.5, epsg=5520)

    bmap.arcgisimage(service='World_Shaded_Relief', xpixels=1500, verbose=True)
    #bmap.bluemarble()

    mazia = '/home/anne-laure/Documents/Datasets/Italy_Study_area/OutletMazia_WatershedSameAsComiti2019_4326FromOriginalCRS'
    solda = '/home/anne-laure/Documents/Datasets/Italy_Study_area/OutletSolda_WatershedAbovePonteDiStelvio_4326FromOriginalCRS'
    navisence = '/home/anne-laure/Documents/Datasets/Swiss_Study_area/LaNavisence_Chippis/125595'
    borgne = '/home/anne-laure/Documents/Datasets/Swiss_Study_area/LaBorgne_Bramois/CHVS-005'

    bmap.readshapefile(mazia, 'mazia', drawbounds = False)
    bmap.readshapefile(solda, 'solda', drawbounds = False)
    bmap.readshapefile(navisence, 'navisence', drawbounds = False)
    bmap.readshapefile(borgne, 'borgne', drawbounds = False)

    bmap.drawmeridians(np.arange(5, 15, 1), labels=[0,0,0,1], linewidth=0.5)
    bmap.drawparallels(np.arange(40, 50, 1), labels=[1,0,0,0], linewidth=0.5)

    offset = 5000
    for _, shape in zip(bmap.mazia_info, bmap.mazia):
        x, y = zip(*shape)
        bmap.plot(x, y, marker=None, color='purple')
        print(x[0])
        plt.text(x[0] + offset, y[0] + offset, 'Mazia', color='purple')
    for _, shape in zip(bmap.solda_info, bmap.solda):
        x, y = zip(*shape)
        bmap.plot(x, y, marker=None, color='indigo')
        plt.text(x[0] + 3 * offset, y[0] - offset, 'Solda', color='indigo')
    for _, shape in zip(bmap.navisence_info, bmap.navisence):
        x, y = zip(*shape)
        bmap.plot(x, y, marker=None, color='orangered')
        plt.text(x[0] + offset, y[0] + offset, 'Navisence', color='orangered')
    for _, shape in zip(bmap.borgne_info, bmap.borgne):
        x, y = zip(*shape)
        bmap.plot(x, y, marker=None, color='darkred')
        plt.text(x[0] + 3 * offset, y[0], 'Borgne', color='darkred')

    plt.savefig(output_filename)
    plt.show()

def plot_geology(tif_filename, shapefile, mask, target_crs):
    print('Plot shapefiles')
    hatch1 = '..'
    hatch2 = 'OO'

    # WARP SHAPEFILE TO CORRECT CRS
    out_shapefile = shapefile + '_EPSG' + target_crs
    gdf = gpd.read_file(shapefile + '.shp')
    gdf.to_crs(epsg=target_crs, inplace=True)
    gdf.to_file(out_shapefile + '.shp')

    # INITIALIZE FIGURE
    fig, ax = plt.subplots(figsize=(20,20))
    ax.set_aspect(1)

    # OPEN WATERSHED SHAPEFILE, TRANSFORM IT TO A PATH FOR POLYGON CLIPPING AND GET ITS EXTENT FOR DEM CROPPING.
    sf = shp.Reader(mask)
    for shape_rec in sf.shapeRecords():
        vertices = []
        codes = []
        pts = shape_rec.shape.points
        prt = list(shape_rec.shape.parts) + [len(pts)]
        for i in range(len(prt) - 1):
            for j in range(prt[i], prt[i+1]):
                vertices.append((pts[j][0], pts[j][1]))
            codes += [Path.MOVETO]
            codes += [Path.LINETO] * (prt[i+1] - prt[i] -2)
            codes += [Path.CLOSEPOLY]
        clip = Path(vertices, codes)
        clip = PathPatch(clip, transform=ax.transData)
        unzipped = list(zip(*pts))
        minx = min(list(unzipped[0]))
        maxx = max(list(unzipped[0]))
        miny = min(list(unzipped[1]))
        maxy = max(list(unzipped[1]))
        print(minx, miny, maxx, maxy)

    # OPEN TIF AND CROP IT TO MAX EXTENT
    dem = gdal.Open(tif_filename)
    ulx, xres, _, uly, _, yres  = dem.GetGeoTransform()
    lrx = ulx + (dem.RasterXSize * xres)
    lry = uly + (dem.RasterYSize * yres)
    print('Extent of DEM before cropping:', lrx, lry, ulx, uly)
    dem = gdal.Translate(tif_filename + 'cropped.tif', dem, projWin = [minx, maxy, maxx, miny])
    ulx, xres, _, uly, _, yres  = dem.GetGeoTransform()
    lrx = ulx + (dem.RasterXSize * xres)
    lry = uly + (dem.RasterYSize * yres)
    print('Extent of DEM after cropping:', lrx, lry, ulx, uly)
    print(tif_filename + 'cropped.tif')

    # CREATE HILLSHADE
    hillshade = gdal.DEMProcessing('hillshade.tif', dem, 'hillshade',
                                   computeEdges = True,
                                   multiDirectional = True, zFactor=2)
    hillshade_array = hillshade.GetRasterBand(1).ReadAsArray()

    # MASK HILLSHADE WITH WATERSHED SHAPEFILE
    gpd_mask = gpd.read_file(mask)
    xi = np.linspace(ulx, lrx, dem.RasterXSize)
    yi = np.linspace(uly, lry, dem.RasterYSize)
    xi, yi = np.meshgrid(xi, yi)
    mask_geometry = gpd_mask.dissolve().geometry.item()
    shp_mask = shapely.vectorized.contains(mask_geometry, xi, yi)
    masked_hillshade = np.where(shp_mask, hillshade_array, np.nan)

    # GEOLOGICAL DICTIONNARY
    # https://stackoverflow.com/questions/48520393/filling-shapefile-polygons-with-a-color-in-matplotlib
    UG_dict = {0: "NULL",
               6: "Urbanised",
               7: "Glaciers",
               8: "Lakes",
               9: "Generic landfill",
               10: "Quarry/mine dump",
               26: "Granodiorites and tonalites",
               28: "Andesites and basalts",
               49: "Paragneisses",
               76: "Silicate marbles (msc)",
               86: "Undifferentiated milonites (ex mil)",
               114: "Granitic-granodioritic orthogneisses",
               115: "Biotitic or two-mica paragneisses",
               116: "Phyllonites",
               117: "Quartzites and quartzschists",
               119: "Prasinites",
               125: "Phyllites",
               127: "Orthogneisses",
               128: "Chlorite schists",
               130: "Serpentinites",
               134: "Gabbros (pl. Sondalo; ex gb)",
               135: "Diorites (pl. Sondalo; ex dr)",
               136: "Quartzodiorites (pl. Sondalo; ex qzd)",
               137: "Granites and granodiorites (pl. di Sondalo; ex gr)",
               143: "Orthogneiss",
               144: "Two-mica orthogneisses",
               145: "Leucocratic orthogneiss (garnet)",
               146: "Leucocratic orthogneiss",
               147: "Chlorite and sericite micaschists",
               148: "MS-gt micaschists",
               149: "Contact facies",
               150: "Gt - st micaschists and paragneisses (OMI)",
               152: "Banded paragneisses",
               153: "Two-mica paragneisses",
               156: "Sillimanite paragneisses",
               158: "Contact sillimanite paragneisses",
               159: "Quartzites",
               163: "Metavulcanites-metariolites (Porphyroids Auct.) (prd)",
               166: "Amphibolites",
               167: "Green schists ('Prasinites') (ex prs)",
               168: "Marbles",
               171: "Metagranites",
               172: "Orthogneisses",
               175: "Banded Micaschists and Paragneisses",
               176: "Silvery mica schists",
               177: "Minute paragneisses",
               178: "Quartzites",
               179: "Lasa Marbles",
               180: "Amphibolites",
               349: "Basic/intermediate strands (fb)",
               357: "Acidic seams (fa)",
               358: "Basic/intermediate strands (fb)",
               412: "Alpine Verrucano",
               414: "Indistinct Middle Triassic Carbonates",
               416: "Val Forcola/Fanez Formation",
               417: "Chalks",
               418: "Main Dolomite - Hauptdolomit",
               419: "Intraformational Breccias",
               420: "Subtidal fine dolomite (Ramp dolomite facies)",
               422: "Stratified inner platform dolomites",
               423: "Quattervals Limestone",
               424: "Pra Grata Formation",
               425: "Plattenkalk",
               426: "Fraele Formation",
               745: "Po Synthema (Postglacial Unit)",
               746: "Allomembrane of the Ancient Po (Ancient Postglacial Unit)",
               747: "Allomembrane of the Little Ice Age (Postglacial Little Ice Age Unit)",
               748: "Allomembrane of the Recent Po (Recent Postglacial Unit)",
               752: "Allformation of Cantu",
               753: "Iseo allformation in synonymy with Allof. Cantu'",
               755: "Braulio Unit",
               756: "Passo di S. Maria Unit",
               757: "Recent Scorluzzo Unit",
               758: "Ancient Scorluzzo Unit",
               759: "Rese di Scorluzzo Unit",
               760: "Forcola Unit",
               763: "Vitelli Unit",
               764: "Glandadura Unit",
               765: "Recent Mogenaccia Unit",
               766: "Ancient Mogenaccia Unit",
               767: "Recent Pedenoletto Unit",
               768: "Ancient Pedenoletto Unit",
               769: "Pedenolo Unit",
               770: "Piz Umbrail Unit",
               771: "Valle del Gesso Unit",
               772: "Val Viola Unit",
               774: "Allomembro della Valle del Prete",
               775: "Allomembro della Vallecetta",
               776: "Allomembro del M. Vallecetta",
               778: "M. Oultoir Unit",
               780: "Mala Valley allomembers",
               785: "Allomembro del Frodolfo",
               786: "Valle delle Rosole Unit",
               787: "Val Cedec Unit",
               789: "Val Pisella Unit",
               790: "Val Manzina Unit",
               791: "Pasquale Unit",
               792: "Valle Confinale Unit",
               793: "Valle Cavallaro Unit",
               796: "Val d'Uzza Unit",
               797: "Punta Segnale Unit",
               799: "Costa Sobretta Unit",
               801: "Val Sobretta Unit",
               803: "Grasso di Boccolina Unit",
               804: "Valle Sarasina Unit",
               1070: "GARDA SYNTHEMA",
               1072: "Allomembro di Bondo",
               1073: "Allomembro Malga Fontana Bianca",
               1074: "Allomembro Monte delle Pecore",
               1075: "Allomembro Croda di Cengles",
               1076: "Allomembro Dosso di Cengles",
               1077: "Allomembro Punta della Cascata",
               1078: "Allomembro Testa del Toro",
               1080: "Allomembro di Solda (high valleys)",
               1081: "Piz Minschuns Unit (Atr2)",
               1082: "Pizzo della Forcola Unit (Atr3)",
               1083: "Alpe di Tarres Unit (Atr4)",
               1084: "Rocca Bianca Unit (Atr5)",
               1085: "Alpe di Glorenza Unit (Atr6)",
               1086: "Forcella del'Orso North Unit",
               1088: "Malghe di Stelvio Unit (Atr1)",
               1089: "Val Tabaretta Unit",
               1090: "Val di Piaies Unit",
               1091: "Val di Zai Unit",
               1092: "Passo di Zai Unit (r.gl.)",
               1093: "Monte Muta Unit (r.gl.)",
               1094: "Monte dei Covoni Unit (r.gl.)",
               1237: "Colma del Piano Unit (indistinct)"}

    # CREATE LIST OF MAPPED UNITS
    shapefile = gpd.GeoDataFrame.from_file(out_shapefile + '.shp')
    UG_present_in_mask = []
    for UG, poly in zip(shapefile.UG, shapefile.geometry):
        intersection = mask_geometry.intersection(poly)
        if not intersection.is_empty:
            UG_present_in_mask.append(UG)
    UG_present_in_mask = list(set(UG_present_in_mask)) # Get unique values
    UG_present_in_mask.sort()
    print(UG_present_in_mask)
    #color_levels = [list(UG_dict).index(UG) for UG in UG_present_in_mask]
    #print(color_levels)

    #UG_present_in_mask1 = UG_present_in_mask[:len(UG_present_in_mask)//2]
    #UG_present_in_mask2 = UG_present_in_mask[len(UG_present_in_mask)//2:]
    UG_present_in_mask1, UG_present_in_mask2, UG_present_in_mask3 = np.array_split(UG_present_in_mask, 3)

    # CREATE COLORMAP
    cmap = plt.cm.RdYlBu
    norm1 = plt.cm.colors.BoundaryNorm(UG_present_in_mask1, 256)
    norm2 = plt.cm.colors.BoundaryNorm(UG_present_in_mask2, 256)
    norm3 = plt.cm.colors.BoundaryNorm(UG_present_in_mask3, 256)

    # DRAW BASEMAP
    bmap = Basemap(epsg = '4149',
                resolution = 'l',
                llcrnrlon = minx, #ulx,
                llcrnrlat = miny, #lry,
                urcrnrlon = maxx, #lrx,
                urcrnrlat = maxy) #uly)
    print(ulx, lry, lrx, uly)
    bmap.imshow(masked_hillshade, cmap='gray', origin='upper')
    bmap.drawmeridians(np.arange(5, 15, 0.05), labels=[0,0,0,1], linewidth=0.5)
    bmap.drawparallels(np.arange(40, 50, 0.05), labels=[1,0,0,0], linewidth=0.5)

    # ADD GEOLOGICAL POLYGONS!
    shapefile = gpd.GeoDataFrame.from_file(out_shapefile + '.shp')
    patches1 = []
    patches2 = []
    patches3 = []
    for UG, poly in zip(shapefile.UG, shapefile.geometry):
        if UG in UG_present_in_mask1:
            color = cmap(norm1(UG))
            if poly.geom_type == 'Polygon':
                mpoly = shapely.ops.transform(bmap, poly)
                patches1.append(PolygonPatch(mpoly, color=color, linewidth=0.5, alpha=0.5))
            elif poly.geom_type == 'MultiPolygon':
                for subpoly in poly:
                    mpoly = shapely.ops.transform(bmap, poly)
                    patches1.append(PolygonPatch(mpoly, color=color, linewidth=0.5, alpha=0.5))
            else:
                print("'poly' is neither a polygon nor a multi-polygon. Skipping it.")
        elif UG in UG_present_in_mask2:
            color = cmap(norm2(UG))
            if poly.geom_type == 'Polygon':
                mpoly = shapely.ops.transform(bmap, poly)
                patches2.append(PolygonPatch(mpoly, color=color, linewidth=0.5, alpha=0.5))
            elif poly.geom_type == 'MultiPolygon':
                for subpoly in poly:
                    mpoly = shapely.ops.transform(bmap, poly)
                    patches2.append(PolygonPatch(mpoly, color=color, linewidth=0.5, alpha=0.5))
            else:
                print("'poly' is neither a polygon nor a multi-polygon. Skipping it.")
        else:
            color = cmap(norm3(UG))
            if poly.geom_type == 'Polygon':
                mpoly = shapely.ops.transform(bmap, poly)
                patches3.append(PolygonPatch(mpoly, color=color, linewidth=0.5, alpha=0.5))
            elif poly.geom_type == 'MultiPolygon':
                for subpoly in poly:
                    mpoly = shapely.ops.transform(bmap, poly)
                    patches3.append(PolygonPatch(mpoly, color=color, linewidth=0.5, alpha=0.5))
            else:
                print("'poly' is neither a polygon nor a multi-polygon. Skipping it.")
    pc1 = PatchCollection(patches1, match_original=True, edgecolor='k', linewidths=0.5, hatch=hatch1)
    pc2 = PatchCollection(patches2, match_original=True, edgecolor='k', linewidths=0.5, hatch=hatch2)
    pc3 = PatchCollection(patches3, match_original=True, edgecolor='k', linewidths=0.5)
    ax.add_collection(pc1)
    ax.add_collection(pc2)
    ax.add_collection(pc3)

    # CLIP POLYGONS TO WATERSHED EXTENT
    for contour in ax.collections:
        contour.set_clip_path(clip)

    # ADDING A GEOLOGICAL LEGEND
    handles = []
    for UG in UG_present_in_mask1:
        handles.append(Polygon([(0,0),(10,0),(0,-10)], facecolor=cmap(norm1(UG)),
                               label=UG_dict[UG], alpha=0.5, edgecolor='k', hatch=hatch1))
    for UG in UG_present_in_mask2:
        handles.append(Polygon([(0,0),(10,0),(0,-10)], facecolor=cmap(norm2(UG)),
                               label=UG_dict[UG], alpha=0.5, edgecolor='k', hatch=hatch2))
    for UG in UG_present_in_mask3:
        handles.append(Polygon([(0,0),(10,0),(0,-10)], facecolor=cmap(norm3(UG)),
                               label=UG_dict[UG], alpha=0.5, edgecolor='k'))
    plt.legend(handles=handles, loc='center left', bbox_to_anchor=(1, 0.5), ncol=1, fontsize="10")
    plt.subplots_adjust(right=1)

    filename = '/home/anne-laure/Documents/Datasets/OutputFigures/BormioGeologicalMap'
    ax.set_title('Bormio geological map 10k', fontsize=20)
    plt.savefig(filename + '.svg', format="svg", bbox_inches='tight', dpi=30)
    plt.savefig(filename + '.png', format="png", bbox_inches='tight', dpi=300)
    plt.show()

def plot_global_geology(tif_filename, shapefile, mask, output_filename, cascade_network, title, target_crs):
    print('Plot shapefiles, the watershed must already be in the correct CRS.')

    # WARP SHAPEFILE TO CORRECT CRS
    out_shapefile = shapefile + '_EPSG' + target_crs
    gdf = gpd.read_file(shapefile + '.shp')
    gdf.to_crs(epsg=target_crs, inplace=True)
    gdf.to_file(out_shapefile + '.shp')

    # INITIALIZE FIGURE
    fig, ax = plt.subplots()
    ax.set_aspect(1)

    # OPEN WATERSHED SHAPEFILE, TRANSFORM IT TO A PATH FOR POLYGON CLIPPING AND GET ITS EXTENT FOR DEM CROPPING.
    sf = shp.Reader(mask)
    for shape_rec in sf.shapeRecords():
        vertices = []
        codes = []
        pts = shape_rec.shape.points
        prt = list(shape_rec.shape.parts) + [len(pts)]
        for i in range(len(prt) - 1):
            for j in range(prt[i], prt[i+1]):
                vertices.append((pts[j][0], pts[j][1]))
            codes += [Path.MOVETO]
            codes += [Path.LINETO] * (prt[i+1] - prt[i] -2)
            codes += [Path.CLOSEPOLY]
        clip = Path(vertices, codes)
        clip = PathPatch(clip, transform=ax.transData)
        unzipped = list(zip(*pts))
        minx = min(list(unzipped[0]))
        maxx = max(list(unzipped[0]))
        miny = min(list(unzipped[1]))
        maxy = max(list(unzipped[1]))
        print(minx, miny, maxx, maxy)

    # OPEN TIF AND CROP IT TO MAX EXTENT
    dem = gdal.Open(tif_filename)
    ulx, xres, _, uly, _, yres  = dem.GetGeoTransform()
    lrx = ulx + (dem.RasterXSize * xres)
    lry = uly + (dem.RasterYSize * yres)
    print('Extent of DEM before cropping:', lrx, lry, ulx, uly)
    dem = gdal.Translate(tif_filename + 'cropped.tif', dem, projWin = [minx, maxy, maxx, miny])
    ulx, xres, _, uly, _, yres  = dem.GetGeoTransform()
    lrx = ulx + (dem.RasterXSize * xres)
    lry = uly + (dem.RasterYSize * yres)
    print('Extent of DEM after cropping:', lrx, lry, ulx, uly)
    print(tif_filename + 'cropped.tif')

    # CREATE HILLSHADE
    hillshade = gdal.DEMProcessing('hillshade.tif', dem, 'hillshade',
                                   computeEdges = True,
                                   multiDirectional = True, zFactor=2)
    hillshade_array = hillshade.GetRasterBand(1).ReadAsArray()

    # MASK HILLSHADE WITH WATERSHED SHAPEFILE
    gpd_mask = gpd.read_file(mask)
    xi = np.linspace(ulx, lrx, dem.RasterXSize)
    yi = np.linspace(uly, lry, dem.RasterYSize)
    xi, yi = np.meshgrid(xi, yi)
    mask_geometry = gpd_mask.dissolve().geometry.item()
    shp_mask = shapely.vectorized.contains(mask_geometry, xi, yi)
    masked_hillshade = np.where(shp_mask, hillshade_array, np.nan)
    masked_dem = np.where(shp_mask, dem.GetRasterBand(1).ReadAsArray(), np.nan)

    # GEOLOGICAL DICTIONNARY
    # https://stackoverflow.com/questions/48520393/filling-shapefile-polygons-with-a-color-in-matplotlib
    tettonica = {"PENNIDICO": "PENNIDIC",
                 "AUSTROALPINO": "AUSTROALPINE",
                 "QUATENARIO": "QUATERNARY",
                 "SUDALPINO": "SOUTH ALPINE",
                 "MAGMATITI TARDO-ALPINE": "LATE ALPINE MAGMATITE",
                 "AVAMPAESE": "FORELAND BASIN"
                 }

    leg_it = {"Gneiss centrale": "Central Gneiss",
              "Calcescisti con ofioliti": "Bündner schist with ophiolites",
              "Basamento cristallino e relative coperture": "Crystalline basement and associated covers",
              "Coperture permo-mesozoiche delle varie falde di basamento: Mesozoico del Brennero": "Permo-Mesozoic covers of the various basement strata: Brenner Mesozoic",
              "Coperture permo-mesozoiche delle varie falde di basamento: Permomesozoico del Tarntal": "Permo-Mesozoic covers of the various basement strata: Tarntal Permomesozoic",
              "Fillade di Innsbruck - unità con metamorfismo varisico di basso grado ed alpino trascurabile": "Innsbruck phyllade - Unit with low-grade Variscan and insignificant Alpine metamorphism",
              "Depositi quaternari": "Quaternary deposits",
              "Basamento metamorfico varisico di medio grado: Scisti di Rendena": "Variscan metamorphic basement of medium grade: Rendena Shale",
              "Plutone permiano: Dos del Sabion": "Permian Pluton: Dos del Sabion",
              "Plutone permiano: Cima d'Asta": "Permian Pluton: Cima d'Asta",
              "Successione sedimentaria permo-giurassica": "Permo-Jurassic sedimentary succession",
              "Successione sedimentaria cretacico-paleogenica": "Cretaceous-paleogene sedimentary succession",
              "Basamento metamorfico varisico di basso grado: Fillade quarzifera della Val Sugana": "Variscan metamorphic basement of low grade: Val Sugana quartz phyllite",
              "Vulcaniti permiane: Vulcaniti della Val Trompia": "Permian volcanic rocks: Val Trompia volcanic rocks",
              "Basamento metamorfico varisico di basso grado": "Variscan metamorphic basement of low grade",
              "Unità di Anterselva - unità con metamorfismo varisico di medio-alto grado e alpino trascurabile": "Antholz / Anterselva unit - Unit with medium- to high-grade Variscan and insignificant Alpine metamorphism",
              "Plutone terziario: lamella tonalitica": "Tertiary Pluto: tonalitic lamella",
              "Successione paleozoica (Alpi Carniche)": "Paleozoic succession (Carnic Alps)",
              "Falda Silvretta - unità con metamorfismo varisico di medio-alto grado ed alpino medio": "Silvretta stratum - Unit with medium- to high-grade Variscan and medium Alpine metamorphism",
              "Plutone terziario: Vedrette di Ries": "Tertiary Pluto: Rieserferner / Vedrette di Ries",
              "Coperture permo-mesozoiche delle varie falde di basamento": "Permo-Mesozoic covers of the various basement strata",
              "Unità di Monteneve - unità con metamorfismo alpino di medio-alto grado": "Schneeberger range / Monteneve unit - Unit with medium- to high-grade Alpine metamorphism",
              "Falda di Tasna (successione sedimentaria triassico-eocenica con scaglie di basamento cristallino)": "Tasna stratum (Triassic-Eocene sedimentary succession with crystalline basement flakes)",
              "Falda di Tasna (successione sedimentaria triassico-eocenica con scglie di basamento cristallino)": "Tasna stratum (Triassic-Eocene sedimentary succession with crystalline basement flakes)",
              "Plutone terziario": "Tertiary Pluto",
              "Unità del Thurntaler - unità con metamorfismo varisico di medio grado e alpino trascurabile": "Thurntaler unit - Unit with medium-grade Variscan and negligible Alpine metamorphism",
              "Coperture permo-mesozoiche delle varie falde di basamento: Scaglia di Pennes": "Permo-Mesozoic covers of the various basement strata: Pennes flake",
              "Plutone permiano: Ivigna-Bressanone": "Permian Pluton: Ifinger-Brixen / Ivigna-Bressanone",
              "Coperture permo-mesozoiche delle varie falde di basamento: Dolomiti di Lienz": "Permo-Mesozoic covers of the various basement strata: Lienz Dolomites",
              "Coperture permo-mesozoiche delle varie falde di basamento: Scaglia di Kalkstein": "Permo-Mesozoic covers of the various basement strata: Kalkstein flake",
              "Coperture permo-mesozoiche delle varie falde di basamento: S-charl": "Permo-Mesozoic covers of the various basement strata: S-charl",
              "Coperture permo-mesozoiche delle varie falde di basamento: Drauzug": "Permo-Mesozoic covers of the various basement strata: Drauzug",
              "Basamento metamorfico varisico di basso grado: Fillade quarzifera di Bressanone": "Variscan metamorphic basement of low grade: Brixen / Bressanone quartz phyllite",
              "Plutone permiano: Monte Croce": "Permian Pluton: Kreuzberg / Monte Croce",
              "Vulcaniti permiane: Gruppo Vulcanico Atesino": "Permian volcanic rocks: Atesino volcanic group",
              "Falda di Campo - unità con metamorfismo varisico di medio-alto grado ed alpino basso": "Campo stratum - Unit with medium- to high-grade Variscan and low Alpine metamorphism",
              "Scaglia dello Zebrù - unità con metamorfismo varisico di basso grado ed alpino trascurabile": "Zebrù flake - Unit with low-grade Variscan and insignificant Alpine metamorphism",
              "Plutone triassico: Plutone dei Monzoni": "Triassic pluton: Monzoni Pluto",
              "Plutone triassico: Plutone di Predazzo": "Triassic pluton: Predazzo Pluto",
              "Plutone terziario: Tonalite dell'Adamello": "Tertiary Pluto: Adamello Tonalite",
              "Basamento metamorfico varisico di medio grado: Unita del Passo Cavalcafiche": "Variscan metamorphic basement of medium grade: Cavalcafiche Pass unit",
              "Basamento metamorfico varisico di basso grado: Fillade quarzifera di Agordo": "Variscan metamorphic basement of low grade: Agordo quartz phyllite",
              "Successione permo-mesozica delle Alpi Calcaree Settentrionali": "Permo-Mesozoic succession of the Northern Limestone Alps",
              "Unità elvetiche": "Helvetic unit",
              "Molassa alpina": "Alpine Molasse",
              "Successione paleozoica settentrionale": "Northern Paleozoic sequence (graywacke zone)",
              "Fillade di Landeck - unità con metamorfismo varisico di basso grado ed alpino trascurabile": "Landeck phyllade - Unit with low-grade Variscan and insignificant Alpine metamorphism",
              "Unità dell'Ötztal - unità con metamorfismo varisico di medio-alto grado e alpino da medio a basso": "Ötztal unit - Unit with medium- to high-grade Variscan and medium to low Alpine metamorphism",
              "Unità di Tessa - unità con metamorfismo varisico e alpino di alto grado": "Tessa unit - Unit with high-grade Variscan and Alpine metamorphism",
              "Unità di Sesvenna - unità con metamorfismo varisico di medio-alto grado ed alpino trascurabile": "Sesvenna unit - Unit with medium- to high-grade Variscan and insignificant Alpine metamorphism",
              "Scaglia di Marlengo - unità con metamorfismo varisico di medio-alto grado e alpino basso": "Marlinger / Marlengo flake - Unit with medium- to high-grade Variscan and low Alpine metamorphism",
              "Zona di Arosa (flysch cretacico-eocenici)": "Arosa Zone (Cretaceous-Eocene flysch)",
              "Unità tettoniche della Passiria - unità con metamorfismo varisico di medio-alto grado e alpino trascurabile": "Passeier tectonic unit - Unit with medium- to high-grade Variscan and insignificant Alpine metamorphism",
              "Unità dell'Ötztal  (Scaglia dell' Umbrail-Chavalatsch) - unità con metamorfismo varisico di medio-alto grado e alpino da medio a basso": "Ötztal unit (Umbrail-Chavalatsch flake) - Unit with medium- to high-grade Variscan and medium to low Alpine metamorphism",
              "Plutone permiano: Granito di Martello": "Permian Pluto: Marteller Granite / di Martello Granite",
              "Unità di Tures - unità con metamorfismo varisico di medio-alto grado ed alpino da medio ad alto": "Taufers / Tures unit - Unit with medium- to high-grade Variscan and medium to high Alpine metamorphism",
              "vulcaniti e vulcanoclastiti triassiche": "Triassic volcanic rocks and volcanoclastites",
              "Falda del Tonale - unità con metamorfismo varisico di alto grado e alpino trascurabile": "Tonale stratum - Unit with high-grade Variscan and negligible Alpine metamorphism",
              "Permische Vulkanite: Vulcaniti di Tione": "Permian volcanic rocks: Tione volcanic rocks",
              "Plutone permiano: Bressanone": "Permian Pluton: Brixen / Bressanone",
              "Plutone permiano: Ivigna": "Permian Pluton: Ifinger / Ivigna",
              "Zona di taglio della Val Venosta": "Vinschgauer / Venosta Valley shear zone",
              "Coperture permo-mesozoiche delle varie falde di basamento: Falda di Quettervals": "Permo-Mesozoic covers of the various basement strata: Quettervals stratum",
              "Falda del Bernina - unità con metamorfismo varisico di medio-alto grado ed alpino basso": "Bernina stratum - Unit with medium- to high-grade Variscan and low Alpine metamorphism",
              "Falda di Steinach - unità con metamorfismo varisico di basso grado e alpino basso": "Steinach stratum - Unit with low-grade Variscan and low Alpine metamorphism",
              "Plutone permiano: Gabbro di Sondalo": "Permian Pluton: Sondalo Gabbro",
              "Plutone permiano: Gabbro di Tirano": "Permian Pluton: Tirano Gabbro",
              "Plutone permiano: Chiusa": "Permian Pluton: Klausen / Chiusa",
              "Basamento metamorfico varisico di basso grado: Scisti di Edolo": "Variscan metamorphic basement of low grade: Edolo schist",
              "Vulcaniti permiane: Vulcaniti Orobiche": "Permian volcanic rocks: Orobian volcanic rocks",
              "Coperture permo-mesozoiche delle varie falde di basamento: Jaggl": "Permo-Mesozoic covers of the various basement strata: Jaggl",
              "Coperture permo-mesozoiche delle varie falde di basamento: Falda dell'Ortles": "Permo-Mesozoic covers of the various basement strata: Ortler stratum"
              }

    # CREATE LIST OF MAPPED UNITS
    shapefile = gpd.GeoDataFrame.from_file(out_shapefile + '.shp')
    unit_present_in_mask = []
    for unit, poly in zip(shapefile.LEG_IT, shapefile.geometry):
        intersection = mask_geometry.intersection(poly)
        if not intersection.is_empty:
            unit_present_in_mask.append(unit)
    unit_present_in_mask = list(set(unit_present_in_mask)) # Get unique values
    unit_present_in_mask.sort()
    color_levels = [list(leg_it).index(unit) for unit in unit_present_in_mask]
    color_levels.sort()

    # CREATE COLORMAP
    cmap = plt.cm.RdYlBu
    if len(color_levels) > 1:
        norm = plt.cm.colors.BoundaryNorm(color_levels, 256)
    else:
        norm = plt.Normalize(0, len(unit_present_in_mask))

    # DRAW BASEMAP
    bmap = Basemap(epsg = '4149',
                resolution = 'l',
                llcrnrlon = minx, #ulx,
                llcrnrlat = miny, #lry,
                urcrnrlon = maxx, #lrx,
                urcrnrlat = maxy) #uly)
    print(ulx, lry, lrx, uly)
    bmap.imshow(masked_hillshade, cmap='gray', origin='upper')
    bmap.drawmeridians(np.arange(5, 15, 0.10), labels=[0,0,0,1], linewidth=0.5) # Here to change the annotation's places
    bmap.drawparallels(np.arange(40, 50, 0.10), labels=[1,0,0,0], linewidth=0.5)
    cs = bmap.contour(xi, yi, masked_dem, linewidths=0.4, colors='gray')
    ax.clabel(cs, cs.levels, inline=True, fontsize=5)

    # ADD GEOLOGICAL POLYGONS!
    from descartes import PolygonPatch  # https://pypi.org/project/descartes/

    # WARNING: This library had to be updated BY HAND
    # https://stackoverflow.com/questions/75287534/indexerror-descartes-polygonpatch-wtih-shapely
    # https://stackoverflow.com/questions/21824157/how-to-extract-interior-polygon-coordinates-using-shapely
    shapefile = gpd.GeoDataFrame.from_file(out_shapefile + '.shp')
    patches = []
    for unit, poly in zip(shapefile.LEG_IT, shapefile.geometry):
        color = cmap(norm(list(leg_it).index(unit)))
        if poly.geom_type == 'Polygon':
            mpoly = shapely.ops.transform(bmap, poly)
            patches.append(PolygonPatch(mpoly, color=color, alpha=0.5))
        elif poly.geom_type == 'MultiPolygon':
            for subpoly in poly:
                mpoly = shapely.ops.transform(bmap, poly)
                patches.append(PolygonPatch(mpoly, color=color, alpha=0.5))
        else:
            print("'poly' is neither a polygon nor a multi-polygon. Skipping it.")
    pc = PatchCollection(patches, match_original=True, edgecolor='k', linewidths=0.5)
    ax.add_collection(pc)

    # CLIP POLYGONS TO WATERSHED EXTENT
    for contour in ax.collections:
        contour.set_clip_path(clip)

    # CREATE COLORMAP
    shapefile = gpd.GeoDataFrame.from_file(cascade_network)
    cmap2 = plt.cm.BuGn
    norm2 = plt.Normalize(min(shapefile.Slope), max(shapefile.Slope))

    # ADD THE CASCADE NETWORK
    patches = []
    for slope, poly in zip(shapefile.Slope, shapefile.geometry):
        color = cmap2(norm2(slope))
        if poly.geom_type == 'LineString':
            mline = shapely.ops.transform(bmap, poly)
            plt.plot(mline.xy[0], mline.xy[1], color=color, alpha=1, linewidth=1)
        else:
            print("'poly' is not a LineString. Skipping it.")
    for slope, poly in zip(shapefile.Slope, shapefile.geometry):
        if poly.geom_type == 'LineString':
            mline = shapely.ops.transform(bmap, poly)
            plt.plot(mline.xy[0][-1], mline.xy[1][-1], '.', markersize = 2, color='k', alpha=1, linewidth=1)
        else:
            print("'poly' is not a LineString. Skipping it.")

    # ADDING A GEOLOGICAL LEGEND
    handles = []
    for unit in unit_present_in_mask:
        handles.append(Polygon([(0,0),(10,0),(0,-10)], color=cmap(norm(list(leg_it).index(unit))),
                               label=leg_it[unit], alpha=0.5))
    legend = plt.legend(handles=handles, loc='upper left', bbox_to_anchor=(0, -0.05), ncol=1, fontsize="6")
    plt.subplots_adjust(right=1)

    # ADD COLORBAR FOR REACH COLOR
    cax = fig.add_axes([1.02, 0.2, 0.02, 0.6])
    cb = ColorbarBase(cax, cmap=cmap2, norm=norm2, spacing='proportional')
    cb.set_label('Reach slope')


    # ADD MAP SCALE -> Not possible in this CRS
    #bmap.drawmapscale(ulx, uly, ulx, uly, 5, fontsize = 14)

    ax.set_title(title)
    plt.savefig(output_filename + '.svg', format="svg", bbox_inches='tight', dpi=300)
    plt.savefig(output_filename + '.png', format="png", bbox_inches='tight', dpi=300)
    plt.show()

def plot_orthophoto(tif_filename, mask, output_filename, year, target_crs, source_crs):
    print('Plot orthophoto')

    # INITIALIZE FIGURE
    fig, ax = plt.subplots(figsize=(20,20))
    ax.set_aspect(1)

    # OPEN WATERSHED SHAPEFILE, TRANSFORM IT TO A PATH FOR POLYGON CLIPPING AND GET ITS EXTENT FOR DEM CROPPING.
    sf = shp.Reader(mask)
    for shape_rec in sf.shapeRecords():
        vertices = []
        codes = []
        pts = shape_rec.shape.points
        prt = list(shape_rec.shape.parts) + [len(pts)]
        for i in range(len(prt) - 1):
            for j in range(prt[i], prt[i+1]):
                vertices.append((pts[j][0], pts[j][1]))
            codes += [Path.MOVETO]
            codes += [Path.LINETO] * (prt[i+1] - prt[i] -2)
            codes += [Path.CLOSEPOLY]
        clip = Path(vertices, codes)
        clip = PathPatch(clip, transform=ax.transData)
        unzipped = list(zip(*pts))
        minx = min(list(unzipped[0]))
        maxx = max(list(unzipped[0]))
        miny = min(list(unzipped[1]))
        maxy = max(list(unzipped[1]))
        print(minx, miny, maxx, maxy)

    # OPEN TIF AND CROP IT TO MAX EXTENT
    print('EPSG:' + target_crs)
    print(tif_filename + '_EPSG' + target_crs + '.tif')

    gdal.Warp(tif_filename + '_EPSG' + target_crs + '.tif', tif_filename, dstSRS='EPSG:' + target_crs, srcSRS='EPSG:' + source_crs)
    dem = gdal.Open(tif_filename + '_EPSG' + target_crs + '.tif')
    print(dem)
    #dem = gdal.Open(tif_filename)
    ulx, xres, _, uly, _, yres  = dem.GetGeoTransform()
    lrx = ulx + (dem.RasterXSize * xres)
    lry = uly + (dem.RasterYSize * yres)
    print('Extent of DEM before cropping:', lrx, lry, ulx, uly)
    dem = gdal.Translate(tif_filename + 'cropped.tif', dem, projWin = [minx, maxy, maxx, miny])
    ulx, xres, _, uly, _, yres  = dem.GetGeoTransform()
    lrx = ulx + (dem.RasterXSize * xres)
    lry = uly + (dem.RasterYSize * yres)
    print('Extent of DEM after cropping:', lrx, lry, ulx, uly)
    print(tif_filename + 'cropped.tif')
    rgb = dem.ReadAsArray()
    rgb = rgb.transpose(1,2,0)
    raster_x_size = dem.RasterXSize
    raster_y_size = dem.RasterYSize
    dem = None

    # MASK HILLSHADE WITH WATERSHED SHAPEFILE
    gpd_mask = gpd.read_file(mask)
    xi = np.linspace(ulx, lrx, raster_x_size)
    yi = np.linspace(uly, lry, raster_y_size)
    xi, yi = np.meshgrid(xi, yi)
    mask_geometry = gpd_mask.dissolve().geometry.item()
    shp_mask = shapely.vectorized.contains(mask_geometry, xi, yi)
    shp_mask3D = np.repeat(shp_mask[:, :, np.newaxis], 3, axis=2)
    masked_orthophoto = np.where(shp_mask3D, rgb, 255)

    # DRAW BASEMAP
    bmap = Basemap(epsg = '4149',
                resolution = 'l',
                llcrnrlon = minx, #ulx,
                llcrnrlat = miny, #lry,
                urcrnrlon = maxx, #lrx,
                urcrnrlat = maxy) #uly)
    print(ulx, lry, lrx, uly)
    bmap.imshow(masked_orthophoto, origin='upper')
    bmap.drawmeridians(np.arange(5, 15, 0.05), labels=[0,0,0,1], linewidth=0.5)
    bmap.drawparallels(np.arange(40, 50, 0.05), labels=[1,0,0,0], linewidth=0.5)

    ax.set_title('Orthophoto ' + year, fontsize=20)
    plt.savefig(output_filename + '.svg', format="svg", bbox_inches='tight', dpi=300)
    plt.savefig(output_filename + '.png', format="png", bbox_inches='tight', dpi=300)
    plt.show()

def example_DEM_plot(tif):
    # -- Optional dx and dy for accurate vertical exaggeration --------------------
    # If you need topographically accurate vertical exaggeration, or you don't want
    # to guess at what *vert_exag* should be, you'll need to specify the cellsize
    # of the grid (i.e. the *dx* and *dy* parameters).  Otherwise, any *vert_exag*
    # value you specify will be relative to the grid spacing of your input data
    # (in other words, *dx* and *dy* default to 1.0, and *vert_exag* is calculated
    # relative to those parameters).  Similarly, *dx* and *dy* are assumed to be in
    # the same units as your input z-values.  Therefore, we'll need to convert the
    # given dx and dy from decimal degrees to meters.
    raster = gdal.Open(tif)

    # Get the first band.
    band = raster.GetRasterBand(1)
    # We need to nodata value for our MaskedArray later.
    nodata = band.GetNoDataValue()
    # Load the entire dataset into one numpy array.
    z = band.ReadAsArray(0, 0, band.XSize, band.YSize)

    gt = raster.GetGeoTransform()
    dx = gt[1]
    dy = -gt[5]
    ymin = gt[3]
    # Close the dataset.
    raster = None

    dy = 111200 * dy
    dx = 111200 * dx * np.cos(np.radians(ymin))
    # -----------------------------------------------------------------------------

    # Shade from the northwest, with the sun 45 degrees from horizontal
    ls = LightSource(azdeg=315, altdeg=45)
    cmap = plt.cm.gist_earth

    fig, axs = plt.subplots(nrows=4, ncols=3, figsize=(8, 9))
    plt.setp(axs.flat, xticks=[], yticks=[])

    # Vary vertical exaggeration and blend mode and plot all combinations
    for col, ve in zip(axs.T, [0.1, 1, 10]):
        # Show the hillshade intensity image in the first row
        col[0].imshow(ls.hillshade(z, vert_exag=ve, dx=dx, dy=dy), cmap='gray')

        # Place hillshaded plots with different blend modes in the rest of the rows
        for ax, mode in zip(col[1:], ['hsv', 'overlay', 'soft']):
            rgb = ls.shade(z, cmap=cmap, blend_mode=mode,
                           vert_exag=ve, dx=dx, dy=dy)
            ax.imshow(rgb)

    # Label rows and columns
    for ax, ve in zip(axs[0], [0.1, 1, 10]):
        ax.set_title(f'{ve}', size=18)
    for ax, mode in zip(axs[:, 0], ['Hillshade', 'hsv', 'overlay', 'soft']):
        ax.set_ylabel(mode, size=18)

    # Group labels...
    axs[0, 1].annotate('Vertical Exaggeration', (0.5, 1), xytext=(0, 30),
                       textcoords='offset points', xycoords='axes fraction',
                       ha='center', va='bottom', size=20)
    axs[2, 0].annotate('Blend Mode', (0, 0.5), xytext=(-30, 0),
                       textcoords='offset points', xycoords='axes fraction',
                       ha='right', va='center', size=20, rotation=90)
    fig.subplots_adjust(bottom=0.05, right=0.95)

    plt.show()

def pixel2coord(col, row, gt):
    """Returns global coordinates to pixel center using base-0 raster index"""
    xp = gt[1] * col + gt[2] * row + gt[1] * 0.5 + gt[2] * 0.5 + gt[0]
    yp = gt[4] * col + gt[5] * row + gt[4] * 0.5 + gt[5] * 0.5 + gt[3]
    return(xp, yp)

def plot_altitude_plots(elevations, *argv):

    elev = np.loadtxt(elevations, delimiter=',', skiprows=1)

    figs, axs = plt.subplots(nrows=1, ncols=len(argv)//3+1, figsize=(10, 4))
    col0 = axs.T[0]
    col0.plot(elev[:,2], elev[:,1], '-o')
    col0.set_xlabel('Area (m²?)')
    col0.set_ylabel('Altitude (m)')
    for i, col in enumerate(axs.T[1:]):
        arg1 = argv[i*3]
        arg2 = argv[i*3+1]
        arg3 = argv[i*3+2]
        f = open(arg1)
        header = f.readline()
        data = np.loadtxt(arg2, delimiter=',')
        col.plot(data[0], elev[:,1], '-o')
        data = np.loadtxt(arg1, delimiter=',')
        col.plot(data[0], elev[:,1], '-o') #, label=header
        col.set_xlabel(arg3)
        col.set_ylabel('Altitude (m)')
    figs.suptitle("Altitudinal input for Hydrobricks")
    figs.tight_layout()
    plt.show()

def plot_simulated_discharge(time_file, data_file, label):

    data = np.loadtxt(data_file, delimiter=',')
    time = range(len(data))

    plt.figure()
    plt.plot(time, data)
    plt.xlabel('Time (day)')
    plt.ylabel(label)

    plt.title("Hydrobricks output")
    plt.tight_layout()
    plt.show()

def plot_netcdf(filename, start_datetime, day, title, label, varname, width=500000, height=350000, data_type=1):

    nc = NetCDFFile(filename)

    # Read the variables from the netCDF file and assign them to Python variables
    if data_type == 1:
        lat = nc.variables['lat'][:]
        lon = nc.variables['lon'][:]
    elif data_type == 2:
        lat = nc.variables['y'][:]
        lon = nc.variables['x'][:]
    time = nc.variables['time'][:]
    var = nc.variables[varname][:]
    if data_type == 1:
        date = start_datetime + timedelta(days = time[day])
    elif data_type == 2:
        date = time[day]

    # Get some parameters for the Stereographic Projection
    lon_0 = lon.mean()
    lat_0 = lat.mean()

    ## Let's create a map centered over the Alps (45N,10E) and extending 20 degrees W-E and 10 degrees N-S
    #bmap = Basemap(projection='merc',llcrnrlon=5.,llcrnrlat=43.,urcrnrlon=15.,urcrnrlat=48.,resolution='i') # projection, lat/lon extents and resolution of polygons to draw
    ## resolutions: c - crude, l - low, i - intermediate, h - high, f - full

    ## Now, let's prepare the data for the map.
    ## We have to transform the lat/lon data to map coordinates.
    #lons,lats= np.meshgrid(lon-180,lat) # for this dataset, longitude is 0 through 360, so you need to subtract 180 to properly display on map
    bmap = Basemap(width=width,height=height, resolution='l',projection='stere', lat_ts=40, lat_0=lat_0, lon_0=lon_0)

    # Because our lon and lat variables are 1D, use meshgrid to create 2D arrays
    # Not necessary if coordinates are already in 2D arrays.
    lon, lat = np.meshgrid(lon, lat)
    x, y = bmap(lon, lat)

    # If you'd like to add Lat/Lon, you can do that too:
    parallels = np.arange(30,50,5.) # make latitude lines ever 5 degrees from 30N-50N
    meridians = np.arange(-95,-70,5.) # make longitude lines every 5 degrees from 95W to 70W
    bmap.drawparallels(parallels,labels=[1,0,0,0],fontsize=10)
    bmap.drawmeridians(meridians,labels=[0,0,0,1],fontsize=10)

    # Plot the netCDF data on the map.
    plt.figure(1)
    bmap.drawcoastlines()
    bmap.drawstates()
    bmap.drawcountries()
    bmap.drawlsmask(land_color='Linen', ocean_color='#CCFFFF')
    bmap.drawcounties()
    clevs = np.arange(np.min(var[day,:,:]), np.max(var[day,:,:]), 4)
    cs = bmap.contour(x, y, var[day,:,:], clevs, colors='blue', linewidths=1., linestyles='solid')
    plt.clabel(cs, fontsize=9, inline=1) # contour labels
    plt.title(f'{title} on {date}')

    # Plot the netCDF data on the map as contours.
    plt.figure(2)
    bmap.drawcoastlines()
    bmap.drawstates()
    bmap.drawcountries()
    bmap.drawlsmask(land_color='Linen', ocean_color='#CCFFFF')
    bmap.drawcounties()
    temp = bmap.contourf(x, y, var[day,:,:])
    cb = bmap.colorbar(temp,"bottom", size="5%", pad="2%")
    plt.title(f'{title} on {date}')
    cb.set_label(label)

    # note if you want to create plots in an automated script (aka without X-Window at all), add the following to the very top of your script:
    # import matplotlib matplotlib.use('Agg')
    # This will allow matplotlib to use the Agg backend instead of Qt, and will create plots in a batch format, rather than interactively.

    plt.show()

def plot_netcdf_with_background_DEM(tif_filename, filename, start_datetime, day, title, varname='Hargreaves ETO', color_label='', cmap=plt.cm.Blues, data_type=1):
    dem = gdal.Open(tif_filename)
    ulx, xres, _, uly, _, yres  = dem.GetGeoTransform()
    lrx = ulx + (dem.RasterXSize * xres)
    lry = uly + (dem.RasterYSize * yres)

    hillshade = gdal.DEMProcessing('hillshade.tif', dem, 'hillshade',
                                   computeEdges = True,
                                   multiDirectional = True, zFactor=2)
    hillshade_array = hillshade.GetRasterBand(1).ReadAsArray()

    if filename.lower().endswith('.tif'):
        tif = gdal.Open(filename)
        band = tif.GetRasterBand(1)
        nodata = band.GetNoDataValue()
        elev_array = band.ReadAsArray(0, 0, band.XSize, band.YSize)
        elev_array[elev_array == nodata] = 0
    elif filename.lower().endswith('.nc'):
        nc = NetCDFFile(filename)
        var = nc.variables[varname][:]
        elev_array = var[day,:,:]
        time = nc.variables['time'][:]
        if data_type == 1:
            date = start_datetime + timedelta(days = np.float64(time[day]))
        elif data_type == 2:
            date = time[day]

    plt.figure(figsize=(15,40))
    bmap = Basemap(epsg = '4149',
                resolution = 'l',
                llcrnrlon = ulx,
                llcrnrlat = lry,
                urcrnrlon = lrx,
                urcrnrlat = uly)
    bmap.imshow(hillshade_array, cmap='gray', origin='upper')

    bmap.imshow(elev_array, cmap=cmap, alpha=0.43, origin='upper')
    bmap.drawcoastlines(color='black', linewidth=2)
    bmap.drawmeridians(np.arange(5, 15, 0.10), labels=[0,0,0,1], linewidth=0.5)
    bmap.drawparallels(np.arange(40, 50, 0.10), labels=[1,0,0,0], linewidth=0.5)
    cbar= plt.colorbar()
    cbar.ax.set_ylabel(color_label, labelpad=15, rotation=270)
    if filename.lower().endswith('.tif'):
        plt.title(f'{title}')
    elif filename.lower().endswith('.nc'):
        plt.title(f'{title} on {date}')
    plt.show()

def classify_tif_according_to_elevation_bands(filename, outfile, elevation_thres):
    tif = xa.open_dataarray(filename)
    elev_array = tif.to_numpy()[0,:,:]
    ys = tif['y'].to_numpy()
    xs = tif['x'].to_numpy()

    elev_thres = np.loadtxt(elevation_thres, skiprows=1)
    for i, min_thr in enumerate(elev_thres[:-1]):
        elev_array[np.bitwise_and(elev_array > min_thr, elev_array < elev_thres[i+1])] = i
    ds = xa.DataArray(elev_array,
                      name='elev_classified',
                      dims=['y', 'x'],
                      coords={
                          "x": xs,
                          "y": ys})
    #ds.rio.write_crs(CRS, inplace=True)
    ds.rio.to_raster(outfile)
    ds.close()


path = '/home/anne-laure/Documents/Datasets/'
results = f'{path}Outputs/'
it_study_area = '/home/anne-laure/Documents/Datasets/Italy_Study_area/'

###########################################################################################################################

time = 'Weird time'
plot_simulated_discharge(time, f"{results}simulated_discharge.csv", 'Simulated discharge (mm/day?)')

###########################################################################################################################

path1 = "/home/anne-laure/eclipse-workspace/eclipse-workspace/GSM-SOCONT/tests/files/catchments/Val_d_Anniviers/"
elev_bands = f"{path1}elevation_bands.csv"
plot_altitude_plots(elev_bands, f"{results}spatialized_pet.csv", f"{results}spatialized_pet_interp.csv", 'Modeled potential evapotranspiration (mm/day)',
                    f"{results}spatialized_temperature.csv", f"{results}spatialized_temperature_interp.csv", 'Temperature (°C)',
                    f"{results}spatialized_precipitation.csv", f"{results}spatialized_precipitation_interp.csv", 'Precipitation (mm/day)')
plot_altitude_plots(elev_bands, f"{results}spatialized_pet_HR.csv", f"{results}spatialized_pet_interp_HR.csv", 'Modeled potential evapotranspiration HR (mm/day)',
                    f"{results}spatialized_temperature_HR.csv", f"{results}spatialized_temperature_interp_HR.csv", 'Temperature HR (°C)',
                    f"{results}spatialized_precipitation_HR.csv", f"{results}spatialized_precipitation_interp_HR.csv", 'Precipitation HR (mm/day)')

###########################################################################################################################

plot_Alps(f'{path}OutputFigures/CatchmentLocalisationInTheAlps.png')

###########################################################################################################################

main_path = '/home/anne-laure/Documents/Datasets/Italy_maps/bormio/Bormio/'
tif = f'{it_study_area}dtm_2pt5m_utm_st_whole_StudyArea_4326FromOriginalCRS.tif'
limits = f'{main_path}limiti_10k_foglio_024_bormio'
polygons = f'{main_path}poligeo_10k_foglio_024_bormio'
mask = f'{it_study_area}OutletSolda_WatershedAbovePonteDiStelvio_4326FromOriginalCRS.shp'
plot_geology(tif, polygons, mask, target_crs='4326')

main_path = '/home/anne-laure/Documents/Datasets/Italy_maps/Geokatalog.buergernetz.bz.it/'
tif = f'{it_study_area}dtm_2pt5m_utm_st_whole_StudyArea_4326FromOriginalCRS.tif'
limits = f'{main_path}FaultsOverview_line/DownloadService/FaultsOverview_line'
polygons = f'{main_path}GeologicalUnitsOverview_polygon/DownloadService/GeologicalUnitsOverview_polygon'

mask = f'{it_study_area}OutletSolda_WatershedAbovePonteDiStelvio_4326FromOriginalCRS.shp'
output_filename = f'{path}OutputFigures/Solda_GlobalGeologicalMap_1km2'
cascade_network = f'{path}Italy_maps/CASCADE_Networks/RN_Solda_1km2_1km_georef_EPSG4326.shp'
plot_global_geology(tif, polygons, mask, output_filename, cascade_network, title='Reaches with 1km² of area, max 1km of reach length', target_crs='4326')
output_filename = f'{path}OutputFigures/Solda_GlobalGeologicalMap_2km2'
cascade_network = f'{path}Italy_maps/CASCADE_Networks/RN_Solda_2km2_1km_georef_EPSG4326.shp'
plot_global_geology(tif, polygons, mask, output_filename, cascade_network, title='Reaches with 2km² of area, max 1km of reach length', target_crs='4326')
output_filename = f'{path}OutputFigures/Solda_GlobalGeologicalMap_5km2'
cascade_network = f'{path}Italy_maps/CASCADE_Networks/RN_Solda_5km2_1km_georef_EPSG4326.shp'
plot_global_geology(tif, polygons, mask, output_filename, cascade_network, title='Reaches with 5km² of area, max 1km of reach length', target_crs='4326')
mask = f'{it_study_area}OutletMazia_WatershedSameAsComiti2019_4326FromOriginalCRS.shp'
output_filename = f'{path}OutputFigures/Mazia_GlobalGeologicalMap'
plot_global_geology(tif, polygons, mask, output_filename, cascade_network, title=None, target_crs='4326')

###########################################################################################################################

input_data = "CH2018" # Choose between "CH2018", "CrespiEtAl", "MeteoSwiss"
day = -1
start_datetime = datetime(1900,1,1,0,0,0)
#####
tif = f"{path1}dem_EPSG4149_reproj.tif"
filename = f'{results}Swiss_reproj.tif'
outfile =  f'{results}Swiss_reproj_classified.tif'
elev_thres = f"{path1}elevation_thresholds.csv"
classify_tif_according_to_elevation_bands(filename, outfile, elev_thres)
print(plt.cm.tab20.colors[:9])
cmap = plt.cm.get_cmap('tab20c', 7) #plt.cm.tab20.colors[:9]
plot_netcdf_with_background_DEM(tif, outfile, start_datetime, day, title='Reprojected elevation', varname='', color_label='Elevation band N°', cmap=cmap)
####
tif = f"{path1}dem_EPSG4149_reproj.tif"
filename = f'{results}Swiss_reproj.tif'
plot_netcdf_with_background_DEM(tif, filename, start_datetime, day, title='Reprojected elevation', varname='', color_label='Elevation')
####
tif = f"{path1}dem_EPSG4149_reproj.tif"
filename = f'{results}CH2018_tas_TestOut.nc'
plot_netcdf_with_background_DEM(tif, filename, start_datetime, day, title='Mean temperature (reprojected)', varname='tas', color_label='Mean temperature')
#####
tif = f"{path1}dem_EPSG4149_reproj.tif"
filename = f'{results}CH2018_pet_TestOut.nc'
plot_netcdf_with_background_DEM(tif, filename, start_datetime, day, title='CH2018 ETO Hargreaves', varname='pet', color_label='PET (Hargreaves ETO)')



tif = f"{path1}dem_EPSG4149.tif"
example_DEM_plot(tif)



filename = f'{path}Swiss_Future_CH2018/CH2018_tasmin_SMHI-RCA_ECEARTH_EUR11_RCP26_QMgrid_1981-2099.nc'
start_datetime = datetime(1900,1,1,0,0,0)
day = -1
title = 'CH2018 Min Temperature'
label = 'Daily minimum temperature (°C)'
plot_netcdf(filename, start_datetime, day, title, label, varname='tasmin')

###########################################################################################################################

mask = f'{it_study_area}OutletMazia_WatershedSameAsComiti2019_4326FromOriginalCRS.shp'
# gdalwarp */DownloadService/Aerial-2020-RGB.tif Mosaik_2020.tif
tif = f'{it_study_area}2014-2020/Mazia/Mosaik_2015_upscaled1m.tif'
# The TIF was too heavy, so I upscaled it to 1 m. AM I USING THE RIGHT METHOD?
# gdalwarp -tr 1 1 Mosaik_2020.tif Mosaik_2020_upscaled1m.tif -r cubic -overwrite
output_filename = f'{path}OutputFigures/Mazia_Orthophoto2015'
plot_orthophoto(tif, mask, output_filename, year='2015', target_crs='4326', source_crs='25832')
tif = f'{it_study_area}2014-2020/Mazia/Mosaik_2017_upscaled1m.tif'
# The TIF was too heavy, so I upscaled it to 1 m. AM I USING THE RIGHT METHOD?
# gdalwarp -tr 1 1 Mosaik_2020.tif Mosaik_2020_upscaled1m.tif -r cubic -overwrite
output_filename = f'{path}OutputFigures/Mazia_Orthophoto2017'
plot_orthophoto(tif, mask, output_filename, year='2017', target_crs='4326', source_crs='25832')
tif = f'{it_study_area}2014-2020/Mazia/Mosaik_2020_upscaled1m.tif'
# The TIF was too heavy, so I upscaled it to 1 m. AM I USING THE RIGHT METHOD?
# gdalwarp -tr 1 1 Mosaik_2020.tif Mosaik_2020_upscaled1m.tif -r cubic -overwrite
output_filename = f'{path}OutputFigures/Mazia_Orthophoto2020'
plot_orthophoto(tif, mask, output_filename, year='2020', target_crs='4326', source_crs='25832')

mask = f'{it_study_area}OutletSolda_WatershedAbovePonteDiStelvio_4326FromOriginalCRS.shp'
tif = f'{it_study_area}2014-2020/Solda/Mosaik_2020_upscaled1m.tif'
# The TIF was too heavy, so I upscaled it to 1 m. AM I USING THE RIGHT METHOD?
# gdalwarp -tr 1 1 Mosaik_2020.tif Mosaik_2020_upscaled1m.tif -r cubic -overwrite
output_filename = f'{path}OutputFigures/Solda_Orthophoto2020'
plot_orthophoto(tif, mask, output_filename, year='2020', target_crs='4326', source_crs='25832')
tif = f'{it_study_area}2014-2020/Solda/Mosaik_2017_upscaled1m.tif'
# The TIF was too heavy, so I upscaled it to 1 m. AM I USING THE RIGHT METHOD?
# gdalwarp -tr 1 1 Mosaik_2020.tif Mosaik_2020_upscaled1m.tif -r cubic -overwrite
output_filename = f'{path}OutputFigures/Solda_Orthophoto2017'
plot_orthophoto(tif, mask, output_filename, year='2017', target_crs='4326', source_crs='25832')
tif = f'{it_study_area}2014-2020/Solda/Mosaik_2017_upscaled1m.tif'
# The TIF was too heavy, so I upscaled it to 1 m. AM I USING THE RIGHT METHOD?
# gdalwarp -tr 1 1 Mosaik_2020.tif Mosaik_2020_upscaled1m.tif -r cubic -overwrite
output_filename = f'{path}OutputFigures/Solda_Orthophoto2017'
plot_orthophoto(tif, mask, output_filename, year='2017', target_crs='4326', source_crs='25832')
tif = f'{it_study_area}2014-2020/Solda/Mosaik_2015_upscaled1m.tif'
# The TIF was too heavy, so I upscaled it to 1 m. AM I USING THE RIGHT METHOD?
# gdalwarp -tr 1 1 Mosaik_2020.tif Mosaik_2020_upscaled1m.tif -r cubic -overwrite
output_filename = f'{path}OutputFigures/Solda_Orthophoto2015'
plot_orthophoto(tif, mask, output_filename, year='2015', target_crs='4326', source_crs='25832')

