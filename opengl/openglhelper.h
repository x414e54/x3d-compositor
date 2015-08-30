#ifndef OPENGLHELPER_H
#define OPENGLHELPER_H

#include <map>
#include <thread>
#include <functional>
#include <vector>
#include <atomic>

#include <glm/glm.hpp>

#include <QThreadStorage>
#include <QOffscreenSurface>

class OpenGLOutput;
class QOpenGLBuffer;
class QOpenGLVertexArrayObject;
class QOpenGLContext;
class QOpenGLFramebufferObject;
class QOpenGLExtension_ARB_multi_draw_indirect;
class QOpenGLExtension_ARB_vertex_attrib_binding;
class QOpenGLExtension_ARB_separate_shader_objects;
class QOpenGLExtension_ARB_debug_output;
class QOpenGLExtension_ARB_texture_buffer_object;
typedef void (*QOpenGLExtension_ARB_buffer_storage) (int target, ptrdiff_t size, const void *data, int flags);
class QOpenGLFunctions_3_2_Core;

typedef float Scalar;

class Attribute
{
public:
    size_t type;
    size_t components;
    bool normalized;
    size_t offset;

    Attribute() : type(0), components(0), normalized(false), offset(0) {}

    Attribute(size_t type, size_t components, bool normalized, size_t offset)
        : type(type), components(components), normalized(normalized), offset(offset) {}

    bool operator<(const Attribute& b) const {
        return this->type < b.type ||
               this->normalized < b.normalized ||
               this->components < b.components;
    }

    bool operator==(const Attribute& b) const {
        return this->type == b.type &&
               this->normalized == b.normalized &&
               this->components == b.components;
    }
};

static const int MAX_ATTRIBUTES = 16;
class VertexFormat
{
public:
    size_t num_attribs;
    Attribute attribs[MAX_ATTRIBUTES];
    VertexFormat() : num_attribs(0) {}

    void addAttribute(size_t type, size_t components, bool normalized, size_t offset)
    {
        if (num_attribs < MAX_ATTRIBUTES - 1) {
            attribs[num_attribs++] = Attribute(type, components, normalized, offset);
        }
    }

    bool operator<(const VertexFormat& b) const {

        if (this->num_attribs > b.num_attribs) {
            return false;
        } else if (this->num_attribs > b.num_attribs) {
            return true;
        }

        for (size_t i = 0; i < num_attribs; ++i) {
            const Attribute& attrib_a = this->attribs[i];
            const Attribute& attrib_b = b.attribs[i];
            if (attrib_a < attrib_b) {
                return true;
            } else if (!(attrib_a == attrib_b)) {
                return false;
            }
        }

        return false;
    }

    bool operator==(const VertexFormat& b) const {

        if (this->num_attribs != b.num_attribs) {
            return false;
        }

        for (size_t i = 0; i < num_attribs; ++i) {
            const Attribute& attrib_a = this->attribs[i];
            const Attribute& attrib_b = b.attribs[i];
            if (!(attrib_a == attrib_b)) {
                return false;
            }
        }

        return true;
    }
};

class StreamedBuffer
{
protected:

    struct Allocation
    {
        size_t start;
        size_t end;
    };

    typedef std::map<size_t, std::list<Allocation>> FreeList;
    typedef std::list<Allocation> SafeList;

    FreeList free_list;
    SafeList safe_list;
public:
    static const size_t NUM_FRAMES = 3;

    StreamedBuffer() : buffer(0), offset(0),
        max_bytes(0), current_pos(0), frame_num(0), data(nullptr) {}
    unsigned int buffer;
    size_t offset; // meant of offset into buffer e.g. per vertex format type if sharing one buffer
    size_t max_bytes;
    size_t current_pos; // current position in bytes from offset
    size_t frame_num;
    char *data; // This should take into account the buffer offset

    // TODO make this more efficient?
    // TODO thread safety, etc.
    virtual size_t allocate(size_t size)
    {
        size_t pos = 0;
        // scoped cas lock
        for (auto zone = safe_list.begin(); zone != safe_list.end(); ++zone) {
            if ((zone->end - zone->start) >= size) {
                pos = zone->start;

                size_t overused = (zone->end - zone->start) - size;
                safe_list.erase(zone);
                if (overused > 0) {
                    free(pos + size, overused, true);
                }

                break;
            }
        }

        if (pos == 0) {
            if (this->current_pos + size >= this->max_bytes) {
                // TODO too many draws
                throw;
            }

            pos = this->current_pos;
            this->current_pos += size;
        }
        return pos;
    }

    // TODO enforce append only - only new data can change
    virtual size_t reallocate(size_t pos, size_t old_size, size_t new_size, bool append)
    {
        size_t new_pos = 0;

        {
            // scoped cas lock
            if (pos + old_size == this->current_pos && append) {
                new_pos = pos;
                this->current_pos += (new_size - old_size);
            } else {
                new_pos = allocate(new_size);
            }
        }

        if (new_pos != 0 && new_pos != pos) {
            memcpy(this->data + new_pos, this->data + pos, old_size);
            free(pos, old_size);
        }

        return new_pos;
    }

    virtual void free(size_t start, size_t size, bool safe = false)
    {
        // scoped cas lock
        Allocation alloc;
        alloc.end = start + size;
        alloc.start = start;

        if (safe) {
            safe_list.push_back(alloc);
        } else {
            auto& list = free_list[this->frame_num];
            list.push_back(alloc);
        }
    }

    virtual void advance()
    {
        // TODO this needs GPU sync
        auto& list = free_list[this->frame_num];
        for (auto zone = list.begin(); zone != list.end(); ++zone) {
            Allocation alloc = *zone;
            for (auto safe_zone = safe_list.begin(); safe_zone != safe_list.end();) {
                if (safe_zone->start == alloc.end) {
                    alloc.end = safe_zone->end;
                    safe_zone = safe_list.erase(safe_zone);
                } else if (safe_zone->end == alloc.start) {
                    alloc.start = safe_zone->start;
                    safe_zone = safe_list.erase(safe_zone);
                } else {
                    ++safe_zone;
                }
            }

            safe_list.push_back(alloc);
        }
        list.clear();
    }
};

class DrawBuffer : public StreamedBuffer
{
public:
    DrawBuffer() {}
};

class IndexBuffer : public StreamedBuffer
{
public:
    IndexBuffer() {}
};

class VertexBuffer : public StreamedBuffer
{
public:
    VertexBuffer() {}
};

class ShaderBuffer : public StreamedBuffer
{
public:
    ShaderBuffer() {}
};

class PixelBuffer : public StreamedBuffer
{
public:
    PixelBuffer() : texture(0) {}
    unsigned int texture;
};

class DrawInfoBuffer : public StreamedBuffer
{
public:
    typedef glm::ivec4 DrawInfo;
    DrawInfoBuffer() {}
};

class DrawBatch;

class Material
{
public:
    std::string name;
    std::vector<DrawBatch> batches;
    unsigned int pass;
    unsigned int frag;
    unsigned int vert;
    unsigned int frag_params;
    size_t total_objects;
    size_t id;
    bool operator<(const Material& b) const {
        return this->name.compare(b.name) < 0;
    }

    DrawBatch& get_batch(const VertexFormat& format, size_t format_stride, size_t primitive_type, size_t element_type);
};

class DrawInstance
{
public:
    DrawInstance(const DrawInfoBuffer::DrawInfo& draw_info) : draw_info(draw_info), updated(true) {}
    void update(const DrawInfoBuffer::DrawInfo& draw_info);

    bool operator==(const DrawInstance& b) const {
        return this->draw_info == b.draw_info;
    }
//private:
    bool updated;
    DrawInfoBuffer::DrawInfo draw_info;
};

class Draw
{
public:
    size_t verts;
    size_t elements;
    size_t vert_offset;
    size_t element_offset;

    Draw(size_t verts, size_t elements, size_t vert_offset, size_t element_offset)
        : verts(verts), elements(elements), vert_offset(vert_offset), element_offset(element_offset)
        , num_instances(0), buffer_offset(0), updated(true)
    {
    }

    DrawInstance& add_instance(const DrawInfoBuffer::DrawInfo& draw_info);
    void remove_instance(const DrawInstance& draw_id);

    bool operator==(const Draw& b) const {
        return this->verts == b.verts && this->elements == b.elements
                && this->vert_offset == b.vert_offset
                && this->element_offset == b.element_offset;
    }

    DrawInstance* get_base_instance()
    {
        return (instances.size() > 0) ? &instances.front() : nullptr;
    }

//private
    bool updated;
    size_t num_instances;
    size_t buffer_offset;
    std::list<DrawInstance> instances;
};

typedef struct {
    uint  count;
    uint  instanceCount;
    uint  first;
    uint  baseInstance;
} DrawArraysIndirectCommand;

typedef  struct {
    uint  count;
    uint  primCount;
    uint  firstIndex;
    uint  baseVertex;
    uint  baseInstance;
} DrawElementsIndirectCommand;

class DrawBatch
{
public:
    DrawBatch(Material& material, const VertexFormat& format, int format_stride, int element_type, int primitive_type)
        : material(material), format(format), format_stride(format_stride), element_type(element_type), buffer_offset(0),
          num_draws(0), primitive_type(primitive_type), updated(true)
    {
        if (element_type != 0) {
            draw_stride = sizeof(DrawElementsIndirectCommand);
        } else {
            draw_stride = sizeof(DrawArraysIndirectCommand);
        }
    }

    Material& material;
    VertexFormat format;
    int format_stride;
    int element_type;
    int buffer_offset;
    int num_draws;
    int draw_stride;
    int primitive_type;

    Draw& add_draw(size_t verts, size_t elements, size_t vert_offset, size_t element_offset);

    void remove_draw(const Draw& batch_id);
//private
    bool updated;
    std::list<Draw> draws;
};

class ShaderPass
{
public:
    static const int DISABLED = -1;
    class StencilOp
    {

    };

    ShaderPass(const char* name, size_t pass_id, ssize_t in, ssize_t out, bool color_mask, bool depth_mask,
               bool stencil_mask, int clear, int depth_func, int blend_equation,
               int blend_dst, int blend_src, int cull_face, int stencil_func)
        : name(name), pass_id(pass_id), in(in), out(out), color_mask(color_mask), depth_mask(depth_mask),
          stencil_mask(stencil_mask), clear(clear), depth_func(depth_func), blend_equation(blend_equation),
          blend_dst(blend_dst), blend_src(blend_src), cull_face(cull_face), stencil_func(stencil_func) {}

    const char* name;

    size_t pass_id;
    ssize_t in;
    ssize_t out;

    bool color_mask;
    bool depth_mask;
    bool stencil_mask;

    int clear;
    int depth_func;
    int blend_equation;
    int blend_dst;
    int blend_src;
    int cull_face;
    int stencil_func;
};

struct GlobalParameters
{
    glm::mat4x4 view;
    glm::mat4x4 projection;
    glm::mat4x4 view_projection;
    glm::vec4 position;
    int width;
    int height;
};

class RenderTarget
{
public:
    static const size_t MAX_ATTACHMENTS = 4;

    RenderTarget(size_t num_attachments, bool depth)
        : width(0), height(0), num_attachments(num_attachments),
          use_depth(depth), initialized(false)
    {
    }

    unsigned int attachments[MAX_ATTACHMENTS];
    unsigned int depth;
    size_t width;
    size_t height;
    size_t num_attachments;
    bool use_depth;
    bool initialized;

    bool operator<(const RenderTarget& b) const {
        if (this->num_attachments > b.num_attachments) {
            return false;
        } else if (this->num_attachments < b.num_attachments) {
            return true;
        }

        for (size_t i = 0; i < num_attachments; ++i) {
            if (this->attachments[i] < b.attachments[i]) {
                return true;
            } else if (this->attachments[i] > b.attachments[i]) {
                return false;
            }
        }

        return false;
    }
};

class RenderOuputGroup
{
public:
    RenderOuputGroup() :  enabled(false), uniform_offset(0),
        g_buffer(4, true), back_buffer(1, true)
    {
    }

    inline const RenderTarget& get_render_target(size_t id) const
    {
        if (id == 0) {
            return back_buffer;
        }
        return g_buffer;
    }

    glm::mat4x4 projection;
    glm::mat4x4 view_offset;

    bool enabled;
    size_t uniform_offset;
    RenderTarget g_buffer;
    RenderTarget back_buffer;
};

class Viewpoint
{
public:
    Viewpoint() : output(nullptr) {}
    RenderOuputGroup left;
    RenderOuputGroup right;
    OpenGLOutput* output;
};

typedef std::map<VertexFormat, VertexBuffer> VertexFormatBufferMap;
typedef std::map<VertexFormat, int> VertexFormatVaoMap;
typedef std::map<RenderTarget, int> RenderTargetFboMap;
typedef std::map<Material, int> MaterialPipelineMap;

class ContextPoolContext
{
public:
    ContextPoolContext(const ContextPoolContext&) = delete;
    ContextPoolContext& operator=(const ContextPoolContext&) = delete;
    ContextPoolContext(ContextPoolContext&& old) noexcept
    {
        // TODO - This is a mess
        if (!old.assign(std::this_thread::get_id()) || count > 0) {
            throw;
        }
        fbos = std::move(old.fbos);
        pipelines = std::move(old.pipelines);
        vaos = std::move(old.vaos);
        surface = old.surface;
        context = old.context;
        gl = old.gl;
        indirect = old.indirect;
        reserved = old.reserved;
        vab = old.vab;
        sso = old.sso;
        tex = old.tex;
        buffer = old.buffer;
        debug = old.debug;
        old.reserved = false;
        old.surface = nullptr;
        old.context = nullptr;
        old.gl = nullptr;
        old.indirect = nullptr;
        old.sso = nullptr;
        old.tex = nullptr;
        old.buffer = nullptr;
        old.debug = nullptr;
        old.vab = nullptr;
        old.used.clear();
    }

    ContextPoolContext(QOpenGLContext* share_context, bool reserved = false);
    ~ContextPoolContext();

    std::atomic_flag used = ATOMIC_FLAG_INIT;
    bool reserved;
    int count = 0;
    std::thread::id thread;
    QOffscreenSurface* surface;
    QOpenGLContext* context;
    QOpenGLContext* prev_context;
    QOpenGLExtension_ARB_multi_draw_indirect* indirect;
    QOpenGLExtension_ARB_vertex_attrib_binding* vab;
    QOpenGLExtension_ARB_separate_shader_objects* sso;
    QOpenGLExtension_ARB_buffer_storage buffer;
    QOpenGLExtension_ARB_texture_buffer_object* tex;
    QOpenGLExtension_ARB_debug_output* debug;
    QOpenGLFunctions_3_2_Core* gl;
    RenderTargetFboMap fbos;
    VertexFormatVaoMap vaos;
    MaterialPipelineMap pipelines;

    int get_vao(const VertexFormat& format);
    int get_fbo(const RenderTarget& render_target);
    int get_pipeline(const Material& material);
    void setup_for_pass(const ShaderPass& pass, const RenderOuputGroup& output);

    bool make_current();
    void done_current();
    bool assign(std::thread::id thread);
    void release();
    void increment();
    bool decrement();
};

class ContextPool
{
public:
    ContextPool();
    ~ContextPool();
    ContextPoolContext& assign_context(int named_id);
    void return_context(ContextPoolContext&);
    QOpenGLContext* share_context;
private:
    std::vector<ContextPoolContext> context_pool;
    static thread_local ContextPoolContext* thread_pool_context;
};

class ScopedContext
{
public:
    ScopedContext(ContextPool& pool, int named_id = -1)
        : pool(pool), context(pool.assign_context(named_id))
    {
        context.make_current();
    }

    ~ScopedContext()
    {
        context.done_current();
        pool.return_context(context);
    }

    ContextPool& pool;
    ContextPoolContext& context;
};

inline float calc_light_radius(float cutoff, float intensity, float const_att, float linear_att, float quad_att)
{
    Q_UNUSED(cutoff);
    Q_UNUSED(intensity);
    Q_UNUSED(const_att);
    Q_UNUSED(linear_att);
    Q_UNUSED(quad_att);
    return 1.0;
}

inline size_t align(size_t val, size_t alignment)
{
    return (val + (alignment - 1)) - ((val + (alignment - 1)) % alignment);
}

#endif // OPENGLRENDERER_H

