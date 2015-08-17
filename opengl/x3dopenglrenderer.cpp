/******************************************************************
*
*	CyberX3D for C++
*
*	Copyright (C) Satoshi Konno 1996-2003
*
*	File:	X3DBrowserFunc.cpp
*
******************************************************************/

#include "x3dopenglrenderer.h"
#define CX3D_SUPPORT_OPENGL
#include <cybergarage/x3d/CyberX3D.h>
using namespace CyberX3D;

#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLVertexArrayObject>
#include <QtGui/QOpenGLBuffer>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions_3_0>
#include <QtOpenGLExtensions/QOpenGLExtensions>
#include <QtGui/QOpenGLFramebufferObject>

#include <math.h>
const int OGL_RENDERING_TEXTURE = 0;
const int OGL_RENDERING_WIRE = 1;

// start glu
// This is just temporarily here but will be removed later.

/*
 * SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008)
 * Copyright (C) 1991-2000 Silicon Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice including the dates of first publication and
 * either this permission notice or a reference to
 * http://oss.sgi.com/projects/FreeB/
 * shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * SILICON GRAPHICS, INC. BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of Silicon Graphics, Inc.
 * shall not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization from
 * Silicon Graphics, Inc.
 */
static void __gluMakeIdentityd(double m[16])
{
    m[0+4*0] = 1; m[0+4*1] = 0; m[0+4*2] = 0; m[0+4*3] = 0;
    m[1+4*0] = 0; m[1+4*1] = 1; m[1+4*2] = 0; m[1+4*3] = 0;
    m[2+4*0] = 0; m[2+4*1] = 0; m[2+4*2] = 1; m[2+4*3] = 0;
    m[3+4*0] = 0; m[3+4*1] = 0; m[3+4*2] = 0; m[3+4*3] = 1;
}

static void __gluMultMatricesd(const float (&a)[4][4], const double (&b)[4][4],
                double (&r)[16])
{
    int i, j;

    for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
        r[i*4+j] =
        a[i][0]*b[0][j] +
        a[i][1]*b[1][j] +
        a[i][2]*b[2][j] +
        a[i][3]*b[3][j];
    }
    }
}

/*
** Invert 4x4 matrix.
** Contributed by David Moore (See Mesa bug #6748)
*/
static int __gluInvertMatrixd(const GLdouble m[16], GLdouble invOut[16])
{
    double inv[16], det;
    int i;

    inv[0] =   m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
             + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4] =  -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15]
             - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8] =   m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15]
             + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14]
             - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1] =  -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15]
             - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5] =   m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15]
             + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9] =  -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15]
             - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] =  m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14]
             + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2] =   m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15]
             + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
    inv[6] =  -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15]
             - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
    inv[10] =  m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15]
             + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14]
             - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
    inv[3] =  -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11]
             - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
    inv[7] =   m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11]
             + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11]
             - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
    inv[15] =  m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10]
             + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

    det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (det == 0)
        return GL_FALSE;

    det = 1.0 / det;

    for (i = 0; i < 16; i++)
        invOut[i] = inv[i] * det;

    return GL_TRUE;
}

static void __gluMultMatrixVecd(const GLdouble matrix[16], const GLdouble in[4],
              GLdouble out[4])
{
    int i;

    for (i=0; i<4; i++) {
    out[i] =
        in[0] * matrix[0*4+i] +
        in[1] * matrix[1*4+i] +
        in[2] * matrix[2*4+i] +
        in[3] * matrix[3*4+i];
    }
}

#define __glPi 3.14159265358979323846

static void create_projection(double (&m)[4][4], double fovy, double aspect, double zNear, double zFar)
{
    double sine, cotangent, deltaZ;
    double radians = fovy / 2 * __glPi / 180;

    deltaZ = zFar - zNear;
    sine = sin(radians);
    if ((deltaZ == 0) || (sine == 0) || (aspect == 0)) {
    return;
    }
    cotangent = cos(radians) / sine;

    __gluMakeIdentityd(&m[0][0]);
    m[0][0] = cotangent / aspect;
    m[1][1] = cotangent;
    m[2][2] = -(zFar + zNear) / deltaZ;
    m[2][3] = -1;
    m[3][2] = -2 * zNear * zFar / deltaZ;
    m[3][3] = 0;
}

bool to_ray(const double (&finalMatrix)[16], const double (&in)[4], double (&out)[3])
{
    __gluMultMatrixVecd(finalMatrix, in, out);
    if (out[3] == 0.0) return false;
    out[0] /= out[3];
    out[1] /= out[3];
    out[2] /= out[3];
    return true;
}

X3DOpenGLRenderer::X3DOpenGLRenderer()
{
}

X3DOpenGLRenderer::~X3DOpenGLRenderer()
{

}

bool X3DOpenGLRenderer::get_ray(double x, double y,
                          const float (&model)[4][4],
                          double (&from)[3], double (&to)[3])
{
    double finalMatrix[16];
    double in[4];

    __gluMultMatricesd(model, active_viewpoint.left.projection, finalMatrix);
    if (!__gluInvertMatrixd(finalMatrix, finalMatrix)) return false;

    in[0]=x;
    in[1]=y;
    in[2]=0.0;
    in[3]=1.0;

    /* Map x and y from window coordinates */
    in[0] = in[0] / active_viewpoint.left.viewport_width;
    in[1] = in[1] / active_viewpoint.left.viewport_height;

    /* Map to range -1 to 1 */
    in[0] = in[0] * 2 - 1;
    in[1] = in[1] * 2 - 1;
    in[2] = in[2] * 2 - 1;

    bool has_ray = to_ray(finalMatrix, in, from);
    in[2]=1.0;
    has_ray &= to_ray(finalMatrix, in, to);

    return has_ray;
}
// end glu

void X3DOpenGLRenderer::set_projection(double fovy, double aspect, double zNear, double zFar)
{
    create_projection(active_viewpoint.left.projection, fovy, aspect, zNear, zFar);
}

////////////////////////////////////////////////////////// 
//  render
////////////////////////////////////////////////////////// 

static int gnLights;
static PointLightNode headLight;

void PushLightNode(LightNode *lightNode)
{
	if (!lightNode->isOn()) 
		return;

	GLint	nMaxLightMax;
	glGetIntegerv(GL_MAX_LIGHTS, &nMaxLightMax);

	if (nMaxLightMax < gnLights) {
		gnLights++;
		return;
	}

	float	color[4];
	float	pos[4];
	float	attenuation[3];
	float	direction[3];
	float	intensity;

	if (lightNode->isPointLightNode()) {
		
		PointLightNode *pLight = (PointLightNode *)lightNode;

		glEnable(GL_LIGHT0+gnLights);
		
		pLight->getAmbientColor(color);
		glLightfv(GL_LIGHT0+gnLights, GL_AMBIENT, color);

		pLight->getColor(color);
		intensity = pLight->getIntensity();
		color[0] *= intensity; 
		color[1] *= intensity; 
		color[2] *= intensity; 
		glLightfv(GL_LIGHT0+gnLights, GL_DIFFUSE, color);
		glLightfv(GL_LIGHT0+gnLights, GL_SPECULAR, color);

		pLight->getLocation(pos); pos[3] = 1.0f;
		glLightfv(GL_LIGHT0+gnLights, GL_POSITION, pos);

		direction[0] = 0.0f; direction[0] = 0.0f; direction[0] = 0.0f;
		glLightfv(GL_LIGHT0+gnLights, GL_SPOT_DIRECTION, direction);
		glLightf(GL_LIGHT0+gnLights, GL_SPOT_EXPONENT, 0.0f);
		glLightf(GL_LIGHT0+gnLights, GL_SPOT_CUTOFF, 180.0f);

		pLight->getAttenuation(attenuation);
		glLightf(GL_LIGHT0+gnLights, GL_CONSTANT_ATTENUATION, attenuation[0]);
		glLightf(GL_LIGHT0+gnLights, GL_LINEAR_ATTENUATION, attenuation[1]);
		glLightf(GL_LIGHT0+gnLights, GL_QUADRATIC_ATTENUATION, attenuation[2]);
		
		gnLights++;
	}
	else if (lightNode->isDirectionalLightNode()) {

		DirectionalLightNode *dLight = (DirectionalLightNode *)lightNode;
		
		glEnable(GL_LIGHT0+gnLights);
		
		dLight->getAmbientColor(color);
		glLightfv(GL_LIGHT0+gnLights, GL_AMBIENT, color);

		dLight->getColor(color);
		intensity = dLight->getIntensity();
		color[0] *= intensity; 
		color[1] *= intensity; 
		color[2] *= intensity; 
		glLightfv(GL_LIGHT0+gnLights, GL_DIFFUSE, color);
		glLightfv(GL_LIGHT0+gnLights, GL_SPECULAR, color);

		dLight->getDirection(pos); pos[3] = 0.0f;
		glLightfv(GL_LIGHT0+gnLights, GL_POSITION, pos);

		direction[0] = 0.0f; direction[0] = 0.0f; direction[0] = 0.0f;
		glLightfv(GL_LIGHT0+gnLights, GL_SPOT_DIRECTION, direction);
		glLightf(GL_LIGHT0+gnLights, GL_SPOT_EXPONENT, 0.0f);
		glLightf(GL_LIGHT0+gnLights, GL_SPOT_CUTOFF, 180.0f);

		glLightf(GL_LIGHT0+gnLights, GL_CONSTANT_ATTENUATION, 1.0);
		glLightf(GL_LIGHT0+gnLights, GL_LINEAR_ATTENUATION, 0.0);
		glLightf(GL_LIGHT0+gnLights, GL_QUADRATIC_ATTENUATION, 0.0);

		gnLights++;
	}
	else if (lightNode->isSpotLightNode()) {

		SpotLightNode *sLight = (SpotLightNode *)lightNode;

		glEnable(GL_LIGHT0+gnLights);
		
		sLight->getAmbientColor(color);
		glLightfv(GL_LIGHT0+gnLights, GL_AMBIENT, color);

		sLight->getColor(color);
		intensity = sLight->getIntensity();
		color[0] *= intensity; 
		color[1] *= intensity; 
		color[2] *= intensity; 
		glLightfv(GL_LIGHT0+gnLights, GL_DIFFUSE, color);
		glLightfv(GL_LIGHT0+gnLights, GL_SPECULAR, color);

		sLight->getLocation(pos); pos[3] = 1.0f;
		glLightfv(GL_LIGHT0+gnLights, GL_POSITION, pos);

		sLight->getDirection(direction);
		glLightfv(GL_LIGHT0+gnLights, GL_SPOT_DIRECTION, direction);

		glLightf(GL_LIGHT0+gnLights, GL_SPOT_EXPONENT, 0.0f);
		glLightf(GL_LIGHT0+gnLights, GL_SPOT_CUTOFF, sLight->getCutOffAngle());

		sLight->getAttenuation(attenuation);
		glLightf(GL_LIGHT0+gnLights, GL_CONSTANT_ATTENUATION, attenuation[0]);
		glLightf(GL_LIGHT0+gnLights, GL_LINEAR_ATTENUATION, attenuation[1]);
		glLightf(GL_LIGHT0+gnLights, GL_QUADRATIC_ATTENUATION, attenuation[2]);

		gnLights++;
	}
}

void PopLightNode(LightNode *lightNode)
{
	if (!lightNode->isOn()) 
		return;

	GLint	nMaxLightMax;
	glGetIntegerv(GL_MAX_LIGHTS, &nMaxLightMax);

	gnLights--;
	
	if (gnLights < nMaxLightMax)
		glDisable(GL_LIGHT0+gnLights);
}

VertexFormat convert_to_internal(const GeometryRenderInfo::VertexFormat& format)
{
    VertexFormat new_format;
    for (size_t i = 0; i < format.num_attribs; ++i) {
        const GeometryRenderInfo::Attribute& attrib = format.attribs[i];
        new_format.addAttribute(GL_UNSIGNED_BYTE, attrib.attrib_size, attrib.normalized);
    }
    return new_format;
}

void X3DOpenGLRenderer::DrawShapeNode(SceneGraph *sg, ShapeNode *shape, int drawMode)
{
    ScopedContext context(this->context_pool, 0);
    auto gl = context.context.gl;
    auto vab = context.context.vab;

	glPushMatrix ();

	/////////////////////////////////
	//	Appearance(Material)
	/////////////////////////////////

	float	color[4];
	color[3] = 1.0f;

	AppearanceNode			*appearance = shape->getAppearanceNodes();
	MaterialNode			*material = NULL;
	ImageTextureNode		*imgTexture = NULL;
	TextureTransformNode	*texTransform = NULL;

	bool				bEnableTexture = false;

	if (appearance) {

		// Texture Transform
		TextureTransformNode *texTransform = appearance->getTextureTransformNodes();
		if (texTransform) {
			float texCenter[2];
			float texScale[2];
			float texTranslation[2];
			float texRotation;

			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();

			texTransform->getTranslation(texTranslation);
			glTranslatef(texTranslation[0], texTranslation[1], 0.0f);

			texTransform->getCenter(texCenter);
			glTranslatef(texCenter[0], texCenter[1], 0.0f);

			texRotation = texTransform->getRotation();
			glRotatef(0.0f, 0.0f, 1.0f, texRotation);

			texTransform->getScale(texScale);
			glScalef(texScale[0], texScale[1], 1.0f);

			glTranslatef(-texCenter[0], -texCenter[1], 0.0f);
		}
		else {
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();
			glTranslatef(0.0f, 0.0f, 1.0f);
		}

		glMatrixMode(GL_MODELVIEW);

		// Texture
		imgTexture = appearance->getImageTextureNodes();
		if (imgTexture && drawMode == OGL_RENDERING_TEXTURE) {

			int width = imgTexture->getWidth();
			int height = imgTexture->getHeight();
			RGBAColor32 *color = imgTexture->getImage();

		//if (0 < width && 0 < height && color != NULL)
		if (imgTexture->getTextureName() != 0)
				bEnableTexture = true;

			if (bEnableTexture == true) {
				if (imgTexture->hasTransparencyColor() == true) {
					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				}
				else
					glDisable(GL_BLEND);

				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glBindTexture(GL_TEXTURE_2D, imgTexture->getTextureName());

				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				glEnable(GL_TEXTURE_2D);

				glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
				glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
				glEnable(GL_COLOR_MATERIAL);
			}
			else {
				glDisable(GL_TEXTURE_2D);
				glDisable(GL_COLOR_MATERIAL);
			}
		}
		else {
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_COLOR_MATERIAL);
		}

		// Material
        material = appearance->getMaterialNodes();
		if (material) {
			float	ambientIntesity = material->getAmbientIntensity();

			glDisable(GL_COLOR_MATERIAL);
			material->getDiffuseColor(color);
			if (material->getTransparency() != 0.0) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			} else if (!bEnableTexture) {
				glDisable(GL_BLEND);
			}

			color[3] = 1.0 - material->getTransparency();
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);

			material->getSpecularColor(color);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, color);

			material->getEmissiveColor(color);
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, color);

			material->getDiffuseColor(color);
			color[0] *= ambientIntesity; 
			color[1] *= ambientIntesity; 
			color[2] *= ambientIntesity; 
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);

			glMateriali(GL_FRONT, GL_SHININESS, (int)(material->getShininess()*128.0));
		}

    }

	if (!appearance || !material) {
		color[0] = 0.8f; color[1] = 0.8f; color[2] = 0.8f;
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);
		color[0] = 0.0f; color[1] = 0.0f; color[2] = 0.0f;
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, color);
		glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, color);
		color[0] = 0.8f*0.2f; color[1] = 0.8f*0.2f; color[2] = 0.8f*0.2f;
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);
		glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, (int)(0.2*128.0));
		glDisable(GL_BLEND);
	}

	if (!appearance || !imgTexture || drawMode != OGL_RENDERING_TEXTURE) {
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_COLOR_MATERIAL);
	}

	/////////////////////////////////
	//	Transform 
	/////////////////////////////////

	float	m4[4][4];
	shape->getTransformMatrix(m4);
	glMultMatrixf((float *)m4);

	glColor3f(1.0f, 1.0f, 1.0f);

	/////////////////////////////////
	//	Geometry3D
	/////////////////////////////////

	Geometry3DNode *gnode = shape->getGeometry3D();
    if (gnode) {
        if (gnode->getNumVertexArrays() > 0) {
            GeometryRenderInfo::VertexArray array;
            VertexFormat format = convert_to_internal(array.format);

/* this will be done elsewhere*/
            int vao = context.context.get_vao(format);
            gl->glBindVertexArray(vao);

            QOpenGLBuffer* vbo = get_buffer(format);
            vbo->allocate(array.getBufferSize());
            void *data = vbo->map(QOpenGLBuffer::ReadWrite);
/* only this part will be done here */
            gnode->getVertexData(array, data);
/*  */
            vbo->unmap();
            vab->glBindVertexBuffer(0, vbo->bufferId(), 0, array.format.size);

            if (array.num_elements > 0) {

            } else {
                glDrawArrays(GL_TRIANGLES, 0, 1);
            }
/**/
        } else if (0 < gnode->getDisplayList()) {
            gnode->draw();
        }
	}

	ShapeNode *selectedShapeNode = sg->getSelectedShapeNode();
	if (gnode && selectedShapeNode == shape) {
		float	bboxCenter[3];
		float	bboxSize[3];
		gnode->getBoundingBoxCenter(bboxCenter);
		gnode->getBoundingBoxSize(bboxSize);

		glColor3f(1.0f, 1.0f, 1.0f);
		glDisable(GL_LIGHTING);
//		glDisable(GL_COLOR_MATERIAL);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);

		glBegin(GL_LINES);
		int x, y, z;
		for (x=0; x<2; x++) {
			for (y=0; y<2; y++) {
				float point[3];
				point[0] = (x==0) ? bboxCenter[0] - bboxSize[0] : bboxCenter[0] + bboxSize[0];
				point[1] = (y==0) ? bboxCenter[1] - bboxSize[1] : bboxCenter[1] + bboxSize[1];
				point[2] = bboxCenter[2] - bboxSize[2];
				glVertex3fv(point);
				point[2] = bboxCenter[2] + bboxSize[2];
				glVertex3fv(point);
			}
		}
		for (x=0; x<2; x++) {
			for (z=0; z<2; z++) {
				float point[3];
				point[0] = (x==0) ? bboxCenter[0] - bboxSize[0] : bboxCenter[0] + bboxSize[0];
				point[1] = bboxCenter[1] - bboxSize[1];
				point[2] = (z==0) ? bboxCenter[2] - bboxSize[2] : bboxCenter[2] + bboxSize[2];
				glVertex3fv(point);
				point[1] = bboxCenter[1] + bboxSize[1];
				glVertex3fv(point);
			}
		}
		for (y=0; y<2; y++) {
			for (z=0; z<2; z++) {
				float point[3];
				point[0] = bboxCenter[0] - bboxSize[0];
				point[1] = (y==0) ? bboxCenter[1] - bboxSize[1] : bboxCenter[1] + bboxSize[1];
				point[2] = (z==0) ? bboxCenter[2] - bboxSize[2] : bboxCenter[2] + bboxSize[2];
				glVertex3fv(point);
				point[0] = bboxCenter[0] + bboxSize[0];
				glVertex3fv(point);
			}
		}
		glEnd();

		glEnable(GL_LIGHTING);
//		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
	}

	glPopMatrix();
}


void X3DOpenGLRenderer::DrawNode(SceneGraph *sceneGraph, Node *firstNode, int drawMode)
{
	if (!firstNode)
		return;

	Node	*node;

//	for (node = firstNode; node; node=node->next()) {
//		if (node->isLightNode())
//			PushLightNode((LightNode *)node);
//	}

	for (node = firstNode; node; node=node->next()) {
		if (node->isShapeNode()) 
			DrawShapeNode(sceneGraph, (ShapeNode *)node, drawMode);
		else
			DrawNode(sceneGraph, node->getChildNodes(), drawMode);
	}

//	for (node = firstNode; node; node=node->next()) {
//		if (node->isLightNode())
//			PopLightNode((LightNode *)node);
//	}
}

void X3DOpenGLRenderer::render(SceneGraph *sg)
{
    ScopedContext context(context_pool, 0);
    context.context.gl->glBindFramebuffer(GL_FRAMEBUFFER,
        context.context.get_fbo(active_viewpoint.left.render_target->texture()));
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    glViewport(0, 0, active_viewpoint.left.viewport_width, active_viewpoint.left.viewport_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMultMatrixd(&active_viewpoint.left.projection[0][0]);

    const int drawMode = OGL_RENDERING_TEXTURE;

	/////////////////////////////////
	//	Headlight 
	/////////////////////////////////

	NavigationInfoNode *navInfo = sg->getNavigationInfoNode();
	if (navInfo == NULL)
		navInfo = sg->getDefaultNavigationInfoNode();

	if (navInfo->getHeadlight()) {
		float	location[3];
		ViewpointNode *view = sg->getViewpointNode();
		if (view == NULL)
			view = sg->getDefaultViewpointNode();
		view->getPosition(location);
		headLight.setLocation(location);
		headLight.setAmbientIntensity(0.3f);
		headLight.setIntensity(0.7f);
		sg->addNode(&headLight);
	}

	/////////////////////////////////
	//	Viewpoint 
	/////////////////////////////////

	ViewpointNode *view = sg->getViewpointNode();
	if (view == NULL)
		view = sg->getDefaultViewpointNode();

	/////////////////////////////////
	//	Rendering 
	/////////////////////////////////

	glEnable(GL_DEPTH_TEST);
	switch (drawMode) {
	case OGL_RENDERING_WIRE:
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		break;
	default:
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);

	glEnable(GL_LIGHTING);
//	glShadeModel (GL_FLAT);
	glShadeModel (GL_SMOOTH);

	/////////////////////////////////
	//	Background 
	/////////////////////////////////

	float clearColor[] = {0.0f, 0.0f, 0.0f};
	
	BackgroundNode *bg = sg->getBackgroundNode();
	if (bg != NULL) {
		if (0 < bg->getNSkyColors())
			bg->getSkyColor(0, clearColor);
	}

	glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	if (!view)
		return;

	/////////////////////////////////
	//	Viewpoint Matrix
	/////////////////////////////////

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	float	m4[4][4];
	view->getMatrix(m4);
	glMultMatrixf((float *)m4);

	/////////////////////////////////
	//	Light
	/////////////////////////////////

	GLint	nMaxLightMax;
	glGetIntegerv(GL_MAX_LIGHTS, &nMaxLightMax);
	for (int n = 0; n < nMaxLightMax; n++)
		glDisable(GL_LIGHT0+n);
	gnLights = 0;

	/////////////////////////////////
	//	General Node
	/////////////////////////////////

	DrawNode(sg, sg->getNodes(), drawMode);

	/////////////////////////////////
	//	Headlight 
	/////////////////////////////////

	headLight.remove();

	/////////////////////////////////
	//	glFlush 
	/////////////////////////////////

	glFlush();

    render_viewpoints();
}
