#include "x3dopenglrenderer.h"

#include <math.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define CX3D_SUPPORT_OPENGL
#include <cybergarage/x3d/CyberX3D.h>
using namespace CyberX3D;

#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLVertexArrayObject>
#include <QtGui/QOpenGLBuffer>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions_3_2_Core>
#include <QtOpenGLExtensions/QOpenGLExtensions>
#include <QtGui/QOpenGLFramebufferObject>

class RenderingNodeListener : public Node::NodeListener
{
public:
    X3DOpenGLRenderer *renderer;

    RenderingNodeListener(X3DOpenGLRenderer* renderer) : renderer(renderer)
    {

    }

    virtual ~RenderingNodeListener()
    {

    }

    virtual void onUpdated(Node* node)
    {

    }

    virtual void onDeleted(Node* node)
    {
        if (node->isGeometry3DNode() || node->isLightNode()) {
            /*if (node->isInstanceNode()) {

            }
            Draw* batch_id = (Draw*)node->getValue();
            if (batch_id != nullptr) { renderer->remove_from_batch(*batch_id); delete batch_id; }
            node->setValue(nullptr);*/
        }
    }
};

struct X3DLightNodeInfo
{
    // RGB + intensity
    float color_intensity[4] = {1.0, 1.0, 1.0, 0.8*0.2};
    // Attenuation + ambient intensity
    float attenuation_ambient_intensity[4] = {0.0, 0.0, 0.0, 0.8*0.2};
    glm::vec4 position = {0.0, 0.0, 0.0, 1.0};
    glm::vec4 direction = {0.0, 0.0, 0.0, 1.0};
    int type;
};

struct X3DLightNode
{
    glm::mat4x4 transform;
    int type;
    X3DLightNodeInfo light;
};

struct X3DMaterialNode
{
    // RGBA
    glm::vec4 diffuse_color = {0.8, 0.8, 0.8, 1.0};
    // Emissive RGB + ambient_intensity
    float emissive_ambient_intensity[4] = {0.0, 0.0, 0.0, 0.8};
    // Specular RGB + shininess
    float specular_shininess[4] = {0.0, 0.0, 0.0, 0.2};
};

struct X3DTextureTransformNode
{
    float center_scale[4];
    float translation_rotation[4];
};

struct X3DTextureNode
{
    int offset_width_height[4] = {0, 0, 0, 0};
};

struct X3DAppearanceNode
{
    X3DMaterialNode material;
    X3DTextureTransformNode tex_transform;
    X3DTextureNode texture;
};

struct X3DShapeNode
{
    glm::mat4x4 transform;
};

static VertexFormat convert_to_internal(const GeometryRenderInfo::VertexFormat& format)
{
    VertexFormat new_format;
    for (size_t i = 0; i < format.getNumAttributes(); ++i) {
        const GeometryRenderInfo::Attribute* attrib = format.getAttribute(i);
        GLenum type = GL_INVALID_ENUM;
        const std::type_info& cpp_type = attrib->getType();
        if (cpp_type == typeid(int) || cpp_type == typeid(unsigned int)) {
            type = GL_UNSIGNED_INT;
        } else if (cpp_type == typeid(short) || cpp_type == typeid(unsigned short)) {
            type = GL_UNSIGNED_SHORT;
        } else if (cpp_type == typeid(char) || cpp_type == typeid(unsigned char)) {
            type = GL_UNSIGNED_BYTE;
        } else if (cpp_type == typeid(float)) {
            type = GL_FLOAT;
        } else if (cpp_type == typeid(double)) {
            type = GL_DOUBLE;
        }
        new_format.addAttribute(type, attrib->getComponents(), attrib->getNormalized(), attrib->getOffset());
    }
    return new_format;
}

X3DOpenGLRenderer::X3DOpenGLRenderer()
{
    passes.push_back(ShaderPass("Geometry", 0, -1, 1, true, true, false,
                                GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_LESS, ShaderPass::DISABLED,
                                GL_ZERO, GL_ZERO, GL_BACK, ShaderPass::DISABLED));

    passes.push_back(ShaderPass("Lighting", 1, 1, 0, true, false, false,
                                GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, ShaderPass::DISABLED, GL_FUNC_ADD,
                                GL_ONE, GL_ONE, GL_FRONT, ShaderPass::DISABLED));

    create_material("x3d-default", ":/shaders/default.vert", ":/shaders/default.frag", 0);
    create_material("x3d-default-light", ":/shaders/default-light.vert", ":/shaders/default-light.frag", 1);
    this->node_listener = new RenderingNodeListener(this);
    this->headlight = new DirectionalLightNode();
}

X3DOpenGLRenderer::~X3DOpenGLRenderer()
{
    delete this->headlight;
    delete this->node_listener;
}

void X3DOpenGLRenderer::set_projection(Scalar fov, Scalar aspect, Scalar near, Scalar far)
{
    active_viewpoint.left.projection = glm::perspective(fov, aspect, near, far);
    active_viewpoint.right.projection = glm::perspective(fov, aspect, near, far);
}

void X3DOpenGLRenderer::process_background_node(BackgroundNode *background)
{

}

void X3DOpenGLRenderer::process_light_node(LightNode *light_node)
{
    // TODO LIGHT NODE REMOVE FROM BATCH
    ScopedContext context(this->context_pool, 0);
    const auto gl = context.context.gl;

    if (light_node->getNodeListener() != nullptr) {
        light_node->getNodeListener()->onDeleted(light_node);
    } else {
        light_node->setNodeListener(this->node_listener);
    }

    if (!light_node->isOn()) {
		return;
    }

    X3DLightNode node;
    light_node->getColor(node.light.color_intensity);
    node.light.color_intensity[3] = light_node->getIntensity();
    node.light.attenuation_ambient_intensity[3] = light_node->getAmbientIntensity();

    Material& default_material = get_material("x3d-default-light");

    float location[3];

    if (light_node->isPointLightNode()) {
        PointLightNode *point_light = (PointLightNode *)light_node;
        node.light.type = 0;

        point_light->getAttenuation(node.light.attenuation_ambient_intensity);
        SphereNode sphere;
        sphere.setRadius(calc_light_radius(0, node.light.color_intensity[3],
                                           node.light.attenuation_ambient_intensity[0],
                                           node.light.attenuation_ambient_intensity[1],
                                           node.light.attenuation_ambient_intensity[2]));
        point_light->getLocation(location);

        node.light.position = glm::vec4(glm::make_vec3(&location[0]), 1.0);
        node.transform = glm::translate(node.transform, glm::vec3(node.light.position));
        ++default_material.total_objects;
        DrawInfoBuffer::DrawInfo info = {default_material.total_objects -1, 0, 0, 0};
        process_geometry_node(&sphere, default_material, info);
        light_node->setValue(sphere.getValue());
    } else if (light_node->isDirectionalLightNode()) {
        DirectionalLightNode *direction_light = (DirectionalLightNode *)light_node;
        node.light.type = 1;

        BoxNode box;

        direction_light->getDirection(location);
        node.light.direction = glm::vec4(glm::make_vec3(&location[0]), 1.0);
        ++default_material.total_objects;
        DrawInfoBuffer::DrawInfo info = {default_material.total_objects -1, 0, 0, 0};
        process_geometry_node(&box, default_material, info);
        light_node->setValue(box.getValue());
    } else if (light_node->isSpotLightNode()) {
        SpotLightNode *spot_light = (SpotLightNode *)light_node;
        node.light.type = 2;

        ConeNode cone;
        cone.setBottom(false);

        spot_light->getLocation(location);

        node.light.position = glm::vec4(glm::make_vec3(&location[0]), 1.0);
        //spot_light->getDirection(direction);
        float attenuation[3];
        spot_light->getAttenuation(attenuation);
        cone.setBottomRadius(calc_light_radius(spot_light->getCutOffAngle(), spot_light->getIntensity(),
                                               attenuation[0], attenuation[1], attenuation[2]));
        node.transform = glm::translate(node.transform, glm::vec3(node.light.position));
        ++default_material.total_objects;
        DrawInfoBuffer::DrawInfo info = {default_material.total_objects -1, 0, 0, 0};
        process_geometry_node(&cone, default_material, info);
        light_node->setValue(cone.getValue());
    }

    node.type = node.light.type;
    gl->glBindBuffer(GL_UNIFORM_BUFFER, default_material.vert_params);
    void* data = gl->glMapBufferRange(GL_UNIFORM_BUFFER, (default_material.total_objects - 1) * (sizeof(X3DLightNode) - sizeof(X3DLightNode::light)),
                                      sizeof(X3DLightNode) - sizeof(X3DLightNode::light), GL_MAP_WRITE_BIT);
    memcpy(data, &node, sizeof(X3DLightNode) - sizeof(X3DLightNode::light));
    gl->glUnmapBuffer(GL_UNIFORM_BUFFER);

    gl->glBindBuffer(GL_UNIFORM_BUFFER, default_material.frag_params);
    data = gl->glMapBufferRange(GL_UNIFORM_BUFFER, (default_material.total_objects - 1) * sizeof(X3DLightNode::light),
                                sizeof(X3DLightNode::light), GL_MAP_WRITE_BIT);
    memcpy(data, &node.light, sizeof(X3DLightNode::light));
    gl->glUnmapBuffer(GL_UNIFORM_BUFFER);
}

void X3DOpenGLRenderer::process_geometry_node(Geometry3DNode *geometry, Material& material, DrawInfoBuffer::DrawInfo& draw_info)
{
    if (geometry != nullptr) {
        if (geometry->isInstanceNode()) {
            if (geometry->getValue() != nullptr) {
                DrawInstance* instance = (DrawInstance*)geometry->getValue();
                instance->update(draw_info);
            } else {
                Node *reference = geometry->getReferenceNode();
                Draw *draw = (Draw*)reference->getValue();

                // TODO instance declared before reference?
                // process_geometry_node(reference, material);

                DrawInstance& instance = draw->add_instance(draw_info);
                geometry->setValue(&instance);
                geometry->setNodeListener(this->node_listener);
            }
        } else if (geometry->getValue() != nullptr) {
            Draw* draw = (Draw*)geometry->getValue();
            DrawInstance* instance = draw->get_base_instance();
            instance->update(draw_info);
        } else if (geometry->getNumVertexArrays() > 0) {
            if (geometry->getNumVertexArrays() > 1) {
                // TODO handle multiple arrays
                throw;
            }

            GeometryRenderInfo::VertexArray array;
            geometry->getVertexArray(array, 0);
            VertexFormat format = convert_to_internal(array.getFormat());

            VertexBuffer& vbo = get_buffer(format);
            size_t vbo_pos = vbo.allocate(array.getBufferSize());

            char* data = (char*)vbo.data + vbo_pos;
            geometry->getVertexData(0, data);

            IndexBuffer& ebo = get_index_buffer();
            size_t ebo_pos = 0;
            if (array.getNumElements() > 0) {
                ebo_pos = ebo.allocate(array.getNumElements() * sizeof(int));

                data = (char*)ebo.data + ebo_pos;
                geometry->getElementData(0, data);
            }

            DrawBatch& batch = material.get_batch(format, array.getFormat().getSize(),
                                                  GL_TRIANGLES, array.getNumElements() > 0 ? GL_UNSIGNED_INT : 0);
            Draw& draw = batch.add_draw(array.getNumVertices(), array.getNumElements(), vbo_pos, ebo_pos);
            draw.add_instance(draw_info); // base instance
            geometry->setValue((void*)&draw);
            if (geometry->getParentNode() != nullptr) {
                geometry->setNodeListener(this->node_listener);
            }

            // TODO this is not needed anymore.
            vbo.num_verts += array.getNumVertices();
            ebo.num_elements += array.getNumElements();
        }
    }
}

Material& X3DOpenGLRenderer::process_apperance_node(AppearanceNode *appearance, glm::mat4x4 transform)
{
    ScopedContext context(this->context_pool, 0);
    const auto gl = context.context.gl;

    if (appearance->isInstanceNode()) {
        // TODO instance appearance nodes
    }

    X3DAppearanceNode node;

    if (appearance != nullptr) {
        TextureTransformNode *transform = appearance->getTextureTransformNodes();
        if (transform != nullptr) {
            transform->getTranslation(node.tex_transform.translation_rotation);
            transform->getCenter(node.tex_transform.center_scale);
            node.tex_transform.translation_rotation[2] = transform->getRotation();
            transform->getScale(&node.tex_transform.center_scale[2]);
        }

        ImageTextureNode *texture = appearance->getImageTextureNodes();
        if (texture != nullptr && texture->getTextureName() != 0) {
            // TODO make resident, add to ssbo
            // TODO allow setable texture node width/height
            PixelBuffer& buffer = get_pixel_buffer();
            node.texture.offset_width_height[1] = texture->getWidth();
            node.texture.offset_width_height[2] = texture->getHeight();
            node.texture.offset_width_height[0] = (buffer.offset + buffer.current_pos) / 4;
            size_t bbp = texture->hasTransparencyColor() + 3;
            size_t num_bytes = texture->getWidth() * texture->getHeight() * bbp;

            if (buffer.current_pos + num_bytes >= buffer.max_bytes) {
                throw;
            }

            gl->glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer.buffer);
            gl->glBindTexture(GL_TEXTURE_2D, texture->getTextureName());
            gl->glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, (void*)buffer.offset + buffer.current_pos);

            buffer.current_pos += num_bytes;
            //texture->getRepeatS();
            //texture->getRepeatT();
            node.material.diffuse_color = glm::vec4(0.0, 0.0, 0.0, 1.0);
        }

        MaterialNode *material = appearance->getMaterialNodes();
        if (material != nullptr) {
            material->getDiffuseColor(&node.material.diffuse_color[0]);
            node.material.diffuse_color[3] = 1 - material->getTransparency();
            material->getSpecularColor(node.material.specular_shininess);
            node.material.specular_shininess[3] = material->getShininess();
            material->getEmissiveColor(node.material.emissive_ambient_intensity);
            node.material.emissive_ambient_intensity[3] = material->getAmbientIntensity();
        }
    }

    Material& default_material = get_material("x3d-default");

    if (default_material.total_objects >= 1024) {
        // TODO too many objects, convert to SSBO and instance any nodes
        throw;
    }

    gl->glBindBuffer(GL_UNIFORM_BUFFER, default_material.vert_params);
    void* data = gl->glMapBufferRange(GL_UNIFORM_BUFFER, default_material.total_objects * sizeof(X3DShapeNode::transform),
                                      sizeof(X3DShapeNode::transform), GL_MAP_WRITE_BIT);
    memcpy(data, &transform, sizeof(X3DShapeNode::transform));
    gl->glUnmapBuffer(GL_UNIFORM_BUFFER);

    gl->glBindBuffer(GL_UNIFORM_BUFFER, default_material.frag_params);
    data = gl->glMapBufferRange(GL_UNIFORM_BUFFER, default_material.total_objects * sizeof(node),
                                sizeof(node), GL_MAP_WRITE_BIT);
    memcpy(data, &node, sizeof(node));
    gl->glUnmapBuffer(GL_UNIFORM_BUFFER);

    ++default_material.total_objects;
    appearance->setNodeListener(this->node_listener);
    return default_material;
}

void X3DOpenGLRenderer::process_shape_node(ShapeNode *shape, bool selected)
{
    ScopedContext context(this->context_pool, 0);
    const auto gl = context.context.gl;

    X3DShapeNode node;
    shape->setNodeListener(this->node_listener);

    float matrix[4][4];
    shape->getTransformMatrix(matrix);
    node.transform = glm::make_mat4x4(&matrix[0][0]);

    Material& apperance = process_apperance_node(shape->getAppearanceNodes(), node.transform);
    DrawInfoBuffer::DrawInfo info = {apperance.total_objects -1, 0, 0, 0};
    process_geometry_node(shape->getGeometry3D(), apperance, info);
}

void X3DOpenGLRenderer::process_node(SceneGraph *sg, Node *root)
{
    if (root == nullptr || sg == nullptr) {
        return;
    }

    for (Node *node = root; node != nullptr; node = node->next()) {
        if (node->isLightNode()) {
            process_light_node((LightNode *)node);
        } else if (node->isShapeNode()) {
            process_shape_node((ShapeNode *)node, sg->getSelectedShapeNode() == node);
        } else {
            process_node(sg, node->getChildNodes());
        }
    }
}

void X3DOpenGLRenderer::render(SceneGraph *sg)
{
    ScopedContext context(context_pool, 0);

    ViewpointNode *view = sg->getViewpointNode();
    if (view == nullptr) {
        if ((view = sg->getDefaultViewpointNode()) == nullptr) {
            return;
        }
    }

    process_background_node(sg->getBackgroundNode());

    float matrix[4][4];
    view->getMatrix(matrix);
    glm::mat4x4 view_mat = glm::make_mat4x4(&matrix[0][0]);
    set_viewpoint_view(0, view_mat);

    NavigationInfoNode *nav_info = sg->getNavigationInfoNode();
    if (nav_info == nullptr) {
        nav_info = sg->getDefaultNavigationInfoNode();
    }

    if (nav_info != nullptr &&
        nav_info->getHeadlight()) {
        headlight->setAmbientIntensity(0.0);
        headlight->setIntensity(1.0);
        glm::vec4 direction = -glm::inverse(view_mat)[2];
        headlight->setDirection(direction.x, direction.y, direction.z);
        process_light_node(headlight);
	}

    process_node(sg, sg->getNodes());

    write_batches(get_draw_buffer(), get_draw_info_buffer());

    render_viewpoints();
}

bool X3DOpenGLRenderer::get_ray(Scalar x, Scalar y,
                          const Scalar (&model)[4][4],
                          Scalar (&from)[3], Scalar (&to)[3])
{
    glm::mat4x4 matrix = glm::make_mat4x4(&model[0][0]);

    glm::vec4 viewport(0, 0, active_viewpoint.left.back_buffer.width,
                       active_viewpoint.left.back_buffer.height);

    glm::vec3 glm_from = glm::unProject(glm::vec3(x * active_viewpoint.left.back_buffer.width,
                                                  y * active_viewpoint.left.back_buffer.height, 0.0),
                            matrix,
                            active_viewpoint.left.projection, viewport);
    glm::vec3 glm_to = glm::unProject(glm::vec3(x * active_viewpoint.left.back_buffer.width,
                                                y * active_viewpoint.left.back_buffer.height, 1.0),
                            matrix,
                            active_viewpoint.left.projection, viewport);
    from[0] = glm_from.x; from[1] = glm_from.y; from[2] = glm_from.z;
    to[0] = glm_to.x; to[1] = glm_to.y; to[2] = glm_to.z;
    return true;
}
