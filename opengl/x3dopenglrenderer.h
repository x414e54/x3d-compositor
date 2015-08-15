#ifndef X3DOPENGLRENDERER_H
#define X3DOPENGLRENDERER_H

#include "x3d/x3drenderer.h"
#include "opengl/openglrenderer.h"
#include <map>

namespace CyberX3D
{
    class ShapeNode;
    class Node;
}

class X3DOpenGLRenderer : public X3DRenderer, public OpenGLRenderer
{
public:
    X3DOpenGLRenderer();
    virtual ~X3DOpenGLRenderer();
    void set_projection(double fovy, double aspect, double zNear, double zFar);
    bool get_ray(double x, double y, const float (&model)[4][4], double (&from)[3], double (&to)[3]);
    void render(CyberX3D::SceneGraph *sg);
private:
    void DrawShapeNode(CyberX3D::SceneGraph *sg, CyberX3D::ShapeNode *shape, int drawMode);
    void DrawNode(CyberX3D::SceneGraph *sceneGraph, CyberX3D::Node *firstNode, int drawMode);
};

#endif // X3DOPENGLRENDERER_H

