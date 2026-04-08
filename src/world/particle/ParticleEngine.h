#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "../../threading/ThreadPool.h"
#include "../../utils/math/Vec3.h"
#include "ParticleSpawnParams.h"

class Level;

class ParticleEngine
{
public:
    struct RenderParticle
    {
        ParticleType type;
        Vec3 position;
        Vec3 oldPosition;
        uint32_t color;
        float size;
        bool lit;
    };

    ParticleEngine();
    ~ParticleEngine();

    void tick(float delta);
    void clear();

    void spawn(const ParticleSpawnParams &params);
    void copyRenderParticles(std::vector<RenderParticle> *out) const;
    size_t getParticleCount() const;

private:
    struct Particle
    {
        ParticleType type;
        Vec3 position;
        Vec3 oldPosition;
        Vec3 velocity;
        uint32_t color;
        float size;
        float age;
        int lifetime;
        float gravity;
        float drag;
        bool lit;
        bool collides;
    };

    void applyCompletedUpdate();
    void spawnParticles(const std::vector<ParticleSpawnParams> &spawns,
                        std::vector<Particle> *particles) const;
    void updateParticles(float delta, std::vector<Particle> *particles) const;
    void buildRenderParticles(const std::vector<Particle> &particles,
                              std::vector<RenderParticle> *renderParticles) const;

    std::unique_ptr<ThreadPool> m_pool;
    std::atomic<bool> m_updateRunning;
    std::atomic<bool> m_updateReady;

    mutable std::mutex m_stateMutex;
    mutable std::mutex m_completedMutex;

    std::vector<Particle> m_particles;
    std::vector<RenderParticle> m_renderParticles;
    std::vector<ParticleSpawnParams> m_pendingSpawns;

    std::vector<Particle> m_completedParticles;
    std::vector<RenderParticle> m_completedRenderParticles;
};
