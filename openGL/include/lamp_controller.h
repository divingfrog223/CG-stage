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

    // 灯光效果模式
    enum LightMode {
        STATIC,      // 静态
        GRADIENT,    // 渐变
        BREATH,      // 呼吸
        SCAN         // 扫描
    };

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

    // 新增方法
    void UpdateDynamicEffects(float deltaTime);
    void SetLightMode(int index, LightMode mode);
    void SetAllLightMode(LightMode mode);
    void SetGradientSpeed(float speed);
    void SetBreathSpeed(float speed);
    void SetScanSpeed(float speed);

    float GetGradientSpeed() const { return gradientSpeed; }
    float GetBreathSpeed() const { return breathSpeed; }
    float GetScanSpeed() const { return scanSpeed; }

    void ToggleGradientForAll();
    void ToggleBreathForAll();
    void SetManualControl(int index, bool manual);
    void SetLightEffects(int index, bool scan, bool gradient, bool breath);
    void SetAllLightEffects(bool scan, bool gradient, bool breath);
    void ToggleScanForAll();

private:

    // 新增成员变量
    std::vector<LightMode> lampModes;
    std::vector<float> originalStrengths;  // 原始强度
    std::vector<glm::vec3> originalColors; // 原始颜色

    // 改为三个独立的布尔状态数组
    std::vector<bool> scanEnabled;
    std::vector<bool> gradientEnabled;
    std::vector<bool> breathEnabled;
    std::vector<bool> isManuallyControlled; // 标记是否被手动控制

    // 保存手动控制前的状态
    std::vector<bool> savedScanEnabled;
    std::vector<bool> savedGradientEnabled;
    std::vector<bool> savedBreathEnabled;

    void InitializeLampPositions();
    void InitializeLampLights();

    // 效果参数
    float gradientTime = 0.0f;
    float breathTime = 0.0f;
    float scanTime = 0.0f;
    float gradientSpeed = 1.0f;
    float breathSpeed = 1.0f;
    float scanSpeed = 1.0f;

    // 效果相关方法
    void UpdateGradientEffect(int index, float time);
    void UpdateBreathEffect(int index, float time);
    void UpdateScanEffect(int index, float time);
};

#endif