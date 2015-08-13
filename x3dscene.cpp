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
    event_filter = NULL;
    fake_rotating = false;
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

void X3DScene::addTexture(int textureId, const QRectF &targetRect, const QSize &textureSize, int depth, bool targethasInvertedY, bool sourceHasInvertedY, void* data)
{
    std::map<int, Texture2DNode*>::iterator found;
    if ((found = nodes.find(textureId)) == nodes.end()) {
        TransformNode* transform = new TransformNode();
            transform->setTranslation(2.0f + (1.0f * nodes.size()), 0.0f, 0.0f);
            TouchSensorNode* touch_node = new TouchSensorNode();
            touch_node->setValue(data);
            transform->addChildNode(touch_node);
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

void X3DScene::installEventFilter(SceneEventFilter* filter)
{
    event_filter = filter;
}

void X3DScene::sendPointerEvent(int id, const QPointF& viewportPos, Qt::TouchPointState state)
{
    double from[3];
    double to[3];
    if (id == 2 && state == Qt::TouchPointPressed) {
        fake_rotating = true;
    } else if (id == 2 && state == Qt::TouchPointReleased) {
        fake_rotation = 0.0;
        fake_rotating = false;
    } else if (state == Qt::TouchPointPressed &&
        m_renderer->get_ray(viewportPos.x(), viewportPos.y(), this->view, from, to)) {
        btVector3 bt_from(from[0], from[1], from[2]);
        btVector3 bt_to(to[0], to[1], to[2]);
        btCollisionWorld::ClosestRayResultCallback ray_result(bt_from, bt_to);

        m_world->rayTest(bt_from, bt_to, ray_result);

        if (ray_result.hasHit()) {
            Node* node = static_cast<Node*>(ray_result.m_collisionObject->getUserPointer());
            if (node != NULL)
            {
                TouchSensorNode* touch_node = node->getTouchSensorNodes();
                if (touch_node != NULL)
                {
                    touch_node->setHitPointChanged(ray_result.m_hitPointWorld.x(),
                                                   ray_result.m_hitPointWorld.y(),
                                                   ray_result.m_hitPointWorld.z());
                    touch_node->setHitNormalChanged(ray_result.m_hitNormalWorld.x(),
                                                    ray_result.m_hitNormalWorld.y(),
                                                    ray_result.m_hitNormalWorld.z());

                    // This is just a quick hack for the prototype.
                    btVector3 hitPointLocal = ray_result.m_collisionObject->getWorldTransform().inverse() * ray_result.m_hitPointWorld;

                    ShapeNode* shape = node->getShapeNodes();
                    Geometry3DNode* bounded_node = NULL;
                    if (shape != NULL) {
                        bounded_node = shape->getGeometry3DNode();
                    }

                    if (bounded_node) {
                        float size[3];
                        bounded_node->getBoundingBoxSize(size);
                        btVector3 bt_size(size[0], size[1], size[2]);
                        btVector3 texCoord = (hitPointLocal + bt_size) / (bt_size * 2);
                        touch_node->setHitTexCoord(texCoord.x(), texCoord.y());
                        //touch_node->setTouchTime(double value);
                        if (touch_node->getValue() != NULL) {
                            if (event_filter != NULL) {
                                event_filter->sceneEventFilter(touch_node->getValue(), {texCoord.x(), texCoord.y()});
                            }
                        }
                    }
                }
            }
        }
    }
}

void X3DScene::sendAxisEvent(int id, const double& value)
{
    if (id == 0 && fake_rotating) {
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

    const float speed = navInfo->getSpeed();
    float view_translation[3] = {fake_velocity[0] * speed,
                                 fake_velocity[1] * speed,
                                 -fake_velocity[2] * speed};

    view->translate(view_translation);

    float view_rotation[4] = {0.0f, 1.0f, 0.0f, -fake_rotation * 0.01f};
    view->rotate(view_rotation);

    m_root->update();

    // Only touch sensors for now top level only
    Node* node = m_root->getTransformNodes();
    while (node != NULL) {
        TouchSensorNode* touch_node = node->getTouchSensorNodes();
        if (touch_node != NULL) {
            // Only shape nodes for now
            ShapeNode* shape = node->getShapeNodes();
            Geometry3DNode* bounded_node = NULL;
            if (shape != NULL) {
                bounded_node = shape->getGeometry3DNode();
            }

            if (bounded_node) {
                float trans[4][4];
                bounded_node->getTranslationMatrix(trans);
                float size[3];
                float center[3];
                bounded_node->getBoundingBoxSize(size);
                bounded_node->getBoundingBoxCenter(center);

                btCollisionShape* bt_collision = new btBoxShape(btVector3(size[0], size[1], size[2]));
                btDefaultMotionState* bt_motionstate = new btDefaultMotionState(btTransform(
                        btMatrix3x3(trans[0][0], trans[0][1], trans[0][2],
                                    trans[1][0], trans[1][1], trans[1][2],
                                    trans[2][0], trans[2][1], trans[2][2]),
                        btVector3(trans[3][0], -trans[3][1], trans[3][2])
                        ));

                btRigidBody::btRigidBodyConstructionInfo bt_info(
                            0,
                            bt_motionstate,
                            bt_collision,
                            btVector3(0,0,0)
                            );

                btRigidBody *bt_rigid_body = new btRigidBody(bt_info);
                bt_rigid_body->setUserPointer(node);
                m_world->addRigidBody(bt_rigid_body);
            }
        }
        node = node->next();
    }

    m_world->stepSimulation((btScalar)physics.restart()/(btScalar)1000, 10);

    view->getMatrix(this->view);
}

void X3DScene::render(const QSize &viewportSize)
{
    double aspect = (double)viewportSize.width()/(double)viewportSize.height();

    ViewpointNode *view = m_root->getViewpointNode();
    if (view == NULL)
        view = m_root->getDefaultViewpointNode();

    double fov = (view->getFieldOfView() / 3.14) * 180.0;

    m_renderer->set_projection(fov, aspect, 0.1f, 10000.0f);
    m_renderer->set_viewport(viewportSize.width(), viewportSize.height());
    m_renderer->render(m_root);
}
QT_END_NAMESPACE
