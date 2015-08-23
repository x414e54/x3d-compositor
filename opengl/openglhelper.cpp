#include "openglrenderer.h"

#include <thread>
#include <iostream>

#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLVertexArrayObject>
#include <QtGui/QOpenGLBuffer>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QOpenGLFunctions_3_2_Core>
#include <QtOpenGLExtensions/QOpenGLExtensions>
#include <QFuture>
#include <QtConcurrent/QtConcurrentRun>
#include <QThreadPool>

#include "opengloutput.h"

static void debug_callback( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *msg, const void *data )
{

    std::cout << msg;
}

ContextPoolContext::ContextPoolContext(QOpenGLContext* share_context, bool reserved)
    : reserved(reserved), count(0), surface(NULL), context(NULL)
{
    QSurfaceFormat format;
    format.setMajorVersion(3);
    format.setMinorVersion(2);
    format.setSwapInterval(0);
    format.setSwapBehavior(QSurfaceFormat::SingleBuffer);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setOption(QSurfaceFormat::DebugContext);
    surface = new QOffscreenSurface();
    surface->setFormat(format);
    surface->create();
    if (!surface->isValid()) {
        throw;
    }

    context = new QOpenGLContext();
    context->setShareContext(share_context);
    context->setFormat(surface->requestedFormat());

    if (!context->create()) {
        throw;
    }

    context->makeCurrent(surface);
    gl = context->versionFunctions<QOpenGLFunctions_3_2_Core>();
    if (gl == nullptr || !gl->initializeOpenGLFunctions()) {
        throw;
    }
    indirect = new QOpenGLExtension_ARB_multi_draw_indirect();
    if (!indirect->initializeOpenGLFunctions()) {
        throw;
    }
    vab = new QOpenGLExtension_ARB_vertex_attrib_binding();
    if (!vab->initializeOpenGLFunctions()) {
        throw;
    }
    sso = new QOpenGLExtension_ARB_separate_shader_objects();
    if (!sso->initializeOpenGLFunctions()) {
        throw;
    }
    debug = new QOpenGLExtension_ARB_debug_output();
    if (!debug->initializeOpenGLFunctions()) {
        throw;
    }
    debug->glDebugMessageCallbackARB(debug_callback, nullptr);
    context->doneCurrent();
    context->moveToThread(nullptr);
}

ContextPoolContext::~ContextPoolContext()
{
    if (context != NULL) {
        delete context;
    }
    if (surface != NULL) {
        delete surface;
    }
}

void ContextPoolContext::increment()
{
    ++count;
}

bool ContextPoolContext::decrement()
{
    return (--count == 0);
}

bool ContextPoolContext::assign(std::thread::id thread)
{
    if (used.test_and_set() == false) {
        this->thread = thread;
    }
    return this->thread == thread;
}

void ContextPoolContext::release()
{
    thread = std::thread::id();
    used.clear();
}

bool ContextPoolContext::make_current()
{
    if (context != NULL) {
        prev_context = QOpenGLContext::currentContext();
        if (prev_context != context) {
            context->moveToThread(QThread::currentThread());
            return context->makeCurrent(surface);
        } else {
            return true;
        }
    } else {
        return false;
    }
}

int ContextPoolContext::get_fbo(const RenderTarget& render_target)
{
    RenderTargetFboMap::iterator it = fbos.find(render_target);
    if (it == fbos.end()) {
        unsigned int fbo = 0;
        gl->glGenFramebuffers(1, &fbo);
        gl->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        for (size_t i = 0; i < render_target.num_attachments; ++i) {
            gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                                       GL_TEXTURE_2D, render_target.attachments[i], 0);
        }

        if (render_target.use_depth) {
            gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                       GL_TEXTURE_2D, render_target.depth, 0);
        }

        if (gl->glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                throw;
        }
        fbos[render_target] = fbo;
        return fbo;
    } else {
        return it->second;
    }
}

int ContextPoolContext::get_pipeline(const Material& material)
{
    MaterialPipelineMap::iterator it = pipelines.find(material);
    if (it == pipelines.end()) {
        unsigned int pipeline = 0;
        sso->glGenProgramPipelines(1, &pipeline);
        sso->glUseProgramStages(pipeline, GL_VERTEX_SHADER_BIT, material.vert);
        sso->glUseProgramStages(pipeline, GL_FRAGMENT_SHADER_BIT, material.frag);
        pipelines[material] = pipeline;
        return pipeline;
    } else {
        return it->second;
    }
}

int ContextPoolContext::get_vao(const VertexFormat& format)
{
    VertexFormatVaoMap::iterator it = vaos.find(format);
    if (it == vaos.end()) {
        unsigned int vao = 0;
        gl->glGenVertexArrays(1, &vao);
        gl->glBindVertexArray(vao);

        // First is always draw information
        gl->glEnableVertexAttribArray(0);
        vab->glVertexAttribBinding(0, 0);
        vab->glVertexAttribFormat(0, 4, GL_UNSIGNED_INT, false, 0);

        for (size_t i = 1; i < format.num_attribs + 1; ++i) {
            const Attribute& attrib = format.attribs[i - 1];
            gl->glEnableVertexAttribArray(i);
            vab->glVertexAttribBinding(i, 1);
            vab->glVertexAttribFormat(i, attrib.components, attrib.type, attrib.normalized, attrib.offset);
        }
        vaos[format] = vao;
        return vao;
    } else {
        return it->second;
    }
}

void ContextPoolContext::done_current()
{
    if (context != NULL && prev_context != context) {
        context->doneCurrent();
        context->moveToThread(nullptr);
        if (prev_context != NULL) {
            prev_context->makeCurrent(prev_context->surface());
        }
    }
}

thread_local ContextPoolContext* ContextPool::thread_pool_context = NULL;

ContextPool::ContextPool()
{
    QThreadPool* global_pool = QThreadPool::globalInstance();
    context_pool.push_back(ContextPoolContext(nullptr));
    share_context = context_pool[0].context;

    int num_to_create = std::min(global_pool->maxThreadCount(), 2);
    for (int i = 0; i < num_to_create; ++i) {
        context_pool.push_back(ContextPoolContext(this->share_context, i < 2));
    }
}

ContextPool::~ContextPool()
{

}

ContextPoolContext& ContextPool::assign_context(int named_id)
{
    std::thread::id thread = std::this_thread::get_id();
    if (named_id >= 0 && named_id < context_pool.size()) {
        while (!context_pool[named_id].assign(thread)) {
            std::this_thread::yield();
        }
        context_pool[named_id].increment();
        return context_pool[named_id];
    }

    if (thread_pool_context != nullptr) {
        // Already assigned on current thread
        thread_pool_context->increment();
        return *thread_pool_context;
    }

    for (std::vector<ContextPoolContext>::iterator it = context_pool.begin(); it != context_pool.end(); ++ it) {
        if (!it->reserved && it->assign(thread)) {
            thread_pool_context = &(*it);
            thread_pool_context->increment();
            return *it;
        }
    }
    throw;
}

void ContextPool::return_context(ContextPoolContext& context)
{
    if (context.decrement()) {
        if (thread_pool_context == &context) {
            thread_pool_context = nullptr;
        }
        context.release();
    }
}
