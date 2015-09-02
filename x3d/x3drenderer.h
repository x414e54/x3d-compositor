#ifndef X3DRENDERER_H
#define X3DRENDERER_H

namespace CyberX3D
{
    class SceneGraph;
}

typedef float Scalar;

class X3DRenderer
{
public:
    virtual ~X3DRenderer() {}
    virtual void set_projection(Scalar fovy, Scalar aspect, Scalar zNear, Scalar zFar) = 0;
    virtual bool get_ray(Scalar x, Scalar y, const Scalar (&model)[4][4], Scalar (&from)[3], Scalar (&to)[3]) = 0;
    virtual void render(CyberX3D::SceneGraph *sg) = 0;

    virtual void debug_render_increase() = 0;
    virtual void debug_render_decrease() = 0;
};

#endif // X3DRENDERER_H

