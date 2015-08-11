#include "x3dscene.h"

#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtDebug>
#include <QTemporaryFile>

#define CX3D_SUPPORT_OPENGL
#include <cybergarage/x3d/CyberX3D.h>
using namespace CyberX3D;

#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>

#include "x3drenderer.h"

QT_BEGIN_NAMESPACE

X3DScene::X3DScene()
    : fake_velocity{0.0f, 0.0f, 0.0f}
{
    m_root = new SceneGraph();
    m_renderer = new X3DRenderer();
    physics.start();
    m_btinterface = new btDbvtBroadphase();
    m_btconfiguration = new btDefaultCollisionConfiguration();
    m_btdispatcher = new btCollisionDispatcher(m_btconfiguration);
    m_btsolver = new btSequentialImpulseConstraintSolver();
    m_world = new btDiscreteDynamicsWorld(m_btdispatcher, m_btinterface, m_btsolver, m_btconfiguration);
    m_world->setGravity(btVector3(0, -9.80665, 0));
}

X3DScene::~X3DScene()
{
    if (m_renderer != NULL) {
        delete m_renderer;
    }
    if (m_root != NULL) {
        delete m_root;
    }

    if (m_world != NULL) {
        delete m_world;
    }
    if (m_btsolver != NULL) {
        delete m_btsolver;
    }
    if (m_btdispatcher != NULL) {
        delete m_btdispatcher;
    }
    if (m_btconfiguration != NULL) {
        delete m_btconfiguration;
    }
    if (m_btinterface != NULL) {
        delete m_btinterface;
    }
}

void X3DScene::load(const QString& filename)
{
    std::string file_utf8 = filename.toUtf8().constData();
    if (m_root->load(file_utf8.c_str()) == false) {
        qCritical() << "Loading error"
            << "\nLine Number: " << m_root->getParserErrorLineNumber()
            << "\nError Message: " << m_root->getParserErrorMessage()
            << "\nError Token: " << m_root->getParserErrorToken()
            << "\nError Line: " << m_root->getParserErrorLineString();
    }

    m_root->initialize();
    if (m_root->getViewpointNode() == NULL)
        m_root->zoomAllViewpoint();

    nodes.clear();
    physics.restart();
}

void X3DScene::addTexture(int textureId, const QRectF &targetRect, const QSize &textureSize, int depth, bool targethasInvertedY, bool sourceHasInvertedY)
{
    std::map<int, Texture2DNode*>::iterator found;
    if ((found = nodes.find(textureId)) == nodes.end()) {
        TransformNode* transform = new TransformNode();
            transform->setTranslation(0.0f + (1.0f * nodes.size()), 2.0f, 0.0f);
            ShapeNode* shape = new ShapeNode();
                AppearanceNode* appearance = new AppearanceNode();
                    ImageTextureNode* texture = new ImageTextureNode();
                        texture->setTextureName(textureId);
                    appearance->addChildNode(texture);
                shape->addChildNode(appearance);
                BoxNode* box = new BoxNode();
                box->setSize(targetRect.width(), targetRect.height(), 1.0f/100.0f);
                shape->addChildNode(box);
            transform->addChildNode(shape);
        nodes[textureId] = texture;
        m_root->addNode(transform);
    } else if (found->second != NULL){
        found->second->setTextureName(textureId);
    }
}

void X3DScene::sendKeyDown(uint code)
{
    if (code == 25) {
        fake_velocity[2] += 1.0f;
    } else if(code == 39) {
        fake_velocity[2] -= 1.0f;
    } else if(code == 38) {
        fake_velocity[0] -= 1.0f;
    } else if(code == 40) {
        fake_velocity[0] += 1.0f;
    }
}

void X3DScene::sendKeyUp(uint code)
{
    if (code == 25) {
        fake_velocity[2] -= 1.0f;
    } else if(code == 39) {
        fake_velocity[2] += 1.0f;
    } else if(code == 38) {
        fake_velocity[0] += 1.0f;
    } else if(code == 40) {
        fake_velocity[0] -= 1.0f;
    }
}

void X3DScene::sendPointerEvent(int id, const QPointF& viewportPos, Qt::TouchPointState state)
{
}

void X3DScene::sendAxisEvent(int id, const double& value)
{
    if (id == 0) {
        fake_rotation = value;
    } else if (id == 1) {
    }
}

void X3DScene::update()
{
    ViewpointNode *view = m_root->getViewpointNode();
    if (view == NULL) {
        view = m_root->getDefaultViewpointNode();
    }

    NavigationInfoNode *navInfo = m_root->getNavigationInfoNode();
    if (navInfo == NULL) {
        navInfo = m_root->getDefaultNavigationInfoNode();
    }
/*
    btCollisionWorld::ClosestRayResultCallback ray_result(
    );

    m_world->rayTest(
        ray_result
    );

    if (ray_result.hasHit()) {
        Node* node = static_cast<Node>(ray_result.m_collisionObject->getUserPointer());
        if node->getTouchSensorNodes()
        {

        }
    }
*/
    const float speed = navInfo->getSpeed();
    float view_translation[3] = {fake_velocity[0] * speed,
                                 fake_velocity[1] * speed,
                                 -fake_velocity[2] * speed};

    view->translate(view_translation);

    float view_rotation[4] = {0.0f, 1.0f, 0.0f, -fake_rotation * 0.01f};
    view->rotate(view_rotation);

    m_root->update();

    m_world->stepSimulation((btScalar)physics.restart()/(btScalar)1000, 10);
}

void X3DScene::render(const QSize &viewportSize)
{
    m_renderer->UpdateViewport(m_root,viewportSize.width(),viewportSize.height());
    m_renderer->render(m_root);
}
QT_END_NAMESPACE
