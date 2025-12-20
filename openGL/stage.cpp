//#define STB_IMAGE_IMPLEMENTATION
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

#include "include/shader.h"
#include "include/camera.h"
#include "include/model_animation.h"
#include "include/lamp_controller.h"
#include "include/ibl.h"

// 窗口设置
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 1200;

// 全局对象
Camera camera(glm::vec3(0.0f, 20.0f, 40.0f));
LampController lampController;
SimpleBoneController boneController;

// 全局状态
static bool bKeyPressedLastFrame = true;  // 添加这一行
bool bloom = true;
bool bloomKeyPressed = false;
float exposure = 1.0f;
bool resetMouseNextFrame = false;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool lampControlMode = false;
bool lampKeyPressed = false;
int currentLampIndex = 0;
bool isAnimating = false;
bool useBones = true;
float currentRotationX = 0.0f;
float currentRotationY = 0.0f;
float cameraSpeed = 10.0f;  // 相机速度

// 回调函数
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

IBLMaps ibl;

// 添加Bloom相关的全局变量
unsigned int hdrFBO = 0;
unsigned int colorBuffers[2] = { 0, 0 };
unsigned int rboDepth = 0;
unsigned int pingpongFBO[2] = { 0, 0 };
unsigned int pingpongBuffer[2] = { 0, 0 };
unsigned int quadVAO = 0, quadVBO = 0;

Shader* bloomShader = nullptr;
Shader* blurShader = nullptr;
Shader* combineShader = nullptr;

// 创建全屏四边形
void SetupQuad() {
    float quadVertices[] = {
        // 位置       // 纹理坐标
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

// 创建Bloom所需的帧缓冲
void SetupBloomBuffers(int width, int height) {
    // 创建HDR帧缓冲
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    // 创建两个颜色附件
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }

    // 创建深度缓冲
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    // 设置要绘制的颜色附件
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 创建Ping-pong帧缓冲用于高斯模糊
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongBuffer);

    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongBuffer[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongBuffer[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Ping-pong framebuffer not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// 渲染全屏四边形
void RenderQuad() {
    if (quadVAO == 0) SetupQuad();
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}


struct PBRTextures {
    unsigned int albedo;
    unsigned int normal;
    unsigned int metallic;
    unsigned int roughness;
    unsigned int ao;
};

// 创建纯白纹理
unsigned int CreateWhiteTexture() {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    unsigned char whiteData[] = { 255, 255, 255 };

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, whiteData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return textureID;
}

// 加载PBR纹理集
PBRTextures LoadPBRTextures() {
    PBRTextures tex;

    auto loadTex = [](const std::string& path) -> unsigned int {
        unsigned int textureID;
        glGenTextures(1, &textureID);

        int width, height, nrComponents;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrComponents, 0);
        if (data) {
            GLenum format;
            if (nrComponents == 1) format = GL_RED;
            else if (nrComponents == 3) format = GL_RGB;
            else if (nrComponents == 4) format = GL_RGBA;

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            stbi_image_free(data);
            std::cout << "Loaded: " << path << " (" << width << "x" << height << ")" << std::endl;
        }
        else {
            std::cout << "ERROR: Failed to load texture: " << path << std::endl;
            stbi_image_free(data);
            return 0;
        }

        return textureID;
        };

    std::string basePath = "assets/texture/Metal052A_2K";

    tex.albedo = loadTex(basePath + "-PNG_Color.png");
    tex.normal = loadTex(basePath + "-PNG_NormalGL.png");
    tex.metallic = loadTex(basePath + "-PNG_Metalness.png");
    tex.roughness = loadTex(basePath + "-PNG_Roughness.png");
    tex.ao = CreateWhiteTexture();

    return tex;
}

// 镜球位置，在全局变量区域添加
glm::vec3 mirrorBallPosition = glm::vec3(0.0f, 25.0f, 0.0f);  // 舞台中央上方25单位
float mirrorBallRotation = 0.0f;  // 初始旋转角度
float rotationSpeed = 20.0f;  // 旋转速度（度/秒）

int main() {

    // 1. 初始化GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Stage Light Control", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // 2. 初始化GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    std::cout << "[GLAD] OpenGL context ready, version: " << glGetString(GL_VERSION) << std::endl;

    // 3. 加载IBL
    ibl = LoadIBLFromHDR("assets/hdri/citrus_orchard_puresky_2k.hdr");
    // 添加检查
    if (ibl.envCubemap == 0 || ibl.irradianceMap == 0 ||
        ibl.prefilterMap == 0 || ibl.brdfLUTTexture == 0) {
        std::cerr << "[ERROR] IBL textures not created properly!" << std::endl;
        std::cerr << "  envCubemap: " << ibl.envCubemap << std::endl;
        std::cerr << "  irradianceMap: " << ibl.irradianceMap << std::endl;
        std::cerr << "  prefilterMap: " << ibl.prefilterMap << std::endl;
        std::cerr << "  brdfLUTTexture: " << ibl.brdfLUTTexture << std::endl;
        return -1;
    }

    // 4. 设置回调
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // 5. 加载模型
    Model milkbar("assets/milkbar.fbx");
    Model spotlight("assets/shotlight.fbx");
    Model gan("assets/gan.obj");
    //Model musicbox4("assets/musicbox4.fbx");
    Model mirrorBall("assets/disco_ball.glb");
    std::cout << "镜球模型加载成功！面数: "
        << mirrorBall.meshes.size() << " 个网格" << std::endl;

    // 6. 初始化骨骼和灯具
    boneController.SetupFromModel(spotlight);
    lampController.InitializeLamps(spotlight);

    // 7. 加载PBR纹理
    std::cout << "Loading PBR textures..." << std::endl;
    PBRTextures floorTextures = LoadPBRTextures();
    std::cout << "PBR textures loaded successfully!" << std::endl;

    // 8. 加载着色器
    Shader modelShader("openGL/shader/model.vert", "openGL/shader/model.frag");
    Shader skyboxShader("openGL/shader/skybox.vert", "openGL/shader/skybox.frag");
    Shader mirrorBallShader("openGL/shader/mirror_ball.vert", "openGL/shader/mirror_ball.frag");

    // 在加载其他着色器之后，加载Bloom着色器
    std::cout << "Loading Bloom shaders..." << std::endl;
    bloomShader = new Shader("openGL/shader/bloom.vert", "openGL/shader/bloom.frag");
    blurShader = new Shader("openGL/shader/bloom.vert", "openGL/shader/bloom_blur.frag");
    combineShader = new Shader("openGL/shader/bloom.vert", "openGL/shader/bloom_combine.frag");


    // 初始化四边形和帧缓冲
    SetupQuad();
    SetupBloomBuffers(SCR_WIDTH, SCR_HEIGHT);

    // 检查着色器是否成功编译
    if (!modelShader.ID) {
        std::cerr << "[ERROR] Model shader failed to compile!" << std::endl;
        return -1;
    }

    // 9. 地面顶点数据
    float floorVertices[] = {
        -50.0f, 0.0f, -50.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
         50.0f, 0.0f, -50.0f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f,
         50.0f, 0.0f,  50.0f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f,
        -50.0f, 0.0f,  50.0f,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f
    };

    unsigned int floorIndices[] = {
        0, 1, 2,
        2, 3, 0
    };

    // 10. 地面VAO/VBO/EBO
    unsigned int floorVAO, floorVBO, floorEBO;
    glGenVertexArrays(1, &floorVAO);
    glGenBuffers(1, &floorVBO);
    glGenBuffers(1, &floorEBO);

    glBindVertexArray(floorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(floorIndices), floorIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    // 11. 天空盒数据
    unsigned int skyboxVAO, skyboxVBO;
    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // 11. 启用深度测试
    glEnable(GL_DEPTH_TEST);

    // 12. 主渲染循环
    while (!glfwWindowShouldClose(window)) {

        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);

        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // 清屏
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ====== 主渲染 ======

        // ====== 渲染到HDR帧缓冲 ======
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        modelShader.use();

        // 设置相机矩阵
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom * 0.5f),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f,
            1000.0f
        );
        glm::mat4 view = camera.GetViewMatrix();

        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);
        modelShader.setVec3("viewPos", camera.Position);

        // 更新灯光到着色器
        lampController.UpdateLightsInShader(modelShader);

        // 绑定PBR纹理
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, floorTextures.albedo);
        modelShader.setInt("material.albedoMap", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, floorTextures.normal);
        modelShader.setInt("material.normalMap", 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, floorTextures.metallic);
        modelShader.setInt("material.metallicMap", 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, floorTextures.roughness);
        modelShader.setInt("material.roughnessMap", 3);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, floorTextures.ao);
        modelShader.setInt("material.aoMap", 4);

        //// 绑定IBL纹理
        modelShader.setInt("irradianceMap", 5);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_CUBE_MAP, ibl.irradianceMap);

        modelShader.setInt("prefilterMap", 6);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_CUBE_MAP, ibl.prefilterMap);

        modelShader.setInt("brdfLUT", 7);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, ibl.brdfLUTTexture);


        // A. 渲染地面
        glm::mat4 floorModel = glm::mat4(1.0f);
        floorModel = glm::translate(floorModel, glm::vec3(0.0f, -10.0f, 12.0f));
        modelShader.setMat4("model", floorModel);

        glBindVertexArray(floorVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // B. 渲染所有灯具
        modelShader.setBool("useBones", useBones);
        for (int i = 0; i < NUM_LAMPS; i++) {
            //for (int i = 0; i <1; i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, lampController.lampPositions[i]);
            model = glm::scale(model, glm::vec3(4.0f));
            model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            if (i >= NUM_LAMPS / 2)
                model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));

            modelShader.setMat4("model", model);

            // 更新骨骼动画
            if (isAnimating && i == currentLampIndex) {
                lampController.lampBoneControllers[i].RotateBone("LampBone",
                    lampController.lampRotationX[currentLampIndex],
                    lampController.lampRotationY[currentLampIndex]);
            }

            // 设置骨骼矩阵
            auto boneTransforms = lampController.lampBoneControllers[i].GetBoneMatrices();
            for (int j = 0; j < boneTransforms.size(); ++j) {
                //for (int j = 0; j < 1; ++j) {
                std::string name = "boneMatrices[" + std::to_string(j) + "]";
                modelShader.setMat4(name, boneTransforms[j]);
            }

            // 绘制灯具
            spotlight.Draw(modelShader);
        }

        // C. 渲染gan模型
        glm::mat4 ganModel = glm::mat4(1.0f);
        ganModel = glm::scale(ganModel, glm::vec3(8.0f));
        ganModel = glm::translate(ganModel, glm::vec3(0.0f, 2.5f, 3.53f));
        modelShader.setMat4("model", ganModel);
        gan.Draw(modelShader);




        //// 在渲染镜球前设置特殊材质参数
        //modelShader.setFloat("material.metallic", 0.9f);   // 高金属度
        //modelShader.setFloat("material.roughness", 0.2f);  // 低粗糙度（更光滑）

        //mirrorBallShader.use();  // 替换 modelShader.use()

        //// 设置变换矩阵（和以前一样）
        //mirrorBallShader.setMat4("projection", projection);
        //mirrorBallShader.setMat4("view", view);
        //mirrorBallShader.setMat4("model", mirrorModel);
        //// 设置相机位置
        //mirrorBallShader.setVec3("viewPos", camera.Position);

        //// 2. 设置灯光（和modelShader一样）
        //for (int i = 0; i < 8; ++i) {
        //    std::string prefix = "light[" + std::to_string(i) + "].";
        //    mirrorBallShader.setVec3(prefix + "position", lampController.lampPositions[i]);
        //    mirrorBallShader.setVec3(prefix + "direction", lampController.lampLights[i].direction);
        //    mirrorBallShader.setVec3(prefix + "color", lampController.lampLights[i].color);
        //    //mirrorBallShader.setFloat(prefix + "strength", lampController.lampLights[i].intensity);
        //    // 设置其他聚光灯参数...
        //}

        // 3. 设置IBL纹理（和modelShader一样）
        //mirrorBallShader.setInt("irradianceMap", 5);
        //glActiveTexture(GL_TEXTURE5);
        //glBindTexture(GL_TEXTURE_CUBE_MAP, ibl.irradianceMap);

        //mirrorBallShader.setInt("prefilterMap", 6);
        //glActiveTexture(GL_TEXTURE6);
        //glBindTexture(GL_TEXTURE_CUBE_MAP, ibl.prefilterMap);

        //mirrorBallShader.setInt("brdfLUT", 7);
        //glActiveTexture(GL_TEXTURE7);
        //glBindTexture(GL_TEXTURE_2D, ibl.brdfLUTTexture);

        // 4. 设置镜球特有参数
        //mirrorBallShader.setVec3("ballColor", glm::vec3(0.95f, 0.97f, 1.0f)); // 冷白色
        //mirrorBallShader.setFloat("glowIntensity", 2.0f);     // 发光强度
        //mirrorBallShader.setFloat("sparkleIntensity", 1.5f);  // 闪烁强度
        //mirrorBallShader.setFloat("reflectivity", 0.8f);      // 额外反射
        //mirrorBallShader.setFloat("u_time", glfwGetTime());   // 时间

        // +++ 渲染镜球 +++
        mirrorBallRotation += rotationSpeed * deltaTime;  // 更新旋转角度

        glm::mat4 mirrorModel = glm::mat4(1.0f);
        mirrorModel = glm::translate(mirrorModel, glm::vec3(0.0f, 20.0f, 43.0f));  // 位置
        mirrorModel = glm::rotate(mirrorModel, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        mirrorModel = glm::scale(mirrorModel, glm::vec3(0.3f));  // 缩放
        mirrorModel = glm::rotate(mirrorModel, glm::radians(mirrorBallRotation),
            glm::vec3(0.0f, 0.0f, 1.0f));  // 旋转

        //mirrorBallShader.setMat4("model", mirrorModel);
        //mirrorBall.Draw(mirrorBallShader);

        modelShader.setMat4("model", mirrorModel);
        mirrorBall.Draw(modelShader);


        // D. 渲染牛奶吧模型
        modelShader.use();

        glm::mat4 barModel = glm::mat4(1.0f);
        barModel = glm::scale(barModel, glm::vec3(0.01f));
        barModel = glm::rotate(barModel, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        barModel = glm::rotate(barModel, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        barModel = glm::rotate(barModel, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));


        modelShader.setMat4("model", barModel);
        milkbar.Draw(modelShader);



        // ======渲染天空盒（最后渲染） ======
        // 注意：要先禁用深度写入，天空盒应该总是在背景
        glDepthFunc(GL_LEQUAL);  // 深度值<=1.0的都通过
        glDepthMask(GL_FALSE);   // 禁用深度写入

        skyboxShader.use();

        // 移除天空盒的平移部分，只保留旋转
        glm::mat4 skyboxView = glm::mat4(glm::mat3(camera.GetViewMatrix()));

        skyboxShader.setMat4("view", skyboxView);
        skyboxShader.setMat4("projection", projection);
        skyboxShader.setFloat("brightness", 1.0f);  // 可调节亮度

        // 绑定环境立方体贴图
        skyboxShader.setInt("skybox", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, ibl.envCubemap);  // 使用IBL的环境贴图

        // 渲染天空盒
        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);

        // 恢复深度设置
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);


        // ====== 第3步：处理Bloom效果（如果需要） ======
        if (bloom) {
            // 提取高亮区域
            bloomShader->use();
            bloomShader->setFloat("threshold", 1.0f);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, colorBuffers[1]); // 第二个附件是亮色

            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[0]);
            RenderQuad();

            // 高斯模糊（Ping-pong技术）
            blurShader->use();
            unsigned int amount = 10;
            bool horizontal = true, first_iteration = true;

            for (unsigned int i = 0; i < amount; i++) {
                glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
                blurShader->setBool("horizontal", horizontal);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, first_iteration ? pingpongBuffer[!horizontal] : pingpongBuffer[!horizontal]);
                RenderQuad();
                horizontal = !horizontal;
                if (first_iteration) first_iteration = false;
            }

            // ====== 第4步：合并Bloom效果 ======
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            combineShader->use();
            combineShader->setFloat("exposure", exposure);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, colorBuffers[0]); // 场景颜色
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, pingpongBuffer[!horizontal]); // 模糊后的Bloom
            RenderQuad();
        }
        else {
            // ====== 第4步：如果不使用Bloom，直接渲染HDR场景 ======
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // 使用一个简单的着色器来渲染HDR纹理
            combineShader->use();
            combineShader->setFloat("exposure", exposure);
            combineShader->setInt("bloomTexture", 1); // 使用空的Bloom纹理

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, colorBuffers[0]); // 场景颜色

            // 创建一个空的纹理用于Bloom通道（当Bloom关闭时）
            static unsigned int emptyTexture = 0;
            if (emptyTexture == 0) {
                glGenTextures(1, &emptyTexture);
                glBindTexture(GL_TEXTURE_2D, emptyTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1, 1, 0, GL_RGBA, GL_FLOAT, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, emptyTexture);
            RenderQuad();
        }

        // 交换缓冲区和轮询事件
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 清理
    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // ====== 灯具控制模式切换 ======
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS && !lampKeyPressed) {
        lampControlMode = !lampControlMode;
        isAnimating = lampControlMode;
        std::cout << (lampControlMode ? "进入灯具控制模式" : "退出灯具控制模式") << std::endl;
        lampKeyPressed = true;

        if (lampControlMode) {
            glfwSetCursorPos(window, SCR_WIDTH / 2, SCR_HEIGHT / 2);
            resetMouseNextFrame = true;
        }
    }

    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_RELEASE) {
        lampKeyPressed = false;
    }

    // ====== 数字键选择灯具 ======
    for (int i = 0; i < NUM_LAMPS; i++) {
        if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS) {
            currentLampIndex = i;
            std::cout << "切换到灯具: " << (currentLampIndex + 1)
                << " 角度(X:" << lampController.lampRotationX[currentLampIndex]
                << " Y:" << lampController.lampRotationY[currentLampIndex] << ")" << std::endl;
        }
    }

    // ====== 相机控制 ======
    if (!lampControlMode) {
        float velocity = cameraSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.ProcessKeyboard(FORWARD, velocity);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.ProcessKeyboard(BACKWARD, velocity);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.ProcessKeyboard(LEFT, velocity);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.ProcessKeyboard(RIGHT, velocity);

        // 高度限制
        float groundLevel = -10.0f;
        float minCameraHeight = groundLevel + 1.0f;

        if (camera.Position.y < minCameraHeight) {
            camera.Position.y = minCameraHeight;
        }

        // 下降
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            camera.Position.y -= velocity * 2.0f;
            if (camera.Position.y < minCameraHeight) {
                camera.Position.y = minCameraHeight;
            }
        }

        // 上升
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            camera.Position.y += velocity * 2.0f;
        }
    }

    // Bloom控制 B开启Bloom效果，N关闭Bloom效果，Q降低曝光度，E增加曝光度
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {
        //bKeyPressedLastFrame = false;
        if (!bKeyPressedLastFrame) {
            bloom = true;
            std::cout << "Bloom: " << (bloom ? "ON" : "OFF") << std::endl;
            bKeyPressedLastFrame = true;

        }
    }
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) {
        //bKeyPressedLastFrame = false;
        if (bKeyPressedLastFrame) {
            bloom = false;
            std::cout << "Bloom: " << (bloom ? "ON" : "OFF") << std::endl;
            bKeyPressedLastFrame = false;
        }
    }

    // 曝光度调整
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        exposure -= deltaTime * 2.0f;
        if (exposure < 0.1f) exposure = 0.1f;
        std::cout << "Exposure: " << exposure << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        exposure += deltaTime * 2.0f;
        if (exposure > 5.0f) exposure = 5.0f;
        std::cout << "Exposure: " << exposure << std::endl;
    }

}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    static bool firstMouse = true;
    static float lastX = SCR_WIDTH / 2.0f;
    static float lastY = SCR_HEIGHT / 2.0f;

    if (resetMouseNextFrame) {
        lastX = SCR_WIDTH / 2.0f;
        lastY = SCR_HEIGHT / 2.0f;
        resetMouseNextFrame = false;
        return;
    }

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
        return;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    if (fabs(xoffset) < 0.1f && fabs(yoffset) < 0.1f) {
        return;
    }

    if (lampControlMode) {
        float sensitivity = 0.5f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        // 更新当前灯具的旋转角度
        lampController.lampRotationX[currentLampIndex] -= xoffset;
        lampController.lampRotationY[currentLampIndex] += yoffset;

        // 限制角度范围
        lampController.lampRotationY[currentLampIndex] = glm::clamp(lampController.lampRotationY[currentLampIndex], -80.0f, 80.0f);
        lampController.lampRotationX[currentLampIndex] = glm::clamp(lampController.lampRotationX[currentLampIndex], -80.0f, 80.0f);

        // 更新骨骼
        lampController.lampBoneControllers[currentLampIndex].RotateBone("LampBone",
            lampController.lampRotationX[currentLampIndex],
            lampController.lampRotationY[currentLampIndex]);

        // 计算新的聚光灯方向
        glm::mat4 rotation = glm::mat4(1.0f);
        rotation = glm::rotate(rotation,
            glm::radians(lampController.lampRotationX[currentLampIndex]),
            glm::vec3(0.0f, 1.0f, 0.0f));
        rotation = glm::rotate(rotation,
            glm::radians(lampController.lampRotationY[currentLampIndex]),
            glm::vec3(1.0f, 0.0f, 0.0f));

        glm::vec3 newDirection = glm::normalize(glm::vec3(rotation * glm::vec4(lampController.dir2, 0.0f)));

        if (currentLampIndex < NUM_LAMPS / 2) {
            newDirection.z = -newDirection.z;
            newDirection.x = -newDirection.x;
        }

        // 更新灯光方向
        if ((abs(lampController.lampRotationY[currentLampIndex] + 45.0f)) > 0.2f)
            lampController.lampLights[currentLampIndex].direction = newDirection;

        std::cout << "旋转灯具 " << (currentLampIndex + 1)
            << " - X: " << lampController.lampRotationX[currentLampIndex]
            << " Y: " << lampController.lampRotationY[currentLampIndex] << std::endl;
    }
    else {
        camera.ProcessMouseMovement(xoffset, yoffset);
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// 在 stage.cpp 文件的最后（scroll_callback函数之后）添加：

void RenderCube()
{
    static unsigned int VAO = 0;
    static unsigned int VBO = 0;
    if (VAO == 0)
    {
        float vertices[] = {
            // 后面
            -1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            // 前面
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,
            // 左面
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            // 右面
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             // 底面
             -1.0f, -1.0f, -1.0f,
              1.0f, -1.0f, -1.0f,
              1.0f, -1.0f,  1.0f,
              1.0f, -1.0f,  1.0f,
             -1.0f, -1.0f,  1.0f,
             -1.0f, -1.0f, -1.0f,
             // 顶面
             -1.0f,  1.0f, -1.0f,
             -1.0f,  1.0f,  1.0f,
              1.0f,  1.0f,  1.0f,
              1.0f,  1.0f,  1.0f,
              1.0f,  1.0f, -1.0f,
             -1.0f,  1.0f, -1.0f,
        };
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glBindVertexArray(VAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    }
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}