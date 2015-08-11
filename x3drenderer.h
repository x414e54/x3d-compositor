#ifndef X3DRENDERER_H
#define X3DRENDERER_H

namespace CyberX3D
{
    class SceneGraph;
}

class X3DRenderer
{
public:
    void UpdateViewport(CyberX3D::SceneGraph *sg, int width, int height);
    void render(CyberX3D::SceneGraph *sg);
};

#endif // X3DRENDERER_H

