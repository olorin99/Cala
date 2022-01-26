#!/bin/bash

glslangValidator -H -V -o triangle.vert.spv triangle.vert
glslangValidator -H -V -o triangle.frag.spv triangle.frag