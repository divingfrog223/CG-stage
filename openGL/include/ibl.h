#ifndef IBL_H
#define IBL_H

#include <glad/glad.h>
#include <string>

struct IBLMaps {
    unsigned int envCubemap = 0;
    unsigned int irradianceMap = 0;
    unsigned int prefilterMap = 0;
    unsigned int brdfLUTTexture = 0;
};

// 加载HDR并生成所有IBL贴图
IBLMaps LoadIBLFromHDR(const std::string& hdrPath);

// 释放IBL资源
void CleanupIBL(IBLMaps& ibl);

#endif