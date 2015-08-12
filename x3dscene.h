#ifndef X3DSCENE_H
#define X3DSCENE_H

#include <QtGui/QMatrix4x4>
#include <QElapsedTimer>

namespace CyberX3D
{
    class SceneGraph;
    class Texture2DNode;
}

class X3DRenderer;
class btDiscreteDynamicsWorld;
class btBroadphaseInterface;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;

class SceneEventFilter
{
public:
    virtual bool sceneEventFilter(void *, const float (&pos)[2]) = 0;
};

class X3DScene
{
public:
    X3DScene();
    ~X3DScene();
    void installEventFilter(SceneEventFilter* filter);
    void addTexture(int textureId, const QRectF &sourceGeometry,
                    const QSize &textureSize, int depth,
                    bool targethasInvertedY, bool sourceHasInvertedY, QObject* data);
    void render(const QSize &viewportSize);
    void load(const QString& filename);
    void update();

    void sendKeyDown(uint code);
    void sendKeyUp(uint code);

    void sendPointerEvent(int id, const QPointF& viewportPos, Qt::TouchPointState state);
    void sendAxisEvent(int id, const double& value);

private:
    SceneEventFilter* event_filter;
    QElapsedTimer physics;
    float model[4][4];
    float fake_velocity[3];
    float fake_rotation;
    CyberX3D::SceneGraph* m_root;
    btDiscreteDynamicsWorld* m_world;
    btBroadphaseInterface* m_btinterface;
    btDefaultCollisionConfiguration* m_btconfiguration;
    btCollisionDispatcher* m_btdispatcher;
    btSequentialImpulseConstraintSolver* m_btsolver;

    X3DRenderer* m_renderer;
    std::map<int, CyberX3D::Texture2DNode*> nodes;
};

#endif // X3DSCENE_H
