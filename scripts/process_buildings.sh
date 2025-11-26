!/bin/sh -x


pip install pyshp
pip install numpy
pip install matplotlib



#unzip to common directories and copy where it belongs
WDIR=$PWD
TDIR=$PWD


for file in *.pbf
do
    mv ./$file ./latest.osm.pbf
done


# convert to intermiddiate format
ogr2ogr -f "ESRI Shapefile" "./buildings.shp" "./latest.osm.pbf" -sql "SELECT * from multipolygons WHERE other_tags LIKE '%building%' OR building != 'NULL'" -lco ENCODING=UTF-8
		
				
# run python scripts
echo "Phython processing"
python ./process_buildings.py
cp buildings.prj buildings_levels.prj



exit
