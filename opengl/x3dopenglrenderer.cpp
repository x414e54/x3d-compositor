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

struct X3DTransformNode
{
    glm::mat4x4 transform;
};

struct X3DLightNode
{
    // RGB + intensity
    float color_intensity[4] = {1.0, 1.0, 1.0, 0.8*0.2};
    // Attenuation + ambient intensity
    float attenuation_ambient_intensity[4] = {0.0, 0.0, 0.0, 0.8*0.2};
    glm::vec4 position = {0.0, 0.0, 0.0, 1.0};
    glm::vec4 direction = {0.0, 0.0, 0.0, 1.0};
    int type;
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
    glm::ivec4 ambient_offset_width_height;
    glm::ivec4 diffuse_offset_width_height;
    glm::ivec4 specular_offset_width_height;
    glm::ivec4 normal_offset_width_height;
    glm::ivec4 displacement_offset_width_height;
    glm::ivec4 alpha_offset_width_height;
    /// etc.
};

struct X3DAppearanceNode
{
    X3DMaterialNode material;
    X3DTextureTransformNode tex_transform;
    X3DTextureNode texture;
};

struct X3DToneMapNode
{
    int type;
    int direction;
    float bloom_saturation;
    float bloom_exponent;
    float bloom_scale;
    float tonemap_rate;
    int time_delta;
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

    passes.push_back(ShaderPass("Lighting", 1, 1, 1, true, false, false,
                                GL_DEPTH_BUFFER_BIT, ShaderPass::DISABLED, ShaderPass::DISABLED,
                                GL_ZERO, GL_ZERO, GL_FRONT, ShaderPass::DISABLED));

    passes.push_back(ShaderPass("Exposure", 2, 1, 1, true, false, false,
                                GL_DEPTH_BUFFER_BIT, ShaderPass::DISABLED, ShaderPass::DISABLED,
                                GL_ZERO, GL_ZERO, GL_FRONT, ShaderPass::DISABLED, 5));

    passes.push_back(ShaderPass("Post", 3, 1, 0, true, false, false,
                                GL_DEPTH_BUFFER_BIT, ShaderPass::DISABLED, ShaderPass::DISABLED,
                                GL_ZERO, GL_ZERO, GL_FRONT, ShaderPass::DISABLED));

    create_material("x3d-default", ":/shaders/default.vert", ":/shaders/default.frag", 0);
    create_material("x3d-default-light", ":/shaders/default-light.vert", ":/shaders/default-light.frag", 1);
    create_material("x3d-default-exposure", ":/shaders/default-light.vert", ":/shaders/default-exposure.frag", 2);
    create_material("x3d-default-blur", ":/shaders/default-light.vert", ":/shaders/default-blur.frag", 2);
    create_material("x3d-default-tonemap", ":/shaders/default-light.vert", ":/shaders/default-tonemap.frag", 3);

    this->node_listener = new RenderingNodeListener(this);

    this->headlight = new DirectionalLightNode();
    headlight->setAmbientIntensity(0.0);
    headlight->setIntensity(1.0);

    // TODO stop this being a light node
    this->post_process.push_back(new DirectionalLightNode());
    DirectionalLightNode *blur_h = new DirectionalLightNode();
    blur_h->setDirection(0, 0, 1);
    this->post_process.push_back(blur_h);
    DirectionalLightNode *blur_v = new DirectionalLightNode();
    blur_v->setDirection(0, 1, 1);
    this->post_process.push_back(blur_v);
    DirectionalLightNode *tonemap = new DirectionalLightNode();
    tonemap->setDirection(0, 0, 2);
    this->post_process.push_back(tonemap);
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

void X3DOpenGLRenderer::debug_render_increase()
{
    ++this->render_type;
}

void X3DOpenGLRenderer::debug_render_decrease()
{
    --this->render_type;
}

void X3DOpenGLRenderer::process_background_node(BackgroundNode *background)
{

}

void X3DOpenGLRenderer::process_effect_node(DirectionalLightNode *effect_node)
{
    ScopedContext context(this->context_pool, 0);
    const auto gl = context.context.gl;

    Material* default_material = nullptr;

    float params[3];
    effect_node->getDirection(params);

    if (params[2] == 1) {
        default_material = &get_material("x3d-default-blur");
    } else if (params[2] == 2) {
        default_material = &get_material("x3d-default-tonemap");
    } else {
        default_material = &get_material("x3d-default-exposure");
    }

    size_t effect_id = 0;

    bool is_new = false;
    if (effect_node->getValue() != nullptr) {
        effect_id = (size_t)effect_node->getValue() - 1;
    } else {
        is_new = true;
        effect_id = default_material->total_objects++;
        effect_node->setValue((void*)effect_id + 1);
    }

    X3DToneMapNode node;
    node.tonemap_rate = 1.0;
    node.bloom_saturation = 3.0;
    node.bloom_exponent = 0.8;
    node.bloom_scale = 1.0;
    node.direction = params[1];
    // TODO make time delta global
    node.time_delta = 0.1;

    if (is_new) {
        effect_node->addChildNode(new BoxNode(), false);
    }

    DrawInfoBuffer::DrawInfo info = {effect_id, default_material->id, 0, 1};
    process_geometry_node(effect_node->getGeometry3DNode(), info);

    gl->glBindBuffer(GL_UNIFORM_BUFFER, default_material->frag_params);
    void *data = gl->glMapBufferRange(GL_UNIFORM_BUFFER, (effect_id) * sizeof(X3DToneMapNode),
                                sizeof(X3DToneMapNode), GL_MAP_WRITE_BIT);
    memcpy(data, &node, sizeof(X3DToneMapNode));
    gl->glUnmapBuffer(GL_UNIFORM_BUFFER);

    if (effect_node->getNodeListener() != nullptr) {
        effect_node->setNodeListener(this->node_listener);
    }
}

void X3DOpenGLRenderer::process_light_node(LightNode *light_node)
{
    ScopedContext context(this->context_pool, 0);
    const auto gl = context.context.gl;

    if (!light_node->isOn()) {
		return;
    }

    Material& default_material = get_material("x3d-default-light");

    size_t light_id = 0;

    bool is_new = false;
    if (light_node->getValue() != nullptr) {
        light_id = (size_t)light_node->getValue() - 1;
    } else {
        is_new = true;
        light_id = default_material.total_objects++;
        light_node->setValue((void*)light_id + 1);
    }

    X3DLightNode node;
    X3DTransformNode transform;
    light_node->getColor(node.color_intensity);
    node.color_intensity[3] = light_node->getIntensity();
    node.attenuation_ambient_intensity[3] = light_node->getAmbientIntensity();

    float location[3];
    // TODO update light direction, etc.
    if (light_node->isPointLightNode()) {
        PointLightNode *point_light = (PointLightNode *)light_node;
        node.type = 0;

        if (is_new) {
            light_node->addChildNode(new SphereNode(), false);
        }

        point_light->getAttenuation(node.attenuation_ambient_intensity);
        /*sphere.setRadius(calc_light_radius(0, node.color_intensity[3],
                                           node.attenuation_ambient_intensity[0],
                                           node.attenuation_ambient_intensity[1],
                                           node.attenuation_ambient_intensity[2]));*/
        point_light->getLocation(location);

        node.position = glm::vec4(glm::make_vec3(&location[0]), 1.0);
        transform.transform = glm::translate(transform.transform, glm::vec3(node.position));
    } else if (light_node->isDirectionalLightNode()) {
        DirectionalLightNode *direction_light = (DirectionalLightNode *)light_node;
        node.type = 1;

        if (is_new) {
            light_node->addChildNode(new BoxNode(), false);
        }

        direction_light->getDirection(location);
        node.direction = glm::vec4(glm::make_vec3(&location[0]), 1.0);
    } else if (light_node->isSpotLightNode()) {
        SpotLightNode *spot_light = (SpotLightNode *)light_node;
        node.type = 2;

        if (is_new) {
            light_node->addChildNode(new ConeNode(), false);
        }

        spot_light->getLocation(location);

        node.position = glm::vec4(glm::make_vec3(&location[0]), 1.0);
        //spot_light->getDirection(direction);
        float attenuation[3];
        spot_light->getAttenuation(attenuation);
        /*cone.setBottomRadius(calc_light_radius(spot_light->getCutOffAngle(), spot_light->getIntensity(),
                                               attenuation[0], attenuation[1], attenuation[2]));*/
        transform.transform = glm::translate(transform.transform, glm::vec3(node.position));
    }

    DrawInfoBuffer::DrawInfo info = {light_id, default_material.id, 0, node.type};
    process_geometry_node(light_node->getGeometry3DNode(), info);

    // TODO update transform
    gl->glBindBuffer(GL_UNIFORM_BUFFER, default_material.frag_params);
    void *data = gl->glMapBufferRange(GL_UNIFORM_BUFFER, (light_id) * sizeof(X3DLightNode),
                                sizeof(X3DLightNode), GL_MAP_WRITE_BIT);
    memcpy(data, &node, sizeof(X3DLightNode));
    gl->glUnmapBuffer(GL_UNIFORM_BUFFER);

    if (light_node->getNodeListener() != nullptr) {
        light_node->setNodeListener(this->node_listener);
    }
}

void X3DOpenGLRenderer::process_geometry_node(Geometry3DNode *geometry, DrawInfoBuffer::DrawInfo& draw_info)
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

            Material& material = *get_material(draw_info[1]);
            DrawBatch& batch = material.get_batch(format, array.getFormat().getSize(),
                                                  GL_TRIANGLES, array.getNumElements() > 0 ? GL_UNSIGNED_INT : 0);
            Draw& draw = batch.add_draw(array.getNumVertices(), array.getNumElements(),
                                        vbo_pos / array.getFormat().getSize(), ebo_pos / sizeof(int));
            draw.add_instance(draw_info); // base instance
            geometry->setValue((void*)&draw);
            if (geometry->getParentNode() != nullptr) {
                geometry->setNodeListener(this->node_listener);
            }
        }
    }
}

static size_t clamp(int in, int max)
{
    return std::max(0, std::min(in, max));
}

static float lookup_pixel(RGBAColor32 *image, int in_x, int in_y, size_t width, size_t height)
{
    size_t x = clamp(in_x, width);
    size_t y = clamp(in_y, height);
    return image[(y * width) + x][3] / 255.0;
}

void X3DOpenGLRenderer::process_texture_node(TextureNode *base_texture, glm::ivec4& info, size_t filter)
{
    if (base_texture != nullptr && base_texture->isNode(IMAGETEXTURE_NODE)) {
        ImageTextureNode *texture = (ImageTextureNode*)base_texture;
        if (texture->isInstanceNode()) {
            texture = (ImageTextureNode*)texture->getReferenceNode();
        }

        if (texture->getTextureName() != 0) {
            // TODO check capabilities here
            if (true) {
                size_t aligned_size = align(sizeof(GLuint64), 16);
                info[0] = (texture->getTextureName() - 1) / aligned_size;
            } else {
                info[0] = (texture->getTextureName() - 1) / 4;
            }
            info[1] = texture->getWidth();
            info[2] = texture->getWidth();
        } else if (texture->getWidth() > 0 && texture->getHeight() > 0) {
            ScopedContext context(this->context_pool, 0);
            const auto gl = context.context.gl;
            // TODO allow setable texture node width/height
            // TODO convert to ARB from NV spec (qt does not have ARB)
            info[1] = texture->getWidth();
            info[2] = texture->getHeight();
            if (context.context.bind_tex != nullptr) {
                GLuint tex = 0;
                gl->glGenTextures(1, &tex);
                gl->glBindTexture(GL_TEXTURE_2D, tex);
                gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                RGBAColor32 *image = texture->getImage();
                if (filter == 1) {
                    const glm::ivec3 o(-1, 0, 1);
                    for (size_t i = 0; i < info[1]; ++i) {
                        for (size_t j = 0; j < info[2]; ++j) {
                            image[(j * info[1]) + i][3] = image[(j * info[1]) + i][0];
                        }
                    }

                    for (size_t i = 0; i < info[1]; ++i) {
                        for (size_t j = 0; j < info[2]; ++j) {
                            float h_tl = lookup_pixel(image, i + o.x, j + o.x, info[1], info[2]);
                            float h_t  = lookup_pixel(image, i + o.y, j + o.x, info[1], info[2]);
                            float h_tr = lookup_pixel(image, i + o.z, j + o.x, info[1], info[2]);

                            float h_l  = lookup_pixel(image, i + o.x, j + o.y, info[1], info[2]);
                            float h_r  = lookup_pixel(image, i + o.z, j + o.y, info[1], info[2]);

                            float h_bl = lookup_pixel(image, i + o.x, j + o.z, info[1], info[2]);
                            float h_b  = lookup_pixel(image, i + o.y, j + o.z, info[1], info[2]);
                            float h_br = lookup_pixel(image, i + o.z, j + o.z, info[1], info[2]);

                            float d_x = h_tl - h_tr + (2.0 * h_l) - (2.0 * h_r) + h_bl - h_br;
                            float d_y = h_tl + (2.0 * h_t) + h_tr - h_bl - (2.0 * h_b) - h_br;
                            glm::vec3 normal(d_x, -d_y, 0.3 * sqrt(1.0 - (d_x * d_x) - (d_y * d_y)));
                            glm::normalize(normal);
                            normal += 1.0;
                            normal /= 2.0;
                            image[(j * info[1]) + i][2] = normal.x * 255;
                            image[(j * info[1]) + i][1] = normal.y * 255;
                            image[(j * info[1]) + i][0] = normal.z * 255;
                        }
                    }
                }
                gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,  texture->getWidth(), texture->getHeight(), 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, image);
                gl->glGenerateMipmap(GL_TEXTURE_2D);
                GLuint64 handle = context.context.bind_tex->glGetTextureHandleNV(tex);
                texture->setValue((void*)handle);

                context.context.bind_tex->glMakeTextureHandleResidentNV(handle);

                ShaderBuffer& buffer = get_uniform_buffer();
                size_t aligned_size = align(sizeof(GLuint64), 16);
                size_t pos = buffer.allocate(aligned_size);
                memcpy(buffer.data + pos, &handle, sizeof(GLuint64));
                texture->setTextureName(pos + 1);
                info[0] = pos / aligned_size;
            } else {
                PixelBuffer& buffer = get_pixel_buffer();
                size_t bbp = 4;
                size_t num_bytes = texture->getWidth() * texture->getHeight() * bbp;
                size_t pos = buffer.allocate(num_bytes);
                info[0] =  pos / 4;
                texture->setTextureName(pos + 1);

                memcpy(buffer.data + pos, texture->getImage(), num_bytes);

                //texture->getRepeatS();
                //texture->getRepeatT();
                }
        }
    }
}

void X3DOpenGLRenderer::process_apperance_node(AppearanceNode *appearance, DrawInfoBuffer::DrawInfo& info)
{
    Material& default_material = get_material("x3d-default");

    info[1] = default_material.id;
    if (appearance != nullptr) {
        X3DAppearanceNode node;

        if (appearance->isInstanceNode()) {
            Node *reference = appearance->getReferenceNode();
            if ((size_t)reference->getValue() == 0) {
                process_apperance_node((AppearanceNode *)reference, info);
            } else {
                info[2] = (int)(size_t)reference->getValue() - 1;
            }
            return;
        }

        if (appearance->getValue()) {
            info[2] = (int)(size_t)appearance->getValue() - 1;
            return;
        }

        CommonSurfaceShaderNode *shader = appearance->getCommonSurfaceShaderNodes();
        if (shader != nullptr) {
            process_texture_node((TextureNode*)shader->getAlphaTextureField()->getValue(), node.texture.alpha_offset_width_height);
            process_texture_node((TextureNode*)shader->getAmbientTextureField()->getValue(), node.texture.ambient_offset_width_height);
            process_texture_node((TextureNode*)shader->getDiffuseTextureField()->getValue(), node.texture.diffuse_offset_width_height);
            process_texture_node((TextureNode*)shader->getDisplacementTextureField()->getValue(), node.texture.displacement_offset_width_height);
            //process_texture_node((TextureNode*)shader->getEmissiveTextureField()->getValue(), node.texture.);
            process_texture_node((TextureNode*)shader->getSpecularTextureField()->getValue(), node.texture.specular_offset_width_height);
            //process_texture_node((TextureNode*)shader->getShininessTextureField()->getValue(), node.texture.);
            //process_texture_node((TextureNode*)shader->getTransmissionTextureField()->getValue(), node.texture.tr);
            //if (shader->getNormalFormat()-> ) //TODO check here for bump map or normal map
            // TODO remove hard coded flip
            node.texture.normal_offset_width_height[3] = 1.0;
            process_texture_node((TextureNode*)shader->getNormalTextureField()->getValue(), node.texture.normal_offset_width_height, 1);
            //process_texture_node((TextureNode*)shader->getReflectionTextureField()->getValue());
            //process_texture_node((TextureNode*)shader->getEnvironmentTextureField()->getValue(), node.texture.);
        } else {
            TextureTransformNode *transform = appearance->getTextureTransformNodes();
            if (transform != nullptr) {
                transform->getTranslation(node.tex_transform.translation_rotation);
                transform->getCenter(node.tex_transform.center_scale);
                node.tex_transform.translation_rotation[2] = transform->getRotation();
                transform->getScale(&node.tex_transform.center_scale[2]);
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

            ImageTextureNode *texture = appearance->getImageTextureNodes();
            if (texture == nullptr) {
                MultiTextureNode *multi_texture = appearance->getMultiTextureNodes();
                // TODO handle multitexture
                /*while (multi_texture != nullptr) {
                    //process_texture_node(multi_texture)
                }*/
            } else {
                process_texture_node(texture, node.texture.diffuse_offset_width_height);
            }
        }

        if (default_material.total_objects >= 600) {
            // TODO too many objects, convert to SSBO and instance any nodes
            throw;
        }

        ScopedContext context(this->context_pool, 0);
        const auto gl = context.context.gl;

        gl->glBindBuffer(GL_UNIFORM_BUFFER, default_material.frag_params);
        void *data = gl->glMapBufferRange(GL_UNIFORM_BUFFER, default_material.total_objects * sizeof(node),
                                    sizeof(node), GL_MAP_WRITE_BIT);
        memcpy(data, &node, sizeof(node));
        gl->glUnmapBuffer(GL_UNIFORM_BUFFER);

        info[2] = default_material.total_objects;
        ++default_material.total_objects;
        appearance->setValue((void*)default_material.total_objects);
        appearance->setNodeListener(this->node_listener);
    }
}

void X3DOpenGLRenderer::process_shape_node(ShapeNode *shape, bool selected)
{
    DrawInfoBuffer::DrawInfo info;
    if (shape->getValue()) {
        info[0] = (int)((size_t)shape->getValue() / sizeof(X3DTransformNode));
    } else {
        X3DTransformNode node;
        shape->setNodeListener(this->node_listener);

        float matrix[4][4];
        shape->getTransformMatrix(matrix);
        node.transform = glm::make_mat4x4(&matrix[0][0]);

        ShaderBuffer& buffer = get_transform_buffer();
        size_t pos = buffer.allocate(sizeof(X3DTransformNode));
        memcpy(buffer.data + pos, &node, sizeof(X3DTransformNode));
        info[0] = pos / sizeof(X3DTransformNode);
        shape->setValue((void*)pos);
    }
    process_apperance_node(shape->getAppearanceNodes(), info);
    process_geometry_node(shape->getGeometry3D(), info);
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
        glm::vec4 direction = -glm::inverse(view_mat)[2];
        headlight->setDirection(direction.x, -1, direction.z);
        process_light_node(headlight);
    }

    for (auto effect = this->post_process.begin(); effect != this->post_process.end(); ++effect) {
        process_effect_node(*effect);
    }

    process_node(sg, sg->getNodes());

    write_batches();

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
