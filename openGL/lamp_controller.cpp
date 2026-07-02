#include "include/lamp_controller.h"
#include "include/model_animation.h"
#include "include/shader.h"
#include <iostream>
# define M_PI 3.1415926
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

        lampLights[i].strength = 20000.0f;
        lampLights[i].constant = 1.0f;
        lampLights[i].linear = 0.03f;
        lampLights[i].quadratic = 0.004f;
        lampLights[i].cutOff = glm::cos(glm::radians(25.0f));
        lampLights[i].outerCutOff = glm::cos(glm::radians(45.0f));
        lampLights[i].softEdge = 0.3f;
    }
}

// 在 InitializeLamps 方法中添加初始化：
void LampController::InitializeLamps(Model& spotlightModel) {
    lampBoneControllers.resize(NUM_LAMPS);
    lampPositions.resize(NUM_LAMPS);
    lampRotationX.resize(NUM_LAMPS, 0.0f);
    lampRotationY.resize(NUM_LAMPS, 0.0f);
    lampLights.resize(NUM_LAMPS);
    lampModes.resize(NUM_LAMPS, STATIC);
    originalStrengths.resize(NUM_LAMPS);
    originalColors.resize(NUM_LAMPS);

    // 改为四个独立的布尔状态数组
    scanEnabled.resize(NUM_LAMPS, false); // 默认关闭扫描
    gradientEnabled.resize(NUM_LAMPS, false);
    breathEnabled.resize(NUM_LAMPS, false);
    isManuallyControlled.resize(NUM_LAMPS, false);

    // 保存状态数组
    savedScanEnabled.resize(NUM_LAMPS, false);
    savedGradientEnabled.resize(NUM_LAMPS, false);
    savedBreathEnabled.resize(NUM_LAMPS, false);

    InitializeLampPositions();

    for (int i = 0; i < NUM_LAMPS; i++) {
        lampBoneControllers[i].SetupFromModel(spotlightModel);
    }

    InitializeLampLights();

    // 保存原始值
    for (int i = 0; i < NUM_LAMPS; i++) {
        originalStrengths[i] = lampLights[i].strength;
        originalColors[i] = lampLights[i].color;
    }
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

// 新增方法实现：
void LampController::SetLightMode(int index, LightMode mode) {
    if (index >= 0 && index < NUM_LAMPS) {
        lampModes[index] = mode;
    }
}

void LampController::SetAllLightMode(LightMode mode) {
    for (int i = 0; i < NUM_LAMPS; i++) {
        lampModes[i] = mode;
    }
}

void LampController::SetGradientSpeed(float speed) {
    gradientSpeed = speed;
}

void LampController::SetBreathSpeed(float speed) {
    breathSpeed = speed;
}

void LampController::SetScanSpeed(float speed) {
    scanSpeed = speed;
}

void LampController::UpdateGradientEffect(int index, float time) {
    // 使用相位差让每盏灯颜色不同
    float phase = index * 0.5f;

    // RGB 循环渐变
    float r = (sin(time * 0.5f + phase) + 1.0f) * 0.5f;
    float g = (sin(time * 0.8f + phase) + 1.0f) * 0.5f;
    float b = (sin(time * 1.2f + phase) + 1.0f) * 0.5f;

    // 渐变颜色（与当前颜色混合）
    glm::vec3 gradientColor = glm::vec3(
        r * 0.8f + 0.2f,
        g * 0.8f + 0.2f,
        b * 0.8f + 0.2f
    );

    // 混合当前颜色和渐变颜色
    lampLights[index].color = lampLights[index].color * gradientColor;

    // 渐变时稍微降低强度
    lampLights[index].strength *= 0.8f;
}

void LampController::UpdateBreathEffect(int index, float time) {
    // 呼吸强度变化 (0.5 ~ 1.5倍)
    float breath = (sin(time * 2.0f) + 1.0f) * 0.25f + 0.5f;

    // 呼吸时颜色也轻微变化（暖色呼吸）
    float warm = (sin(time * 1.5f) + 1.0f) * 0.1f;

    // 混合呼吸效果到当前颜色
    lampLights[index].color = glm::vec3(
        lampLights[index].color.r * (1.0f + warm),
        lampLights[index].color.g,
        lampLights[index].color.b * (1.0f - warm * 0.5f)
    );

    // 应用呼吸强度变化
    lampLights[index].strength *= breath;
}

void LampController::UpdateScanEffect(int index, float time) {
    float phase = index * 0.5f;
    float angleX = sin(time + phase) * 35.0f;
    float angleY = -90.f;

    lampRotationX[index] = angleX;
    lampRotationY[index] = angleY;

    // 1. 更新骨骼
    //lampBoneControllers[index].RotateBone("LampBone", angleX, angleY);

    float modelOffsetPitch = 90.0f; // 调整
    float modelOffsetYaw = 0.0f;   // 调整

    lampBoneControllers[index].RotateBone("LampBone",
        -angleX + modelOffsetYaw,
        angleY + modelOffsetPitch);

    // 2. 计算光束方向
    glm::mat4 rotation = glm::mat4(1.0f);
    // 关键修复：这里的旋转顺序必须和 RotateBone 函数内部完全一致！
    // 按照 RotateBone：先绕 X 轴转 (angleY)，再绕 Y 轴转 (angleX)
    rotation = glm::rotate(rotation, glm::radians(angleY), glm::vec3(1.0f, 0.0f, 0.0f));
    rotation = glm::rotate(rotation, glm::radians(angleX), glm::vec3(0.0f, 1.0f, 0.0f));

    // 关键修复：将基础方向改为朝向舞台 (-Z)
    glm::vec3 baseDir = glm::vec3(0.0f, -1.0f, -1.0f);
    glm::vec3 newDirection = glm::normalize(glm::vec3(rotation * glm::vec4(baseDir, 0.0f)));

    // 3. 处理后排灯 (index >= 4) 的 180 度掉头
    if (index >= 4) {
        // 因为你在 stage.cpp 里把模型转了 180 度，所以这里光束也要水平转 180 度
        newDirection.x = -newDirection.x;
        newDirection.z = -newDirection.z;
    }

    lampLights[index].direction = newDirection;
    lampLights[index].strength = originalStrengths[index] * 1.5f;
}

void LampController::UpdateDynamicEffects(float deltaTime) {
    // 更新时间
    static float elapsed = 0.0f;
    elapsed += deltaTime;

    gradientTime += deltaTime * gradientSpeed;
    breathTime += deltaTime * breathSpeed;
    scanTime = elapsed * scanSpeed;

    for (int i = 0; i < NUM_LAMPS; i++) {
        // 如果是手动控制状态，跳过自动效果更新
        if (isManuallyControlled[i]) {
            continue;
        }

        // 重置为原始值（作为效果叠加的基础）
        lampLights[i].color = originalColors[i];
        lampLights[i].strength = originalStrengths[i];

        // 应用扫描效果
        if (scanEnabled[i]) {
            UpdateScanEffect(i, elapsed * scanSpeed);
        }

        // 应用渐变效果（叠加在扫描基础上）
        if (gradientEnabled[i]) {
            UpdateGradientEffect(i, gradientTime);
        }

        // 应用呼吸效果（叠加在前两个效果基础上）
        if (breathEnabled[i]) {
            UpdateBreathEffect(i, breathTime);
        }
    }
}

// 设置所有灯的效果状态（完整实现）
void LampController::SetAllLightEffects(bool scan, bool gradient, bool breath) {
    for (int i = 0; i < NUM_LAMPS; i++) {
        if (!isManuallyControlled[i]) { // 只有非手动控制的灯才受影响
            scanEnabled[i] = scan;
            gradientEnabled[i] = gradient;
            breathEnabled[i] = breath;
        }
    }
}

// 设置单个灯的效果状态（完整实现）
void LampController::SetLightEffects(int index, bool scan, bool gradient, bool breath) {
    if (index < 0 || index >= NUM_LAMPS) return;

    // 如果当前是手动控制状态，保存到临时状态
    if (isManuallyControlled[index]) {
        savedScanEnabled[index] = scan;
        savedGradientEnabled[index] = gradient;
        savedBreathEnabled[index] = breath;
    }
    else {
        scanEnabled[index] = scan;
        gradientEnabled[index] = gradient;
        breathEnabled[index] = breath;
    }
}

// 切换渐变效果（完整实现）
void LampController::ToggleGradientForAll() {
    bool anyEnabled = false;
    for (int i = 0; i < NUM_LAMPS; i++) {
        if (gradientEnabled[i] || (isManuallyControlled[i] && savedGradientEnabled[i])) {
            anyEnabled = true;
            break;
        }
    }

    bool newState = !anyEnabled;
    for (int i = 0; i < NUM_LAMPS; i++) {
        if (!isManuallyControlled[i]) {
            gradientEnabled[i] = newState;
        }
        else {
            savedGradientEnabled[i] = newState;
        }
    }

    std::cout << "渐变效果: " << (newState ? "开启" : "关闭") << std::endl;
}

// 切换呼吸效果（完整实现）
void LampController::ToggleBreathForAll() {
    bool anyEnabled = false;
    for (int i = 0; i < NUM_LAMPS; i++) {
        if (breathEnabled[i] || (isManuallyControlled[i] && savedBreathEnabled[i])) {
            anyEnabled = true;
            break;
        }
    }

    bool newState = !anyEnabled;
    for (int i = 0; i < NUM_LAMPS; i++) {
        if (!isManuallyControlled[i]) {
            breathEnabled[i] = newState;
        }
        else {
            savedBreathEnabled[i] = newState;
        }
    }

    std::cout << "呼吸效果: " << (newState ? "开启" : "关闭") << std::endl;
}

// 进入/退出手动控制模式（完整实现）
void LampController::SetManualControl(int index, bool manual) {
    if (index < 0 || index >= NUM_LAMPS) return;

    if (manual && !isManuallyControlled[index]) {
        // 进入手动控制：保存当前状态
        savedScanEnabled[index] = scanEnabled[index];
        savedGradientEnabled[index] = gradientEnabled[index];
        savedBreathEnabled[index] = breathEnabled[index];

        // 禁用所有自动效果
        scanEnabled[index] = false;
        gradientEnabled[index] = false;
        breathEnabled[index] = false;

        isManuallyControlled[index] = true;
    }
    else if (!manual && isManuallyControlled[index]) {
        // 退出手动控制：恢复保存的状态
        scanEnabled[index] = savedScanEnabled[index];
        gradientEnabled[index] = savedGradientEnabled[index];
        breathEnabled[index] = savedBreathEnabled[index];

        isManuallyControlled[index] = false;
    }
}

// 切换扫描效果（完整实现）
void LampController::ToggleScanForAll() {
    bool anyEnabled = false;
    for (int i = 0; i < NUM_LAMPS; i++) {
        if (scanEnabled[i] || (isManuallyControlled[i] && savedScanEnabled[i])) {
            anyEnabled = true;
            break;
        }
    }

    bool newState = !anyEnabled;
    for (int i = 0; i < NUM_LAMPS; i++) {
        if (!isManuallyControlled[i]) {
            scanEnabled[i] = newState;
        }
        else {
            savedScanEnabled[i] = newState;
        }
    }

    std::cout << "扫描效果: " << (newState ? "开启" : "关闭") << std::endl;
}