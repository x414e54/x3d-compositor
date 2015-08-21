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
#include <QFile>

#include "opengloutput.h"

OpenGLRenderer::OpenGLRenderer()
{
    draw_calls_pos = 0;

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

    for (std::vector<ShaderPass>::iterator pass_it = renderer->passes.begin(); pass_it != renderer->passes.end(); ++pass_it) {
        const RenderTarget& target = output.get_render_target(pass_it->out);

        context.context.gl->glBindFramebuffer(GL_FRAMEBUFFER,
            context.context.get_fbo(target));
        GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                            GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
        context.context.gl->glDrawBuffers(target.num_attachments, buffers);

        context.context.gl->glViewport(0, 0, target.width, target.height);

        context.context.gl->glEnable(GL_DEPTH_TEST);
        context.context.gl->glCullFace(GL_BACK);
        context.context.gl->glEnable(GL_CULL_FACE);
        context.context.gl->glClearColor(renderer->clear_color[0], renderer->clear_color[1],
                                         renderer->clear_color[2], renderer->clear_color[3]);
        context.context.gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //context.context.gl->glBindBufferRange(GL_UNIFORM_BUFFER, 0, this->global_uniforms, 0, output.uniform_offset);
        context.context.gl->glBindBufferBase(GL_UNIFORM_BUFFER, 0, renderer->global_uniforms);
        context.context.gl->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderer->draw_calls);

        for (std::map<std::string, Material>::iterator material_it = renderer->materials.begin(); material_it != renderer->materials.end(); ++material_it) {
            // TODO make this more efficient
            if (material_it->second.pass != pass_it->pass_id) {
                continue;
            }

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

// Temporary
    context.context.gl->glBindFramebuffer(GL_READ_FRAMEBUFFER,
        context.context.get_fbo(output.g_buffer));
    context.context.gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
        context.context.get_fbo(output.back_buffer));
    context.context.gl->glReadBuffer(GL_COLOR_ATTACHMENT0);
    context.context.gl->glDrawBuffer(GL_COLOR_ATTACHMENT0);
    context.context.gl->glBlitFramebuffer(0, 0, output.g_buffer.width, output.g_buffer.height,
                                          0, 0, output.back_buffer.width, output.back_buffer.height,
                                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
// Temporary
}

void OpenGLRenderer::set_viewpoint_viewport(int id, size_t width, size_t height)
{
    ScopedContext context(this->context_pool, 0);

    set_render_target_size(active_viewpoint.left.g_buffer, width, height);
    set_render_target_size(active_viewpoint.left.back_buffer, width, height);
    set_render_target_size(active_viewpoint.right.g_buffer, width, height);
    set_render_target_size(active_viewpoint.right.back_buffer, width, height);

    if (active_viewpoint.output != nullptr) {
        active_viewpoint.output->set_textures(active_viewpoint.left.back_buffer.attachments[0],
                                              active_viewpoint.right.back_buffer.attachments[0],
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

// Move to helpers
Material& OpenGLRenderer::get_material(const std::string& name)
{
    std::map<std::string, Material>::iterator it = materials.find(name);
    if (it == materials.end()) {
        return materials[name];
    } else {
        return it->second;
    }
}

void OpenGLRenderer::create_material(const std::string& name,
                                     const std::string& vert_filename,
                                     const std::string& frag_filename,
                                     int pass)
{
    ScopedContext context(context_pool, 0);
    Material& material = get_material(name);
    if (material.frag != 0 || material.vert != 0) {
        return;
    }

    material.pass = pass;

    QFile vert(vert_filename.c_str());
    QFile frag(frag_filename.c_str());

    if (!vert.open(QIODevice::ReadOnly) ||
        !frag.open(QIODevice::ReadOnly)) {
        throw;
    }

    QByteArray vert_data = vert.readAll();
    QByteArray frag_data = frag.readAll();

    if (vert_data.size() == 0 || frag_data.size() == 0) {
        throw;
    }

    const char* vert_list[1] = {vert_data.constData()};
    const char* frag_list[1] = {frag_data.constData()};
    // TODO cache shader program by filename these
    material.vert = context.context.sso->glCreateShaderProgramv(GL_VERTEX_SHADER, 1, vert_list);
    material.frag = context.context.sso->glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, frag_list);
    context.context.gl->glUniformBlockBinding(material.vert, 0, 0);
    context.context.gl->glUniformBlockBinding(material.vert, 1, 1);
    context.context.gl->glUniformBlockBinding(material.frag, 0, 1);

    // TODO convert to one ssbo?
    context.context.gl->glGenBuffers(1, &material.params);
    context.context.gl->glBindBuffer(GL_UNIFORM_BUFFER, material.params);
    context.context.gl->glBufferData(GL_UNIFORM_BUFFER, 512, nullptr, GL_DYNAMIC_DRAW);
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

void OpenGLRenderer::add_to_batch(Material& material, const VertexFormat& format, size_t stride, size_t verts, size_t elements)
{
    ScopedContext context(this->context_pool, 0);
    const auto gl = context.context.gl;

    if (elements > 0) {
        // TODO elements draw
        throw;
    } else {
        if (draw_calls_pos + sizeof(DrawArraysIndirectCommand) > 65535) {
            // TODO too many draws
            throw;
        }

        gl->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, this->draw_calls);
        void* data = gl->glMapBufferRange(GL_DRAW_INDIRECT_BUFFER, draw_calls_pos, sizeof(DrawArraysIndirectCommand), GL_MAP_WRITE_BIT);
        DrawArraysIndirectCommand cmd = {verts, 1, 0, 0};
        memcpy(data, &cmd, sizeof(cmd));
        gl->glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);
        DrawBatch batch(format, stride, draw_calls_pos, 1, 0, GL_TRIANGLES);
        draw_calls_pos += sizeof(DrawArraysIndirectCommand);
        material.batches.push_back(batch);
    }
}
//

void OpenGLRenderer::set_render_target_size(RenderTarget& rt, size_t width, size_t height)
{
    ScopedContext context(this->context_pool, 0);

    if (rt.width != width ||
        rt.height != height)
    {
        if (!rt.initialized) {
            context.context.gl->glGenTextures(rt.num_attachments, rt.attachments);
            if (rt.use_depth) {
                context.context.gl->glGenTextures(1, &rt.depth);
            }
            rt.initialized = true;
        }

        rt.width = width;
        rt.height = height;
        for (size_t i = 0; i < rt.num_attachments; ++i) {
            context.context.gl->glBindTexture(GL_TEXTURE_2D, rt.attachments[i]);
            context.context.gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            context.context.gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            context.context.gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            context.context.gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            context.context.gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                                             width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
        }
        if (rt.use_depth) {
            context.context.gl->glBindTexture(GL_TEXTURE_2D, rt.depth);
            context.context.gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            context.context.gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            context.context.gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            context.context.gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            context.context.gl->glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
            //context.context.gl->glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
            context.context.gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
            context.context.gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8,
                         width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
        }
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
    draw_calls_pos = 0;
}
