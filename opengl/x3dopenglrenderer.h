#ifndef X3DOPENGLRENDERER_H
#define X3DOPENGLRENDERER_H

#include "x3d/x3drenderer.h"
#include "opengl/openglrenderer.h"
#include <map>

namespace CyberX3D
{
    class Geometry3DNode;
    class LightNode;
    class ShapeNode;
    class Node;
}

class X3DOpenGLRenderer : public X3DRenderer, public OpenGLRenderer
{
public:
    X3DOpenGLRenderer();
    virtual ~X3DOpenGLRenderer();
    void set_projection(Scalar fovy, Scalar aspect, Scalar zNear, Scalar zFar);
    bool get_ray(Scalar x, Scalar y, const Scalar (&model)[4][4], Scalar (&from)[3], Scalar (&to)[3]);
    void render(CyberX3D::SceneGraph *sg);
private:
    void process_geometry_node(CyberX3D::Geometry3DNode *geometry, Material& material);
    void process_light_node(CyberX3D::LightNode *light);
    void process_shape_node(CyberX3D::ShapeNode *shape, bool selected);
    void process_node(CyberX3D::SceneGraph *sg, CyberX3D::Node *root);
};

#endif // X3DOPENGLRENDERER_H

