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
    frame_num = 0;

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
    context.context.gl->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderer->draw_calls.buffer);
    if (renderer->textures.texture != 0) {
        context.context.gl->glActiveTexture(GL_TEXTURE0);
        context.context.gl->glBindTexture(GL_TEXTURE_BUFFER, renderer->textures.texture);
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
            context.context.gl->glBindBufferBase(GL_UNIFORM_BUFFER, 1, material_it->second.vert_params);
            context.context.gl->glBindBufferBase(GL_UNIFORM_BUFFER, 2, material_it->second.frag_params);

            for (std::vector<DrawBatch>::iterator batch_it = material_it->second.batches.begin(); batch_it != material_it->second.batches.end(); ++batch_it) {
                vao = context.context.get_vao(batch_it->format);
                if (last_vao != vao) {
                    last_vao = vao;
                    context.context.gl->glBindVertexArray(vao);
                    VertexBuffer& vbo = renderer->get_buffer(batch_it->format);
                    context.context.vab->glBindVertexBuffer(0, renderer->draw_info.buffer, renderer->draw_info.offset, sizeof(int) * 4);
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

// TODO UNIFY BUFFER CREATION
PixelBuffer& OpenGLRenderer::get_pixel_buffer()
{
    // TODO convert to bindless textures
    PixelBuffer& buffer = this->textures;
    if (buffer.buffer == 0) {
        ScopedContext context(context_pool);

        GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        context.context.gl->glGenBuffers(1, &buffer.buffer);
        context.context.gl->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer.buffer);
        buffer.max_bytes = 100 * 1024 * 1024;
        context.context.buffer(GL_PIXEL_UNPACK_BUFFER, buffer.max_bytes * StreamedBuffer::NUM_FRAMES, nullptr, flags | GL_DYNAMIC_STORAGE_BIT);
        buffer.current_pos = 0;
        buffer.offset = buffer.max_bytes * frame_num;
        buffer.data = (char*)context.context.gl->glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, buffer.max_bytes * StreamedBuffer::NUM_FRAMES, flags);
        context.context.gl->glGenTextures(1, &buffer.texture);
        context.context.gl->glBindTexture(GL_TEXTURE_BUFFER, buffer.texture);
        context.context.tex->glTexBufferARB(GL_TEXTURE_BUFFER, GL_RGBA8, buffer.buffer);
    } else {
        if (buffer.offset != buffer.max_bytes * frame_num) {
            buffer.current_pos = 0;
            buffer.offset = buffer.max_bytes * frame_num;
        }
    }
    return buffer;
}

DrawInfoBuffer& OpenGLRenderer::get_draw_info_buffer()
{
    DrawInfoBuffer& buffer = this->draw_info;
    if (buffer.buffer == 0) {
        ScopedContext context(context_pool);

        GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        context.context.gl->glGenBuffers(1, &buffer.buffer);
        context.context.gl->glBindBuffer(GL_ARRAY_BUFFER, buffer.buffer);
        buffer.max_bytes = 65536;
        context.context.buffer(GL_ARRAY_BUFFER, buffer.max_bytes, nullptr, flags | GL_DYNAMIC_STORAGE_BIT);
        buffer.current_pos = 0;
        buffer.offset = 0;
        buffer.frame_num = 0;
        buffer.data = (char*)context.context.gl->glMapBufferRange(GL_ARRAY_BUFFER, 0, buffer.max_bytes, flags);
    } else if (buffer.frame_num != frame_num) {
        buffer.frame_num = frame_num;
        buffer.advance();
    }
    return buffer;
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
        context.context.buffer(GL_DRAW_INDIRECT_BUFFER, buffer.max_bytes, nullptr, flags | GL_DYNAMIC_STORAGE_BIT);
        buffer.current_pos = 0;
        buffer.offset = 0;
        buffer.frame_num = 0;
        buffer.data = (char*)context.context.gl->glMapBufferRange(GL_DRAW_INDIRECT_BUFFER, 0, buffer.max_bytes, flags);
    } else if (buffer.frame_num != frame_num) {
        buffer.frame_num = frame_num;
        buffer.advance();
    }
    return buffer;
}

IndexBuffer& OpenGLRenderer::get_index_buffer()
{
    IndexBuffer& buffer = this->indices;
    if (buffer.buffer == 0) {
        ScopedContext context(context_pool);

        GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        context.context.gl->glGenBuffers(1, &buffer.buffer);
        context.context.gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.buffer);
        buffer.max_bytes = 1024 * 1024 * 20;
        context.context.buffer(GL_ELEMENT_ARRAY_BUFFER, buffer.max_bytes * StreamedBuffer::NUM_FRAMES, nullptr, flags | GL_DYNAMIC_STORAGE_BIT);
        buffer.current_pos = 0;
        buffer.num_elements = 0;
        buffer.offset = buffer.max_bytes * frame_num;
        buffer.data = (char*)context.context.gl->glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, buffer.max_bytes * StreamedBuffer::NUM_FRAMES, flags);
    } else {
        if (buffer.offset != buffer.max_bytes * frame_num) {
            buffer.current_pos = 0;
            buffer.num_elements = 0;
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
        buffer.max_bytes = 1024 * 1024 * 20;
        context.context.buffer(GL_ARRAY_BUFFER, buffer.max_bytes * StreamedBuffer::NUM_FRAMES, nullptr, flags | GL_DYNAMIC_STORAGE_BIT);
        buffer.current_pos = 0;
        buffer.num_verts = 0;
        buffer.offset = buffer.max_bytes * frame_num;
        buffer.data = (char*)context.context.gl->glMapBufferRange(GL_ARRAY_BUFFER, 0, buffer.max_bytes * StreamedBuffer::NUM_FRAMES, flags);
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
// --- TODO UNIFY BUFFER CREATION

void OpenGLRenderer::add_instance_to_batch(const DrawBatch::Draw& batch_id, const DrawInfoBuffer::DrawInfo& draw_info)
{
    DrawBuffer& draws = get_draw_buffer();
    DrawInfoBuffer& infos = get_draw_info_buffer();
    DrawBatch& batch = batch_id.batch;

    const size_t size = batch.draw_stride;
    size_t pos = batch_id.find_offset();

    char* data = (char*)draws.data + batch.buffer_offset + (pos * size);

    unsigned int *instances = nullptr;
    unsigned int *draw_index = nullptr;

    if (batch.element_type != 0) {
        DrawElementsIndirectCommand *cmd = (DrawElementsIndirectCommand*)data;
        instances = &cmd->primCount;
        draw_index = &cmd->baseInstance;
    } else {
        DrawArraysIndirectCommand *cmd = (DrawArraysIndirectCommand*)data;
        instances = &cmd->instanceCount;
        draw_index = &cmd->baseInstance;
    }

    *draw_index = infos.append(*draw_index, draw_info, (*instances)++);
}

void OpenGLRenderer::remove_from_batch(const DrawBatch::Draw& batch_id)
{
    DrawBuffer& draws = get_draw_buffer();
    DrawInfoBuffer& infos = get_draw_info_buffer();
    DrawBatch& batch = batch_id.batch;
    size_t batch_pos = batch_id.find_offset();

    const size_t size = batch.draw_stride;

    char* data = (char*)draws.data + batch.buffer_offset + (batch_pos * size);

    unsigned int *instances = nullptr;
    unsigned int *draw_index = nullptr;

    if (batch.element_type != 0) {
        DrawElementsIndirectCommand *cmd = (DrawElementsIndirectCommand*)data;
        instances = &cmd->primCount;
        draw_index = &cmd->baseInstance;
    } else {
        DrawArraysIndirectCommand *cmd = (DrawArraysIndirectCommand*)data;
        instances = &cmd->instanceCount;
        draw_index = &cmd->baseInstance;
    }

    // TODO assert instances = 0
    infos.free_index(*draw_index, *instances);

    --batch.num_draws;
    if (batch.num_draws > 0) {
        int old_pos = batch.buffer_offset;
        batch.buffer_offset = draws.allocate(batch.num_draws * size);

        if (batch_pos > 0) {
            memcpy(draws.data + batch.buffer_offset, draws.data + old_pos, batch_pos * size);
        }

        if (batch_pos < batch.num_draws) {
            memcpy(draws.data + batch.buffer_offset + (batch_pos * size), draws.data + old_pos + ((batch_pos + 1) * size),
                   (batch.num_draws - batch_pos) * size);
        }
        draws.free(old_pos, (batch.num_draws + 1) * size);
    } else {
        draws.free(batch.buffer_offset, 1 * size);
        // TODO correct batch here
        batch.material.batches.clear();
    }

    if (batch.first == &batch_id) {
        batch.first = batch_id.next;
    } else {
        batch_id.prev->next = batch_id.next;
    }

    if (batch.last == &batch_id) {
        batch.last = batch_id.prev;
    } else {
        batch_id.next->prev = batch_id.prev;
    }
}

DrawBatch::Draw* OpenGLRenderer::add_to_batch(Material& material, const VertexFormat& format, size_t stride, size_t verts, size_t elements, size_t vert_offset, size_t element_offset, const DrawInfoBuffer::DrawInfo& draw_info)
{
    DrawBuffer& draws = get_draw_buffer();
    DrawInfoBuffer& infos = get_draw_info_buffer();

    const size_t size = (elements > 0) ? sizeof(DrawElementsIndirectCommand)
                                       : sizeof(DrawArraysIndirectCommand);

    size_t draw_info_pos = infos.add(draw_info);

    // Use same struct here but contents differ
    DrawElementsIndirectCommand cmd;
    if (elements > 0) {
        cmd = {elements, 1, element_offset, vert_offset, draw_info_pos};
    } else {
        cmd = {verts, 1, vert_offset, draw_info_pos};
    }

    for (std::vector<DrawBatch>::iterator batch = material.batches.begin(); batch != material.batches.end(); ++batch) {
        if (batch->primitive_type == GL_TRIANGLES && ((batch->element_type == 0 && elements == 0)
                || (batch->element_type == GL_UNSIGNED_INT && elements > 0)) && batch->format == format) {
            batch->buffer_offset = draws.reallocate(batch->buffer_offset,
                                                    batch->num_draws * size, (batch->num_draws + 1) * size, true);
            memcpy(draws.data + batch->buffer_offset + (batch->num_draws * size), &cmd, size);
            ++batch->num_draws;
            return new DrawBatch::Draw(*batch);
        }
    }

    size_t draw_pos = draws.allocate(size);
    DrawBatch batch(material, format, stride, elements > 0 ? GL_UNSIGNED_INT : 0, draw_pos, 1, size, GL_TRIANGLES);
    memcpy(draws.data + draw_pos, &cmd, size);
    material.batches.push_back(batch);
    return new DrawBatch::Draw(*(material.batches.end() - 1));
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

    // TODO do not clear here
    for (std::map<std::string, Material>::iterator material_it = materials.begin();
         material_it != materials.end(); ++material_it) {
        material_it->second.total_objects = 0;
    }

    ++frame_num %= StreamedBuffer::NUM_FRAMES;
}
