#include "include/lamp_controller.h"
#include "include/model_animation.h"
#include "include/shader.h"
#include <iostream>

// SimpleBoneController 实现
SimpleBoneController::SimpleBoneController() : boneCount(0) {}
SimpleBoneController::SimpleBoneController(const SimpleBoneController& other) {
    this->boneTransforms = other.boneTransforms;
    this->boneIDMap = other.boneIDMap;
    this->boneCount = other.boneCount;
}

void SimpleBoneController::SetupFromModel(Model& model) {
    auto& boneInfoMap = model.GetBoneInfoMap();
    boneCount = model.GetBoneCount();
    std::cout << "模型包含 " << boneCount << " 个骨骼" << std::endl;

    for (auto& pair : boneInfoMap) {
        std::string boneName = pair.first;
        boneTransforms[boneName] = glm::mat4(1.0f);
        boneIDMap[boneName] = pair.second.id;
    }
}

void SimpleBoneController::RotateBone(const std::string& boneName, float angleX, float angleY, glm::vec3 front) {
    if (boneTransforms.find(boneName) != boneTransforms.end()) {
        glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(angleY), glm::vec3(1.0f, 0.0f, 0.0f));
        rotation = glm::rotate(rotation, glm::radians(angleX), glm::vec3(0.0f, 1.0f, 0.0f));
        boneTransforms[boneName] = rotation;
    }
    else {
        std::cout << "未找到骨骼: " << boneName << std::endl;
    }
}

std::vector<glm::mat4> SimpleBoneController::GetBoneMatrices() {
    std::vector<glm::mat4> matrices(100, glm::mat4(1.0f));
    for (auto& pair : boneTransforms) {
        std::string boneName = pair.first;
        if (boneIDMap.find(boneName) != boneIDMap.end()) {
            int boneID = boneIDMap[boneName];
            if (boneID >= 0 && boneID < 100) {
                matrices[boneID] = pair.second;
            }
        }
    }
    return matrices;
}

// LampController 实现
void LampController::InitializeLampPositions() {
    for (float i = 0; i < 4; i++)
        lampPositions[i] = glm::vec3(-18.0f + 11 * i, 20.0f, 30.0f);
    for (float i = 4; i < 8; i++)
        lampPositions[i] = glm::vec3(-18.0f + 11 * (i - 4), 20.0f, 55.5f);
}

void LampController::InitializeLampLights() {
    for (int i = 0; i < NUM_LAMPS; i++) {
        lampLights[i].position = lampPositions[i];

        if (i < NUM_LAMPS / 2)
            lampLights[i].direction = glm::normalize(dir1);
        else
            lampLights[i].direction = glm::normalize(dir2);

        if (i < 2) lampLights[i].color = glm::vec3(1.0f, 0.2f, 0.2f);
        else if (i < 4) lampLights[i].color = glm::vec3(0.2f, 1.0f, 0.2f);
        else if (i < 6) lampLights[i].color = glm::vec3(0.2f, 0.2f, 1.0f);
        else lampLights[i].color = glm::vec3(1.0f, 1.0f, 1.0f);

        lampLights[i].strength = 500000.0f;
        lampLights[i].constant = 1.0f;
        lampLights[i].linear = 0.03f;
        lampLights[i].quadratic = 0.004f;
        lampLights[i].cutOff = glm::cos(glm::radians(25.0f));
        lampLights[i].outerCutOff = glm::cos(glm::radians(45.0f));
        lampLights[i].softEdge = 0.3f;
    }
}

void LampController::InitializeLamps(Model& spotlightModel) {
    lampBoneControllers.resize(NUM_LAMPS);
    lampPositions.resize(NUM_LAMPS);
    lampRotationX.resize(NUM_LAMPS, 0.0f);
    lampRotationY.resize(NUM_LAMPS, 0.0f);
    lampLights.resize(NUM_LAMPS);

    InitializeLampPositions();

    for (int i = 0; i < NUM_LAMPS; i++) {
        lampBoneControllers[i].SetupFromModel(spotlightModel);
    }

    InitializeLampLights();
}

void LampController::UpdateLightsInShader(Shader& shader) {
    for (unsigned int i = 0; i < lampLights.size(); i++) {
        shader.setVec3("light[" + std::to_string(i) + "].position", lampLights[i].position);
        shader.setVec3("light[" + std::to_string(i) + "].direction", lampLights[i].direction);
        shader.setVec3("light[" + std::to_string(i) + "].color", lampLights[i].color);
        shader.setFloat("light[" + std::to_string(i) + "].strength", lampLights[i].strength);
        shader.setFloat("light[" + std::to_string(i) + "].constant", lampLights[i].constant);
        shader.setFloat("light[" + std::to_string(i) + "].linear", lampLights[i].linear);
        shader.setFloat("light[" + std::to_string(i) + "].quadratic", lampLights[i].quadratic);
        shader.setFloat("light[" + std::to_string(i) + "].cutOff", lampLights[i].cutOff);
        shader.setFloat("light[" + std::to_string(i) + "].outerCutOff", lampLights[i].outerCutOff);
        shader.setFloat("light[" + std::to_string(i) + "].softEdge", lampLights[i].softEdge);
    }
}