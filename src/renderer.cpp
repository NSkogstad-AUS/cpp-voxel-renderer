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
#include <cmath>
#include <map>
#include <algorithm>
#include <random>

// Chunk/world configuration
constexpr int CHUNK_SIZE = 4;
constexpr int CHUNK_HEIGHT = 32;
constexpr int VIEW_DISTANCE = 32;
constexpr int WATER_LEVEL = 10;
constexpr int MAX_CHUNK_BUILDS_PER_FRAME = 32;
constexpr bool DRAW_WIREFRAME = false;
constexpr int SHADOW_MAP_SIZE = 4096;

TerrainSettings Renderer::terrainSettings = TerrainSettings{};

enum class BlockType : uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Water
};

namespace {
inline float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

inline float grad(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float perlin(float x, float y, float z) {
    static const int permutation[] = { 151,160,137,91,90,15,
    131,13,201,95,96,53,194,233,7,225,140,36,103,30,
    69,142,8,99,37,240,21,10,23,190, 6,148,247,120,
    234,75,0,26,197,62,94,252,219,203,117,35,11,32,
    57,177,33,88,237,149,56,87,174,20,125,136,171,
    168, 68,175,74,165,71,134,139,48,27,166,77,146,
    158,231,83,111,229,122,60,211,133,230,220,105,
    92,41,55,46,245,40,244,102,143,54, 65,25,63,161,
    1,216,80,73,209,76,132,187,208,89,18,169,200,
    196,135,130,116,188,159,86,164,100,109,198,173,
    186, 3,64,52,217,226,250,124,123,5,202,38,147,
    118,126,255,82,85,212,207,206,59,227,47,16,58,
    17,182,189,28,42,223,183,170,213,119,248,152,
    2,44,154,163,70,221,153,101,155,167,43,172,9,
    129,22,39,253, 19,98,108,110,79,113,224,232,178,
    185, 112,104,218,246,97,228,251,34,242,193,238,
    210,144,12,191,179,162,241,81,51,145,235,249,
    14,239,107,49,192,214,31,181,199,106,157,184,
    84,204,176,115,121,50,45,127, 4,150,254,138,
    236,205,93,222,114,67,29,24,72,243,141,128,195,
    78,66,215,61,156,180 };

    static int p[512];
    static bool initialized = false;
    if (!initialized) {
        for (int i = 0; i < 256; ++i) {
            p[256 + i] = p[i] = permutation[i];
        }
        initialized = true;
    }

    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);

    float u = fade(x);
    float v = fade(y);
    float w = fade(z);

    int A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z;
    int B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;

    float res = lerp(w, lerp(v, lerp(u, grad(p[AA], x, y, z),
                                    grad(p[BA], x - 1, y, z)),
                                lerp(u, grad(p[AB], x, y - 1, z),
                                    grad(p[BB], x - 1, y - 1, z))),
                        lerp(v, lerp(u, grad(p[AA + 1], x, y, z - 1),
                                    grad(p[BA + 1], x - 1, y, z - 1)),
                                lerp(u, grad(p[AB + 1], x, y - 1, z - 1),
                                    grad(p[BB + 1], x - 1, y - 1, z - 1))));
    return res;
}

float octavePerlin(float x, float y, float z, int octaves, float persistence) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        total += perlin(x * frequency, y * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / maxValue;
}

// Run-time seed to randomize terrain each launch
float noiseOffsetX = 0.0f;
float noiseOffsetZ = 0.0f;
bool noiseSeeded = false;
} // namespace

void Renderer::setTerrainSettings(const TerrainSettings& settings) {
    terrainSettings = settings;
}

TerrainSettings Renderer::getTerrainSettings() {
    return terrainSettings;
}

void Renderer::setViewportSize(int width, int height) {
    viewportWidth = std::max(1, width);
    viewportHeight = std::max(1, height);
}

void Renderer::clearChunksAndMeshes() {
    for (auto& entry : chunkMeshes) {
        if (entry.second.vao) glDeleteVertexArrays(1, &entry.second.vao);
        if (entry.second.vbo) glDeleteBuffers(1, &entry.second.vbo);
    }
    chunkMeshes.clear();
    chunkData.clear();
    visitedChunks.clear();
}

void Renderer::reseedNoise() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-10000.0f, 10000.0f);
    noiseOffsetX = dist(gen);
    noiseOffsetZ = dist(gen);
    clearChunksAndMeshes();
}

extern Camera camera;

void Renderer::initialise() {
    if (!noiseSeeded) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-10000.0f, 10000.0f);
        noiseOffsetX = dist(gen);
        noiseOffsetZ = dist(gen);
        noiseSeeded = true;
    }

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
    depthShaderProgram = loadShaders("shaders/shadowDepth.vert", "shaders/shadowDepth.frag");
    if (shaderProgram == 0) {
        std::cerr << "Failed to load shaders." << std::endl;
        return;
    }
    if (depthShaderProgram == 0) {
        std::cerr << "Failed to load depth shaders." << std::endl;
        return;
    }

    // Set up shadow map framebuffer
    glGenFramebuffers(1, &depthMapFBO);
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Depth framebuffer not complete!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::cout << "World settings -> CHUNK_SIZE: " << CHUNK_SIZE
              << ", CHUNK_HEIGHT: " << CHUNK_HEIGHT
              << ", VIEW_DISTANCE: " << VIEW_DISTANCE
              << ", MAX_CHUNK_BUILDS_PER_FRAME: " << MAX_CHUNK_BUILDS_PER_FRAME
              << ", noise offsets: (" << noiseOffsetX << ", " << noiseOffsetZ << ")"
              << std::endl;

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS); // Default depth test function

    // Enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Enable blending so translucent blocks can render (e.g. water later)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

    // Update viewport in case the window was resized
    glViewport(0, 0, viewportWidth, viewportHeight);

    // View/projection from camera
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 project = glm::perspective(glm::radians(45.0f), static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight), 0.1f, 2000.0f);

    // Directional light setup
    glm::vec3 lightDir = glm::normalize(glm::vec3(-0.5f, -1.2f, -0.3f));
    float shadowRange = 80.0f;
    glm::vec3 lightPos = camera.Position - lightDir * 50.0f;
    glm::mat4 lightProj = glm::ortho(-shadowRange, shadowRange, -shadowRange, shadowRange, 1.0f, 200.0f);
    glm::mat4 lightView = glm::lookAt(lightPos, camera.Position, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightSpace = lightProj * lightView;

    // Build/generate a limited number of missing chunks per frame
    int buildsThisFrame = 0;
    for (const auto& chunk : visitedChunks) {
        bool hasData = chunkData.find(chunk) != chunkData.end();
        bool hasMesh = chunkMeshes.find(chunk) != chunkMeshes.end();
        bool needsBuild = !hasData || !hasMesh;

        if (needsBuild && buildsThisFrame >= MAX_CHUNK_BUILDS_PER_FRAME) {
            continue; // defer generation/meshing to later frames
        }

        if (!hasData) {
            generateChunk(chunk);
        }
        if (!hasMesh) {
            buildChunkMesh(chunk);
        }
        if (needsBuild) {
            buildsThisFrame++;
        }
    }

    // Depth pass
    renderDepthPass(lightSpace);

    // Main pass
    glUseProgram(shaderProgram);
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint lightSpaceLoc = glGetUniformLocation(shaderProgram, "lightSpaceMatrix");
    GLint lightDirLoc = glGetUniformLocation(shaderProgram, "lightDir");
    GLint lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");
    GLint ambientColorLoc = glGetUniformLocation(shaderProgram, "ambientColor");
    GLint shadowMapLoc = glGetUniformLocation(shaderProgram, "shadowMap");
    GLint shadowTexelSizeLoc = glGetUniformLocation(shaderProgram, "shadowTexelSize");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(project));
    glUniformMatrix4fv(lightSpaceLoc, 1, GL_FALSE, glm::value_ptr(lightSpace));
    glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));
    glm::vec3 lightColor(1.0f, 0.95f, 0.9f);
    glm::vec3 ambientColor(0.2f, 0.2f, 0.22f);
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));
    glUniform3fv(ambientColorLoc, 1, glm::value_ptr(ambientColor));
    glUniform1i(shadowMapLoc, 0);
    glUniform2f(shadowTexelSizeLoc, 1.0f / SHADOW_MAP_SIZE, 1.0f / SHADOW_MAP_SIZE);
    glm::mat4 identityModel(1.0f);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(identityModel));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthMap);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    for (const auto& chunk : visitedChunks) {
        auto meshIt = chunkMeshes.find(chunk);
        if (meshIt == chunkMeshes.end() || meshIt->second.vertexCount == 0) {
            continue;
        }
        glBindVertexArray(meshIt->second.vao);
        glDrawArrays(GL_TRIANGLES, 0, meshIt->second.vertexCount);
    }

    glBindVertexArray(0);

    // Clean up meshes and data no longer in view to keep memory/draw list small
    std::vector<std::pair<int, int>> toRemove;
    for (const auto& entry : chunkMeshes) {
        if (visitedChunks.find(entry.first) == visitedChunks.end()) {
            toRemove.push_back(entry.first);
        }
    }
    for (const auto& key : toRemove) {
        auto it = chunkMeshes.find(key);
        if (it != chunkMeshes.end()) {
            if (it->second.vao) glDeleteVertexArrays(1, &it->second.vao);
            if (it->second.vbo) glDeleteBuffers(1, &it->second.vbo);
            chunkMeshes.erase(it);
        }
        chunkData.erase(key);
    }
}

void Renderer::renderDepthPass(const glm::mat4& lightSpace) {
    // Depth-only pass from light POV
    glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_BACK);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f); // push depth slightly to reduce gaps

    glUseProgram(depthShaderProgram);
    GLint lightSpaceLoc = glGetUniformLocation(depthShaderProgram, "lightSpaceMatrix");
    GLint modelLoc = glGetUniformLocation(depthShaderProgram, "model");
    glm::mat4 identityModel(1.0f);
    glUniformMatrix4fv(lightSpaceLoc, 1, GL_FALSE, glm::value_ptr(lightSpace));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(identityModel));

    for (const auto& chunk : visitedChunks) {
        auto meshIt = chunkMeshes.find(chunk);
        if (meshIt == chunkMeshes.end() || meshIt->second.vertexCount == 0) {
            continue;
        }
        glBindVertexArray(meshIt->second.vao);
        glDrawArrays(GL_TRIANGLES, 0, meshIt->second.vertexCount);
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glCullFace(GL_BACK);

    // Restore viewport for main pass
    glViewport(0, 0, viewportWidth, viewportHeight);
}

void Renderer::generateChunk(const std::pair<int, int>& chunk) {
    if (chunkData.find(chunk) != chunkData.end()) {
        return;
    }

    std::vector<uint8_t> blocks(CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE, static_cast<uint8_t>(BlockType::Air));
    auto blockIndex = [](int lx, int ly, int lz) {
        return (ly * CHUNK_SIZE + lz) * CHUNK_SIZE + lx;
    };

    int chunkMinX = chunk.first * CHUNK_SIZE;
    int chunkMinZ = chunk.second * CHUNK_SIZE;

    auto heightNoise = [&](float wx, float wz) {
        float x = wx + noiseOffsetX;
        float z = wz + noiseOffsetZ;
        float continent = octavePerlin(x * terrainSettings.continentFreq, z * terrainSettings.continentFreq, 0.0f, 4, 0.5f);
        float detail = octavePerlin(x * terrainSettings.detailFreq, z * terrainSettings.detailFreq, 0.0f, 3, 0.6f);
        return continent * terrainSettings.continentWeight + detail * terrainSettings.detailWeight;
    };

    std::vector<int> heights(CHUNK_SIZE * CHUNK_SIZE, 0);

    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
        int worldX = chunkMinX + lx;
        for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
            int worldZ = chunkMinZ + lz;

            float hCenter = heightNoise(worldX + 0.5f, worldZ + 0.5f);
            float hN = heightNoise(worldX + 0.5f, worldZ - 0.8f);
            float hS = heightNoise(worldX + 0.5f, worldZ + 1.8f);
            float hE = heightNoise(worldX + 1.8f, worldZ + 0.5f);
            float hW = heightNoise(worldX - 0.8f, worldZ + 0.5f);
            float hNE = heightNoise(worldX + 1.8f, worldZ - 0.8f);
            float hNW = heightNoise(worldX - 0.8f, worldZ - 0.8f);
            float hSE = heightNoise(worldX + 1.8f, worldZ + 1.8f);
            float hSW = heightNoise(worldX - 0.8f, worldZ + 1.8f);

            float sum = hCenter * terrainSettings.smoothingCenterWeight
                      + (hN + hS + hE + hW) * terrainSettings.smoothingEdgeWeight
                      + (hNE + hNW + hSE + hSW) * terrainSettings.smoothingDiagWeight;
            float weight = terrainSettings.smoothingCenterWeight
                         + 4.0f * terrainSettings.smoothingEdgeWeight
                         + 4.0f * terrainSettings.smoothingDiagWeight;
            float blended = sum / weight;
            float heightValue = pow(blended * 0.5f + 0.5f, terrainSettings.heightCurve);

            int baseHeight = static_cast<int>(CHUNK_HEIGHT * terrainSettings.baseHeightFraction);
            int heightRange = static_cast<int>(CHUNK_HEIGHT * terrainSettings.heightRangeFraction);
            int columnHeight = std::clamp(baseHeight + static_cast<int>(std::round(heightValue * heightRange)), 2, CHUNK_HEIGHT - 2);

            // Clamp slope against immediate neighbors to keep chunk borders aligned
            if (lx > 0) {
                int west = heights[lz * CHUNK_SIZE + (lx - 1)];
                columnHeight = std::clamp(columnHeight, west - 2, west + 2);
            }
            if (lz > 0) {
                int north = heights[(lz - 1) * CHUNK_SIZE + lx];
                columnHeight = std::clamp(columnHeight, north - 2, north + 2);
            }

            heights[lz * CHUNK_SIZE + lx] = columnHeight;

            for (int y = 0; y < columnHeight; ++y) {
                BlockType type = BlockType::Stone;
                if (y >= columnHeight - 1) {
                    type = BlockType::Grass;
                } else if (y >= columnHeight - 4) {
                    type = BlockType::Dirt;
                }
                blocks[blockIndex(lx, y, lz)] = static_cast<uint8_t>(type);
            }

            if (columnHeight < WATER_LEVEL) {
                for (int y = columnHeight; y <= WATER_LEVEL && y < CHUNK_HEIGHT; ++y) {
                    blocks[blockIndex(lx, y, lz)] = static_cast<uint8_t>(BlockType::Water);
                }
            }
        }
    }

    chunkData.emplace(chunk, std::move(blocks));
}

unsigned int Renderer::getBlockAt(int worldX, int worldY, int worldZ, bool generateMissing) {
    if (worldY < 0 || worldY >= CHUNK_HEIGHT) {
        return static_cast<unsigned int>(BlockType::Air);
    }

    int chunkX = static_cast<int>(std::floor(static_cast<float>(worldX) / CHUNK_SIZE));
    int chunkZ = static_cast<int>(std::floor(static_cast<float>(worldZ) / CHUNK_SIZE));
    int localX = worldX - chunkX * CHUNK_SIZE;
    int localZ = worldZ - chunkZ * CHUNK_SIZE;

    std::pair<int, int> key(chunkX, chunkZ);
    if (generateMissing) {
        generateChunk(key);
    }
    auto it = chunkData.find(key);
    if (it == chunkData.end()) {
        return static_cast<unsigned int>(BlockType::Air);
    }

    int idx = (worldY * CHUNK_SIZE + localZ) * CHUNK_SIZE + localX;
    if (idx < 0 || idx >= static_cast<int>(it->second.size())) {
        return static_cast<unsigned int>(BlockType::Air);
    }

    return it->second[idx];
}

void Renderer::buildChunkMesh(const std::pair<int, int>& chunk) {
    // Skip if already built
    if (chunkMeshes.find(chunk) != chunkMeshes.end()) {
        return;
    }

    auto chunkIt = chunkData.find(chunk);
    if (chunkIt == chunkData.end()) {
        return;
    }
    const std::vector<uint8_t>& blocks = chunkIt->second;

    auto blockIndex = [](int lx, int ly, int lz) {
        return (ly * CHUNK_SIZE + lz) * CHUNK_SIZE + lx;
    };

    auto blockAt = [&](int worldX, int worldY, int worldZ) -> BlockType {
        if (worldY < 0 || worldY >= CHUNK_HEIGHT) {
            return BlockType::Air;
        }
        // Allow pulling neighbor chunk data so we don't emit faces between solid neighboring chunks
        return static_cast<BlockType>(getBlockAt(worldX, worldY, worldZ, true));
    };

    auto blockColor = [](BlockType type) -> glm::vec4 {
        switch (type) {
            case BlockType::Grass: return glm::vec4(0.2f, 0.7f, 0.2f, 1.0f);
            case BlockType::Dirt:  return glm::vec4(0.45f, 0.27f, 0.12f, 1.0f);
            case BlockType::Stone: return glm::vec4(0.55f, 0.55f, 0.55f, 1.0f);
            case BlockType::Water: return glm::vec4(0.1f, 0.3f, 0.8f, 0.65f);
            case BlockType::Air:
            default:               return glm::vec4(0.0f);
        }
    };

    struct Vertex {
        float x, y, z;
        float r, g, b, a;
        float nx, ny, nz;
    };

    // Face vertex templates (6 faces, 6 vertices each) in local cube space centered at block position
    static const float faceVertices[6][18] = {
        { // +Z (front)
            -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
             0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,  -0.5f, -0.5f,  0.5f
        },
        { // -Z (back)
            -0.5f, -0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
             0.5f,  0.5f, -0.5f,   0.5f, -0.5f, -0.5f,  -0.5f, -0.5f, -0.5f
        },
        { // -X (left)
            -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
            -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f
        },
        { // +X (right)
             0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f,  0.5f,  0.5f,
             0.5f,  0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f, -0.5f, -0.5f
        },
        { // +Y (top)
            -0.5f,  0.5f, -0.5f,  -0.5f,  0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
             0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,  -0.5f,  0.5f, -0.5f
        },
        { // -Y (bottom)
            -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,
             0.5f, -0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f, -0.5f, -0.5f
        }
    };

    const int chunkMinX = chunk.first * CHUNK_SIZE;
    const int chunkMinZ = chunk.second * CHUNK_SIZE;

    std::vector<Vertex> vertices;
    vertices.reserve(20000); // heuristic to avoid reallocations

    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
        for (int ly = 0; ly < CHUNK_HEIGHT; ++ly) {
            for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                BlockType block = static_cast<BlockType>(blocks[blockIndex(lx, ly, lz)]);
                if (block == BlockType::Air) {
                    continue;
                }

                const int worldX = chunkMinX + lx;
                const int worldZ = chunkMinZ + lz;

                bool drawFace[6] = {
                    blockAt(worldX, ly, worldZ + 1) == BlockType::Air, // +Z
                    blockAt(worldX, ly, worldZ - 1) == BlockType::Air, // -Z
                    blockAt(worldX - 1, ly, worldZ) == BlockType::Air, // -X
                    blockAt(worldX + 1, ly, worldZ) == BlockType::Air, // +X
                    blockAt(worldX, ly + 1, worldZ) == BlockType::Air, // +Y
                    blockAt(worldX, ly - 1, worldZ) == BlockType::Air  // -Y
                };

                if (!(drawFace[0] || drawFace[1] || drawFace[2] || drawFace[3] || drawFace[4] || drawFace[5])) {
                    continue;
                }

                glm::vec4 color = blockColor(block);

                for (int face = 0; face < 6; ++face) {
                    if (!drawFace[face]) continue;
                    const float* fv = faceVertices[face];
                    glm::vec3 normal = glm::vec3(0.0f);
                    if (face == 0) normal = glm::vec3(0, 0, 1);
                    else if (face == 1) normal = glm::vec3(0, 0, -1);
                    else if (face == 2) normal = glm::vec3(-1, 0, 0);
                    else if (face == 3) normal = glm::vec3(1, 0, 0);
                    else if (face == 4) normal = glm::vec3(0, 1, 0);
                    else if (face == 5) normal = glm::vec3(0, -1, 0);
                    for (int v = 0; v < 6; ++v) {
                        Vertex vert;
                        vert.x = fv[v * 3 + 0] + worldX;
                        vert.y = fv[v * 3 + 1] + ly;
                        vert.z = fv[v * 3 + 2] + worldZ;
                        vert.r = color.r;
                        vert.g = color.g;
                        vert.b = color.b;
                        vert.a = color.a;
                        vert.nx = normal.x;
                        vert.ny = normal.y;
                        vert.nz = normal.z;
                        vertices.push_back(vert);
                    }
                }
            }
        }
    }

    ChunkMesh mesh;
    mesh.vertexCount = static_cast<int>(vertices.size());
    if (mesh.vertexCount > 0) {
        glGenVertexArrays(1, &mesh.vao);
        glGenBuffers(1, &mesh.vbo);
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(7 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    chunkMeshes.emplace(chunk, mesh);
}

void Renderer::updateVisitedChunks(const std::pair<int, int>& chunk) {
    visitedChunks.clear();
    for (int dx = -VIEW_DISTANCE; dx <= VIEW_DISTANCE; ++dx) {
        for (int dz = -VIEW_DISTANCE; dz <= VIEW_DISTANCE; ++dz) {
            visitedChunks.insert(std::make_pair(chunk.first + dx, chunk.second + dz));
        }
    }
}

std::pair<int, int> Renderer::getCurrentChunk(float cameraX, float cameraZ) {
    int chunkX = static_cast<int>(std::floor(cameraX / CHUNK_SIZE));
    int chunkZ = static_cast<int>(std::floor(cameraZ / CHUNK_SIZE));
    return std::make_pair(chunkX, chunkZ);
}

void Renderer::cleanup() {
    // Good practice to clean up :)
    glDeleteProgram(shaderProgram);
    glDeleteProgram(depthShaderProgram);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    if (depthMap) glDeleteTextures(1, &depthMap);
    if (depthMapFBO) glDeleteFramebuffers(1, &depthMapFBO);
    for (auto& entry : chunkMeshes) {
        if (entry.second.vao) glDeleteVertexArrays(1, &entry.second.vao);
        if (entry.second.vbo) glDeleteBuffers(1, &entry.second.vbo);
    }
    chunkMeshes.clear();
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
