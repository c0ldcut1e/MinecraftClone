#include "ParticleEngine.h"

#include <algorithm>

#include "../../threading/ThreadStorage.h"
#include "../../utils/Random.h"
#include "../../utils/math/Mth.h"
#include "ParticleRegistry.h"

static double randomCentered(Random *random, double spread)
{
    if (spread == 0.0)
    {
        return 0.0;
    }

    return (random->nextDouble() * 2.0 - 1.0) * spread;
}

static Vec3 randomSpread(Random *random, const Vec3 &spread)
{
    return Vec3(randomCentered(random, spread.x), randomCentered(random, spread.y),
                randomCentered(random, spread.z));
}

ParticleEngine::ParticleEngine()
    : m_pool(std::make_unique<ThreadPool>(1)), m_updateRunning(false), m_updateReady(false)
{}

ParticleEngine::~ParticleEngine()
{
    if (m_pool)
    {
        m_pool->wait();
        m_pool.reset();
    }
}

void ParticleEngine::tick(float delta)
{
    applyCompletedUpdate();

    if (!m_pool || m_updateRunning.load())
    {
        return;
    }

    std::vector<Particle> particles;
    std::vector<ParticleSpawnParams> spawns;

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_particles.empty() && m_pendingSpawns.empty())
        {
            return;
        }

        particles = m_particles;
        spawns.swap(m_pendingSpawns);
    }

    if (delta <= 0.0f)
    {
        delta = 0.05f;
    }

    m_updateRunning.store(true);

    m_pool->detachTask(
            [this, delta, particles = std::move(particles), spawns = std::move(spawns)]() mutable {
                ThreadStorage::useDefaultThreadStorage();
                spawnParticles(spawns, &particles);
                updateParticles(delta, &particles);

                std::vector<RenderParticle> renderParticles;
                buildRenderParticles(particles, &renderParticles);

                {
                    std::lock_guard<std::mutex> lock(m_completedMutex);
                    m_completedParticles       = std::move(particles);
                    m_completedRenderParticles = std::move(renderParticles);
                }

                m_updateReady.store(true);
                m_updateRunning.store(false);
            });
}

void ParticleEngine::clear()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_particles.clear();
    m_renderParticles.clear();
    m_pendingSpawns.clear();
}

void ParticleEngine::spawn(const ParticleSpawnParams &params)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_pendingSpawns.push_back(params);
}

void ParticleEngine::copyRenderParticles(std::vector<RenderParticle> *out) const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    *out = m_renderParticles;
}

size_t ParticleEngine::getParticleCount() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_particles.size() + m_pendingSpawns.size();
}

void ParticleEngine::applyCompletedUpdate()
{
    if (!m_updateReady.load())
    {
        return;
    }

    {
        std::lock_guard<std::mutex> completedLock(m_completedMutex);
        std::lock_guard<std::mutex> stateLock(m_stateMutex);
        m_particles       = std::move(m_completedParticles);
        m_renderParticles = std::move(m_completedRenderParticles);
    }

    m_updateReady.store(false);
}

void ParticleEngine::spawnParticles(const std::vector<ParticleSpawnParams> &spawns,
                                    std::vector<Particle> *particles) const
{
    static thread_local Random random;

    for (const ParticleSpawnParams &params : spawns)
    {
        const ParticleDefinition *definition = ParticleRegistry::get(params.type);
        if (!definition)
        {
            continue;
        }

        int amount = std::max(params.amount, 1);
        for (int i = 0; i < amount; i++)
        {
            Vec3 spawnOffset    = randomSpread(&random, params.spread);
            Vec3 velocityOffset = randomSpread(&random, definition->velocitySpread);

            Particle particle;
            particle.type        = params.type;
            particle.position    = params.position.add(spawnOffset);
            particle.oldPosition = particle.position;
            particle.velocity    = definition->velocity.add(velocityOffset);
            particle.color       = definition->color;
            particle.size        = definition->size;
            particle.age         = 0.0f;
            particle.lifetime    = definition->lifetime;
            particle.gravity     = definition->gravity;
            particle.drag        = definition->drag;
            particle.lit         = definition->lit;
            particle.collides    = definition->collides;

            if (particle.lifetime <= 0 || particle.size <= 0.0f)
            {
                continue;
            }

            particles->push_back(particle);
        }
    }
}

void ParticleEngine::updateParticles(float delta, std::vector<Particle> *particles) const
{
    std::vector<Particle> nextParticles;
    nextParticles.reserve(particles->size());

    for (Particle &particle : *particles)
    {
        particle.oldPosition = particle.position;
        particle.age += delta * 20.0f;
        if (particle.age >= (float) particle.lifetime)
        {
            continue;
        }

        particle.velocity.y += (double) particle.gravity * (double) delta;
        particle.position = particle.position.add(particle.velocity.scale(delta));

        float drag        = Mth::clampf(1.0f - particle.drag * delta, 0.0f, 1.0f);
        particle.velocity = particle.velocity.scale((double) drag);

        nextParticles.push_back(particle);
    }

    particles->swap(nextParticles);
}

void ParticleEngine::buildRenderParticles(const std::vector<Particle> &particles,
                                          std::vector<RenderParticle> *renderParticles) const
{
    renderParticles->clear();
    renderParticles->reserve(particles.size());

    for (const Particle &particle : particles)
    {
        float life = particle.lifetime > 0 ? particle.age / (float) particle.lifetime : 1.0f;
        float fade = Mth::clampf(1.0f - life, 0.0f, 1.0f);

        uint8_t a = (uint8_t) ((float) ((particle.color >> 24) & 0xFF) * fade + 0.5f);
        if (a == 0)
        {
            continue;
        }

        RenderParticle renderParticle;
        renderParticle.type        = particle.type;
        renderParticle.position    = particle.position;
        renderParticle.oldPosition = particle.oldPosition;
        renderParticle.color       = (particle.color & 0x00FFFFFFu) | ((uint32_t) a << 24);
        renderParticle.size        = particle.size;
        renderParticle.lit         = particle.lit;
        renderParticles->push_back(renderParticle);
    }
}
