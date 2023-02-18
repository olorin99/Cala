#!/bin/bash

function compile_folder() {
  cd "$1"
  for filename in *; do
    if [ -d "$filename" ]; then
      compile_folder "$filename"
      continue
    fi

    if [[ $filename == *.spv ]] || [[ $filename == *.sh ]] || [[ $filename == *.glsl ]]
    then
      continue
    fi

  #    y=${filename%.*}
  #    echo ${y##*/}

    output="${filename}.spv"

    echo "glslangValidator -V -o $output $filename"

    glslangValidator -V -o $output $filename
  done
  cd ..
}

compile_folder "."