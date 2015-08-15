#ifndef OPENGLRENDERER_H
#define OPENGLRENDERER_H

#include <map>
#include <vector>
#include <QAtomicInt>
#include <QThreadStorage>
#include <QOffscreenSurface>

class OpenGLOutput;
class QOpenGLBuffer;
class QOpenGLVertexArrayObject;
class QOpenGLContext;
class QOpenGLFramebufferObject;
class QOpenGLFunctions_4_3_Compatibility;

class Material
{
public:
    QOpenGLVertexArrayObject* vao;
    QOpenGLBuffer* opaque_objects;
    QOpenGLBuffer* tranparent_objects;
};

class RenderOuputGroup
{
public:
    QOpenGLFramebufferObject* fbo;
    float projection[4][4];
    float view[4][4];
    bool enabled;
};

class Viewpoint
{
public:
    RenderOuputGroup left;
    RenderOuputGroup right;
    OpenGLOutput* output;
};

class ContextPoolContext : public QOffscreenSurface
{
public:
    ContextPoolContext();
    QAtomicInt used;
    QOpenGLContext* context;
    QOpenGLFunctions_4_3_Compatibility* gl;

    bool make_current();
    void release();
};
typedef std::map<QOpenGLVertexArrayObject*, QOpenGLBuffer*> VertexFormatBufferMap;

class OpenGLRenderer
{
public:
    OpenGLRenderer();
    virtual ~OpenGLRenderer();
    void set_viewpoint_output(int id, OpenGLOutput* output);
    void render_viewpoints();
protected:
    void render_viewpoint(const float (&proj)[4][4], const float (&view)[4][4]);
    Viewpoint active_viewpoint;
    VertexFormatBufferMap buffers;
    std::vector<ContextPoolContext> context_pool;
    QThreadStorage<ContextPoolContext> current_context;
};

#endif // OPENGLRENDERER_H

