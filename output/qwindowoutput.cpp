#include "qwindowoutput.h"

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <QOpenGLContext>
#include <QtGui/QOpenGLFunctions_3_2_Core>
#include <QtGui/QOpenGLVertexArrayObject>
#include <QResizeEvent>

QWindowOutput::QWindowOutput() : context(nullptr)
{
    QSurfaceFormat format;
    format.setMajorVersion(3);
    format.setMinorVersion(2);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    setMinimumSize(QSize(1920, 1080));
    setFormat(format);
    setSurfaceType(OpenGLSurface);
    create();

    this->quad_buffered = this->format().stereo();
    this->output_width = this->width();
    this->output_height = this->height();
    this->stereo = false;

    this->installEventFilter(this);

#if !USE_WAYLAND
    update_scheduler.setSingleShot(false);
    update_scheduler.setInterval(10);
    connect(&update_scheduler,SIGNAL(timeout()),this,SLOT(update()));
    update_scheduler.start();
#endif
}


QWindowOutput::~QWindowOutput()
{

}

static void mesa_hack(QOpenGLFunctions_3_2_Core* gl)
{
    int mesa_hack = gl->glCreateProgram();
    int mesa_hack_vp = gl->glCreateShader(GL_VERTEX_SHADER);
    int mesa_hack_fp = gl->glCreateShader(GL_FRAGMENT_SHADER);
    const char* empty = "#version 150\nvoid main() {}";
    gl->glShaderSource(mesa_hack_vp, 1, &empty, nullptr);
    gl->glCompileShader(mesa_hack_vp);
    gl->glShaderSource(mesa_hack_fp, 1, &empty, nullptr);
    gl->glCompileShader(mesa_hack_fp);
    gl->glAttachShader(mesa_hack, mesa_hack_vp);
    gl->glAttachShader(mesa_hack, mesa_hack_fp);
    gl->glLinkProgram(mesa_hack);
    GLint valid = GL_FALSE;
    gl->glGetProgramiv(mesa_hack, GL_LINK_STATUS, &valid);
    if (valid == GL_FALSE) {
        throw;
    }
    gl->glUseProgram(mesa_hack);
    float vertex[] = {
        0.0f,  0.0f,  0.0f
    };
    GLuint vbo = 0;
    gl->glGenBuffers(1, &vbo);
    gl->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    gl->glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, GL_STATIC_DRAW);
    gl->glEnableVertexAttribArray (0);
    gl->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    gl->glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    gl->glDrawArrays(GL_TRIANGLES, 0, 1);
}

void QWindowOutput::init_context(QOpenGLContext* share_context)
{
    if (context != nullptr) {
        if (share_context == context->shareContext()) {
            return;
        }

        context->makeCurrent(this);
        if (vao != nullptr) {
            delete vao;
        }
        if (gl != nullptr) {
            delete gl;
        }
        context->doneCurrent();
        delete context;
    }

    context = new QOpenGLContext();
    context->setShareContext(share_context);
    context->setFormat(requestedFormat());
    if (!context->create()) {
        throw;
    }

    context->makeCurrent(this);
    gl = context->versionFunctions<QOpenGLFunctions_3_2_Core>();
    if (gl == nullptr || !gl->initializeOpenGLFunctions()) {
        throw;
    }
    vao = new QOpenGLVertexArrayObject();
    vao->bind();

    mesa_hack(gl);

    context->doneCurrent();
}

void QWindowOutput::swap_buffers()
{
    ScopedOutputContext context(*this);
    this->context->swapBuffers(this);
}

void QWindowOutput::make_current()
{
    if (context == nullptr) {
        throw;
    }
    context->makeCurrent(this);
}

void QWindowOutput::done_current()
{
    if (context == nullptr) {
        throw;
    }
    context->doneCurrent();
}

void QWindowOutput::resizeEvent(QResizeEvent* event)
{
   QWindow::resizeEvent(event);
   this->output_height = event->size().height();
   this->output_width = event->size().width();
}

void QWindowOutput::get_eye_matrix(glm::mat4x4 &left, glm::mat4x4 &right)
{
    const float fake_ipd = 60.0/1000.0;
    left = glm::translate(glm::mat4x4(), glm::vec3(-fake_ipd, 0.0, 0.0));
    right = glm::translate(glm::mat4x4(), glm::vec3(fake_ipd, 0.0, 0.0));
}

bool QWindowOutput::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != this || this->listener == nullptr) {
        return false;
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        listener->send_pointerevent(me->button(), me->localPos().x() / this->width(),
                                  me->localPos().y() / this->height(), Qt::TouchPointPressed);
        return true;
    }
    case QEvent::MouseButtonRelease: {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        listener->send_pointerevent(me->button(), me->localPos().x() / this->width(),
                                  me->localPos().y() / this->height(), Qt::TouchPointReleased);
        return true;
    }
    case QEvent::MouseMove: {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        listener->send_pointerevent(0, me->localPos().x() / this->width(),
                                  me->localPos().y() / this->height(), Qt::TouchPointMoved);
        double x = double(me->localPos().x()/this->width() - 0.5f);
        double y = double(me->localPos().y()/this->height() - 0.5f);
        listener->send_axisevent(0, x);
        listener->send_axisevent(1, y);
        break;
    }
    case QEvent::KeyPress: {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        listener->send_keydown(ke->nativeScanCode());
        break;
    }
    case QEvent::KeyRelease: {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        listener->send_keyup(ke->nativeScanCode());
        break;
    }
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    {
        QTouchEvent *te = static_cast<QTouchEvent *>(event);
        QList<QTouchEvent::TouchPoint> points = te->touchPoints();
        QPointF point = points.at(0).pos().toPoint();
        if (!points.isEmpty()) {
            listener->send_pointerevent(points.at(0).id(),
                                     point.x() / this->width(),
                                     point.y() / this->height(), Qt::TouchPointPressed);
        }
        break;
    }
    default:
        break;
    }
    return false;
}

void QWindowOutput::update()
{
    if (this->listener == nullptr) {
        return;
    }

    listener->update();
    listener->render(this->size().width(), this->size().height());
    this->swap_buffers();
}
