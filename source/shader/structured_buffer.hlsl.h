#ifndef STRUCTURED_BUFFER_HLSL_H_INCLUDED
#define STRUCTURED_BUFFER_HLSL_H_INCLUDED
#include "shader_defines.hlsl.h"

struct Particle {
    Vector4f position;
    Vector4f velocity;
    Vector4f color;

    float scale;
    float lifeSpan;
    float lifeRemaining;
    int isActive;
};

struct ParticleCounter {
    int aliveCount[2];
    int deadCount;
    int simulationCount;
    int emissionCount;
};

#define SBUFFER_LIST                                    \
    SBUFFER(ParticleCounter, GlobalParticleCounter, 16) \
    SBUFFER(int, GlobalDeadIndices, 17)                 \
    SBUFFER(int, GlobalAliveIndicesPreSim, 18)          \
    SBUFFER(int, GlobalAliveIndicesPostSim, 19)         \
    SBUFFER(Particle, GlobalParticleData, 24)

#endif
