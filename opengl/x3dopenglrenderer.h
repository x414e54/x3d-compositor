#ifndef X3DOPENGLRENDERER_H
#define X3DOPENGLRENDERER_H

#include "x3d/x3drenderer.h"
#include "opengl/openglrenderer.h"
#include <map>

namespace CyberX3D
{
    class BackgroundNode;
    class Geometry3DNode;
    class AppearanceNode;
    class LightNode;
    class ShapeNode;
    class Node;
}

class RenderingNodeListener;

class X3DOpenGLRenderer : public X3DRenderer, public OpenGLRenderer
{
public:
    X3DOpenGLRenderer();
    virtual ~X3DOpenGLRenderer();
    void exec_texture();
    void set_projection(Scalar fovy, Scalar aspect, Scalar zNear, Scalar zFar);
    bool get_ray(Scalar x, Scalar y, const Scalar (&model)[4][4], Scalar (&from)[3], Scalar (&to)[3]);
    void render(CyberX3D::SceneGraph *sg);
private:
    Material& process_apperance_node(CyberX3D::AppearanceNode *apperance, glm::mat4x4 transform);
    void process_geometry_node(CyberX3D::Geometry3DNode *geometry, Material& material);
    void process_background_node(CyberX3D::BackgroundNode *background);
    void process_light_node(CyberX3D::LightNode *light);
    void process_shape_node(CyberX3D::ShapeNode *shape, bool selected);
    void process_node(CyberX3D::SceneGraph *sg, CyberX3D::Node *root);
    friend class RenderingNodeListener;
    RenderingNodeListener* node_listener;
};

#endif // X3DOPENGLRENDERER_H

