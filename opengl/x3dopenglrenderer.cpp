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

struct X3DLightNodeInfo
{
    int type;
    float intensity = 0.8*0.2;
    float color[4] = {0.8, 0.8, 0.8};
    float attenuation[3] = {0.0, 0.0, 0.0};
    float ambient_intensity = 0.8*0.2;
    glm::vec3 position = {0.0, 0.0, 0.0};
};

struct X3DLightNode
{
    glm::mat4x4 transform;
    X3DLightNodeInfo light;
};

struct X3DMaterialNode
{
    float ambient_intensity = 0.8*0.2;
    float diffuse_color[4] = {0.8, 0.8, 0.8};
    float emissive_color[3] = {0.0, 0.0, 0.0};
    float shininess = 0.2*128.0;
    float specular_color[3] = {0.0, 0.0, 0.0};
};

struct X3DTextureTransformNode
{
    float center[2];
    float scale[2];
    float translation[2];
    float rotation;
};

struct X3DShapeNode
{
    glm::mat4x4 transform;
    X3DMaterialNode material;
    X3DTextureTransformNode tex_transform;
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
    passes.push_back(ShaderPass(0, -1, 1, true, true));
    passes.push_back(ShaderPass(1, 1, 0, false, false));
    create_material("x3d-default", ":/shaders/default.vert", ":/shaders/default.frag", 0);
    create_material("x3d-default-light", ":/shaders/default-light.vert", ":/shaders/default-light.frag", 1);
}

X3DOpenGLRenderer::~X3DOpenGLRenderer()
{

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
    ScopedContext context(this->context_pool, 0);
    const auto gl = context.context.gl;

    if (!light_node->isOn()) {
		return;
    }

    X3DLightNode node;
    light_node->getColor(node.light.color);
    node.light.intensity = light_node->getIntensity();
    node.light.ambient_intensity = light_node->getAmbientIntensity();

    Material& default_material = get_material("x3d-default-light");

    float location[3];

    if (light_node->isPointLightNode()) {
        PointLightNode *point_light = (PointLightNode *)light_node;
        node.light.type = 0;

        point_light->getAttenuation(node.light.attenuation);
        SphereNode sphere;
        sphere.setRadius(calc_light_radius(0, node.light.intensity,
                                           node.light.attenuation[0],
                                           node.light.attenuation[1],
                                           node.light.attenuation[2]));
        point_light->getLocation(location);

        node.light.position = glm::make_vec3(&location[0]);
        node.transform = glm::translate(node.transform, node.light.position);
        ++default_material.total_objects;
        process_geometry_node(&sphere, default_material);
    } else if (light_node->isDirectionalLightNode()) {
        DirectionalLightNode *direction_light = (DirectionalLightNode *)light_node;
        node.light.type = 1;

        BoxNode box;
        ++default_material.total_objects;
        process_geometry_node(&box, default_material);
    } else if (light_node->isSpotLightNode()) {
        SpotLightNode *spot_light = (SpotLightNode *)light_node;
        node.light.type = 2;

        ConeNode cone;
        cone.setBottom(false);

        spot_light->getLocation(location);

        node.light.position = glm::make_vec3(&location[0]);
        //spot_light->getDirection(direction);
        float attenuation[3];
        spot_light->getAttenuation(attenuation);
        cone.setBottomRadius(calc_light_radius(spot_light->getCutOffAngle(), spot_light->getIntensity(),
                                               attenuation[0], attenuation[1], attenuation[2]));
        node.transform = glm::translate(node.transform, node.light.position);
        ++default_material.total_objects;
        process_geometry_node(&cone, default_material);
    }

    gl->glBindBuffer(GL_UNIFORM_BUFFER, default_material.vert_params);
    void* data = gl->glMapBufferRange(GL_UNIFORM_BUFFER, (default_material.total_objects - 1) * sizeof(X3DLightNode::transform),
                                      sizeof(X3DLightNode::transform), GL_MAP_WRITE_BIT);
    memcpy(data, &node.transform, sizeof(X3DLightNode::transform));
    gl->glUnmapBuffer(GL_UNIFORM_BUFFER);

    gl->glBindBuffer(GL_UNIFORM_BUFFER, default_material.frag_params);
    data = gl->glMapBufferRange(GL_UNIFORM_BUFFER, (default_material.total_objects - 1) * sizeof(X3DLightNode::light),
                                sizeof(X3DLightNode::light), GL_MAP_WRITE_BIT);
    memcpy(data, &node.light, sizeof(X3DLightNode::light));
    gl->glUnmapBuffer(GL_UNIFORM_BUFFER);
}

void X3DOpenGLRenderer::process_geometry_node(Geometry3DNode *geometry, Material& material)
{
    ScopedContext context(this->context_pool, 0);
    const auto gl = context.context.gl;

    if (geometry != nullptr) {
        if (geometry->isInstanceNode())
        {
            // TODO implement instancing
            throw;
        } else if (geometry->getNumVertexArrays() > 0) {
            if (geometry->getNumVertexArrays() > 1) {
                // TODO handle multiple arrays
                throw;
            }

            GeometryRenderInfo::VertexArray array;
            geometry->getVertexArray(array, 0);
            VertexFormat format = convert_to_internal(array.getFormat());

            VertexBuffer& vbo = get_buffer(format);
            if (vbo.current_pos + array.getBufferSize() >= vbo.max_bytes) {
                // TODO buffer full
                throw;
            }

            gl->glBindBuffer(GL_ARRAY_BUFFER, vbo.buffer);
            void* data = gl->glMapBufferRange(GL_ARRAY_BUFFER, vbo.offset + vbo.current_pos, array.getBufferSize(), GL_MAP_WRITE_BIT);
            geometry->getVertexData(0, data);
            gl->glUnmapBuffer(GL_ARRAY_BUFFER);

            add_to_batch(material, format, array.getFormat().getSize(), array.getNumVertices(), 0, vbo.num_verts, 0);
            vbo.current_pos += array.getBufferSize();
            vbo.num_verts += array.getNumVertices();
        }
    }
}

void X3DOpenGLRenderer::process_shape_node(ShapeNode *shape, bool selected)
{
    ScopedContext context(this->context_pool, 0);
    const auto gl = context.context.gl;

    X3DShapeNode node;

    AppearanceNode *appearance = shape->getAppearanceNodes();
    if (appearance != nullptr) {
        TextureTransformNode *transform = appearance->getTextureTransformNodes();
        if (transform != nullptr) {
            transform->getTranslation(node.tex_transform.translation);
            transform->getCenter(node.tex_transform.center);
            node.tex_transform.rotation = transform->getRotation();
            transform->getScale(node.tex_transform.scale);
        }

        ImageTextureNode *texture = appearance->getImageTextureNodes();
        if (texture != nullptr && texture->getTextureName() != 0) {
                // make resident, add to ssbo
        }

        MaterialNode *material = appearance->getMaterialNodes();
        if (material != nullptr) {
            material->getDiffuseColor(node.material.diffuse_color);
            node.material.diffuse_color[3] = 1 - material->getTransparency();
            material->getSpecularColor(node.material.specular_color);
            material->getEmissiveColor(node.material.emissive_color);
            node.material.shininess = material->getShininess();
            node.material.ambient_intensity = material->getAmbientIntensity();
        }
    }

    Material& default_material = get_material("x3d-default");

    if (default_material.total_objects >= 1024) {
        // TODO too many objects, covnert to SSBO and instance any nodes
        throw;
    }

    float matrix[4][4];
    shape->getTransformMatrix(matrix);
    node.transform = glm::make_mat4x4(&matrix[0][0]);

    gl->glBindBuffer(GL_UNIFORM_BUFFER, default_material.vert_params);
    void* data = gl->glMapBufferRange(GL_UNIFORM_BUFFER, default_material.total_objects * sizeof(X3DShapeNode::transform),
                                      sizeof(X3DShapeNode::transform), GL_MAP_WRITE_BIT);
    memcpy(data, &node.transform, sizeof(X3DShapeNode::transform));
    gl->glUnmapBuffer(GL_UNIFORM_BUFFER);

    gl->glBindBuffer(GL_UNIFORM_BUFFER, default_material.frag_params);
    data = gl->glMapBufferRange(GL_UNIFORM_BUFFER, default_material.total_objects * sizeof(X3DShapeNode::material),
                                sizeof(X3DShapeNode::material), GL_MAP_WRITE_BIT);
    memcpy(data, &node.material, sizeof(X3DShapeNode::material));
    gl->glUnmapBuffer(GL_UNIFORM_BUFFER);

    ++default_material.total_objects;
    process_geometry_node(shape->getGeometry3D(), default_material);
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
    set_viewpoint_view(0, glm::make_mat4x4(&matrix[0][0]));

    NavigationInfoNode *nav_info = sg->getNavigationInfoNode();
    if (nav_info == nullptr) {
        nav_info = sg->getDefaultNavigationInfoNode();
    }

    if (nav_info != nullptr &&
        nav_info->getHeadlight()) {
        PointLightNode headlight;
        float location[3];
		view->getPosition(location);
        location[2] -= 2;
        headlight.setLocation(location);
        headlight.setAmbientIntensity(0.3f);
        headlight.setIntensity(0.7f);
        process_light_node(&headlight);
	}

    process_node(sg, sg->getNodes());
    render_viewpoints();
}

bool X3DOpenGLRenderer::get_ray(Scalar x, Scalar y,
                          const Scalar (&model)[4][4],
                          Scalar (&from)[3], Scalar (&to)[3])
{
    glm::mat4x4 matrix = glm::make_mat4x4(&model[0][0]);

    glm::vec4 viewport(0, 0, active_viewpoint.left.back_buffer.width,
                       active_viewpoint.left.back_buffer.height);

    glm::vec3 glm_from = glm::unProject(glm::vec3(x, y, 0.0),
                            matrix,
                            active_viewpoint.left.projection, viewport);
    glm::vec3 glm_to = glm::unProject(glm::vec3(x, y, 1.0),
                            matrix,
                            active_viewpoint.left.projection, viewport);
    from[0] = glm_from.x; from[1] = glm_from.y; from[2] = glm_from.z;
    to[0] = glm_to.x; to[1] = glm_to.y; to[2] = glm_to.z;
    return true;
}
