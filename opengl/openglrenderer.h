#ifndef OPENGLRENDERER_H
#define OPENGLRENDERER_H

#include <map>
#include <thread>
#include <functional>
#include <vector>
#include <atomic>
#include <QThreadStorage>
#include <QOffscreenSurface>

#include "openglhelper.h"

class OpenGLOutput;
class QOpenGLBuffer;

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
    unsigned int global_uniforms;
    unsigned int draw_calls;
    Viewpoint active_viewpoint;
    ContextPool context_pool;
    float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
private:
    std::vector<int> render_targets;
    VertexFormatBufferMap buffers;
};

#endif // OPENGLRENDERER_H

