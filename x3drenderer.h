#ifndef _X3DBROWSERFUNC_H_
#define _X3DBROWSERFUNC_H_

#include <cybergarage/x3d/CyberX3D.h>
using namespace CyberX3D;

class X3DRenderer
{
public:
    void MoveViewpoint(SceneGraph *sg, int width, int height, int mosx, int mosy);
    void UpdateViewport(SceneGraph *sg, int width, int height);
    void render(SceneGraph *sg);
};

#endif

