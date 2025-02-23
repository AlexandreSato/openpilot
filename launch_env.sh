#!/usr/bin/env bash

export OMP_NUM_THREADS=1
export MKL_NUM_THREADS=1
export NUMEXPR_NUM_THREADS=1
export OPENBLAS_NUM_THREADS=1
export VECLIB_MAXIMUM_THREADS=1

if [ -z "$AGNOS_VERSION" ]; then
  export AGNOS_VERSION="11.8"
fi

export STAGING_ROOT="/data/safe_staging"

# SatoPilot require flask
if [ -f /AGNOS ]; then
  echo -en "1" > /data/params/d/AleSato_SecondBoot
  pip install flask
fi
