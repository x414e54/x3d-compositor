#ifndef X3DRENDERER_H
#define X3DRENDERER_H

namespace CyberX3D
{
    class SceneGraph;
}

class X3DRenderer
{
public:
    void set_viewport(int width, int height);
    void set_projection(double fovy, double aspect, double zNear, double zFar);
    bool get_ray(double x, double y, const float (&model)[4][4], double (&from)[3], double (&to)[3]);
    void render(CyberX3D::SceneGraph *sg);
private:
    double projection[4][4];
    int width;
    int height;
};

#endif // X3DRENDERER_H

