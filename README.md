# Real-Time Stage Lighting Rendering Demo

这是一个基于 C++ / OpenGL 的实时舞台灯光渲染 Demo，用于展示图形学课程与个人学习中的实时渲染实践。项目围绕舞台场景搭建了模型加载、PBR 材质、IBL 环境光照、HDR 渲染目标、Bloom 后处理、动态聚光灯控制与基础相机交互等模块。

## 技术栈

- C++17
- OpenGL 3.3 Core Profile
- GLSL Shader
- GLFW：窗口创建与输入处理
- GLAD：OpenGL 函数加载
- GLM：矩阵、向量与相机变换
- Assimp：FBX / OBJ / GLB 模型加载
- stb_image：普通纹理与 HDR 贴图加载
- OpenAL：音频播放与控制

## 核心功能

- 基于 Cook-Torrance / GGX 的 PBR 光照计算
- 支持 albedo、metallic、roughness、AO 等材质参数
- HDRI 环境贴图加载与 cubemap 转换
- IBL 相关贴图生成：irradiance map、prefilter map、BRDF LUT
- HDR framebuffer 渲染流程
- Bloom 后处理：亮部提取、ping-pong 高斯模糊、曝光映射与 gamma 校正
- 8 盏舞台聚光灯的颜色、强度、方向与动态效果控制
- Assimp 模型加载与基础骨骼矩阵控制
- 第一人称相机移动与视角控制
- Skybox 环境背景渲染

## 项目结构

```text
.
├── assets/                 # 场景模型、材质贴图、HDRI 环境贴图
├── audio/                  # 音频资源
├── openGL/
│   ├── stage.cpp           # 主程序入口与渲染循环
│   ├── ibl.cpp             # HDRI / IBL 贴图生成
│   ├── lamp_controller.cpp # 舞台灯光控制逻辑
│   ├── include/            # 相机、模型、网格、音频、灯光等头文件
│   └── shader/             # GLSL shader 文件
├── *.dll                   # Windows 运行依赖
└── README.md
```

## 主要代码文件

- `openGL/stage.cpp`：主程序入口，负责窗口初始化、资源加载、主渲染循环、HDR FBO 与 Bloom 后处理流程。
- `openGL/ibl.cpp`：加载 HDR 环境图并生成 cubemap、irradiance map、prefilter map 和 BRDF LUT。
- `openGL/lamp_controller.cpp`：舞台灯光控制模块，负责聚光灯初始化、动态效果与 shader uniform 更新。
- `openGL/shader/model.frag`：导入模型的主要片段着色器，包含 PBR 直接光照、IBL 环境光照、镜球光斑与自发光计算。
- `openGL/shader/bloom.frag`：亮部区域提取。
- `openGL/shader/bloom_blur.frag`：横向 / 纵向高斯模糊。
- `openGL/shader/bloom_combine.frag`：Bloom 合成、曝光映射与 gamma 校正。
- `openGL/include/model_animation.h`：Assimp 模型、材质纹理与骨骼权重加载。

## 运行说明

项目主要在 Visual Studio 2022 环境下开发和运行，未使用 CMake 构建。运行前需要在 VS 2022 项目属性中配置 OpenGL 相关依赖、头文件路径与库文件路径，尤其是 Assimp 的 include / lib 配置。

程序入口位于 `openGL/stage.cpp`。运行时需确保以下资源目录和 DLL 文件与可执行文件保持正确的相对路径：

- `assets/`
- `audio/`
- `openGL/shader/`
- `assimp-vc143-mt.dll`
- `glfw3.dll`
- `glew32.dll`
- `OpenAL32.dll`
- `sndfile.dll`

仓库中的 Assimp 头文件位于 `openGL/include/`，库文件位于 `openGL/lib/`。如果在新的本地环境中打开项目，需要根据本机路径重新检查 VS 2022 的附加包含目录、附加库目录和链接器输入项。

## 操作说明

### 相机控制

| 按键 / 操作 | 功能 |
| --- | --- |
| 鼠标移动 | 控制视角方向 |
| W / A / S / D | 控制相机前后左右移动 |
| Ctrl | 控制相机向下移动 |
| Q / E | 调整环境亮度 |

### 灯光控制

| 按键 | 功能 |
| --- | --- |
| K | 切换鼠标控制对象为聚光灯 |
| 1 - 8 | 选择当前控制的灯光 |
| G | 开关灯光渐变效果 |
| U / I | 加快 / 减慢灯光渐变速度 |
| H | 开关灯光呼吸效果 |
| V | 开关灯光扫描效果 |

### 后处理与音频

| 按键 | 功能 |
| --- | --- |
| B / N | 开启 / 关闭 Bloom 效果 |
| R | 播放 / 暂停音乐 |
| ↑ / ↓ | 增大 / 减小音量 |
| ← / → | 切换上一首 / 下一首 |



## 说明

该项目主要用于学习和展示实时渲染管线。当前实现包含 PBR、IBL、HDR framebuffer 与 Bloom 等核心模块。
