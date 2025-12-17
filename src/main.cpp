#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLUT/glut.h>
#include <iostream>
#include <sstream>
#include "renderer.h"
#include "camera.h"

// Camera instance
Camera camera;

// Time tracking variables
float lastFrame = 0.0f;
float lastX = 400, lastY = 300; // Center of the screen
bool firstMouse = true;
bool cursorEnabled = false;

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
    GLFWwindow* window = glfwCreateWindow(800, 600, "3D Render", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // making context current
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = GL_TRUE; // Ensure this is set before initializing GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // set our viewport
    glViewport(0, 0, 800, 600);

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

    // Create and initialise the renderer
    Renderer renderer;
    renderer.initialise();

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

        // Rendering scene
        renderer.render();

        // Check for OpenGL errors
        checkGLError("After rendering");

        // Swap buffers
        glfwSwapBuffers(window);

        // Checking events
        glfwPollEvents();
    }

    // And cleanup
    renderer.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
