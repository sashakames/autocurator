# autocurator
A tool for producing metadata describing a climate dataset
## Prerequisites

conda installation

create a conda environment with libnetcdf.

## build / test steps


```
conda create -n autocur -c conda-forge libnetcdf gcc
source activate autocur
make
export LD_LIBRARY_PATH=$CONDA_PREFIX/lib
./bin/autocurator
```

