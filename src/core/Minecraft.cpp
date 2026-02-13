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
    : m_width(1280), m_height(720), m_window(m_width, m_height, "game"), m_shader(nullptr), m_camera(nullptr), m_world(nullptr), m_player(nullptr), m_worldRenderer(nullptr), m_chunkManager(nullptr), m_farPlane(3000.0), m_projection(), m_model(),
      m_mouseLocked(true) {
    RenderCommand::enableExperimentalFeatures();
    if (!RenderCommand::initialize()) throw std::runtime_error("Failed to initialize OpenGL");
    Logger::logInfo("OpenGL initialized: %s", glGetString(GL_VERSION));

    RenderCommand::enableCull();
    RenderCommand::setFrontFace(GL_CCW);
    RenderCommand::setCullFace(GL_BACK);

    initGlfwEventBridge(m_window.getHandle());
    Logger::logInfo("GLFW event bridge initialized");

    m_camera        = new Camera();
    m_world         = new World();
    m_worldRenderer = new WorldRenderer(m_world);
    m_shader        = new Shader("shaders/block.vert", "shaders/block.frag");
    m_chunkManager  = new ChunkManager(m_world);

    EventManager::addListener([this](Event &event) {
        EventDispatcher dispatcher(event);

        dispatcher.dispatch<KeyPressedEvent>([this](KeyPressedEvent &event) {
            if (event.getKey() == GLFW_KEY_C) toggleMouseLock();
            else if (m_player)
                m_player->onKeyPressed(event.getKey());
        });

        dispatcher.dispatch<KeyReleasedEvent>([this](KeyReleasedEvent &event) {
            if (m_player) m_player->onKeyReleased(event.getKey());
        });

        dispatcher.dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent &event) {
            if (m_player) m_player->onMouseButtonPressed(event.getButton());
        });

        dispatcher.dispatch<MouseMovedEvent>([this](MouseMovedEvent &event) {
            if (m_mouseLocked && m_player) m_player->onMouseMoved(event.getDeltaX(), event.getDeltaY());
        });

        dispatcher.dispatch<WindowResizedEvent>([this](WindowResizedEvent &event) {
            glViewport(0, 0, event.getWidth(), event.getHeight());
            m_projection = Mat4::perspective(70.0 * (M_PI / 180.0), (double) event.getWidth() / (double) event.getHeight(), 0.1, m_farPlane);
        });
    });

    toggleMouseLock();

    initRegistries();

    m_projection = Mat4::perspective(70.0 * (M_PI / 180.0), (double) m_width / (double) m_height, 0.1, m_farPlane);
    m_model      = Mat4::identity();

    int spawnX = 0;
    int spawnZ = 0;
    int spawnY = 64;
    m_player   = new LocalPlayer(m_world, m_camera);
    m_player->setPosition(Vec3(spawnX + 0.5, spawnY, spawnZ + 0.5));
    m_world->addEntity(std::unique_ptr<Entity>(m_player));

    m_chunkManager->start();
}

Minecraft::~Minecraft() {
    delete m_chunkManager;
    delete m_worldRenderer;
    delete m_shader;
    delete m_camera;

    delete m_world;
}

Minecraft *Minecraft::getInstance() {
    static Minecraft instance;
    return &instance;
}

void Minecraft::start() {
    Time::update();

    while (!m_window.shouldClose()) {
        Time::update();

        EventManager::process();

        if (m_player) m_chunkManager->update(m_player->getPosition());

        m_world->tick();

        renderFrame();

        m_window.swapBuffers();
        m_window.pollEvents();
    }
}

const Camera *Minecraft::getCamera() const { return m_camera; }

LocalPlayer *Minecraft::getLocalPlayer() const { return m_player; }

WorldRenderer *Minecraft::getWorldRenderer() const { return m_worldRenderer; }

const Mat4 &Minecraft::getProjection() const { return m_projection; }

ChunkManager *Minecraft::getChunkManager() const { return m_chunkManager; }

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
    RenderCommand::setClearColor(0.6f, 0.9f, 1.0f, 1.0f);
    RenderCommand::clearAll();

    m_shader->bind();
    m_shader->setInt("u_texture", 0);

    m_shader->setMat4("u_model", m_model.data());
    m_shader->setMat4("u_view", m_camera->getViewMatrix().data());
    m_shader->setMat4("u_projection", m_projection.data());

    RenderCommand::enableDepthTest();
    m_worldRenderer->draw();
}
