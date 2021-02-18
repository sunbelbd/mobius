#!/bin/bash

if [ "$#" -lt 4 ]; then
  echo "Usage: $0 <query_data> <built_graph_row> <built_graph_dimension> <search_budget> [display top k]" >&2
  echo "For example: $0 test.txt 1000 128 100" >&2
  echo "Use display top 5: $0 test.txt 1000 128 100 5" >&2
  exit 1
fi

search=$4
display=100

if [ "$#" -ge 5 ]; then
  display=$5
fi

$(dirname $0)/mobius test 0 $1 ${search} $2 $3 ${display} $4

