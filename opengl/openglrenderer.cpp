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
#include <QFile>

#include "opengloutput.h"

OpenGLRenderer::OpenGLRenderer()
{
    num_draw_calls = 0;
    frame_num = 0;

    ScopedContext context(context_pool, 0);

    context.context.gl->glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uniform_alignment);

    context.context.gl->glGenBuffers(1, &this->global_uniforms);
    context.context.gl->glBindBuffer(GL_UNIFORM_BUFFER, this->global_uniforms);
    context.context.gl->glBufferData(GL_UNIFORM_BUFFER, 65536, nullptr, GL_DYNAMIC_DRAW);

    // TODO put this all in VertexBuffer class.
    context.context.gl->glGenBuffers(1, &this->draw_info.buffer);
    context.context.gl->glBindBuffer(GL_ARRAY_BUFFER, this->draw_info.buffer);
    context.context.gl->glBufferData(GL_ARRAY_BUFFER, 65536, nullptr, GL_DYNAMIC_DRAW);
    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    context.context.buffer(GL_ARRAY_BUFFER, 65536, nullptr, flags | GL_DYNAMIC_STORAGE_BIT);
    draw_info.current_pos = 0;
    draw_info.num_verts = 0;
    draw_info.data = context.context.gl->glMapBufferRange(GL_ARRAY_BUFFER, 0, 65536, flags);
}

OpenGLRenderer::~OpenGLRenderer()
{

}

void OpenGLRenderer::set_viewpoint_output(int id, OpenGLOutput& output)
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

    for (std::vector<ShaderPass>::iterator pass_it = renderer->passes.begin(); pass_it != renderer->passes.end(); ++pass_it) {
        const RenderTarget& target = output.get_render_target(pass_it->out);
        if (pass_it->in >= 0) {
            const RenderTarget& in_target = output.get_render_target(pass_it->in);
            for (size_t i = 0; i< in_target.num_attachments; ++i) {
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, in_target.attachments[i]);
            }
        }

        context.context.gl->glBindFramebuffer(GL_FRAMEBUFFER,
            context.context.get_fbo(target));
        GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                            GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
        context.context.gl->glDrawBuffers(target.num_attachments, buffers);

        context.context.gl->glViewport(0, 0, target.width, target.height);

        if (pass_it->depth_test) {
            context.context.gl->glEnable(GL_DEPTH_TEST);
        } else {
            context.context.gl->glDisable(GL_DEPTH_TEST);
        }

        if (pass_it->depth_write) {
            context.context.gl->glDepthMask(GL_TRUE);
        } else {
            context.context.gl->glDepthMask(GL_FALSE);
        }

        context.context.gl->glCullFace(GL_BACK);
        context.context.gl->glEnable(GL_CULL_FACE);
        context.context.gl->glClearColor(renderer->clear_color[0], renderer->clear_color[1],
                                         renderer->clear_color[2], renderer->clear_color[3]);
        context.context.gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        context.context.gl->glBindBufferRange(GL_UNIFORM_BUFFER, 0, renderer->global_uniforms, output.uniform_offset, sizeof(GlobalParameters));
        context.context.gl->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderer->draw_calls.buffer);

        for (std::map<std::string, Material>::iterator material_it = renderer->materials.begin(); material_it != renderer->materials.end(); ++material_it) {
            // TODO make this more efficient
            if (material_it->second.pass != pass_it->pass_id) {
                continue;
            }

            context.context.sso->glBindProgramPipeline(context.context.get_pipeline(material_it->second));
            //context.context.gl->glBindBufferRange(GL_UNIFORM_BUFFER, 1, material_it->params, 0, sizeof(node));
            context.context.gl->glBindBufferBase(GL_UNIFORM_BUFFER, 1, material_it->second.vert_params);
            context.context.gl->glBindBufferBase(GL_UNIFORM_BUFFER, 2, material_it->second.frag_params);
            for (std::vector<DrawBatch>::iterator batch_it = material_it->second.batches.begin(); batch_it != material_it->second.batches.end(); ++batch_it) {
                context.context.gl->glBindVertexArray(context.context.get_vao(batch_it->format));
                VertexBuffer& vbo = renderer->get_buffer(batch_it->format);
                context.context.vab->glBindVertexBuffer(0, renderer->draw_info.buffer, renderer->draw_info.offset, sizeof(int) * 4);
                context.context.vab->glBindVertexBuffer(1, vbo.buffer, vbo.offset, batch_it->format_stride);
                context.context.vab->glVertexBindingDivisor(0, 1);

                context.context.indirect->glMultiDrawArraysIndirect(batch_it->primitive_type, (const void*)batch_it->buffer_offset, batch_it->num_draws, batch_it->draw_stride);
            }
        }
    }
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

    GlobalParameters right_params;
    right_params.view = view * active_viewpoint.right.view_offset;
    right_params.projection = active_viewpoint.right.projection;
    right_params.view_projection = active_viewpoint.right.projection * right_params.view;
    right_params.position = glm::inverse(right_params.view)[3];
    right_params.width = active_viewpoint.right.back_buffer.width;
    right_params.height = active_viewpoint.right.back_buffer.height;

    size_t aligned_size = align(sizeof(GlobalParameters), this->uniform_alignment);
    context.context.gl->glBindBuffer(GL_UNIFORM_BUFFER, this->global_uniforms);
    char* data = (char*)context.context.gl->glMapBufferRange(GL_UNIFORM_BUFFER, 0, aligned_size * 2, GL_MAP_WRITE_BIT);
    memcpy(data, &left_params, sizeof(GlobalParameters));
    memcpy(data + aligned_size, &right_params, sizeof(GlobalParameters));
    context.context.gl->glUnmapBuffer(GL_UNIFORM_BUFFER);

    active_viewpoint.right.uniform_offset = aligned_size;
}

// Move to helpers
Material& OpenGLRenderer::get_material(const std::string& name)
{
    std::map<std::string, Material>::iterator it = materials.find(name);
    if (it == materials.end()) {
        Material& mat = materials[name];
        mat.name = name;
        return mat;
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
    context.context.gl->glUniformBlockBinding(material.frag, 0, 0);
    context.context.gl->glUniformBlockBinding(material.frag, 1, 2);

    // TODO convert to one ssbo?
    context.context.gl->glGenBuffers(1, &material.vert_params);
    context.context.gl->glBindBuffer(GL_UNIFORM_BUFFER, material.vert_params);
    context.context.gl->glBufferData(GL_UNIFORM_BUFFER, 65536, nullptr, GL_DYNAMIC_DRAW);

    context.context.gl->glGenBuffers(1, &material.frag_params);
    context.context.gl->glBindBuffer(GL_UNIFORM_BUFFER, material.frag_params);
    context.context.gl->glBufferData(GL_UNIFORM_BUFFER, 65536, nullptr, GL_DYNAMIC_DRAW);

    material.total_objects = 0;
}

DrawBuffer& OpenGLRenderer::get_draw_buffer()
{
    DrawBuffer& buffer = this->draw_calls;
    if (buffer.buffer == 0) {
        ScopedContext context(context_pool);

        GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        context.context.gl->glGenBuffers(1, &buffer.buffer);
        context.context.gl->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer.buffer);
        buffer.max_bytes = 65536;
        context.context.buffer(GL_DRAW_INDIRECT_BUFFER, buffer.max_bytes * 3, nullptr, flags | GL_DYNAMIC_STORAGE_BIT);
        buffer.current_pos = 0;
        buffer.num_draws = 0;
        buffer.offset = buffer.max_bytes * frame_num;
        buffer.data = context.context.gl->glMapBufferRange(GL_DRAW_INDIRECT_BUFFER, 0, buffer.max_bytes * 3, flags);
    } else {
        if (buffer.offset != buffer.max_bytes * frame_num) {
            buffer.current_pos = 0;
            buffer.num_draws = 0;
            buffer.offset = buffer.max_bytes * frame_num;
        }
    }
    return buffer;
}

VertexBuffer& OpenGLRenderer::get_buffer(const VertexFormat& format)
{
    VertexFormatBufferMap::iterator it = buffers.find(format);
    if (it == buffers.end()) {
        ScopedContext context(context_pool);
        VertexBuffer buffer;

        GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        context.context.gl->glGenBuffers(1, &buffer.buffer);
        context.context.gl->glBindBuffer(GL_ARRAY_BUFFER, buffer.buffer);
        buffer.max_bytes = 1024 * 1024;
        context.context.buffer(GL_ARRAY_BUFFER, buffer.max_bytes * 3, nullptr, flags | GL_DYNAMIC_STORAGE_BIT);
        buffer.current_pos = 0;
        buffer.num_verts = 0;
        buffer.offset = buffer.max_bytes * frame_num;
        buffer.data = context.context.gl->glMapBufferRange(GL_ARRAY_BUFFER, 0, buffer.max_bytes * 3, flags);
        buffers[format] = buffer;
        return buffers[format];
    } else {
        VertexBuffer& buffer = it->second;
        if (buffer.offset != buffer.max_bytes * frame_num) {
            buffer.current_pos = 0;
            buffer.num_verts = 0;
            buffer.offset = buffer.max_bytes * frame_num;
        }
        return it->second;
    }
}

void OpenGLRenderer::add_to_batch(Material& material, const VertexFormat& format, size_t stride, size_t verts, size_t elements, size_t vert_offset, size_t element_offset)
{
    ScopedContext context(this->context_pool, 0);

    if (elements > 0) {
        // TODO elements draw
        throw;
    } else {
        if ((num_draw_calls + 1) * sizeof(DrawArraysIndirectCommand) > 65535) {
            // TODO too many draws
            throw;
        }

        // TODO actually batch draws
        int draw_ids[4] = {material.total_objects - 1, 0, 0, 0};

        //VertexBuffer& info = get_draw_info_buffer();
        char* data = (char*)this->draw_info.data + num_draw_calls * sizeof(draw_ids); // info.offset
        memcpy(data, &draw_ids, sizeof(draw_ids));

        DrawBuffer& draws = get_draw_buffer();
        data = (char*)draws.data + num_draw_calls * sizeof(DrawArraysIndirectCommand) + draws.offset;

        DrawArraysIndirectCommand cmd = {verts, 1, vert_offset, num_draw_calls};
        memcpy(data, &cmd, sizeof(cmd));

        DrawBatch batch(format, stride, num_draw_calls * sizeof(DrawArraysIndirectCommand) + draws.offset, 1, 0, GL_TRIANGLES);
        ++num_draw_calls;
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

    for (std::map<std::string, Material>::iterator material_it = materials.begin();
         material_it != materials.end(); ++material_it) {
        material_it->second.total_objects = 0;
        material_it->second.batches.clear();
    }

    num_draw_calls = 0;

    ++frame_num %= 3;
}
