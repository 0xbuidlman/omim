set -e -u -x
MY_PATH=`echo $0 | grep -o '.*/'`
GENERATOR_TOOL=$MY_PATH../../../omim-build-$1/out/$1/generator_tool

TMPDIR=../../../omim-indexer-tmp/

if ! [ -d $TMPDIR ]
then 
    mkdir $TMPDIR
fi

PV="cat"
if [ `which pv` ]
then
  PV=pv
fi

$PV ../../../omim-maps/$2.osm.bz2 | bzip2 -d | $GENERATOR_TOOL --preprocess_xml=true --use_light_nodes=true --intermediate_data_path=$TMPDIR

$PV ../../../omim-maps/$2.osm.bz2 | bzip2 -d | $GENERATOR_TOOL --use_light_nodes=true --generate_features=true --generate_geometry=true --generate_index=true --intermediate_data_path=$TMPDIR --output=$2
