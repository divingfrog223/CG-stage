#ifndef LAMP_CONTROLLER_H
#define LAMP_CONTROLLER_H

#include <vector>
#include <map>
#include <string>
#include <glm/glm.hpp>

#define NUM_LAMPS 8

struct LampLight {
    glm::vec3 position;
    glm::vec3 direction;
    glm::vec3 color;
    float strength;
    float constant;
    float linear;
    float quadratic;
    float cutOff;
    float outerCutOff;
    float softEdge;
};

class SimpleBoneController {
private:
    std::map<std::string, glm::mat4> boneTransforms;
    std::map<std::string, int> boneIDMap;
    int boneCount;

public:
    SimpleBoneController();
    SimpleBoneController(const SimpleBoneController& other);
    void SetupFromModel(class Model& model);
    void RotateBone(const std::string& boneName, float angleX, float angleY, glm::vec3 front = glm::vec3(0.0f, 1.0f, 0.0f));
    std::vector<glm::mat4> GetBoneMatrices();
};

class LampController {
public:
    // 状态
    bool lampControlMode = false;
    bool lampKeyPressed = false;
    int currentLampIndex = 0;
    bool isAnimating = false;

    // 灯具数据
    std::vector<SimpleBoneController> lampBoneControllers;
    std::vector<glm::vec3> lampPositions;
    std::vector<float> lampRotationX;
    std::vector<float> lampRotationY;
    std::vector<LampLight> lampLights;

    // 方向向量
    glm::vec3 dir1 = glm::vec3(0.0f, -1.0f, 1.0f);
    glm::vec3 dir2 = glm::vec3(0.0f, -1.0f, -1.0f);

    void InitializeLamps(class Model& spotlightModel);
    void UpdateLightsInShader(class Shader& shader);

private:
    void InitializeLampPositions();
    void InitializeLampLights();
};

#endif