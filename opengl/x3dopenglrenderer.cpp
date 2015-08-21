#include "x3dopenglrenderer.h"

#include <math.h>

#include "tmp_glu_internal.h"

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

struct X3DLightNode
{
    int type;
    float transform[4][4];
    float intensity = 0.8*0.2;
    float color[4] = {0.8, 0.8, 0.8};
    float diffuse_color[4] = {0.8, 0.8, 0.8};
    float ambient_color[3] = {0.0, 0.0, 0.0};
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
    float transform[4][4];
    X3DMaterialNode material;
    X3DTextureTransformNode tex_transform;
};

X3DOpenGLRenderer::X3DOpenGLRenderer()
{
    passes.push_back(ShaderPass(0, -1, 1));
    passes.push_back(ShaderPass(1, 1, 0));
    create_material("x3d-default", ":/shaders/default.vert", ":/shaders/default.frag", 0);
    create_material("x3d-default-light", ":/shaders/default-light.vert", ":/shaders/default-light.frag", 1);
}

X3DOpenGLRenderer::~X3DOpenGLRenderer()
{

}

void X3DOpenGLRenderer::set_projection(Scalar fovy, Scalar aspect, Scalar zNear, Scalar zFar)
{
    create_projection(active_viewpoint.left.projection, fovy, aspect, zNear, zFar);
    create_projection(active_viewpoint.right.projection, fovy, aspect, zNear, zFar);
}

void X3DOpenGLRenderer::process_light_node(LightNode *light_node)
{
    ScopedContext context(this->context_pool, 0);
    const auto gl = context.context.gl;

    if (!light_node->isOn()) {
		return;
    }

    X3DLightNode node;
    light_node->getAmbientColor(node.ambient_color);
    light_node->getDiffuseColor(node.diffuse_color);
    light_node->getColor(node.color);

    Material& default_material = get_material("x3d-default-light");

    if (light_node->isPointLightNode()) {
        PointLightNode *point_light = (PointLightNode *)light_node;
        node.type = 0;

        float attenuation [3];
        point_light->getAttenuation(attenuation);

        SphereNode sphere;
        sphere.setRadius(calc_light_radius(point_light->getIntensity(),
                                           attenuation[0], attenuation[1], attenuation[2]));
        //point_light->getLocation(pos);

        reset(node.transform);
    } /*else if (light_node->isDirectionalLightNode()) {
        DirectionalLightNode *direction_light = (DirectionalLightNode *)light_node;
        node.type = 1;

        BoxNode box;
        box
        dLight->getDirection(pos); pos[3] = 0.0f;
        calc_light(1.0, 0.0, 0.0);
    } else if (light_node->isSpotLightNode()) {
        SpotLightNode *spot_light = (SpotLightNode *)light_node;
        node.type = 2;

        ConeNode cone;
        box.setSize();
        spot_light->getLocation(pos); pos[3] = 1.0f;
        spot_light->getDirection(direction);
        spot_light->getAttenuation(attenuation);
        sphere.setRadius(calc_light_radius(spot_light->getCutOffAngle(), spot_light->getIntensity(),
                                           attenuation[0], attenuation[1], attenuation[2]));
    }*/

    gl->glBindBuffer(GL_UNIFORM_BUFFER, default_material.params);
    void* data = gl->glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(X3DLightNode), GL_MAP_WRITE_BIT);
    memcpy(data, &node, sizeof(X3DLightNode));
    gl->glUnmapBuffer(GL_UNIFORM_BUFFER);

}

VertexFormat convert_to_internal(const GeometryRenderInfo::VertexFormat& format)
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

    shape->getTransformMatrix(node.transform);
    gl->glBindBuffer(GL_UNIFORM_BUFFER, default_material.params);
    void* data = gl->glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(X3DShapeNode), GL_MAP_WRITE_BIT);
    memcpy(data, &node, sizeof(X3DShapeNode));
    gl->glUnmapBuffer(GL_UNIFORM_BUFFER);

    Geometry3DNode *geometry = shape->getGeometry3D();
    if (geometry != nullptr) {
        if (geometry->getNumVertexArrays() > 0) {
            GeometryRenderInfo::VertexArray array;
            geometry->getVertexArray(array, 0);
            VertexFormat format = convert_to_internal(array.getFormat());

            QOpenGLBuffer* vbo = get_buffer(format);
            vbo->bind();
            vbo->allocate(array.getBufferSize());
            void *data = vbo->mapRange(0, array.getBufferSize(), QOpenGLBuffer::RangeWrite);
            if (geometry->isBoxNode()) {
                ((BoxNode*)geometry)->getVertexData(0, data);
            }
            vbo->unmap();

            if (array.getNumElements() > 0) {
                // TODO elements draw
                throw;
            } else {
                // TODO too many draws
                if (draw_calls_pos + sizeof(DrawArraysIndirectCommand) > 65535) {
                    throw;
                }

                gl->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, this->draw_calls);
                void* data = gl->glMapBufferRange(GL_DRAW_INDIRECT_BUFFER, draw_calls_pos, sizeof(DrawArraysIndirectCommand), GL_MAP_WRITE_BIT);
                DrawArraysIndirectCommand cmd = {array.getNumVertices(), 1, 0, 0};
                memcpy(data, &cmd, sizeof(cmd));
                gl->glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);
                DrawBatch batch;
                batch.format_stride = array.getFormat().getSize();
                batch.draw_stride = 0;
                batch.format = format;
                batch.primitive_type = GL_TRIANGLES;
                batch.num_draws = 1;
                batch.buffer_offset = draw_calls_pos;
                draw_calls_pos += sizeof(DrawArraysIndirectCommand);
                default_material.batches.push_back(batch);
            }
        }
	}

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

    float matrix[4][4];
    view->getMatrix(matrix);
    set_viewpoint_view(0, matrix);

    NavigationInfoNode *nav_info = sg->getNavigationInfoNode();
    if (nav_info == nullptr) {
        nav_info = sg->getDefaultNavigationInfoNode();
    }

    if (nav_info != nullptr &&
        nav_info->getHeadlight()) {
        PointLightNode headlight;
        float location[3];
		view->getPosition(location);
        headlight.setLocation(location);
        headlight.setAmbientIntensity(0.3f);
        headlight.setIntensity(0.7f);
        process_light_node(&headlight);
	}

    // TODO sky/ground

    process_node(sg, sg->getNodes());
    render_viewpoints();
}
