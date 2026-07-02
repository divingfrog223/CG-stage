#define _CRT_SECURE_NO_WARNINGS 
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <al.h>
#include <alc.h>
#include <sndfile.h>
#include <iomanip> 
#include <set>       
#include <cctype>
#include <glm/glm.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

// 3D音频播放器类
class AudioPlayer3D {
private:
    ALCdevice* device;
    ALCcontext* context;

    // 音频源（每个源可以有独立的位置）
    std::vector<ALuint> sources;
    std::vector<glm::vec3> sourcePositions;
    std::vector<glm::vec3> sourceVelocities;

    // 音频缓冲区
    std::vector<ALuint> buffers;
    std::vector<std::string> musicFiles;
    std::vector<std::string> musicNames;

    // 当前播放状态
    int currentTrack;
    bool isPaused;
    bool isPlaying;
    float currentVolume;

    // 3D音频参数
    glm::vec3 listenerPosition;
    glm::vec3 listenerVelocity;
    glm::vec3 listenerOrientation[2]; // [0]=at, [1]=up

    // 音频属性
    std::set<std::string> supportedExtensions;

public:
    AudioPlayer3D()
        : device(nullptr), context(nullptr),
        currentTrack(0), isPaused(false), isPlaying(false),
        currentVolume(0.5f), listenerPosition(0.0f, 0.0f, 0.0f),
        listenerVelocity(0.0f, 0.0f, 0.0f) {

        // 初始化支持的格式
        supportedExtensions = {
            ".ogg", ".wav", ".flac", ".aiff", ".aif", ".au", ".snd"
        };

        // 初始化监听器方向（看向-Z方向，Y轴向上）
        listenerOrientation[0] = glm::vec3(0.0f, 0.0f, -1.0f); // 看向方向
        listenerOrientation[1] = glm::vec3(0.0f, 1.0f, 0.0f);  // 向上方向
    }

    ~AudioPlayer3D() {
        cleanup();
    }

    // ==================== 初始化相关 ====================

    bool init() {
        // 1. 打开默认音频设备
        device = alcOpenDevice(nullptr);
        if (!device) {
            std::cerr << "错误: 无法打开音频设备" << std::endl;
            return false;
        }

        // 2. 创建上下文
        context = alcCreateContext(device, nullptr);
        if (!context) {
            std::cerr << "错误: 无法创建音频上下文" << std::endl;
            alcCloseDevice(device);
            return false;
        }

        // 3. 设置当前上下文
        if (!alcMakeContextCurrent(context)) {
            std::cerr << "错误: 无法设置音频上下文" << std::endl;
            alcDestroyContext(context);
            alcCloseDevice(device);
            return false;
        }

        // 4. 启用3D音频功能
        enable3DAudio();

        std::cout << "3D音频系统初始化成功" << std::endl;
        return true;
    }

    void enable3DAudio() {
        // 设置距离模型（控制声音随距离衰减的方式）
        // AL_INVERSE_DISTANCE: 1/(1 + rolloff*(distance - reference))
        // AL_INVERSE_DISTANCE_CLAMPED: 同INVERSE_DISTANCE，但距离小于reference时不再增加
        // AL_LINEAR_DISTANCE: (reference - distance)/(reference * (max - reference))
        // AL_LINEAR_DISTANCE_CLAMPED: 同LINEAR_DISTANCE，但有钳制
        // AL_EXPONENT_DISTANCE: (reference/distance)^rolloff
        // AL_EXPONENT_DISTANCE_CLAMPED: 同EXPONENT_DISTANCE，但有钳制
        alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

        // 设置多普勒因子（模拟多普勒效应）
        alDopplerFactor(1.5f); // 1.0 = 真实世界效果

        // 设置声音速度（米/秒）
        alSpeedOfSound(343.3f); // 空气中声速约343.3米/秒
    }

    // ==================== 音频文件加载 ====================

    // 扫描音频目录（与之前相同）
    bool scanAudioDirectory(const std::string& directoryPath) {
        std::cout << "扫描音频目录: " << directoryPath << std::endl;

        musicFiles.clear();
        musicNames.clear();

#ifdef _WIN32
        WIN32_FIND_DATAA findData;
        std::string searchPath = directoryPath + "\\*";
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

        if (hFind == INVALID_HANDLE_VALUE) {
            return false;
        }

        do {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

            std::string filename = findData.cFileName;
            if (isAudioFile(filename)) {
                std::string fullPath = directoryPath + "\\" + filename;
                musicFiles.push_back(fullPath);
                musicNames.push_back(filename);
                std::cout << "  找到: " << filename << std::endl;
            }
        } while (FindNextFileA(hFind, &findData) != 0);

        FindClose(hFind);
#else
        DIR* dir = opendir(directoryPath.c_str());
        if (!dir) return false;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_DIR) continue;

            std::string filename = entry->d_name;
            if (isAudioFile(filename)) {
                std::string fullPath = directoryPath + "/" + filename;
                musicFiles.push_back(fullPath);
                musicNames.push_back(filename);
                std::cout << "  找到: " << filename << std::endl;
            }
        }
        closedir(dir);
#endif

        return !musicFiles.empty();
    }

    bool isAudioFile(const std::string& filename) {
        size_t dotPos = filename.find_last_of(".");
        if (dotPos == std::string::npos) return false;

        std::string ext = filename.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return supportedExtensions.find(ext) != supportedExtensions.end();
    }

    bool loadAudioFile(const std::string& filename) {
        SF_INFO sfinfo;
        memset(&sfinfo, 0, sizeof(sfinfo));

        SNDFILE* sndfile = sf_open(filename.c_str(), SFM_READ, &sfinfo);
        if (!sndfile) {
            std::cerr << "  无法打开音频文件" << std::endl;
            return false;
        }

        int numChannels = sfinfo.channels;
        int sampleRate = sfinfo.samplerate;
        sf_count_t numFrames = sfinfo.frames;

        if (numFrames <= 0) {
            sf_close(sndfile);
            return false;
        }

        std::vector<short> samples(numFrames * numChannels);
        sf_count_t framesRead = sf_readf_short(sndfile, samples.data(), numFrames);

        ALuint buffer;
        alGenBuffers(1, &buffer);

        ALenum format = AL_NONE;
        if (numChannels == 1) {
            format = AL_FORMAT_MONO16; // 单声道适合3D音频
        }
        else if (numChannels == 2) {
            format = AL_FORMAT_STEREO16;
        }
        else {
            format = AL_FORMAT_STEREO16;
        }

        alBufferData(buffer, format, samples.data(),
            framesRead * numChannels * sizeof(short), sampleRate);

        buffers.push_back(buffer);
        sf_close(sndfile);

        // 为每个音频文件创建一个音频源
        ALuint source;
        alGenSources(1, &source);
        sources.push_back(source);

        // 设置默认的3D音频属性
        setupSource3DProperties(source);

        // 默认位置为原点
        sourcePositions.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
        sourceVelocities.push_back(glm::vec3(0.0f, 0.0f, 0.0f));

        return true;
    }

    // 设置音频源的3D属性
    void setupSource3DProperties(ALuint source) {
        // 位置
        alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);

        // 速度（用于多普勒效应）
        alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);

        // 方向（用于定向声音）
        alSource3f(source, AL_DIRECTION, 0.0f, 0.0f, 0.0f);

        // 声音锥参数（内锥角度，外锥角度，外锥增益）
        // 如果direction不为零，可以设置锥形声音
         alSourcef(source, AL_CONE_INNER_ANGLE, 360.0f); // 全向
         alSourcef(source, AL_CONE_OUTER_ANGLE, 360.0f);
         alSourcef(source, AL_CONE_OUTER_GAIN, 0.0f);

        // 衰减参数
        alSourcef(source, AL_ROLLOFF_FACTOR, 3.0f);   // 衰减因子
        alSourcef(source, AL_REFERENCE_DISTANCE, 5.0f); // 参考距离（米）
        alSourcef(source, AL_MAX_DISTANCE, 50.0f);    // 最大距离（米）

        // 其他属性
        alSourcef(source, AL_PITCH, 1.0f);
        alSourcef(source, AL_GAIN, currentVolume);
        alSourcei(source, AL_LOOPING, AL_FALSE);
        alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE); // 位置相对于世界，不是监听器
    }

    // ==================== 3D音频控制 ====================

    // 设置监听器位置（通常是相机位置）
    void setListenerPosition(const glm::vec3& position) {
        listenerPosition = position;
        alListener3f(AL_POSITION, position.x, position.y, position.z);
    }

    void setListenerPosition(float x, float y, float z) {
        setListenerPosition(glm::vec3(x, y, z));
    }

    // 设置监听器速度（用于多普勒效应）
    void setListenerVelocity(const glm::vec3& velocity) {
        listenerVelocity = velocity;
        alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
    }

    // 设置监听器方向（看向方向和向上方向）
    void setListenerOrientation(const glm::vec3& at, const glm::vec3& up) {
        listenerOrientation[0] = at;
        listenerOrientation[1] = up;

        float orientation[6] = {
            at.x, at.y, at.z,
            up.x, up.y, up.z
        };
        alListenerfv(AL_ORIENTATION, orientation);
    }

    // 设置音频源位置
    void setSourcePosition(int sourceIndex, const glm::vec3& position) {
        if (sourceIndex >= 0 && sourceIndex < (int)sources.size()) {
            sourcePositions[sourceIndex] = position;
            alSource3f(sources[sourceIndex], AL_POSITION,
                position.x, position.y, position.z);
        }
    }

    void setSourcePosition(int sourceIndex, float x, float y, float z) {
        setSourcePosition(sourceIndex, glm::vec3(x, y, z));
    }

    // 设置当前播放音源的位置
    void setCurrentSourcePosition(const glm::vec3& position) {
        if (currentTrack >= 0 && currentTrack < (int)sources.size()) {
            setSourcePosition(currentTrack, position);
        }
    }

    void setCurrentSourcePosition(float x, float y, float z) {
        setCurrentSourcePosition(glm::vec3(x, y, z));
    }

    // 设置音频源速度（用于移动的音源）
    void setSourceVelocity(int sourceIndex, const glm::vec3& velocity) {
        if (sourceIndex >= 0 && sourceIndex < (int)sources.size()) {
            sourceVelocities[sourceIndex] = velocity;
            alSource3f(sources[sourceIndex], AL_VELOCITY,
                velocity.x, velocity.y, velocity.z);
        }
    }

    // 设置音频源方向（用于定向声音，如喇叭）
    void setSourceDirection(int sourceIndex, const glm::vec3& direction) {
        if (sourceIndex >= 0 && sourceIndex < (int)sources.size()) {
            alSource3f(sources[sourceIndex], AL_DIRECTION,
                direction.x, direction.y, direction.z);
        }
    }

    // 设置音频源衰减参数
    void setSourceAttenuation(int sourceIndex,
        float rolloffFactor = 3.0f,
        float referenceDistance = 3.0f,
        float maxDistance = 30.0f,
        float coneInnerAngle = 360.0f,
        float coneOuterAngle = 360.0f,
        float coneOuterGain = 0.0f) {
        if (sourceIndex >= 0 && sourceIndex < (int)sources.size()) {
            ALuint source = sources[sourceIndex];

            // 衰减参数
            alSourcef(source, AL_ROLLOFF_FACTOR, rolloffFactor);
            alSourcef(source, AL_REFERENCE_DISTANCE, referenceDistance);
            alSourcef(source, AL_MAX_DISTANCE, maxDistance);

            // 声音锥参数（如果是定向声音）
            alSourcef(source, AL_CONE_INNER_ANGLE, coneInnerAngle);
            alSourcef(source, AL_CONE_OUTER_ANGLE, coneOuterAngle);
            alSourcef(source, AL_CONE_OUTER_GAIN, coneOuterGain);

            std::cout << "音源" << sourceIndex << "衰减参数: "
                << "rolloff=" << rolloffFactor
                << ", refDist=" << referenceDistance
                << ", maxDist=" << maxDistance << std::endl;
        }
    }

    void setDistanceModel(ALenum model) {
        alDistanceModel(model);
        std::cout << "距离模型设置为: ";
        switch (model) {
        case AL_INVERSE_DISTANCE: std::cout << "INVERSE_DISTANCE"; break;
        case AL_INVERSE_DISTANCE_CLAMPED: std::cout << "INVERSE_DISTANCE_CLAMPED"; break;
        case AL_LINEAR_DISTANCE: std::cout << "LINEAR_DISTANCE"; break;
        case AL_LINEAR_DISTANCE_CLAMPED: std::cout << "LINEAR_DISTANCE_CLAMPED"; break;
        case AL_EXPONENT_DISTANCE: std::cout << "EXPONENT_DISTANCE"; break;
        case AL_EXPONENT_DISTANCE_CLAMPED: std::cout << "EXPONENT_DISTANCE_CLAMPED"; break;
        case AL_NONE: std::cout << "NONE (无衰减)"; break;
        default: std::cout << "未知"; break;
        }
        std::cout << std::endl;
    }

    // 添加：获取音源的实时音量（用于调试）
    float getSourceGain(int sourceIndex) {
        if (sourceIndex >= 0 && sourceIndex < (int)sources.size()) {
            float gain;
            alGetSourcef(sources[sourceIndex], AL_GAIN, &gain);
            return gain;
        }
        return 0.0f;
    }

    // 添加：显示详细的3D音效信息
    void printDetailed3DInfo() {
        if (buffers.empty() || currentTrack >= (int)sources.size()) {
            return;
        }

        glm::vec3 sourcePos = sourcePositions[currentTrack];
        float distance = glm::distance(listenerPosition, sourcePos);

        // 获取音源的衰减参数
        float rolloff, refDist, maxDist;
        ALuint source = sources[currentTrack];
        alGetSourcef(source, AL_ROLLOFF_FACTOR, &rolloff);
        alGetSourcef(source, AL_REFERENCE_DISTANCE, &refDist);
        alGetSourcef(source, AL_MAX_DISTANCE, &maxDist);

        // 计算理论音量（根据逆距离衰减公式）
        float theoreticalGain = refDist / (refDist + rolloff * (distance - refDist));
        if (distance < refDist) theoreticalGain = 1.0f;
        if (distance > maxDist) theoreticalGain = 0.0f;

        // 获取实际音量
        float actualGain = getSourceGain(currentTrack);

        std::cout << "\n=== 3D音效详细信息 ===" << std::endl;
        std::cout << "监听器位置: (" << listenerPosition.x << ", "
            << listenerPosition.y << ", " << listenerPosition.z << ")" << std::endl;
        std::cout << "音源位置: (" << sourcePos.x << ", "
            << sourcePos.y << ", " << sourcePos.z << ")" << std::endl;
        std::cout << "距离: " << std::fixed << std::setprecision(2) << distance << " 米" << std::endl;
        std::cout << "衰减参数: rolloff=" << rolloff
            << ", refDist=" << refDist
            << ", maxDist=" << maxDist << std::endl;
        std::cout << "理论音量: " << std::setprecision(3) << theoreticalGain
            << " (" << (int)(theoreticalGain * 100) << "%)" << std::endl;
        std::cout << "实际音量: " << std::setprecision(3) << actualGain
            << " (" << (int)(actualGain * 100) << "%)" << std::endl;
        std::cout << "=====================\n" << std::endl;
    }

    // 设置音频源为全向或定向
    void setSourceCone(int sourceIndex, float innerAngle = 360.0f,
        float outerAngle = 360.0f, float outerGain = 0.0f) {
        if (sourceIndex >= 0 && sourceIndex < (int)sources.size()) {
            ALuint source = sources[sourceIndex];
            alSourcef(source, AL_CONE_INNER_ANGLE, innerAngle);
            alSourcef(source, AL_CONE_OUTER_ANGLE, outerAngle);
            alSourcef(source, AL_CONE_OUTER_GAIN, outerGain);
        }
    }

    // ==================== 播放控制 ====================

    void play() {
        if (buffers.empty()) {
            std::cout << "没有可播放的音乐" << std::endl;
            return;
        }

        if (currentTrack >= (int)buffers.size()) {
            currentTrack = 0;
        }

        if (isPaused) {
            alSourcePlay(sources[currentTrack]);
            isPaused = false;
            isPlaying = true;
            std::cout << "恢复播放 (3D音效)" << std::endl;
            printSourceInfo();
        }
        else {
            stop();

            ALuint source = sources[currentTrack];
            ALuint buffer = buffers[currentTrack];

            alSourcei(source, AL_BUFFER, buffer);
            alSourcePlay(source);

            isPlaying = true;
            isPaused = false;

            std::cout << "开始播放 (3D音效)" << std::endl;
            printSourceInfo();
        }
    }

    void playAtPosition(const glm::vec3& position) {
        setCurrentSourcePosition(position);
        play();
    }

    void playAtPosition(float x, float y, float z) {
        playAtPosition(glm::vec3(x, y, z));
    }

    void pause() {
        if (isPlaying && !isPaused && currentTrack < (int)sources.size()) {
            alSourcePause(sources[currentTrack]);
            isPaused = true;
            std::cout << "音乐已暂停" << std::endl;
        }
    }

    void stop() {
        if (isPlaying && currentTrack < (int)sources.size()) {
            alSourceStop(sources[currentTrack]);
            isPlaying = false;
        }
        isPaused = false;
    }

    void nextTrack() {
        if (buffers.empty()) return;

        std::cout << "切换到下一首" << std::endl;
        stop();
        currentTrack = (currentTrack + 1) % buffers.size();
        play();
    }

    void previousTrack() {
        if (buffers.empty()) return;

        std::cout << "切换到上一首" << std::endl;
        stop();
        currentTrack = (currentTrack - 1 + buffers.size()) % buffers.size();
        play();
    }

    void togglePlayPause() {
        if (buffers.empty()) return;

        if (!isPlaying || isPaused) {
            play();
        }
        else {
            pause();
        }
    }

    // ==================== 音量控制 ====================

    void setVolume(float volume) {
        if (volume < 0.0f) volume = 0.0f;
        if (volume > 1.0f) volume = 1.0f;

        currentVolume = volume;

        // 设置所有源的音量
        for (ALuint source : sources) {
            alSourcef(source, AL_GAIN, volume);
        }
    }

    void increaseVolume() {
        setVolume(currentVolume + 0.1f);
        std::cout << "音量: " << (int)(currentVolume * 100) << "%" << std::endl;
    }

    void decreaseVolume() {
        setVolume(currentVolume - 0.1f);
        std::cout << "音量: " << (int)(currentVolume * 100) << "%" << std::endl;
    }

    // ==================== 信息显示 ====================

    void printSourceInfo() {
        if (buffers.empty() || currentTrack >= (int)sources.size()) {
            std::cout << "没有可播放的音乐" << std::endl;
            return;
        }

        glm::vec3 pos = sourcePositions[currentTrack];
        float distance = glm::distance(listenerPosition, pos);

        std::cout << "当前音源: " << musicNames[currentTrack]
            << " (" << (currentTrack + 1) << "/" << buffers.size() << ")" << std::endl;
        std::cout << "  位置: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
        std::cout << "  距离监听器: " << std::fixed << std::setprecision(2) << distance << " 米" << std::endl;
        std::cout << "  音量: " << (int)(currentVolume * 100) << "%" << std::endl;
    }

    void printAllTracks() {
        if (buffers.empty()) {
            std::cout << "没有可用的音乐" << std::endl;
            return;
        }

        std::cout << "\n=== 3D音源列表 (" << buffers.size() << "个) ===" << std::endl;
        for (size_t i = 0; i < buffers.size(); i++) {
            glm::vec3 pos = sourcePositions[i];
            std::cout << (i + 1) << ". " << musicNames[i];
            std::cout << " [位置: (" << pos.x << ", " << pos.y << ", " << pos.z << ")]";

            if (static_cast<int>(i) == currentTrack) {
                if (isPlaying && !isPaused) {
                    std::cout << " [正在播放]";
                }
                else if (isPaused) {
                    std::cout << " [已暂停]";
                }
            }
            std::cout << std::endl;
        }
        std::cout << "监听器位置: (" << listenerPosition.x << ", "
            << listenerPosition.y << ", " << listenerPosition.z << ")" << std::endl;
        std::cout << "=====================================" << std::endl;
    }

    // ==================== 工具函数 ====================

    bool checkALError(const std::string& operation) {
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            std::cerr << "OpenAL错误 (" << operation << "): " << alGetString(error) << std::endl;
            return true;
        }
        return false;
    }

    void cleanup() {
        // 停止所有源
        for (ALuint source : sources) {
            alSourceStop(source);
        }

        // 删除源
        if (!sources.empty()) {
            alDeleteSources(sources.size(), sources.data());
            sources.clear();
        }

        // 删除缓冲区
        if (!buffers.empty()) {
            alDeleteBuffers(buffers.size(), buffers.data());
            buffers.clear();
        }

        // 清理上下文和设备
        if (context) {
            alcMakeContextCurrent(nullptr);
            alcDestroyContext(context);
            context = nullptr;
        }

        if (device) {
            alcCloseDevice(device);
            device = nullptr;
        }

        std::cout << "3D音频系统已清理" << std::endl;
    }

    // ==================== 获取状态 ====================

    int getTrackCount() const { return (int)buffers.size(); }
    int getCurrentTrackIndex() const { return currentTrack; }
    bool isMusicPlaying() const { return isPlaying && !isPaused; }
    bool isMusicPaused() const { return isPaused; }
    float getVolume() const { return currentVolume; }

    glm::vec3 getListenerPosition() const { return listenerPosition; }
    glm::vec3 getCurrentSourcePosition() const {
        if (currentTrack < (int)sourcePositions.size()) {
            return sourcePositions[currentTrack];
        }
        return glm::vec3(0.0f);
    }

    // 跳转到指定曲目
    void jumpToTrack(int trackIndex) {
        if (trackIndex < 0 || trackIndex >= (int)buffers.size()) return;

        std::cout << "跳转到: " << musicNames[trackIndex] << std::endl;
        stop();
        currentTrack = trackIndex;
        play();
    }

    // 扫描并加载所有音频文件
    bool scanAndLoadTracks() {
        std::vector<std::string> directories = {
            "../audio",
            "./audio",
            "../../audio",
            "audio",
            "../assets/audio",
            "./assets/audio"
        };

        for (const auto& dir : directories) {
            if (scanAudioDirectory(dir)) {
                std::cout << "从目录加载音频文件: " << dir << std::endl;
                break;
            }
        }

        if (musicFiles.empty()) {
            std::cerr << "错误: 未找到音频文件" << std::endl;
            return false;
        }

        // 加载所有音频文件
        for (size_t i = 0; i < musicFiles.size(); i++) {
            std::cout << "[" << i + 1 << "] 加载: " << musicNames[i] << "...";
            if (loadAudioFile(musicFiles[i])) {
                std::cout << " ?" << std::endl;
            }
            else {
                std::cout << " ?" << std::endl;
                musicFiles.erase(musicFiles.begin() + i);
                musicNames.erase(musicNames.begin() + i);
                i--;
            }
        }

        if (buffers.empty()) {
            std::cerr << "错误: 没有成功加载任何音频文件" << std::endl;
            return false;
        }

        std::cout << "? 成功加载 " << buffers.size() << " 个3D音频源" << std::endl;
        return true;
    }
};