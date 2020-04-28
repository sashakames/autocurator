# autocurator
A tool for producing metadata describing a climate dataset
## Prerequisites

conda installation

create a conda environment with libnetcdf.
c++ compiler, eg. gcc/g++

## build / test steps


```
conda create -n autocur -c conda-forge libnetcdf
source activate autocur
export GXX=G++
make
export LD_LIBRARY_PATH=$CONDA_PREFIX/lib
./bin/autocurator
```

