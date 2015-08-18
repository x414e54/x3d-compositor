#ifndef OPENGLRENDERER_H
#define OPENGLRENDERER_H

#include <map>
#include <thread>
#include <functional>
#include <vector>
#include <atomic>
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
class QOpenGLFunctions_3_0;

// Split this into smaller headers

class Attribute
{
public:
    size_t type;
    size_t attrib_size;
    bool normalized;

    Attribute() : type(0), attrib_size(0), normalized(false) {}

    Attribute(size_t type, size_t attrib_size, bool normalized)
        : type(type), attrib_size(attrib_size), normalized(normalized) {}

    bool operator<(const Attribute& b) const {
        return this->type < b.type &&
               this->normalized < b.normalized &&
               this->attrib_size < b.attrib_size;
    }

    bool operator==(const Attribute& b) const {
        return this->type == b.type &&
               this->normalized == b.normalized &&
               this->attrib_size == b.attrib_size;
    }
};

static const int MAX_ATTRIBUTES = 16;
class VertexFormat
{
public:
    size_t num_attribs;
    Attribute attribs[MAX_ATTRIBUTES];
    VertexFormat() : num_attribs(0) {}

    void addAttribute(size_t type, size_t attrib_size, bool normalized)
    {
        if (num_attribs < MAX_ATTRIBUTES - 1) {
            attribs[num_attribs++] = Attribute(type, attrib_size, normalized);
        }
    }

    bool operator<(const VertexFormat& b) const {

        if (this->num_attribs >= b.num_attribs) {
            return false;
        }

        for (size_t i = 0; i < num_attribs; ++i) {
            const Attribute& attrib_a = this->attribs[i];
            const Attribute& attrib_b = b.attribs[i];
            if (!(attrib_a < attrib_b)) {
                return false;
            }
        }

        return true;
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

class DrawBatch
{
public:
    VertexFormat format;
    int buffer_offset;
    int num_draws;
    int draw_stride;
    int primitive_type;
};

class Material
{
public:
    std::string name;
    std::vector<DrawBatch> batches;
    QOpenGLBuffer* parameters;
    bool operator<(const Material& b) const {
        return this->name.compare(b.name);
    }
};

class RenderOuputGroup
{
public:
    RenderOuputGroup() :  enabled(false), render_target(nullptr), viewport_width(0), viewport_height(0) {}
    double projection[4][4];
    double view[4][4];
    bool enabled;
    QOpenGLFramebufferObject* render_target;
    size_t viewport_width;
    size_t viewport_height;
};

class Viewpoint
{
public:
    Viewpoint() : output(nullptr) {}
    RenderOuputGroup left;
    RenderOuputGroup right;
    OpenGLOutput* output;
};

typedef std::map<VertexFormat, QOpenGLBuffer*> VertexFormatBufferMap;
typedef std::map<VertexFormat, int> VertexFormatVaoMap;
typedef std::map<int, int> RenderTargetFboMap;
typedef std::map<Material, int> MaterialPipelineMap;

class ContextPoolContext
{
public:
    ContextPoolContext(const ContextPoolContext&) = delete;
    ContextPoolContext& operator=(const ContextPoolContext&) = delete;
    ContextPoolContext(ContextPoolContext&& old) noexcept
    {
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
        debug = old.debug;
        old.reserved = false;
        old.surface = nullptr;
        old.context = nullptr;
        old.gl = nullptr;
        old.indirect = nullptr;
        old.sso = nullptr;
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
    QOpenGLExtension_ARB_debug_output* debug;
    QOpenGLFunctions_3_0* gl;
    RenderTargetFboMap fbos;
    VertexFormatVaoMap vaos;
    MaterialPipelineMap pipelines;

    int get_vao(const VertexFormat& format);
    int get_fbo(int id);
    int get_pipeline(const Material& material);

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

class OpenGLRenderer
{
public:
    OpenGLRenderer();
    virtual ~OpenGLRenderer();
    void set_viewpoint_output(int id, OpenGLOutput& output);
    void set_viewpoint_viewport(int id, size_t width, size_t height);
    void render_viewpoints();
protected:
    QOpenGLBuffer* get_buffer(const VertexFormat& format);
    Material& get_material(const std::string& name);

    static void render_viewpoint(OpenGLRenderer* renderer, const RenderOuputGroup& output, int context_id);

    std::map<std::string, Material> materials;
    QOpenGLBuffer* global_uniforms;
    QOpenGLBuffer* draw_calls;
    Viewpoint active_viewpoint;
    ContextPool context_pool;
private:
    std::vector<int> render_targets;
    VertexFormatBufferMap buffers;
};

#endif // OPENGLRENDERER_H

