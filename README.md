stage.cpp是最主要的代码，

model.frag是所有导入模型的片段着色器，



assets文件夹存放所有模型

assimp库我是按着网上的教程配置的，这个库是用来导入模型的，详情可以去看learnopengl网站里的“模型导入”那一页。这个库的文件夹（不是我写的，网上能找到）放在opengl/include里，然后lib放在lib文件夹里，但是在自己本地的vs2022上还是要在属性里再配一遍的



lamp\_controller是把stage里灯光控制部分挪出来的。

新增按键：

B/N可以选择是否开启bloom效果，现在有bug，运行时初始是bloom，按N取消bloom，再按B无法恢复bloom。不过我对这个bloom效果也很存疑（感觉只是变亮了一点）所以仅供参考（

QE可以调整整体的亮暗程度，Q暗E亮。



