APP_NAME = app
BUILD_DIR = ./run
CPP_FILES = ./src/main.cpp ./src/renderer.cpp \
            ./imgui/imgui.cpp ./imgui/imgui_draw.cpp ./imgui/imgui_tables.cpp ./imgui/imgui_widgets.cpp \
            ./imgui/backends/imgui_impl_glfw.cpp ./imgui/backends/imgui_impl_opengl3.cpp

# Compiler and flags
CXX = clang++
CXXFLAGS = -Wall -std=c++17 -DGL_SILENCE_DEPRECATION -DIMGUI_IMPL_OPENGL_LOADER_GLEW

# Get the installation paths using Homebrew
GLFW_PATH = $(shell brew --prefix glfw)
GLEW_PATH = $(shell brew --prefix glew)
GLM_PATH = $(shell brew --prefix glm)
GLUT_PATH = $(shell brew --prefix freeglut)

# Include directories
APP_INCLUDES = -I$(GLFW_PATH)/include -I$(GLEW_PATH)/include -I$(GLM_PATH)/include -I$(GLUT_PATH)/include \
               -I./imgui -I./imgui/backends

# Linker flags and libraries
APP_LINKERS = -L$(GLFW_PATH)/lib \
              -L$(GLEW_PATH)/lib \
              -L$(GLUT_PATH)/lib \
              -lglfw -lGLEW -framework GLUT -framework OpenGL

# Build target
build:
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CPP_FILES) -o $(BUILD_DIR)/$(APP_NAME) $(CXXFLAGS) $(APP_INCLUDES) $(APP_LINKERS)

# Clean target
clean:
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/$(APP_NAME)
