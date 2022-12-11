#!/usr/bin/bash

export OMP_NUM_THREADS=1
export MKL_NUM_THREADS=1
export NUMEXPR_NUM_THREADS=1
export OPENBLAS_NUM_THREADS=1
export VECLIB_MAXIMUM_THREADS=1

if [ -z "$AGNOS_VERSION" ]; then
  export AGNOS_VERSION="6.2"
fi

if [ -z "$PASSIVE" ]; then
  export PASSIVE="1"
fi

export STAGING_ROOT="/data/safe_staging"
export MAPBOX_TOKEN="pk.eyJ1IjoiYWxlc2F0byIsImEiOiJjbGJpa3RhMTYwOGc0M3dueGY1a3pmZHA1In0.ZXeXJnMyLfOJx0ZrkFSCNQ"

