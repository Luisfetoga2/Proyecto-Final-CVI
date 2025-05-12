cmake_minimum_required (VERSION 3.10)

project(Tutorial14_ComputeShader CXX)

set(SOURCE
    src/Tutorial14_ComputeShader.cpp
)

set(INCLUDE
    src/Tutorial14_ComputeShader.hpp
)

set(SHADERS
    assets/volume.psh
    assets/volume.vsh
    assets/advect.csh
    assets/apply_forces.csh
    assets/divergence.csh
    assets/jacobi.csh
    assets/project.csh
)

set(ASSETS)

add_sample_app("Tutorial14_ComputeShader" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
