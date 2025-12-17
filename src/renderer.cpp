#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <GL/glew.h>
#include <OpenGL/gl.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include "renderer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "camera.h"
#include <set>
#include <vector>
#include <cstdint>
#define CHUNK_SIZE 8 // Smaller chunk size to reduce the number of cubes

extern Camera camera;

void Renderer::initialise() {
    // Define vertices for a 3D cube (counter-clockwise order)
    float vertices[] = {
        // Front (+Z)
        -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,

        // Back (-Z)
        -0.5f, -0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,   0.5f, -0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,

        // Left (-X)
        -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,

        // Right (+X)
         0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f, -0.5f, -0.5f,

        // Top (+Y)
        -0.5f,  0.5f, -0.5f,  -0.5f,  0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,

        // Bottom (-Y)
        -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f, -0.5f, -0.5f
    };

    // Generate and bind Vertex Array Object (VAO) for the cube
    glGenVertexArrays(1, &cubeVAO);
    glBindVertexArray(cubeVAO);

    // Generate and bind the Vertex Buffer Object (VBO) for the cube
    glGenBuffers(1, &cubeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Define vertex attribute pointer for the cube
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Load and compile shaders
    shaderProgram = loadShaders("shaders/vertexShader.vert", "shaders/fragmentShader.frag");
    if (shaderProgram == 0) {
        std::cerr << "Failed to load shaders." << std::endl;
        return;
    }

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS); // Default depth test function

    // Enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Verify that depth testing and face culling are enabled
    GLint depthTestEnabled, cullFaceEnabled;
    glGetIntegerv(GL_DEPTH_TEST, &depthTestEnabled);
    glGetIntegerv(GL_CULL_FACE, &cullFaceEnabled);
    std::cout << "Depth Test Enabled: " << (depthTestEnabled ? "Yes" : "No") << std::endl;
    std::cout << "Face Culling Enabled: " << (cullFaceEnabled ? "Yes" : "No") << std::endl;

    // Verify shader program
    if (shaderProgram == 0) {
        std::cerr << "Shader program failed to load." << std::endl;
    } else {
        std::cout << "Shader program loaded successfully." << std::endl;
    }

    // Verify vertex data for the cube
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    void* ptr = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
    if (ptr) {
        std::cout << "Vertex data loaded successfully." << std::endl;
        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        std::cerr << "Failed to load vertex data." << std::endl;
    }

    // Unbind the VBO and VAO for the cube to prevent accidental modifications
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Renderer::render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Using shader program
    glUseProgram(shaderProgram);

    // View matrix from the camera
    glm::mat4 view = camera.GetViewMatrix();

    // Projection matrix: perspective projection
    // Increase far plane to cover the full terrain extents
    glm::mat4 project = glm::perspective(glm::radians(45.0f), (float)800 / (float)600, 0.1f, 2000.0f);

    // Set the matrices in the shader
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint colorLoc = glGetUniformLocation(shaderProgram, "color");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(project));

    // Render the cubes with backface culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glBindVertexArray(cubeVAO);

    // Mark every voxel filled; swap this to real world data later
    std::vector<uint8_t> occupancy(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE, 1);
    auto isFilled = [&](int lx, int ly, int lz) -> bool {
        if (lx < 0 || lx >= CHUNK_SIZE || ly < 0 || ly >= CHUNK_SIZE || lz < 0 || lz >= CHUNK_SIZE) {
            return false; // outside the chunk is air
        }
        return occupancy[(ly * CHUNK_SIZE + lz) * CHUNK_SIZE + lx] != 0;
    };

    auto drawFaceRange = [](int offset) {
        glDrawArrays(GL_TRIANGLES, offset, 6);
    };

    for (const auto& chunk : visitedChunks) {
        int chunkMinX = chunk.first * CHUNK_SIZE;
        int chunkMinZ = chunk.second * CHUNK_SIZE;

        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                    if (!isFilled(lx, ly, lz)) {
                        continue;
                    }

                    bool drawFront = !isFilled(lx, ly, lz + 1);
                    bool drawBack = !isFilled(lx, ly, lz - 1);
                    bool drawLeft = !isFilled(lx - 1, ly, lz);
                    bool drawRight = !isFilled(lx + 1, ly, lz);
                    bool drawTop = !isFilled(lx, ly + 1, lz);
                    bool drawBottom = !isFilled(lx, ly - 1, lz);

                    if (!(drawFront || drawBack || drawLeft || drawRight || drawTop || drawBottom)) {
                        continue;
                    }

                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(chunkMinX + lx, ly, chunkMinZ + lz));
                    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

                    // Solid color fill
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                    glUniform4f(colorLoc, 0.0f, 0.6f, 0.2f, 1.0f);

                    if (drawFront) drawFaceRange(0);   // +Z
                    if (drawBack) drawFaceRange(6);    // -Z
                    if (drawLeft) drawFaceRange(12);   // -X
                    if (drawRight) drawFaceRange(18);  // +X
                    if (drawTop) drawFaceRange(24);    // +Y
                    if (drawBottom) drawFaceRange(30); // -Y

                    // Wireframe overlay for cube outlines
                    glEnable(GL_POLYGON_OFFSET_LINE);
                    glPolygonOffset(-1.0f, -1.0f); // pull lines toward camera to reduce z-fighting
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    glUniform4f(colorLoc, 0.9f, 0.9f, 0.9f, 1.0f);

                    if (drawFront) drawFaceRange(0);   // +Z
                    if (drawBack) drawFaceRange(6);    // -Z
                    if (drawLeft) drawFaceRange(12);   // -X
                    if (drawRight) drawFaceRange(18);  // +X
                    if (drawTop) drawFaceRange(24);    // +Y
                    if (drawBottom) drawFaceRange(30); // -Y

                    glDisable(GL_POLYGON_OFFSET_LINE);
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                }
            }
        }
    }

    glBindVertexArray(0);

    // Debug: Check for OpenGL errors
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL error during rendering: " << err << std::endl;
    }
}

void Renderer::updateVisitedChunks(const std::pair<int, int>& chunk) {
    visitedChunks.clear();
    visitedChunks.insert(chunk); // Only render the current chunk to keep count low
}

std::pair<int, int> Renderer::getCurrentChunk(float cameraX, float cameraZ) {
    int chunkX = static_cast<int>(std::floor(cameraX / CHUNK_SIZE));
    int chunkZ = static_cast<int>(std::floor(cameraZ / CHUNK_SIZE));
    return std::make_pair(chunkX, chunkZ);
}

void Renderer::cleanup() {
    // Good practice to clean up :)
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteProgram(shaderProgram);
}

unsigned int Renderer::loadShaders(const char* vertexPath, const char* fragmentPath) {
    // Helper function so we can load shaders more neatly
    std::string vertexCode, fragmentCode;
    std::ifstream vShaderFile, fShaderFile;

    vShaderFile.open(vertexPath);
    fShaderFile.open(fragmentPath);

    std::stringstream vShaderStream, fShaderStream;
    vShaderStream << vShaderFile.rdbuf();
    fShaderStream << fShaderFile.rdbuf();

    vertexCode = vShaderStream.str();
    fragmentCode = fShaderStream.str();

    vShaderFile.close();
    fShaderFile.close();

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    unsigned int vertex, fragment;
    int success;
    char infoLog[512];

    // Vertex shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        return 0;
    }

    // Fragment shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        return 0;
    }

    // Shader program
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return 0;
    }

    // Cleaning up the shaders since they're already linked
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return program;
}

std::vector<float> Renderer::loadHeightMap(const std::string& filePath, int& width, int& height) {
    // Load the height map image using an image loading library like stb_image
    int channels;
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &channels, 1);
    if (!data) {
        std::cerr << "Failed to load height map: " << filePath << std::endl;
        return {};
    }

    std::vector<float> heightMap(width * height);
    for (int i = 0; i < width * height; ++i) {
        heightMap[i] = data[i] / 255.0f; // Normalize to [0, 1]
    }

    // Debug: Print some height values
    std::cout << "Height Map Loaded: " << width << "x" << height << std::endl;
    for (int i = 0; i < std::min(100, width * height); i += 10) {
        std::cout << "Height[" << i << "]: " << heightMap[i] << std::endl;
    }

    stbi_image_free(data);
    return heightMap;
}

std::vector<float> Renderer::generateTerrainVertices(const std::vector<float>& heightMap, int width, int height) {
    std::vector<float> vertices;
    for (int z = 0; z < height; ++z) {
        for (int x = 0; x < width; ++x) {
            float y = heightMap[z * width + x];
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }
    }
    return vertices;
}
