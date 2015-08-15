#ifndef X3DSCENE_H
#define X3DSCENE_H

#include <QtGui/QMatrix4x4>
#include <QElapsedTimer>

namespace CyberX3D
{
    class SceneGraph;
    class Node;
    class Texture2DNode;
}

class X3DRenderer;
class btDiscreteDynamicsWorld;
class btBroadphaseInterface;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;
class btRigidBody;

class SceneEventFilter
{
public:
    virtual bool sceneEventFilter(void *, const float (&pos)[2]) = 0;
};

class X3DScene
{
public:
    struct NodePhysicsGroup
    {
        CyberX3D::Node* top_node;
        CyberX3D::Node* bounded_node;
        CyberX3D::Texture2DNode* texture_node;
        btRigidBody *bt_rigid_body;
    };

    X3DScene(X3DRenderer* renderer);
    ~X3DScene();
    void installEventFilter(SceneEventFilter* filter);
    void addTexture(int textureId, const QRectF &sourceGeometry,
                    const QSize &textureSize, int depth,
                    bool targethasInvertedY, bool sourceHasInvertedY, void* data);
    void removeTexture(void* data);
    void render(const QSize &viewportSize);
    void load(const QString& filename);
    void update();

    void sendKeyDown(uint code);
    void sendKeyUp(uint code);

    void sendPointerEvent(int id, const QPointF& viewportPos, Qt::TouchPointState state);
    void sendAxisEvent(int id, const double& value);

private:
    void addToPhysics(CyberX3D::Node* node);

    SceneEventFilter* event_filter;
    QElapsedTimer physics;
    float view[4][4];
    float fake_velocity[3];
    float fake_rotation;
    bool fake_rotating;
    CyberX3D::SceneGraph* m_root;
    btDiscreteDynamicsWorld* m_world;
    btBroadphaseInterface* m_btinterface;
    btDefaultCollisionConfiguration* m_btconfiguration;
    btCollisionDispatcher* m_btdispatcher;
    btSequentialImpulseConstraintSolver* m_btsolver;

    X3DRenderer* m_renderer;
    std::map<void *, NodePhysicsGroup> nodes;
};

#endif // X3DSCENE_H
