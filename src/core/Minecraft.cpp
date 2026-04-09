#include "Minecraft.h"

#include <cmath>
#include <memory>
#include <stdexcept>

#include <glad/glad.h>

#include <SDL2/SDL.h>

#include "../entity/EntityRendererRegistry.h"
#include "../input/InputManager.h"
#include "../input/SdlControllerBackend.h"
#include "../rendering/GlStateManager.h"
#include "../rendering/RenderCommand.h"
#include "../rendering/Tesselator.h"
#include "../ui/ImGuiSystem.h"
#include "../ui/UIScene_DebugOverlay.h"
#include "../ui/UIScene_MainMenu.h"
#include "../utils/Time.h"
#include "../utils/math/Mth.h"
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
      m_frameFramebuffer(nullptr), m_gammaShader(nullptr), m_gammaFullscreenVao(0),
      m_uiController(nullptr), m_imguiSystem(nullptr), m_inputManager(nullptr),
      m_debugOverlayScene(nullptr), m_farPlane(3000.0), m_projection(), m_playerSpawnPosition(),
      m_gammaPercent(100.0f), m_inWorld(false), m_mouseLocked(false),
      m_restoreMouseLockAfterImGui(false)
{
    Logger::logInfo("OpenGL initialized: %s", glGetString(GL_VERSION));

    initSdlEventBridge(m_window.getHandle());
    Logger::logInfo("Event bridge initialized");

    m_imguiSystem = new ImGuiSystem();
    m_imguiSystem->init(m_window.getHandle());

    m_window.disableVSync();

    m_timer        = new Timer(20.0f, 0.25f);
    m_inputManager = new InputManager(std::make_unique<SdlControllerBackend>());

    m_defaultFont = new Font("fonts/Default.png", 24);
    m_defaultFont->setScreenProjection(
            Mat4::orthographic(0.0, (double) m_width, (double) m_height, 0.0, -1.0, 1.0));
    m_frameFramebuffer   = new Framebuffer(m_width, m_height);
    m_gammaShader        = new Shader("shaders/gamma.vert", "shaders/gamma.frag");
    m_gammaFullscreenVao = RenderCommand::createVertexArray();

    m_uiController = new UIController();
    m_uiController->pushScene(new UIScene_MainMenu());

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

            if (m_inputManager)
            {
                m_inputManager->onKeyPressed(event.getKey());
            }

            if (event.getKey() == SDL_SCANCODE_C && m_inWorld)
            {
                toggleMouseLock();
            }
            else if (event.getKey() == SDL_SCANCODE_L && m_levelRenderer)
            {
                m_levelRenderer->cycleLightingMode();
            }
            else if (event.getKey() == SDL_SCANCODE_B && m_levelRenderer)
            {
                m_levelRenderer->cycleBlockOutlineMode();
            }
            else if (event.getKey() == SDL_SCANCODE_G && m_levelRenderer)
            {
                m_levelRenderer->toggleGrassSideOverlay();
            }
            return true;
        });

        dispatcher.dispatch<KeyReleasedEvent>([this](KeyReleasedEvent &event) {
            if (m_imguiSystem && m_imguiSystem->wantsKeyboardCapture())
            {
                return true;
            }
            if (m_inputManager)
            {
                m_inputManager->onKeyReleased(event.getKey());
            }

            return true;
        });

        dispatcher.dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent &event) {
            if (m_imguiSystem && m_imguiSystem->wantsMouseCapture())
            {
                return true;
            }
            if (m_inputManager)
            {
                m_inputManager->onMouseButtonPressed(event.getButton());
            }

            return true;
        });

        dispatcher.dispatch<MouseMovedEvent>([this](MouseMovedEvent &event) {
            if (m_mouseLocked && m_inputManager &&
                !(m_imguiSystem && m_imguiSystem->wantsMouseCapture()))
            {
                m_inputManager->onMouseMoved(event.getDeltaX(), event.getDeltaY());
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
            if (m_levelRenderer)
            {
                m_levelRenderer->onResize(event.getWidth(), event.getHeight());
            }
            if (m_frameFramebuffer)
            {
                m_frameFramebuffer->resize(event.getWidth(), event.getHeight());
            }

            return true;
        });

        dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent &) {
            m_window.requestClose();
            return true;
        });
    });

    setMouseLock(false);

    initRegistries();

    m_projection = Mat4::perspective(70.0 * (M_PI / 180.0), (double) m_width / (double) m_height,
                                     0.1, m_farPlane);
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
        if (m_inputManager)
        {
            m_inputManager->update(Time::getDelta());
        }

        if (m_inWorld && m_localPlayer && m_inputManager)
        {
            double lookDeltaX = 0.0;
            double lookDeltaY = 0.0;
            m_inputManager->consumeLookDelta(&lookDeltaX, &lookDeltaY);
            if (lookDeltaX != 0.0 || lookDeltaY != 0.0)
            {
                m_localPlayer->applyLookInput(lookDeltaX, lookDeltaY);
            }

            if (m_inputManager->consumeActionPress(GameplayAction::ToggleFly))
            {
                m_localPlayer->toggleFlying();
            }
            if (m_inputManager->consumeActionPress(GameplayAction::BreakBlock))
            {
                m_localPlayer->breakTargetedBlock();
            }
            if (m_inputManager->consumeActionPress(GameplayAction::PlaceBlock))
            {
                m_localPlayer->placeTargetedBlock();
            }
        }

        if (m_window.shouldClose())
        {
            running = false;
            break;
        }

        m_timer->advanceTime(Time::getDelta());
        int elapsedTicks = m_timer->getElapsedTicks();
        for (int i = 0; i < elapsedTicks; i++)
        {
            if (m_inWorld && m_localPlayer && m_inputManager)
            {
                m_localPlayer->setMoveInputs(m_inputManager->getMoveForward(),
                                             m_inputManager->getMoveStrafe());
                m_localPlayer->setJumpHeld(m_inputManager->isActionDown(GameplayAction::Jump));
            }
            if (m_inWorld && m_level)
            {
                m_level->tick();
                m_level->spawnParticle(ParticleType::SPLASH,
                                       m_playerSpawnPosition.add(Vec3(0.0, -65.0, 0.0)),
                                       Vec3(2.5, 0.8, 2.5), 10);
            }
        }

        if (m_inWorld && m_chunkManager && m_localPlayer)
        {
            m_chunkManager->update(m_localPlayer->getPosition());
        }
        if (m_inWorld && m_level)
        {
            m_level->update(m_timer->getPartialTicks());
        }
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

    if (m_inputManager)
    {
        delete m_inputManager;
        m_inputManager = nullptr;
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

    m_localPlayer       = nullptr;
    m_debugOverlayScene = nullptr;

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

    if (m_gammaFullscreenVao)
    {
        RenderCommand::deleteVertexArray(m_gammaFullscreenVao);
        m_gammaFullscreenVao = 0;
    }

    if (m_gammaShader)
    {
        delete m_gammaShader;
        m_gammaShader = nullptr;
    }

    if (m_frameFramebuffer)
    {
        delete m_frameFramebuffer;
        m_frameFramebuffer = nullptr;
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

InputManager *Minecraft::getInputManager() const { return m_inputManager; }

float Minecraft::getGammaPercent() const { return m_gammaPercent; }

void Minecraft::setGammaPercent(float gammaPercent)
{
    m_gammaPercent = Mth::clampf(gammaPercent, 0.0f, 100.0f);
}

void Minecraft::renderFrame()
{
    if (m_imguiSystem)
    {
        m_imguiSystem->beginFrame();
    }

    if (m_frameFramebuffer)
    {
        m_frameFramebuffer->bind();
        RenderCommand::clearAll();
    }
    else
    {
        RenderCommand::clearAll();
    }

    if (m_inWorld && m_levelRenderer)
    {
        m_levelRenderer->render(m_timer->getPartialTicks(), m_frameFramebuffer);
    }
    if (m_uiController)
    {
        m_uiController->render();
    }

    if (m_frameFramebuffer)
    {
        m_frameFramebuffer->unbind();
    }

    renderGammaPass();

    if (m_imguiSystem)
    {
        if (m_inWorld && m_levelRenderer && m_imguiSystem->isVisible())
        {
            m_levelRenderer->renderDynamicLightImGui();
        }

        m_imguiSystem->render();
    }
}

void Minecraft::renderGammaPass()
{
    if (!m_frameFramebuffer || !m_gammaShader || !m_gammaFullscreenVao)
    {
        return;
    }

    float gammaT        = Mth::clampf(m_gammaPercent / 100.0f, 0.0f, 1.0f);
    float gammaExponent = Mth::lerpf(1.35f, 0.60f, gammaT);

    GlStateManager::disableDepthTest();
    GlStateManager::setDepthMask(false);
    GlStateManager::disableCull();
    GlStateManager::disableBlend();

    m_gammaShader->bind();

    RenderCommand::activeTexture(0);
    RenderCommand::bindTexture2D(m_frameFramebuffer->getColorTexture());
    m_gammaShader->setInt("u_colorTexture", 0);
    m_gammaShader->setFloat("u_gammaExponent", gammaExponent);

    RenderCommand::bindVertexArray(m_gammaFullscreenVao);
    RenderCommand::renderArrays(GL_TRIANGLES, 0, 3);
    RenderCommand::bindVertexArray(0);

    GlStateManager::setDepthMask(true);
    GlStateManager::enableDepthTest();
    GlStateManager::enableCull();
}

void Minecraft::setMouseLock(bool locked)
{
    m_mouseLocked = m_inWorld && locked;

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

void Minecraft::toggleMouseLock()
{
    if (!m_inWorld)
    {
        setMouseLock(false);
        return;
    }

    setMouseLock(!m_mouseLocked);
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

    if (m_inputManager)
    {
        m_inputManager->clear();
    }

    if (visible)
    {
        if (m_mouseLocked)
        {
            m_restoreMouseLockAfterImGui = true;
            setMouseLock(false);
        }
    }
    else if (m_inWorld && m_restoreMouseLockAfterImGui && !m_mouseLocked)
    {
        m_restoreMouseLockAfterImGui = false;
        setMouseLock(true);
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
