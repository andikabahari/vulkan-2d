@echo off
setlocal

call glslc -fshader-stage=vertex   .\sample.vert.glsl -o .\sample.vert.spv
call glslc -fshader-stage=fragment .\sample.frag.glsl -o .\sample.frag.spv

echo Shaders compiled.

endlocal
