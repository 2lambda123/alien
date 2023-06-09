﻿#include "Entities.cuh"

#include "Cell.cuh"
#include "Token.cuh"
#include "Particle.cuh"

void Entities::init()
{
    cellPointers.init();
    cells.init();
    tokenPointers.init();
    tokens.init();
    particles.init();
    particlePointers.init();
    stringBytes.init();
    stringBytes.resize(MAX_STRING_BYTES);
}

void Entities::free()
{
    cellPointers.free();
    cells.free();
    tokenPointers.free();
    tokens.free();
    particles.free();
    particlePointers.free();
    stringBytes.free();
}
