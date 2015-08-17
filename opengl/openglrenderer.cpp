#include "openglrenderer.h"

#include <thread>
#include <iostream>

#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLVertexArrayObject>
#include <QtGui/QOpenGLBuffer>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QOpenGLFunctions_3_0>
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
    format.setMajorVersion(2);
    format.setMinorVersion(1);
    format.setSwapInterval(0);
    format.setSwapBehavior(QSurfaceFormat::SingleBuffer);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
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
    gl = context->versionFunctions<QOpenGLFunctions_3_0>();
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

int ContextPoolContext::get_fbo(int render_target_id)
{
    RenderTargetFboMap::iterator it = fbos.find(render_target_id);
    if (it == fbos.end()) {
        unsigned int fbo = 0;
        gl->glGenFramebuffers(1, &fbo);
        gl->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_target_id, 0);
        fbos[render_target_id] = fbo;
        return fbo;
    } else {
        return it->second;
    }
}

int ContextPoolContext::get_pipeline(const Material& material)
{
    MaterialPipelineMap::iterator it = pipelines.find(material);
    if (it == pipelines.end()) {
        int pipeline = 0;
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
        size_t offset = 0;
        for (size_t i = 0; i < format.num_attribs; ++i) {
            const Attribute& attrib = format.attribs[i];
            gl->glEnableVertexAttribArray(i);
            vab->glVertexAttribFormat(i, attrib.attrib_size, GL_UNSIGNED_BYTE, attrib.normalized, offset);
            offset += attrib.attrib_size;
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

OpenGLRenderer::OpenGLRenderer()
{
}

OpenGLRenderer::~OpenGLRenderer()
{

}

void OpenGLRenderer::set_viewpoint_output(int id, OpenGLOutput& output)
{
    ScopedContext context(this->context_pool, 0);
    active_viewpoint.output = &output;
    active_viewpoint.output->init_context(this->context_pool.share_context);
    active_viewpoint.left.enabled = output.is_enabled();
    active_viewpoint.right.enabled = output.is_enabled() && output.is_stereo();
}

void OpenGLRenderer::render_viewpoint(OpenGLRenderer* renderer, const RenderOuputGroup& output, int context_id)
{
    ScopedContext context(renderer->context_pool, context_id);

    for (std::map<std::string, Material>::iterator material_it = renderer->materials.begin(); material_it != renderer->materials.end(); ++material_it) {
        context.context.sso->glBindProgramPipeline(context.context.get_pipeline(material_it->second));
        for (std::vector<DrawBatch>::iterator batch_it = material_it->second.batches.begin(); batch_it != material_it->second.batches.end(); ++batch_it) {
            context.context.gl->glBindVertexArray(context.context.get_vao(batch_it->format));
            context.context.indirect->glMultiDrawArraysIndirect(batch_it->primitive_type, (const void*)batch_it->buffer_offset, batch_it->num_draws, batch_it->draw_stride);
        }
    }
}

void OpenGLRenderer::set_viewpoint_viewport(int id, size_t width, size_t height)
{
    ScopedContext context(this->context_pool, 0);

    QOpenGLFramebufferObjectFormat format;
    format.setSamples(0);
    format.setMipmap(false);
    format.setAttachment(QOpenGLFramebufferObject::NoAttachment);
    format.setTextureTarget(GL_TEXTURE_2D);

    bool set_textures = false;
    if (active_viewpoint.left.viewport_width != width ||
        active_viewpoint.left.viewport_height != height)
    {
        active_viewpoint.left.render_target = new QOpenGLFramebufferObject(width, height, format);
        active_viewpoint.left.viewport_width = width;
        active_viewpoint.left.viewport_height = height;
        set_textures = true;
    }

    if (active_viewpoint.right.viewport_width != width ||
        active_viewpoint.right.viewport_height != height)
    {
        active_viewpoint.right.render_target = new QOpenGLFramebufferObject(width, height, format);
        active_viewpoint.right.viewport_width = width;
        active_viewpoint.right.viewport_height = height;
        set_textures = true;
    }

    if (set_textures && active_viewpoint.output != nullptr) {
        active_viewpoint.output->set_textures(active_viewpoint.left.render_target->texture(),
                                              active_viewpoint.right.render_target->texture(),
                                              width,
                                              height);
    }
}

QOpenGLBuffer* OpenGLRenderer::get_buffer(const VertexFormat& format)
{
    VertexFormatBufferMap::iterator it = buffers.find(format);
    if (it == buffers.end()) {
        QOpenGLBuffer* vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        vbo->create();
        vbo->bind();
        vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
        vbo->unmap();
        buffers[format] = vbo;
        return vbo;
    } else {
        return it->second;
    }
}

void OpenGLRenderer::render_viewpoints()
{
    QFuture<void> left_render;
    if (active_viewpoint.left.enabled) {
        left_render = QtConcurrent::run(render_viewpoint, this, active_viewpoint.left, 1);
    }

    QFuture<void> right_render;
    if (active_viewpoint.right.enabled) {
        right_render = QtConcurrent::run(render_viewpoint, this, active_viewpoint.right, 2);
    }

    left_render.waitForFinished();
    right_render.waitForFinished();

    if (active_viewpoint.output != nullptr) {
        active_viewpoint.output->submit();
    }
}
