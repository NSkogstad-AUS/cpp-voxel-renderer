#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLUT/glut.h>
#include <iostream>
#include <sstream>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "renderer.h"
#include "camera.h"

// Camera instance
Camera camera;

// Time tracking variables
float lastFrame = 0.0f;
float lastX = 400, lastY = 300; // Center of the screen
bool firstMouse = true;
bool cursorEnabled = false;
int windowWidth = 800;
int windowHeight = 600;
Renderer* gRenderer = nullptr;

// Mouse callback to process mouse movement
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (!cursorEnabled && glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xOffset = xpos - lastX;
        float yOffset = lastY - ypos;
        lastX = xpos;
        lastY = ypos;

        camera.ProcessMouseMovement(xOffset, yOffset);
    }
}

// Key callback to process key inputs
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        cursorEnabled = !cursorEnabled;
        glfwSetInputMode(window, GLFW_CURSOR, cursorEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        if (!cursorEnabled) {
            firstMouse = true; // Reset the firstMouse flag
        }
    }
    if (key == GLFW_KEY_E && action == GLFW_PRESS) {
        cursorEnabled = !cursorEnabled;
        glfwSetInputMode(window, GLFW_CURSOR, cursorEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        if (!cursorEnabled) {
            firstMouse = true; // Reset the firstMouse flag
        }
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    windowWidth = width;
    windowHeight = height;
    glViewport(0, 0, width, height);
    if (gRenderer) {
        gRenderer->setViewportSize(width, height);
    }
}

// Window focus callback to re-enable cursor
void window_focus_callback(GLFWwindow* window, int focused) {
    if (focused && !cursorEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true; // Reset the firstMouse flag
    }
}

// Function to check for OpenGL errors
void checkGLError(const std::string& location) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL error at " << location << ": " << err << std::endl;
    }
}

int main(int argc, char** argv) {
    // Initialize GLUT
    glutInit(&argc, argv);

    // Initialising GLFW
    if(!glfwInit()) {
        std::cerr << "Failed to initalise GLFW" << std::endl;
        return -1;
    }

    // Setting OpenGL version to 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Needed for MacOS
    #endif

    // creating the window
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "3D Render", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // making context current
    glfwMakeContextCurrent(window);

    // Query actual framebuffer size (accounts for HiDPI) and use it
    glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

    // Initialize GLEW
    glewExperimental = GL_TRUE; // Ensure this is set before initializing GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // set our viewport
    glViewport(0, 0, windowWidth, windowHeight);

    // Set the clear color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Verify that depth testing is enabled
    GLint depthTestEnabled;
    glGetIntegerv(GL_DEPTH_TEST, &depthTestEnabled);
    std::cout << "Depth Test Enabled: " << (depthTestEnabled ? "Yes" : "No") << std::endl;

    // Set the mouse callback and disable the cursor
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Set the key callback
    glfwSetKeyCallback(window, key_callback);

    // Set the window focus callback
    glfwSetWindowFocusCallback(window, window_focus_callback);
    // Handle window resize
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Create and initialise the renderer
    Renderer renderer;
    gRenderer = &renderer;
    renderer.initialise();
    renderer.setViewportSize(windowWidth, windowHeight);

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    TerrainSettings uiSettings = Renderer::getTerrainSettings();
    bool terrainDirty = false;

    // Set initial camera position
    camera.Position = glm::vec3(0.0f, 10.0f, 20.0f); // Adjust as needed

    // Seed the initial chunk visibility so cubes render immediately
    renderer.updateVisitedChunks(renderer.getCurrentChunk(camera.Position.x, camera.Position.z));

    // Main rendering loop
    while (!glfwWindowShouldClose(window)) {
        // Calculate deltaTime
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Process input for camera movement
        camera.ProcessKeyboard(window, deltaTime);

        // Update visible chunks based on the camera position
        renderer.updateVisitedChunks(renderer.getCurrentChunk(camera.Position.x, camera.Position.z));

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Terrain settings UI
        ImGui::Begin("Terrain Settings");
        terrainDirty = false;
        if (ImGui::SliderFloat("Continent freq", &uiSettings.continentFreq, 0.0005f, 0.02f, "%.5f")) terrainDirty = true;
        if (ImGui::SliderFloat("Detail freq", &uiSettings.detailFreq, 0.001f, 0.02f, "%.5f")) terrainDirty = true;
        if (ImGui::SliderFloat("Continent weight", &uiSettings.continentWeight, 0.0f, 1.0f)) terrainDirty = true;
        if (ImGui::SliderFloat("Detail weight", &uiSettings.detailWeight, 0.0f, 1.0f)) terrainDirty = true;
        if (ImGui::SliderFloat("Height curve", &uiSettings.heightCurve, 0.2f, 2.0f)) terrainDirty = true;
        if (ImGui::SliderFloat("Base height", &uiSettings.baseHeightFraction, 0.0f, 0.8f)) terrainDirty = true;
        if (ImGui::SliderFloat("Height range", &uiSettings.heightRangeFraction, 0.05f, 0.8f)) terrainDirty = true;
        if (ImGui::SliderFloat("Smooth center", &uiSettings.smoothingCenterWeight, 0.0f, 8.0f)) terrainDirty = true;
        if (ImGui::SliderFloat("Smooth edge", &uiSettings.smoothingEdgeWeight, 0.0f, 8.0f)) terrainDirty = true;
        if (ImGui::SliderFloat("Smooth diag", &uiSettings.smoothingDiagWeight, 0.0f, 8.0f)) terrainDirty = true;
        if (ImGui::Button("Reseed noise")) {
            renderer.reseedNoise();
            renderer.updateVisitedChunks(renderer.getCurrentChunk(camera.Position.x, camera.Position.z));
        }
        if (terrainDirty) {
            Renderer::setTerrainSettings(uiSettings);
            renderer.clearChunksAndMeshes();
            renderer.updateVisitedChunks(renderer.getCurrentChunk(camera.Position.x, camera.Position.z));
        }
        ImGui::End();

        // Rendering scene
        renderer.render();

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Check for OpenGL errors
        checkGLError("After rendering");

        // Swap buffers
        glfwSwapBuffers(window);

        // Checking events
        glfwPollEvents();
    }

    // And cleanup
    renderer.cleanup();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
