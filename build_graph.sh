#!/bin/bash

if [ "$#" -lt 3 ]; then
  echo "Usage: $0 <build_data> <row> <dimension> [mobius_pow]" >&2
  echo "For example: $0 base.txt 1000 128 1.7" >&2
  exit 1
fi

if [ "$#" -eq 4 ]; then
	pow=$5
fi

$(dirname $0)/mobius build $1 0 0 $2 $3 0 $4 ${pow}

