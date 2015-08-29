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

static void debug_callback(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar *msg, const void *)
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
    tex = new QOpenGLExtension_ARB_texture_buffer_object();
    if (!tex->initializeOpenGLFunctions()) {
        throw;
    }
    buffer = (QOpenGLExtension_ARB_buffer_storage)context->getProcAddress("glBufferStorage");
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

void ContextPoolContext::setup_for_pass(const ShaderPass &pass, const RenderOuputGroup& output)
{
    // TODO Capture state object and bind if exists?
    const RenderTarget& target = output.get_render_target(pass.out);
    if (pass.in >= 0) {
        const RenderTarget& in_target = output.get_render_target(pass.in);
        for (size_t i = 1; i <= in_target.num_attachments; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, in_target.attachments[i - 1]);
        }
    }

    gl->glBindFramebuffer(GL_FRAMEBUFFER, get_fbo(target));
    GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                        GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
    gl->glDrawBuffers(target.num_attachments, buffers);
    gl->glViewport(0, 0, target.width, target.height);

    if (pass.color_mask) {
        gl->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    } else {
        gl->glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    }

    if (pass.depth_mask) {
        gl->glDepthMask(GL_TRUE);
    } else {
        gl->glDepthMask(GL_FALSE);
    }

    if (pass.stencil_mask) {
        gl->glStencilMask(GL_TRUE);
    } else {
        gl->glStencilMask(GL_FALSE);
    }

    if (pass.depth_func == ShaderPass::DISABLED) {
        gl->glDisable(GL_DEPTH_TEST);
    } else {
        gl->glEnable(GL_DEPTH_TEST);
        gl->glDepthFunc(pass.depth_func);
    }

    if (pass.cull_face == ShaderPass::DISABLED) {
        gl->glDisable(GL_CULL_FACE);
    } else {
        gl->glEnable(GL_CULL_FACE);
        gl->glCullFace(pass.cull_face);
    }

    if (pass.blend_equation == ShaderPass::DISABLED) {
        gl->glDisable(GL_BLEND);
    } else {
        gl->glEnable(GL_BLEND);
        gl->glBlendEquation(pass.blend_equation);
        gl->glBlendFunc(pass.blend_src, pass.blend_dst);
    }

    if (pass.stencil_func == ShaderPass::DISABLED) {
        gl->glDisable(GL_STENCIL_TEST);
    } else {
        gl->glEnable(GL_STENCIL_TEST);
        gl->glStencilFunc(pass.stencil_func, 0, 0);
        //gl->glStencilOpSeparate(GL_BACK, , , );
        //gl->glStencilOpSeparate(GL_FRONT, , , );
    }

    if (pass.clear != ShaderPass::DISABLED) {
        gl->glClear(pass.clear);
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
