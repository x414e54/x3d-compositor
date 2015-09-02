#include "openglrenderer.h"

#include <thread>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
    frame_num = 0;
    render_type = 0;

    ScopedContext context(context_pool, 0);

    context.context.gl->glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uniform_alignment);

    context.context.gl->glGenBuffers(1, &this->global_uniforms);
    context.context.gl->glBindBuffer(GL_UNIFORM_BUFFER, this->global_uniforms);
    context.context.gl->glBufferData(GL_UNIFORM_BUFFER, 65536, nullptr, GL_DYNAMIC_DRAW);
}

OpenGLRenderer::~OpenGLRenderer()
{

}

void OpenGLRenderer::set_viewpoint_output(int, OpenGLOutput& output)
{
    active_viewpoint.output = &output;
    active_viewpoint.output->init_context(this->context_pool.share_context);
    active_viewpoint.left.enabled = output.is_enabled();
    active_viewpoint.right.enabled = output.is_enabled() && output.is_stereo();
    if (output.is_stereo()) {
        active_viewpoint.output->get_eye_matrix(active_viewpoint.left.view_offset,
                                                active_viewpoint.right.view_offset);
    }
}

void OpenGLRenderer::render_viewpoint(OpenGLRenderer* renderer, const RenderOuputGroup& output, int context_id)
{
    ScopedContext context(renderer->context_pool, context_id);

    unsigned int last_vao = 0;
    unsigned int vao = 0;

    context.context.gl->glBindBufferRange(GL_UNIFORM_BUFFER, 0, renderer->global_uniforms, output.uniform_offset, sizeof(GlobalParameters));
    context.context.gl->glBindBufferBase(GL_UNIFORM_BUFFER, 1, renderer->transform_buffer.buffer);

    context.context.gl->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderer->draw_calls.buffer);
    if (renderer->textures.texture != 0) {
        context.context.gl->glActiveTexture(GL_TEXTURE0);
        context.context.gl->glBindTexture(GL_TEXTURE_BUFFER, renderer->textures.texture);
    } else if (renderer->uniform_buffer.buffer != 0) {
        context.context.gl->glBindBufferBase(GL_UNIFORM_BUFFER, 4, renderer->uniform_buffer.buffer);
    }

    for (std::vector<ShaderPass>::iterator pass_it = renderer->passes.begin(); pass_it != renderer->passes.end(); ++pass_it) {
        context.context.setup_for_pass(*pass_it, output);

        for (std::map<std::string, Material>::iterator material_it = renderer->materials.begin(); material_it != renderer->materials.end(); ++material_it) {
            // TODO make this more efficient
            if (material_it->second.pass != pass_it->pass_id) {
                continue;
            }

            context.context.sso->glBindProgramPipeline(context.context.get_pipeline(material_it->second));
            //context.context.gl->glBindBufferRange(GL_UNIFORM_BUFFER, 1, material_it->params, 0, sizeof(node));
            context.context.gl->glBindBufferBase(GL_UNIFORM_BUFFER, 3, material_it->second.frag_params);

            for (std::vector<DrawBatch>::iterator batch_it = material_it->second.batches.begin(); batch_it != material_it->second.batches.end(); ++batch_it) {
                vao = context.context.get_vao(batch_it->format);
                if (last_vao != vao) {
                    last_vao = vao;
                    context.context.gl->glBindVertexArray(vao);
                    VertexBuffer& vbo = renderer->get_buffer(batch_it->format);
                    context.context.vab->glBindVertexBuffer(0, renderer->draw_info.buffer, renderer->draw_info.offset, sizeof(DrawInfoBuffer::DrawInfo));
                    context.context.vab->glBindVertexBuffer(1, vbo.buffer, vbo.offset, batch_it->format_stride);
                    context.context.vab->glVertexBindingDivisor(0, 1);
                    context.context.gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->indices.buffer);
                }

                if (batch_it->element_type != 0) {
                    context.context.indirect->glMultiDrawElementsIndirect(batch_it->primitive_type, batch_it->element_type, (const void*)batch_it->buffer_offset, batch_it->num_draws, batch_it->draw_stride);
                } else {
                    context.context.indirect->glMultiDrawArraysIndirect(batch_it->primitive_type, (const void*)batch_it->buffer_offset, batch_it->num_draws, batch_it->draw_stride);
                }
            }
        }
    }
}

void OpenGLRenderer::set_viewpoint_viewport(int, size_t width, size_t height)
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

void OpenGLRenderer::set_viewpoint_view(int, const glm::mat4x4 &view)
{
    ScopedContext context(this->context_pool, 0);

    // TODO remove duplication
    GlobalParameters left_params;
    left_params.view = view * active_viewpoint.left.view_offset;
    left_params.projection = active_viewpoint.left.projection;
    left_params.view_projection = active_viewpoint.left.projection * left_params.view;
    left_params.position = glm::inverse(left_params.view)[3];
    left_params.width = active_viewpoint.left.back_buffer.width;
    left_params.height = active_viewpoint.left.back_buffer.height;
    left_params.render_type = this->render_type;

    GlobalParameters right_params;
    right_params.view = view * active_viewpoint.right.view_offset;
    right_params.projection = active_viewpoint.right.projection;
    right_params.view_projection = active_viewpoint.right.projection * right_params.view;
    right_params.position = glm::inverse(right_params.view)[3];
    right_params.width = active_viewpoint.right.back_buffer.width;
    right_params.height = active_viewpoint.right.back_buffer.height;
    right_params.render_type = this->render_type;

    size_t aligned_size = align(sizeof(GlobalParameters), this->uniform_alignment);
    context.context.gl->glBindBuffer(GL_UNIFORM_BUFFER, this->global_uniforms);
    char* data = (char*)context.context.gl->glMapBufferRange(GL_UNIFORM_BUFFER, 0, aligned_size * 2, GL_MAP_WRITE_BIT);
    memcpy(data, &left_params, sizeof(GlobalParameters));
    memcpy(data + aligned_size, &right_params, sizeof(GlobalParameters));
    context.context.gl->glUnmapBuffer(GL_UNIFORM_BUFFER);

    active_viewpoint.right.uniform_offset = aligned_size;
}

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
            context.context.gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                                             width, height, 0, GL_RGBA, GL_FLOAT, NULL);
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

    // TODO render simultaneously - (mesa crashes)
    left_render.waitForFinished();

    QFuture<void> right_render;
    if (active_viewpoint.right.enabled) {
        right_render = QtConcurrent::run(render_viewpoint, this, active_viewpoint.right, 2);
    }

    right_render.waitForFinished();

    if (active_viewpoint.output != nullptr) {
        active_viewpoint.output->submit();
    }

    ++frame_num %= StreamedBuffer::NUM_FRAMES;
}
