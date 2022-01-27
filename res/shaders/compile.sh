#!/bin/bash

glslangValidator -H -V -o default.vert.spv default.vert
glslangValidator -H -V -o default.frag.spv default.frag