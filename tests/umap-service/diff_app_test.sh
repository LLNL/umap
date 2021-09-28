#!/bin/bash
if [ "$#" -ne 2 ]; then
    echo "Illegal number of parameters"
    exit
fi

if [[ -z "${UMAP_ROOT}" ]]; then
  echo "UMAP_ROOT needs to be set"
  exit
else
  install_dir="${UMAP_ROOT}/install/bin"
fi

${install_dir}/umap-server > server_out &
sleep 2
${install_dir}/diff_mpumap_app -f $1 -s $2 > diff_out
pkill -f umap-server
