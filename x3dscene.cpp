#include "x3dscene.h"

#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtDebug>
#include <QTemporaryFile>

#define CX3D_SUPPORT_OPENGL
#include <cybergarage/x3d/CyberX3D.h>
using namespace CyberX3D;

#include "x3drenderer.h"

QT_BEGIN_NAMESPACE

X3DScene::X3DScene()
    : fake_velocity{0.0f, 0.0f, 0.0f}
{
    m_root = new SceneGraph();
    m_renderer = new X3DRenderer();
}

X3DScene::~X3DScene()
{
    if (m_renderer != NULL) {
        delete m_renderer;
    }
    if (m_root != NULL) {
        delete m_root;
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

    const float speed = navInfo->getSpeed();
    float view_translation[3] = {fake_velocity[0] * speed,
                                 fake_velocity[1] * speed,
                                 -fake_velocity[2] * speed};

    view->translate(view_translation);

    float view_rotation[4] = {0.0f, 1.0f, 0.0f, fake_rotation};
    view->setOrientation(view_rotation);

    m_root->update();
}

void X3DScene::render(const QSize &viewportSize)
{
    m_renderer->UpdateViewport(m_root,viewportSize.width(),viewportSize.height());
    m_renderer->render(m_root);
}
QT_END_NAMESPACE
