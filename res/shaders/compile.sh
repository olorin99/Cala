#!/bin/bash

glslangValidator -H -V -o default.vert.spv default.vert
glslangValidator -H -V -o default.frag.spv default.frag

glslangValidator -H -V -o gbuffer.frag.spv gbuffer.frag

glslangValidator -H -V -o deferred.vert.spv deferred.vert
glslangValidator -H -V -o deferred.frag.spv deferred.frag

glslangValidator -H -V -o default.comp.spv default.comp
