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

OpenGLRenderer::OpenGLRenderer()
{
    ScopedContext context(context_pool, 0);
    context.context.gl->glGenBuffers(1, &this->global_uniforms);
    context.context.gl->glBindBuffer(GL_UNIFORM_BUFFER, this->global_uniforms);
    context.context.gl->glBufferData(GL_UNIFORM_BUFFER, 65536, nullptr, GL_DYNAMIC_DRAW);

    context.context.gl->glGenBuffers(1, &this->draw_calls);
    context.context.gl->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, this->draw_calls);
    context.context.gl->glBufferData(GL_DRAW_INDIRECT_BUFFER, 65536, nullptr, GL_DYNAMIC_DRAW);
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

    context.context.gl->glBindFramebuffer(GL_FRAMEBUFFER,
        context.context.get_fbo(output.render_target->texture()));
    context.context.gl->glDrawBuffer(GL_COLOR_ATTACHMENT0);
    context.context.gl->glViewport(0, 0, output.viewport_width, output.viewport_height);

    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glClearColor(renderer->clear_color[0], renderer->clear_color[1],
                 renderer->clear_color[2], renderer->clear_color[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //context.context.gl->glBindBufferRange(GL_UNIFORM_BUFFER, 0, this->global_uniforms, 0, output.uniform_offset);
    context.context.gl->glBindBufferBase(GL_UNIFORM_BUFFER, 0, renderer->global_uniforms);
    context.context.gl->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderer->draw_calls);

    for (std::map<std::string, Material>::iterator material_it = renderer->materials.begin(); material_it != renderer->materials.end(); ++material_it) {
        context.context.sso->glBindProgramPipeline(context.context.get_pipeline(material_it->second));
        //context.context.gl->glBindBufferRange(GL_UNIFORM_BUFFER, 1, material_it->params, 0, sizeof(node));
        context.context.gl->glBindBufferBase(GL_UNIFORM_BUFFER, 1, material_it->second.params);
        for (std::vector<DrawBatch>::iterator batch_it = material_it->second.batches.begin(); batch_it != material_it->second.batches.end(); ++batch_it) {
            context.context.gl->glBindVertexArray(context.context.get_vao(batch_it->format));
            QOpenGLBuffer* vbo = renderer->get_buffer(batch_it->format);
            context.context.vab->glBindVertexBuffer(0, vbo->bufferId(), 0, batch_it->format_stride);

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

void OpenGLRenderer::set_viewpoint_view(int, const float (&view)[4][4])
{
    ScopedContext context(this->context_pool, 0);

    GlobalParameters params[2];
    mult(view, active_viewpoint.left.view_offset, params[0].view);
    mult(params[0].view, active_viewpoint.left.projection, params[0].view_projection);
    mult(view, active_viewpoint.right.view_offset, params[1].view);
    mult(params[1].view, active_viewpoint.right.projection, params[1].view_projection);

    context.context.gl->glBindBuffer(GL_UNIFORM_BUFFER, this->global_uniforms);
    void* data = (char*)context.context.gl->glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(GlobalParameters) * 2, GL_MAP_WRITE_BIT);
    memcpy(data, params, sizeof(params));
    context.context.gl->glUnmapBuffer(GL_UNIFORM_BUFFER);

    active_viewpoint.right.uniform_offset = sizeof(params[1]);
}

Material& OpenGLRenderer::get_material(const std::string& name)
{
    std::map<std::string, Material>::iterator it = materials.find(name);
    if (it == materials.end()) {
        return materials[name];
    } else {
        return it->second;
    }
}

QOpenGLBuffer* OpenGLRenderer::get_buffer(const VertexFormat& format)
{
    VertexFormatBufferMap::iterator it = buffers.find(format);
    if (it == buffers.end()) {
        QOpenGLBuffer* vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
        vbo->create();
        vbo->bind();
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

    for (std::map<std::string, Material>::iterator material_it = materials.begin();
         material_it != materials.end(); ++material_it) {

        material_it->second.batches.clear();
    }
}
