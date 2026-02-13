#include "Minecraft.h"

#include <stdexcept>

#include <GLFW/glfw3.h>

#include "../entity/EntityRendererRegistry.h"
#include "../graphics/ImmediateRenderer.h"
#include "../graphics/RenderCommand.h"
#include "../utils/Time.h"
#include "../utils/Utils.h"
#include "../world/WorldRenderer.h"
#include "../world/block/BlockRegistry.h"
#include "Logger.h"
#include "events/EventDispatcher.h"
#include "events/EventManager.h"
#include "events/GlfwEventBridge.h"
#include "events/KeyPressedEvent.h"
#include "events/KeyReleasedEvent.h"
#include "events/MouseButtonPressedEvent.h"
#include "events/MouseMovedEvent.h"
#include "events/WindowResizedEvent.h"

Minecraft::Minecraft()
    : m_width(1280), m_height(720), m_window(m_width, m_height, "game"), m_camera(nullptr), m_world(nullptr), m_localPlayer(nullptr), m_worldRenderer(nullptr), m_chunkManager(nullptr), m_defaultFont(nullptr), m_farPlane(3000.0), m_projection(),
      m_mouseLocked(true) {
    RenderCommand::enableExperimentalFeatures();
    if (!RenderCommand::initialize()) throw std::runtime_error("Failed to initialize OpenGL");
    Logger::logInfo("OpenGL initialized: %s", glGetString(GL_VERSION));

    initGlfwEventBridge(m_window.getHandle());
    Logger::logInfo("GLFW event bridge initialized");

    m_camera        = new Camera();
    m_world         = new World();
    m_worldRenderer = new WorldRenderer(m_world);
    m_chunkManager  = new ChunkManager(m_world);
    m_defaultFont   = new Font("fonts/default.ttf", 24);
    m_defaultFont->setScreenProjection(Mat4::orthographic(0.0, (double) m_width, (double) m_height, 0.0, -1.0, 1.0));

    ImmediateRenderer::getForScreen()->setScreenProjection(Mat4::orthographic(0.0, (double) m_width, (double) m_height, 0.0, -1.0, 1.0));

    EventManager::addListener([this](Event &event) {
        EventDispatcher dispatcher(event);

        dispatcher.dispatch<KeyPressedEvent>([this](KeyPressedEvent &event) {
            if (event.getKey() == GLFW_KEY_C) toggleMouseLock();
            else if (m_localPlayer)
                m_localPlayer->onKeyPressed(event.getKey());
        });

        dispatcher.dispatch<KeyReleasedEvent>([this](KeyReleasedEvent &event) {
            if (m_localPlayer) m_localPlayer->onKeyReleased(event.getKey());
        });

        dispatcher.dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent &event) {
            if (m_localPlayer) m_localPlayer->onMouseButtonPressed(event.getButton());
        });

        dispatcher.dispatch<MouseMovedEvent>([this](MouseMovedEvent &event) {
            if (m_mouseLocked && m_localPlayer) m_localPlayer->onMouseMoved(event.getDeltaX(), event.getDeltaY());
        });

        dispatcher.dispatch<WindowResizedEvent>([this](WindowResizedEvent &event) {
            glViewport(0, 0, event.getWidth(), event.getHeight());
            m_projection = Mat4::perspective(70.0 * (M_PI / 180.0), (double) event.getWidth() / (double) event.getHeight(), 0.1, m_farPlane);
            ImmediateRenderer::getForScreen()->setScreenProjection(Mat4::orthographic(0.0, (double) event.getWidth(), (double) event.getHeight(), 0.0, -1.0, 1.0));
            m_defaultFont->setScreenProjection(Mat4::orthographic(0.0, (double) event.getWidth(), (double) event.getHeight(), 0.0, -1.0, 1.0));
        });
    });

    toggleMouseLock();

    initRegistries();

    m_projection = Mat4::perspective(70.0 * (M_PI / 180.0), (double) m_width / (double) m_height, 0.1, m_farPlane);

    int spawnX    = 0;
    int spawnZ    = 0;
    int spawnY    = 64;
    m_localPlayer = new LocalPlayer(m_world, L"", m_camera);
    m_localPlayer->setPosition(Vec3(spawnX + 0.5, spawnY, spawnZ + 0.5));
    m_world->addEntity(std::unique_ptr<Entity>(m_localPlayer));

    std::unique_ptr<Entity> entity = std::make_unique<Entity>(m_world);
    entity->setPosition(Vec3((int) spawnX, (int) spawnY, (int) spawnZ));
    m_world->addEntity(std::move(entity));

    m_chunkManager->start();
}

Minecraft::~Minecraft() { shutdown(); }

Minecraft *Minecraft::getInstance() {
    static Minecraft instance;
    return &instance;
}

void Minecraft::start() {
    Time::update();

    while (!m_window.shouldClose()) {
        Time::update();

        EventManager::process();

        if (m_localPlayer) m_chunkManager->update(m_localPlayer->getPosition());

        m_world->tick();

        renderFrame();

        m_window.swapBuffers();
        m_window.pollEvents();
    }

    shutdown();
}

void Minecraft::shutdown() {
    if (m_shutdown) return;
    m_shutdown = true;

    if (m_chunkManager) {
        m_chunkManager->stop();
        delete m_chunkManager;
        m_chunkManager = nullptr;
    }

    if (m_worldRenderer) {
        delete m_worldRenderer;
        m_worldRenderer = nullptr;
    }

    if (m_camera) {
        delete m_camera;
        m_camera = nullptr;
    }

    if (m_world) {
        delete m_world;
        m_world = nullptr;
    }
}

const Camera *Minecraft::getCamera() const { return m_camera; }

LocalPlayer *Minecraft::getLocalPlayer() const { return m_localPlayer; }

WorldRenderer *Minecraft::getWorldRenderer() const { return m_worldRenderer; }

const Mat4 &Minecraft::getProjection() const { return m_projection; }

ChunkManager *Minecraft::getChunkManager() const { return m_chunkManager; }

Font *Minecraft::getDefaultFont() const { return m_defaultFont; }

void Minecraft::initRegistries() {
    BlockRegistry::init();
    EntityRendererRegistry::init();
}

void Minecraft::toggleMouseLock() {
    GLFWwindow *window = m_window.getHandle();

    m_mouseLocked = !m_mouseLocked;
    if (m_mouseLocked) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        resetGlfwMouseState();
    } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        resetGlfwMouseState();
    }

    Logger::logInfo("Mouse lock: %d", m_mouseLocked);
}

void Minecraft::renderFrame() {
    RenderCommand::setClearColor(0.85f, 0.85f, 0.85f, 1.0f);
    RenderCommand::clearAll();

    m_worldRenderer->draw();

    m_defaultFont->drawShadow(L"testing", 20.0f, 30.0f, 1.0f, -1);
}
