// @TODO: refactor LOCAL_SIZE
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

#include "../cbuffer.h"
#include "../particle_defines.h"

void main() {
    GlobalParticleCounter[0].emission_count = min(uint(u_ParticlesPerFrame), GlobalParticleCounter[0].dead_count);
    GlobalParticleCounter[0].simulation_count = GlobalParticleCounter[0].alive_count[u_PreSimIdx] + GlobalParticleCounter[0].emission_count;
    GlobalParticleCounter[0].alive_count[u_PostSimIdx] = 0;
}
