#pragma once
#include <set>
#include <vector>
#include <string>
#include <map>
#include <glm/glm.hpp>

struct ChunkMesh {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    int vertexCount = 0;
};

struct TerrainSettings {
    // Noise frequency for the large-scale landmasses (lower -> wider features)
    float continentFreq = 0.0018f;
    // Noise frequency for finer surface detail (higher -> more local variation)
    float detailFreq = 0.0060f;
    // Blend weights between the low-frequency and high-frequency noise
    float continentWeight = 0.85f;
    float detailWeight = 0.15f;
    // Smoothing kernel weights (center, edges, diagonals) to reduce jagged steps
    float smoothingCenterWeight = 4.0f;
    float smoothingEdgeWeight = 2.0f;
    float smoothingDiagWeight = 1.0f;
    // Exponent applied to the normalized height to shape slopes/plateaus (<1 flattens)
    float heightCurve = 0.98f;
    // Fractions of CHUNK_HEIGHT used for base and variable height range
    float baseHeightFraction = 0.34f;
    float heightRangeFraction = 0.32f;
};

class Renderer {
public:
    void initialise();
    void render();
    void cleanup();
    std::vector<float> loadHeightMap(const std::string& filePath, int& width, int& height);
    std::vector<float> generateTerrainVertices(const std::vector<float>& heightMap, int width, int height);
    void setViewportSize(int width, int height);
    void updateVisitedChunks(const std::pair<int, int>& chunk);
    std::pair<int, int> getCurrentChunk(float cameraX, float cameraZ);
    unsigned int loadShaders(const char* vertexPath, const char* fragmentPath);
    static void setTerrainSettings(const TerrainSettings& settings);
    static TerrainSettings getTerrainSettings();
    void clearChunksAndMeshes();
    void reseedNoise();

private:
    void generateChunk(const std::pair<int, int>& chunk);
    void buildChunkMesh(const std::pair<int, int>& chunk);
    unsigned int getBlockAt(int worldX, int worldY, int worldZ, bool generateMissing = true);
    void renderDepthPass(const glm::mat4& lightSpace);

    unsigned int cubeVBO = 0;
    unsigned int cubeVAO = 0;
    unsigned int shaderProgram = 0;
    unsigned int depthShaderProgram = 0;
    unsigned int depthMapFBO = 0;
    unsigned int depthMap = 0;
    int viewportWidth = 800;
    int viewportHeight = 600;
    std::set<std::pair<int, int>> visitedChunks;
    std::map<std::pair<int, int>, std::vector<uint8_t>> chunkData;
    std::map<std::pair<int, int>, ChunkMesh> chunkMeshes;
    static TerrainSettings terrainSettings;
};
