#ifndef X3DSCENE_H
#define X3DSCENE_H

#include <QtGui/QMatrix4x4>

namespace CyberX3D
{
    class SceneGraph;
    class Texture2DNode;
}

class X3DRenderer;
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
    void update();

    void sendKeyDown(uint code);
    void sendKeyUp(uint code);

    void sendPointerEvent(int id, const QPointF& viewportPos, Qt::TouchPointState state);
    void sendAxisEvent(int id, const double& value);

private:
    float fake_velocity[3];
    float fake_rotation;
    CyberX3D::SceneGraph* m_root;
    X3DRenderer* m_renderer;
    std::map<int, CyberX3D::Texture2DNode*> nodes;
};

#endif // X3DSCENE_H
