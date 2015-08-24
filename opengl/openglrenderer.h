#ifndef OPENGLRENDERER_H
#define OPENGLRENDERER_H

#include <map>
#include <vector>

#include "openglhelper.h"

class OpenGLOutput;

class OpenGLRenderer
{
public:
    OpenGLRenderer();
    virtual ~OpenGLRenderer();
    void set_viewpoint_output(int id, OpenGLOutput& output);
    void set_viewpoint_viewport(int id, size_t width, size_t height);
    void set_viewpoint_view(int id, const glm::mat4x4 &view);
    void render_viewpoints();
protected:
    VertexBuffer& get_buffer(const VertexFormat& format);
    DrawBuffer& get_draw_buffer();
    PixelBuffer& get_pixel_buffer();
    Material& get_material(const std::string& name);
    void create_material(const std::string& name,
                         const std::string& vert,
                         const std::string& frag, int pass);
    void set_render_target_size(RenderTarget& rt, size_t width, size_t height);
    void add_to_batch(Material& material, const VertexFormat& format, size_t stride, size_t verts, size_t elements, size_t vert_offset, size_t element_offset);
    static void render_viewpoint(OpenGLRenderer* renderer, const RenderOuputGroup& output, int context_id);

    std::map<std::string, Material> materials;
    unsigned int global_uniforms;
    size_t num_draw_calls;
    Viewpoint active_viewpoint;
    ContextPool context_pool;
    std::vector<ShaderPass> passes;
private:
    DrawBuffer draw_calls;
    VertexBuffer draw_info;
    PixelBuffer textures;
    size_t frame_num;
    int uniform_alignment;
    VertexFormatBufferMap buffers;
};

#endif // OPENGLRENDERER_H

