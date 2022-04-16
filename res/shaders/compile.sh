#!/bin/bash

for filename in *; do
    if [[ $filename == *.spv ]] || [[ $filename == *.sh ]]
    then
        continue
    fi
#    y=${filename%.*}
#    echo ${y##*/}

    output="${filename}.spv"

    glslangValidator -V -o $output $filename
done
