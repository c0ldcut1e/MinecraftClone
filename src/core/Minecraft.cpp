#include "Minecraft.h"

#include <cmath>
#include <stdexcept>

#include <glad/glad.h>

#include <SDL2/SDL.h>

#include "../entity/EntityRendererRegistry.h"
#include "../entity/TestEntity.h"
#include "../rendering/RenderCommand.h"
#include "../rendering/Tesselator.h"
#include "../ui/ImGuiSystem.h"
#include "../ui/UIScene_DebugMenuPanel.h"
#include "../ui/UIScene_DebugOverlay.h"
#include "../ui/UIScene_HUD.h"
#include "../utils/Time.h"
#include "../world/LevelRenderer.h"
#include "../world/biome/BiomeRegistry.h"
#include "../world/block/BlockRegistry.h"
#include "../world/models/ModelRegistry.h"
#include "../world/particle/ParticleRegistry.h"
#include "../world/render/LevelRenderObject.h"
#include "Logger.h"
#include "events/EventDispatcher.h"
#include "events/EventManager.h"
#include "events/KeyPressedEvent.h"
#include "events/KeyReleasedEvent.h"
#include "events/MouseButtonPressedEvent.h"
#include "events/MouseMovedEvent.h"
#include "events/SdlEventBridge.h"
#include "events/WindowCloseEvent.h"
#include "events/WindowResizedEvent.h"

Minecraft::Minecraft()
    : m_width(1280), m_height(720), m_window(m_width, m_height, "game"), m_shutdown(false),
      m_timer(nullptr), m_camera(nullptr), m_level(nullptr), m_localPlayer(nullptr),
      m_levelRenderer(nullptr), m_chunkManager(nullptr), m_defaultFont(nullptr),
      m_uiController(nullptr), m_imguiSystem(nullptr), m_debugOverlayScene(nullptr),
      m_farPlane(3000.0), m_projection(), m_playerSpawnPosition(), m_mouseLocked(true),
      m_restoreMouseLockAfterImGui(false)
{
    Logger::logInfo("OpenGL initialized: %s", glGetString(GL_VERSION));

    initSdlEventBridge(m_window.getHandle());
    Logger::logInfo("Event bridge initialized");

    m_imguiSystem = new ImGuiSystem();
    m_imguiSystem->init(m_window.getHandle());

    m_window.disableVSync();

    m_timer = new Timer(20.0f, 0.25f);

    m_camera = new Camera();
    m_level  = new Level();
    m_level->setWorldBorderEnabled(false);
    m_level->setWorldBorderChunks(8);
    m_level->setEmptyChunksSolid(false);

    m_levelRenderer = new LevelRenderer(m_level, m_width, m_height);
    m_chunkManager  = new ChunkManager(m_level);

    m_defaultFont = new Font("fonts/default.ttf", 24);
    m_defaultFont->setScreenProjection(
            Mat4::orthographic(0.0, (double) m_width, (double) m_height, 0.0, -1.0, 1.0));

    m_uiController      = new UIController();
    m_debugOverlayScene = new UIScene_DebugOverlay();
    m_debugOverlayScene->setVisible(false);
    m_uiController->pushScene(m_debugOverlayScene);
    m_uiController->pushScene(new UIScene_HUD());
    m_uiController->pushScene(new UIScene_DebugMenuPanel());

    Tesselator::getInstance()->getBuilderForScreen()->setScreenProjection(
            Mat4::orthographic(0.0, (double) m_width, (double) m_height, 0.0, -1.0, 1.0));

    EventManager::addListener([this](Event &event) {
        EventDispatcher dispatcher(event);

        dispatcher.dispatch<KeyPressedEvent>([this](KeyPressedEvent &event) {
            if (event.getKey() == SDL_SCANCODE_F3)
            {
                toggleDebugOverlay();
                return true;
            }
            if (event.getKey() == SDL_SCANCODE_F6)
            {
                toggleImGuiOverlay();
                return true;
            }

            if (m_imguiSystem && m_imguiSystem->wantsKeyboardCapture())
            {
                return true;
            }

            if (event.getKey() == SDL_SCANCODE_C)
            {
                toggleMouseLock();
            }
            else if (event.getKey() == SDL_SCANCODE_L)
            {
                m_levelRenderer->cycleLightingMode();
            }
            else if (event.getKey() == SDL_SCANCODE_B)
            {
                m_levelRenderer->cycleBlockOutlineMode();
            }
            else if (event.getKey() == SDL_SCANCODE_G)
            {
                m_levelRenderer->toggleGrassSideOverlay();
            }
            else if (m_localPlayer)
            {
                m_localPlayer->onKeyPressed(event.getKey());
            }

            return true;
        });

        dispatcher.dispatch<KeyReleasedEvent>([this](KeyReleasedEvent &event) {
            if (m_imguiSystem && m_imguiSystem->wantsKeyboardCapture())
            {
                return true;
            }
            if (m_localPlayer)
            {
                m_localPlayer->onKeyReleased(event.getKey());
            }

            return true;
        });

        dispatcher.dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent &event) {
            if (m_imguiSystem && m_imguiSystem->wantsMouseCapture())
            {
                return true;
            }
            if (m_localPlayer)
            {
                m_localPlayer->onMouseButtonPressed(event.getButton());
            }

            return true;
        });

        dispatcher.dispatch<MouseMovedEvent>([this](MouseMovedEvent &event) {
            if (m_mouseLocked && m_localPlayer &&
                !(m_imguiSystem && m_imguiSystem->wantsMouseCapture()))
            {
                m_localPlayer->onMouseMoved(event.getDeltaX(), event.getDeltaY());
            }

            return true;
        });

        dispatcher.dispatch<WindowResizedEvent>([this](WindowResizedEvent &event) {
            m_width  = event.getWidth();
            m_height = event.getHeight();

            RenderCommand::setViewport(0, 0, event.getWidth(), event.getHeight());
            m_projection = Mat4::perspective(70.0 * (M_PI / 180.0),
                                             (double) event.getWidth() / (double) event.getHeight(),
                                             0.1, m_farPlane);
            Tesselator::getInstance()->getBuilderForScreen()->setScreenProjection(
                    Mat4::orthographic(0.0, (double) event.getWidth(), (double) event.getHeight(),
                                       0.0, -1.0, 1.0));
            m_defaultFont->setScreenProjection(Mat4::orthographic(
                    0.0, (double) event.getWidth(), (double) event.getHeight(), 0.0, -1.0, 1.0));
            m_levelRenderer->onResize(event.getWidth(), event.getHeight());

            return true;
        });

        dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent &) {
            m_window.requestClose();
            return true;
        });
    });

    toggleMouseLock();

    initRegistries();

    m_projection = Mat4::perspective(70.0 * (M_PI / 180.0), (double) m_width / (double) m_height,
                                     0.1, m_farPlane);

    int spawnX            = 0;
    int spawnZ            = 0;
    int spawnY            = 135;
    m_playerSpawnPosition = Vec3(spawnX + 0.5, spawnY, spawnZ + 0.5);
    m_localPlayer         = new LocalPlayer(m_level, L"", m_camera);
    m_localPlayer->setPosition(m_playerSpawnPosition);
    m_level->addEntity(std::unique_ptr<Entity>(m_localPlayer));

    std::unique_ptr<Entity> entity = std::make_unique<TestEntity>(m_level);
    entity->setPosition(Vec3(spawnX - 0.5, spawnY, spawnZ - 0.5));
    m_level->addEntity(std::move(entity));

    m_chunkManager->start();
}

Minecraft::~Minecraft() { shutdown(); }

Minecraft *Minecraft::getInstance()
{
    static Minecraft instance;
    return &instance;
}

void Minecraft::start()
{
    Time::update();

    bool running = true;
    while (running)
    {
        Time::update();

        pumpSdlEvents();
        EventManager::process();

        if (m_window.shouldClose())
        {
            running = false;
            break;
        }

        m_timer->advanceTime(Time::getDelta());
        int elapsedTicks = m_timer->getElapsedTicks();
        for (int i = 0; i < elapsedTicks; i++)
        {
            m_level->tick();
            m_level->spawnParticle(ParticleType::SPLASH,
                                   m_playerSpawnPosition.add(Vec3(0.0, -65.0, 0.0)),
                                   Vec3(2.5, 0.8, 2.5), 10);
        }

        m_chunkManager->update(m_localPlayer->getPosition());
        m_level->update(m_timer->getPartialTicks());
        m_uiController->tick();

        renderFrame();

        m_window.swapBuffers();
    }

    shutdown();
}

void Minecraft::shutdown()
{
    if (m_shutdown)
    {
        return;
    }
    m_shutdown = true;

    if (m_chunkManager)
    {
        m_chunkManager->stop();
        delete m_chunkManager;
        m_chunkManager = nullptr;
    }

    if (m_levelRenderer)
    {
        delete m_levelRenderer;
        m_levelRenderer = nullptr;
    }

    if (m_imguiSystem)
    {
        delete m_imguiSystem;
        m_imguiSystem = nullptr;
    }

    if (m_uiController)
    {
        delete m_uiController;
        m_uiController = nullptr;
    }

    if (m_level)
    {
        delete m_level;
        m_level = nullptr;
    }

    if (m_camera)
    {
        delete m_camera;
        m_camera = nullptr;
    }

    if (m_defaultFont)
    {
        delete m_defaultFont;
        m_defaultFont = nullptr;
    }

    if (m_timer)
    {
        delete m_timer;
        m_timer = nullptr;
    }
}

int Minecraft::getWidth() const { return m_width; }

int Minecraft::getHeight() const { return m_height; }

Timer *Minecraft::getTimer() const { return m_timer; }

const Camera *Minecraft::getCamera() const { return m_camera; }

LocalPlayer *Minecraft::getLocalPlayer() const { return m_localPlayer; }

Level *Minecraft::getLevel() const { return m_level; }

LevelRenderer *Minecraft::getLevelRenderer() const { return m_levelRenderer; }

const Mat4 &Minecraft::getProjection() const { return m_projection; }

ChunkManager *Minecraft::getChunkManager() const { return m_chunkManager; }

Font *Minecraft::getDefaultFont() const { return m_defaultFont; }

void Minecraft::renderFrame()
{
    RenderCommand::clearColor();

    if (m_imguiSystem)
    {
        m_imguiSystem->beginFrame();
    }

    m_levelRenderer->render(m_timer->getPartialTicks());
    m_uiController->render();

    if (m_imguiSystem)
    {
        if (m_imguiSystem->isVisible())
        {
            m_levelRenderer->renderDynamicLightImGui();
        }

        m_imguiSystem->render();
    }
}

void Minecraft::toggleMouseLock()
{
    m_mouseLocked = !m_mouseLocked;

    if (m_mouseLocked)
    {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_SetWindowGrab(m_window.getHandle(), SDL_TRUE);
        resetSdlMouseState();
    }
    else
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_SetWindowGrab(m_window.getHandle(), SDL_FALSE);
        resetSdlMouseState();
    }

    Logger::logInfo("Mouse lock: %d", m_mouseLocked);
}

void Minecraft::toggleImGuiOverlay()
{
    if (!m_imguiSystem)
    {
        return;
    }

    bool visible = !m_imguiSystem->isVisible();
    m_imguiSystem->setVisible(visible);

    if (m_localPlayer)
    {
        m_localPlayer->clearInputs();
    }

    if (visible)
    {
        if (m_mouseLocked)
        {
            m_restoreMouseLockAfterImGui = true;
            toggleMouseLock();
        }
    }
    else if (m_restoreMouseLockAfterImGui && !m_mouseLocked)
    {
        m_restoreMouseLockAfterImGui = false;
        toggleMouseLock();
    }
    else
    {
        m_restoreMouseLockAfterImGui = false;
    }
}

void Minecraft::toggleDebugOverlay()
{
    if (m_debugOverlayScene)
    {
        m_debugOverlayScene->toggleVisible();
    }
}

void Minecraft::initRegistries()
{
    BlockRegistry::init();
    ParticleRegistry::init();
    ParticleRegistry::rebuildAtlas(BlockRegistry::getTextureRepository());
    BiomeRegistry::init();
    ModelRegistry::init();
    EntityRendererRegistry::init();
}
