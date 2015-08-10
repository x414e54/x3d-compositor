#ifndef TEXTUREBLITTER_H
#define TEXTUREBLITTER_H

#include <QtGui/QMatrix4x4>

#define CX3D_SUPPORT_OPENGL
#include <cybergarage/x3d/CyberX3D.h>
using namespace CyberX3D;

#include "x3drenderer.h"

QT_BEGIN_NAMESPACE

class X3DScene
{
public:
    X3DScene();
    ~X3DScene();
    void addTexture(int textureId, const QRectF &sourceGeometry,
                    const QSize &textureSize, int depth,
                    bool targethasInvertedY, bool sourceHasInvertedY);
    void render(const QSize &viewportSize);
    void load(const QString& filename);

private:
    SceneGraph m_root;
    X3DRenderer m_renderer;
    std::map<int, Texture2DNode*> nodes;
};

QT_END_NAMESPACE

#endif // TEXTUREBLITTER_H
