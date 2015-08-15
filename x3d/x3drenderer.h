#ifndef X3DRENDERER_H
#define X3DRENDERER_H

namespace CyberX3D
{
    class SceneGraph;
}

class X3DRenderer
{
public:
    virtual void set_projection(double fovy, double aspect, double zNear, double zFar) = 0;
    virtual bool get_ray(double x, double y, const float (&model)[4][4], double (&from)[3], double (&to)[3]) = 0;
    virtual void render(CyberX3D::SceneGraph *sg) = 0;
};

#endif // X3DRENDERER_H

