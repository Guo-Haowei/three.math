#include "../cbuffer.h"
#include "../particle_defines.h"

layout(local_size_x = PARTICLE_LOCAL_SIZE, local_size_y = 1, local_size_z = 1) in;

void push_dead_index(uint index) {
    uint insert_idx = atomicAdd(GlobalParticleCounter[0].dead_count, 1);
    GlobalDeadIndices[insert_idx] = index;
}

uint pop_dead_index() {
    uint index = atomicAdd(GlobalParticleCounter[0].dead_count, -1);
    return GlobalDeadIndices[index - 1];
}

void push_alive_index(uint index) {
    uint insert_idx = atomicAdd(GlobalParticleCounter[0].alive_count[u_PostSimIdx], 1);
    GlobalAliveIndicesPostSim[insert_idx] = index;
}

uint pop_alive_index() {
    uint index = atomicAdd(GlobalParticleCounter[0].alive_count[u_PreSimIdx], -1);
    return GlobalAliveIndicesPreSim[index - 1];
}

void main() {
    uint index = gl_GlobalInvocationID.x;

    if (index < GlobalParticleCounter[0].simulation_count) {
        // Consume an Alive particle index
        uint particle_index = pop_alive_index();

        Particle particle = GlobalParticleData[particle_index];

        // Is it dead?
        if (particle.lifeRemaining <= 0.0f) {
            push_dead_index(particle_index);
        } else {
            particle.lifeRemaining.x -= u_ElapsedTime;
            particle.position += u_ElapsedTime * particle.velocity;

            GlobalParticleData[particle_index] = particle;

            // Append index back into AliveIndices list
            push_alive_index(particle_index);

            // Increment draw count
        }
    }
}

