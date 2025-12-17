#pragma once
#include <set>
#include <vector>
#include <string>
#include <map>

struct ChunkMesh {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    int vertexCount = 0;
};

class Renderer {
public:
    void initialise();
    void render();
    void cleanup();
    std::vector<float> loadHeightMap(const std::string& filePath, int& width, int& height);
    std::vector<float> generateTerrainVertices(const std::vector<float>& heightMap, int width, int height);
    void updateVisitedChunks(const std::pair<int, int>& chunk);
    std::pair<int, int> getCurrentChunk(float cameraX, float cameraZ);
    unsigned int loadShaders(const char* vertexPath, const char* fragmentPath);

private:
    void generateChunk(const std::pair<int, int>& chunk);
    void buildChunkMesh(const std::pair<int, int>& chunk);
    unsigned int getBlockAt(int worldX, int worldY, int worldZ);

    unsigned int cubeVBO = 0;
    unsigned int cubeVAO = 0;
    unsigned int shaderProgram = 0;
    std::set<std::pair<int, int>> visitedChunks;
    std::map<std::pair<int, int>, std::vector<uint8_t>> chunkData;
    std::map<std::pair<int, int>, ChunkMesh> chunkMeshes;
};
