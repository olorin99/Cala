#!/bin/bash

glslangValidator -V -o triangle.vert.spv triangle.vert
glslangValidator -V -o triangle.frag.spv triangle.frag

glslangValidator -V -o default.vert.spv default.vert
glslangValidator -V -o default.frag.spv default.frag

glslangValidator -V -o gbuffer.frag.spv gbuffer.frag

glslangValidator -V -o deferred.vert.spv deferred.vert
glslangValidator -V -o deferred.frag.spv deferred.frag

glslangValidator -V -o default.comp.spv default.comp

glslangValidator -V -o phong.frag.spv phong.frag
glslangValidator -V -o blinn_phong.frag.spv blinn_phong.frag
