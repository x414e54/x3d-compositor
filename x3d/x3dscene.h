#ifndef X3DSCENE_H
#define X3DSCENE_H

#include <QtGui/QMatrix4x4>
#include <QElapsedTimer>

namespace CyberX3D
{
    class SceneGraph;
    class Node;
    class Texture2DNode;
    class TouchSensorNode;
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
    enum SceneEvent
    {
        DOWN,
        UP,
        DRAG,
        OVER,
        EXIT
    };

    virtual bool sceneEventFilter(void *, const float (&pos)[2], SceneEvent state) = 0;

    static SceneEvent convert_event(bool was_active, bool is_active)
    {
        if (is_active && was_active) {
            return DRAG;
        } else if (is_active) {
            return DOWN;
        } else if (was_active) {
            return UP;
        } else {
            return OVER;
        }
    }
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
    void add_texture(int texture_id, float real_width, float real_height,
                     size_t width, size_t height, void* data);
    void remove_texture(void* data);
    void render(const QSize &viewportSize);
    void load(const QString& filename);
    void update();

    void sendKeyDown(uint code);
    void sendKeyUp(uint code);

    void sendPointerEvent(int id, float x, float y, Qt::TouchPointState state);
    void sendAxisEvent(int id, const double& value);

private:
    void addToPhysics(CyberX3D::Node* node);

    SceneEventFilter* event_filter;
    QElapsedTimer physics;
    float view[4][4];
    float fake_velocity[3];
    float fake_rotation;
    bool fake_rotating;
    CyberX3D::TouchSensorNode* m_current_touch;
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
