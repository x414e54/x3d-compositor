/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "x3dscene.h"

#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtDebug>
#include <QTemporaryFile>

QT_BEGIN_NAMESPACE

X3DScene::X3DScene()
{
}

X3DScene::~X3DScene()
{
}

void X3DScene::load(const QString& filename)
{
    std::string file_utf8 = filename.toUtf8().constData();
    if (m_root.load(file_utf8.c_str()) == false) {
        qCritical() << "Loading error"
            << "\nLine Number: " << m_root.getParserErrorLineNumber()
            << "\nError Message: " << m_root.getParserErrorMessage()
            << "\nError Token: " << m_root.getParserErrorToken()
            << "\nError Line: " << m_root.getParserErrorLineString();
    }

    m_root.initialize();
    if (m_root.getViewpointNode() == NULL)
        m_root.zoomAllViewpoint();
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
        m_root.addNode(transform);
    } else if (found->second != NULL){
        found->second->setTextureName(textureId);
    }

    m_root.update();
    return;
}

void X3DScene::render(const QSize &viewportSize)
{
    m_renderer.UpdateViewport(&m_root,viewportSize.width(),viewportSize.height());
    m_renderer.render(&m_root);

    return;
}
QT_END_NAMESPACE
