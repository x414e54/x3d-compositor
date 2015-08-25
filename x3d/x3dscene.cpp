#include "x3dscene.h"

#include <QtDebug>
#include <QTemporaryFile>
#include <QDateTime>

#define CX3D_SUPPORT_OPENGL
#include <cybergarage/x3d/CyberX3D.h>
using namespace CyberX3D;

#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>

#include "x3drenderer.h"

QT_BEGIN_NAMESPACE

X3DScene::X3DScene(X3DRenderer* renderer)
    : fake_velocity{0.0f, 0.0f, 0.0f}
    , fake_rotation(0.0f)
    , m_current_key_device(nullptr)
    , m_current_touch(nullptr)
    , m_renderer(renderer)
{
    m_root = new SceneGraph();
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

void X3DScene::addToPhysics(Node* node)
{
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
                node->setValue(bt_rigid_body);
                m_world->addRigidBody(bt_rigid_body);
            }
        }
        node = node->next();
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

    addToPhysics(m_root->getTransformNodes());
    nodes.clear();
    physics.restart();
}

void X3DScene::add_texture(int texture_id, float real_width, float real_height,
                           size_t width, size_t height, void* data)
{
    std::map<void*, NodePhysicsGroup>::iterator found;
    if ((found = nodes.find(data)) == nodes.end()) {

        ViewpointNode *view = m_root->getViewpointNode();
        if (view == NULL) {
            view = m_root->getDefaultViewpointNode();
        }

        TransformNode* transform = new TransformNode();
            float position[3];
            view->getPosition(position);
            position[2] -= 0.5;
            float rotation[4];
            view->getOrientation(rotation);
            transform->setRotation(rotation);
            transform->setCenter(0.0, 0.0, 0.5);
            transform->setTranslation(position);
            TouchSensorNode* touch_node = new TouchSensorNode();
            touch_node->setValue(data);
            transform->addChildNode(touch_node);
            KeySensorNode* key_node = new KeySensorNode();
            key_node->setValue(data);
            transform->addChildNode(key_node);
            m_root->addRoute(touch_node, touch_node->getIsActiveField(), key_node, key_node->getEnabledField());
            ShapeNode* shape = new ShapeNode();
                AppearanceNode* appearance = new AppearanceNode();
                    ImageTextureNode* texture = new ImageTextureNode();
                        texture->createImageFrom(texture_id, width, height, true);
                    appearance->addChildNode(texture);
                shape->addChildNode(appearance);
                BoxNode* box = new BoxNode();
                box->setSize(real_width, real_height, 1.0f/100.0f);
                shape->addChildNode(box);
            transform->addChildNode(shape);
        nodes[data].top_node = transform;
        nodes[data].texture_node = texture;
        nodes[data].bounded_node = box;
        m_root->addNode(transform);
        addToPhysics(transform);
        nodes[data].bt_rigid_body = (btRigidBody *)transform->getValue();
    } else if (found->second.texture_node != NULL){
        found->second.texture_node->setTextureName(texture_id);
    }
}

void X3DScene::remove_texture(void* data)
{
    std::map<void*, NodePhysicsGroup>::iterator found;
    if ((found = nodes.find(data)) != nodes.end()) {
        if (found->second.top_node != NULL) {
            delete found->second.top_node;
        }

        m_world->removeRigidBody(found->second.bt_rigid_body);
        delete found->second.bt_rigid_body->getMotionState();
        delete found->second.bt_rigid_body->getCollisionShape();
        delete found->second.bt_rigid_body;
        nodes.erase(found);
    }
}

void X3DScene::sendKeyDown(uint code)
{
    // TODO be more efficient here
    m_current_key_device = m_root->getSelectedKeyDeviceSensorNode();

    if (m_current_key_device != nullptr) {
        m_current_key_device->setKeyPress(code);

        // TODO route this
        if (m_current_key_device->getValue() != nullptr
            && event_filter != nullptr) {
            event_filter->sceneKeyEventFilter(m_current_key_device->getValue(),
                                              code, SceneEventFilter::DOWN);
        }
    } else {
        if (code == 25) {
            fake_velocity[2] += 1.0f;
        } else if(code == 39) {
            fake_velocity[2] -= 1.0f;
        } else if(code == 38) {
            fake_velocity[0] -= 1.0f;
        } else if(code == 40) {
            fake_velocity[0] += 1.0f;
        } else if (code == 113) {
            fake_rotating = true;
            fake_rotation = -1.0f;
        } else if(code == 114) {
            fake_rotating = true;
            fake_rotation = 1.0f;
        } else if(code == 111) {
        } else if(code == 116) {
        }
    }
}

void X3DScene::sendKeyUp(uint code)
{
    if (m_current_key_device != nullptr) {
        m_current_key_device->setKeyRelease(code);

        // TODO route this
        if (m_current_key_device->getValue() != nullptr
            && event_filter != nullptr) {
            event_filter->sceneKeyEventFilter(m_current_key_device->getValue(),
                                              code, SceneEventFilter::UP);
        }

        if (code == 9) {
            m_current_key_device->setEnabled(false);
        }
    } else {
        if (code == 25) {
            fake_velocity[2] -= 1.0f;
        } else if(code == 39) {
            fake_velocity[2] += 1.0f;
        } else if(code == 38) {
            fake_velocity[0] += 1.0f;
        } else if(code == 40) {
            fake_velocity[0] -= 1.0f;
        } else if (code == 113) {
            fake_rotating = false;
            fake_rotation = 0.0f;
        } else if(code == 114) {
            fake_rotating = false;
            fake_rotation = 0.0f;
        } else if(code == 111) {
        } else if(code == 116) {
        }
    }
}

void X3DScene::installEventFilter(SceneEventFilter* filter)
{
    event_filter = filter;
}

void X3DScene::sendPointerEvent(int id, float x, float y, Qt::TouchPointState state)
{
    Scalar from[3];
    Scalar to[3];
    if (m_current_touch == nullptr && id == 2 && state == Qt::TouchPointPressed) {
        fake_rotating = true;
    } else if (m_current_touch == nullptr && id == 2 && state == Qt::TouchPointReleased) {
        fake_rotation = 0.0;
        fake_rotating = false;
    } else {
        m_renderer->get_ray(x, y, this->view, from, to);
        btVector3 bt_from(from[0], from[1], from[2]);
        btVector3 bt_to(to[0], to[1], to[2]);
        btCollisionWorld::ClosestRayResultCallback ray_result(bt_from, bt_to);

        m_world->rayTest(bt_from, bt_to, ray_result);

        bool handled = false;
        if (ray_result.hasHit()) {
            Node* node = static_cast<Node*>(ray_result.m_collisionObject->getUserPointer());
            if (node != NULL)
            {
                TouchSensorNode* touch_node = node->getTouchSensorNodes();
                if (touch_node != nullptr && (touch_node == m_current_touch
                                              || m_current_touch == nullptr))
                {
                    handled = true;
                    m_current_touch = touch_node;

                    touch_node->setHitPointChanged(ray_result.m_hitPointWorld.x(),
                                                   ray_result.m_hitPointWorld.y(),
                                                   ray_result.m_hitPointWorld.z());
                    touch_node->setHitNormalChanged(ray_result.m_hitNormalWorld.x(),
                                                    ray_result.m_hitNormalWorld.y(),
                                                    ray_result.m_hitNormalWorld.z());

                    bool was_over = touch_node->isOver();
                    bool was_active = touch_node->isActive();

                    touch_node->setIsOver(true);
                    if (state == Qt::TouchPointPressed) {
                        touch_node->setIsActive(true);
                    } else if (state == Qt::TouchPointReleased) {
                        if (touch_node->isActive()) {
                            touch_node->setTouchTime(QDateTime::currentMSecsSinceEpoch());
                        }
                        touch_node->setIsActive(false);
                    }

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

                        // This should be routed via update
                        if (touch_node->getValue() != nullptr
                            && event_filter != nullptr) {
                            event_filter->sceneEventFilter(touch_node->getValue(),
                                {texCoord.x(), texCoord.y()},
                                SceneEventFilter::convert_event(was_active, touch_node->isActive()));
                        }
                        //
                    }

                    // TODO better way to update routes than this!
                    if (!was_active && touch_node->isActive()) {
                        m_root->updateRoute(touch_node, touch_node->getIsActiveField());
                    }
                }
            }
        }

        if (handled == false && m_current_touch != nullptr) {
            m_current_touch->setIsOver(false);
            if (state == Qt::TouchPointReleased || !m_current_touch->isActive()) {
                m_current_touch->setIsActive(false);

                // This should be routed via update
                if (m_current_touch->getValue() != nullptr
                    && event_filter != nullptr) {
                    float tex_coord[2];
                    m_current_touch->getHitTexCoord(tex_coord);
                    event_filter->sceneEventFilter(m_current_touch->getValue(),
                                                   tex_coord, SceneEventFilter::UP);
                    event_filter->sceneEventFilter(m_current_touch->getValue(),
                                                   tex_coord, SceneEventFilter::EXIT);
                }
                //
                m_current_touch = nullptr;
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

    const float speed = navInfo->getSpeed() * (physics.elapsed() / 1000.0f);
    float view_translation[3] = {fake_velocity[0] * speed,
                                 fake_velocity[1] * speed,
                                 -fake_velocity[2] * speed};

    view->translate(view_translation);

    float view_rotation[4] = {0.0f, 1.0f, 0.0f, -fake_rotation * speed};
    view->rotate(view_rotation);

    m_root->update();

    m_world->stepSimulation((btScalar)physics.restart()/(btScalar)1000, 10);

    view->getMatrix(this->view);
}

void X3DScene::render(const QSize &viewport_size)
{
    if (viewport_size.width() == 0 || viewport_size.height() == 0) {
        return;
    }

    Scalar aspect = (Scalar)viewport_size.width()/(Scalar)viewport_size.height();

    ViewpointNode *view = m_root->getViewpointNode();
    if (view == nullptr) {
        view = m_root->getDefaultViewpointNode();
    }

    Scalar fov = (view->getFieldOfView() / 3.14) * 180.0;

    m_renderer->set_projection(fov, aspect, 0.1f, 10000.0f);
    m_renderer->render(m_root);
}
QT_END_NAMESPACE
