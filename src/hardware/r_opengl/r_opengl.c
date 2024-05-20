// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2019 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file r_opengl.c
/// \brief OpenGL API for Sonic Robo Blast 2

#if defined (_WIN32)
//#define WIN32_LEAN_AND_MEAN
#define RPC_NO_WINDOWS_H
#include <windows.h>
#endif
#undef GETTEXT
#ifdef __GNUC__
#include <unistd.h>
#endif

#include <stdarg.h>
#include <math.h>
#include "../../r_local.h" // For rendertimefrac, used for the leveltime shader uniform
#include "r_opengl.h"
#include "r_vbo.h"
#include "../hw_shaders.h"
#include "../hw_main.h"
#include "../hw_clip.h"

// Eeeeh not sure is this right way, but it works < sry :c < sry again it had to go :c

extern fixed_t fovtan; // also extremely bad, I'm just too lazy!!!

#if defined (HWRENDER) && !defined (NOROPENGL)

struct GLRGBAFloat
{
	GLfloat red;
	GLfloat green;
	GLfloat blue;
	GLfloat alpha;
};
typedef struct GLRGBAFloat GLRGBAFloat;

// lighttable list item
struct LTListItem
{
	UINT32 id;
	struct LTListItem *next;
};
typedef struct LTListItem LTListItem;

// ==========================================================================
//                                                                  CONSTANTS
// ==========================================================================

static const GLubyte white[4] = { 255, 255, 255, 255 };

// With OpenGL 1.1+, the first texture should be 1
static GLuint NOTEXTURE_NUM = 0;

#define      N_PI_DEMI               (M_PIl/2.0f) //(1.5707963268f)

#define      ASPECT_RATIO            (1.0f)  //(320.0f/200.0f)
//#define      FAR_CLIPPING_PLANE      32768.0f // Draw further! Tails 01-21-2001
//#define      FAR_CLIPPING_PLANE      (4096.0f - 512.0f)
static float FAR_CLIPPING_PLANE = 32768.0f;
static float NEAR_CLIPPING_PLANE =   NZCLIP_PLANE;

// **************************************************************************
//                                                                    GLOBALS
// **************************************************************************

static  GLuint      tex_downloaded  = 0;
static  GLuint      lt_downloaded   = 0; // currently bound lighttable texture
static  GLfloat     fov             = 90.0f;
static  FBITFIELD   CurrentPolyFlags;

// Linked list of all textures.
static FTextureInfo *TexCacheTail = NULL;
static FTextureInfo *TexCacheHead = NULL;

static RGBA_t *textureBuffer = NULL;
static size_t textureBufferSize = 0;

// Linked list of all lighttables.
static LTListItem *LightTablesTail = NULL;
static LTListItem *LightTablesHead = NULL;

static RGBA_t screenPalette[256] = {0}; // the palette for the postprocessing step in palette rendering
static GLuint screenPaletteTex = 0; // 1D texture containing the screen palette
static GLuint paletteLookupTex = 0; // 3D texture containing RGB -> palette index lookup table
RGBA_t  myPaletteData[256]; // the palette for converting textures to RGBA

GLint   screen_width    = 0;               // used by Draw2DLine()
GLint   screen_height   = 0;
GLbyte  screen_depth    = 0;
GLint   textureformatGL = 0;
GLint maximumAnisotropy = 0;
static GLboolean MipMap = GL_FALSE;
static GLint min_filter = GL_LINEAR;
static GLint mag_filter = GL_LINEAR;
static GLint anisotropic_filter = 0;
boolean supportMipMap = false;
static FTransform  md2_transform;

const GLubyte *gl_version = NULL;
const GLubyte *gl_renderer = NULL;
const GLubyte *gl_extensions = NULL;

// Loaded OpenGL version
int majorGL = 0, minorGL = 0;

//Hurdler: 04/10/2000: added for the kick ass coronas as Boris wanted;-)
static GLfloat modelMatrix[16];
GLfloat projMatrix[16];
static GLint   viewport[4];

#ifdef USE_FBO_OGL
static boolean GLFramebuffer_IsFuncAvailible(void);

GLuint FramebufferObject, FramebufferTexture, RenderbufferObject;
GLboolean FrameBufferEnabled = GL_FALSE, RenderToFramebuffer = GL_FALSE;

boolean supportFBO = false;
#endif

// Sryder:	NextTexAvail is broken for these because palette changes or changes to the texture filter or antialiasing
//			flush all of the stored textures, leaving them unavailable at times such as between levels
//			These need to start at 0 and be set to their number, and be reset to 0 when deleted so that Intel GPUs
//			can know when the textures aren't there, as textures are always considered resident in their virtual memory
static GLuint screenTextures[NUMSCREENTEXTURES] = {0};

// shortcut for ((float)1/i)
static const GLfloat byte2float[256] = {
	0.000000f, 0.003922f, 0.007843f, 0.011765f, 0.015686f, 0.019608f, 0.023529f, 0.027451f,
	0.031373f, 0.035294f, 0.039216f, 0.043137f, 0.047059f, 0.050980f, 0.054902f, 0.058824f,
	0.062745f, 0.066667f, 0.070588f, 0.074510f, 0.078431f, 0.082353f, 0.086275f, 0.090196f,
	0.094118f, 0.098039f, 0.101961f, 0.105882f, 0.109804f, 0.113725f, 0.117647f, 0.121569f,
	0.125490f, 0.129412f, 0.133333f, 0.137255f, 0.141176f, 0.145098f, 0.149020f, 0.152941f,
	0.156863f, 0.160784f, 0.164706f, 0.168627f, 0.172549f, 0.176471f, 0.180392f, 0.184314f,
	0.188235f, 0.192157f, 0.196078f, 0.200000f, 0.203922f, 0.207843f, 0.211765f, 0.215686f,
	0.219608f, 0.223529f, 0.227451f, 0.231373f, 0.235294f, 0.239216f, 0.243137f, 0.247059f,
	0.250980f, 0.254902f, 0.258824f, 0.262745f, 0.266667f, 0.270588f, 0.274510f, 0.278431f,
	0.282353f, 0.286275f, 0.290196f, 0.294118f, 0.298039f, 0.301961f, 0.305882f, 0.309804f,
	0.313726f, 0.317647f, 0.321569f, 0.325490f, 0.329412f, 0.333333f, 0.337255f, 0.341176f,
	0.345098f, 0.349020f, 0.352941f, 0.356863f, 0.360784f, 0.364706f, 0.368627f, 0.372549f,
	0.376471f, 0.380392f, 0.384314f, 0.388235f, 0.392157f, 0.396078f, 0.400000f, 0.403922f,
	0.407843f, 0.411765f, 0.415686f, 0.419608f, 0.423529f, 0.427451f, 0.431373f, 0.435294f,
	0.439216f, 0.443137f, 0.447059f, 0.450980f, 0.454902f, 0.458824f, 0.462745f, 0.466667f,
	0.470588f, 0.474510f, 0.478431f, 0.482353f, 0.486275f, 0.490196f, 0.494118f, 0.498039f,
	0.501961f, 0.505882f, 0.509804f, 0.513726f, 0.517647f, 0.521569f, 0.525490f, 0.529412f,
	0.533333f, 0.537255f, 0.541177f, 0.545098f, 0.549020f, 0.552941f, 0.556863f, 0.560784f,
	0.564706f, 0.568627f, 0.572549f, 0.576471f, 0.580392f, 0.584314f, 0.588235f, 0.592157f,
	0.596078f, 0.600000f, 0.603922f, 0.607843f, 0.611765f, 0.615686f, 0.619608f, 0.623529f,
	0.627451f, 0.631373f, 0.635294f, 0.639216f, 0.643137f, 0.647059f, 0.650980f, 0.654902f,
	0.658824f, 0.662745f, 0.666667f, 0.670588f, 0.674510f, 0.678431f, 0.682353f, 0.686275f,
	0.690196f, 0.694118f, 0.698039f, 0.701961f, 0.705882f, 0.709804f, 0.713726f, 0.717647f,
	0.721569f, 0.725490f, 0.729412f, 0.733333f, 0.737255f, 0.741177f, 0.745098f, 0.749020f,
	0.752941f, 0.756863f, 0.760784f, 0.764706f, 0.768627f, 0.772549f, 0.776471f, 0.780392f,
	0.784314f, 0.788235f, 0.792157f, 0.796078f, 0.800000f, 0.803922f, 0.807843f, 0.811765f,
	0.815686f, 0.819608f, 0.823529f, 0.827451f, 0.831373f, 0.835294f, 0.839216f, 0.843137f,
	0.847059f, 0.850980f, 0.854902f, 0.858824f, 0.862745f, 0.866667f, 0.870588f, 0.874510f,
	0.878431f, 0.882353f, 0.886275f, 0.890196f, 0.894118f, 0.898039f, 0.901961f, 0.905882f,
	0.909804f, 0.913726f, 0.917647f, 0.921569f, 0.925490f, 0.929412f, 0.933333f, 0.937255f,
	0.941177f, 0.945098f, 0.949020f, 0.952941f, 0.956863f, 0.960784f, 0.964706f, 0.968628f,
	0.972549f, 0.976471f, 0.980392f, 0.984314f, 0.988235f, 0.992157f, 0.996078f, 1.000000f
};

// -----------------+
// GL_DBG_Printf    : Output debug messages to debug log if DEBUG_TO_FILE is defined,
//                  : else do nothing
// Returns          :
// -----------------+

#ifdef DEBUG_TO_FILE
FILE *gllogstream;
#endif

FUNCPRINTF void GL_DBG_Printf(const char *format, ...)
{
#ifdef DEBUG_TO_FILE
	char str[4096] = "";
	va_list arglist;

	if (!gllogstream)
		gllogstream = fopen("ogllog.txt", "w");
	
	va_start(arglist, format);
	vsnprintf(str, 4096, format, arglist);
	va_end(arglist);

	fwrite(str, strlen(str), 1, gllogstream);
#else
	(void)format;
#endif
}

// -----------------+
// GL_MSG_Warning   : Raises a warning.
//                  :
// Returns          :
// -----------------+


static void GL_MSG_Warning(const char *format, ...)
{
	char str[4096] = "";
	va_list arglist;

	va_start(arglist, format);
	vsnprintf(str, 4096, format, arglist);
	va_end(arglist);

#ifdef HAVE_SDL
	CONS_Alert(CONS_WARNING, "%s", str);
#endif
#ifdef DEBUG_TO_FILE
		if (!gllogstream)
		gllogstream = fopen("ogllog.txt", "w");
	fwrite(str, strlen(str), 1, gllogstream);
#endif
}

// -----------------+
// GL_MSG_Error     : Raises an error.
//                  :
// Returns          :
// -----------------+

static void GL_MSG_Error(const char *format, ...)
{
	char str[4096] = "";
	va_list arglist;

	va_start(arglist, format);
	vsnprintf(str, 4096, format, arglist);
	va_end(arglist);

#ifdef HAVE_SDL
	CONS_Alert(CONS_ERROR, "%s", str);
#endif
#ifdef DEBUG_TO_FILE
	if (!gllogstream)
		gllogstream = fopen("ogllog.txt", "w");
	fwrite(str, strlen(str), 1, gllogstream);
#endif
}

#ifdef STATIC_OPENGL
/* 1.0 functions */
/* Miscellaneous */
#define pglClearColor glClearColor
//glClear
#define pglColorMask glColorMask
#define pglAlphaFunc glAlphaFunc
#define pglBlendFunc glBlendFunc
#define pglCullFace glCullFace
#define pglPolygonOffset glPolygonOffset
#define pglScissor glScissor
#define pglEnable glEnable
#define pglDisable glDisable
#define pglGetFloatv glGetFloatv

/* Depth Buffer */
#define pglClearDepth glClearDepth
#define pglDepthFunc glDepthFunc
#define pglDepthMask glDepthMask
#define pglDepthRange glDepthRange

/* Transformation */
#define pglMatrixMode glMatrixMode
#define pglViewport glViewport
#define pglPushMatrix glPushMatrix
#define pglPopMatrix glPopMatrix
#define pglLoadIdentity glLoadIdentity
#define pglMultMatrixd glMultMatrixd
#define pglRotatef glRotatef
#define pglScalef glScalef
#define pglTranslatef glTranslatef

/* Drawing Functions */
#define pglColor4ubv glColor4ubv
#define pglVertexPointer glVertexPointer
#define pglNormalPointer glNormalPointer
#define pglTexCoordPointer glTexCoordPointer
#define pglDrawArrays glDrawArrays
#define pglDrawElements glDrawElements
#define pglEnableClientState glEnableClientState
#define pglDisableClientState glDisableClientState

/* Lighting */
#define pglShadeModel glShadeModel
#define pglLightfv glLightfv
#define pglLightModelfv glLightModelfv
#define pglMaterialfv glMaterialfv
#define pglMateriali glMateriali

/* Raster functions */
#define pglPixelStorei glPixelStorei
#define pglReadPixels glReadPixels

/* Texture mapping */
#define pglTexEnvi glTexEnvi
#define pglTexParameteri glTexParameteri
#define pglTexImage2D glTexImage2D
#define pglTexImage3D glTexImage3D
#define pglTexSubImage2D glTexSubImage2D

/* 1.1 functions */
/* texture objects */ //GL_EXT_texture_object
#define pglGenTextures glGenTextures
#define pglDeleteTextures glDeleteTextures
#define pglBindTexture glBindTexture
/* texture mapping */ //GL_EXT_copy_texture
#define pglCopyTexImage2D glCopyTexImage2D
#define pglCopyTexSubImage2D glCopyTexSubImage2D

#else //!STATIC_OPENGL

/* 1.0 functions */
/* Miscellaneous */
typedef void (APIENTRY * PFNglClearColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
static PFNglClearColor pglClearColor;
typedef void (APIENTRY * PFNglColorMask) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
static PFNglColorMask pglColorMask;
typedef void (APIENTRY * PFNglAlphaFunc) (GLenum func, GLclampf ref);
static PFNglAlphaFunc pglAlphaFunc;
typedef void (APIENTRY * PFNglBlendFunc) (GLenum sfactor, GLenum dfactor);
static PFNglBlendFunc pglBlendFunc;
typedef void (APIENTRY * PFNglCullFace) (GLenum mode);
static PFNglCullFace pglCullFace;
typedef void (APIENTRY * PFNglPolygonOffset) (GLfloat factor, GLfloat units);
static PFNglPolygonOffset pglPolygonOffset;
typedef void (APIENTRY * PFNglScissor) (GLint x, GLint y, GLsizei width, GLsizei height);
static PFNglScissor pglScissor;
typedef void (APIENTRY * PFNglEnable) (GLenum cap);
static PFNglEnable pglEnable;
typedef void (APIENTRY * PFNglDisable) (GLenum cap);
static PFNglDisable pglDisable;
typedef void (APIENTRY * PFNglGetFloatv) (GLenum pname, GLfloat *params);
static PFNglGetFloatv pglGetFloatv;

/* Depth Buffer */
typedef void (APIENTRY * PFNglClearDepth) (GLclampd depth);
static PFNglClearDepth pglClearDepth;
typedef void (APIENTRY * PFNglDepthFunc) (GLenum func);
static PFNglDepthFunc pglDepthFunc;
typedef void (APIENTRY * PFNglDepthMask) (GLboolean flag);
static PFNglDepthMask pglDepthMask;
typedef void (APIENTRY * PFNglDepthRange) (GLclampd near_val, GLclampd far_val);
static PFNglDepthRange pglDepthRange;

/* Transformation */
typedef void (APIENTRY * PFNglMatrixMode) (GLenum mode);
static PFNglMatrixMode pglMatrixMode;
typedef void (APIENTRY * PFNglViewport) (GLint x, GLint y, GLsizei width, GLsizei height);
static PFNglViewport pglViewport;
typedef void (APIENTRY * PFNglPushMatrix) (void);
static PFNglPushMatrix pglPushMatrix;
typedef void (APIENTRY * PFNglPopMatrix) (void);
static PFNglPopMatrix pglPopMatrix;
typedef void (APIENTRY * PFNglLoadIdentity) (void);
static PFNglLoadIdentity pglLoadIdentity;
typedef void (APIENTRY * PFNglMultMatrixf) (const GLfloat *m);
static PFNglMultMatrixf pglMultMatrixf;
typedef void (APIENTRY * PFNglRotatef) (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
static PFNglRotatef pglRotatef;
typedef void (APIENTRY * PFNglScalef) (GLfloat x, GLfloat y, GLfloat z);
static PFNglScalef pglScalef;
typedef void (APIENTRY * PFNglTranslatef) (GLfloat x, GLfloat y, GLfloat z);
static PFNglTranslatef pglTranslatef;

/* Drawing Functions */
typedef void (APIENTRY * PFNglColor4ubv) (const GLubyte *v);
static PFNglColor4ubv pglColor4ubv;
typedef void (APIENTRY * PFNglVertexPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
static PFNglVertexPointer pglVertexPointer;
typedef void (APIENTRY * PFNglNormalPointer) (GLenum type, GLsizei stride, const GLvoid *pointer);
static PFNglNormalPointer pglNormalPointer;
typedef void (APIENTRY * PFNglTexCoordPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
static PFNglTexCoordPointer pglTexCoordPointer;
typedef void (APIENTRY * PFNglDrawArrays) (GLenum mode, GLint first, GLsizei count);
static PFNglDrawArrays pglDrawArrays;
typedef void (APIENTRY * PFNglDrawElements) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
static PFNglDrawElements pglDrawElements;
typedef void (APIENTRY * PFNglEnableClientState) (GLenum cap);
static PFNglEnableClientState pglEnableClientState;
typedef void (APIENTRY * PFNglDisableClientState) (GLenum cap);
static PFNglDisableClientState pglDisableClientState;

/* Lighting */
typedef void (APIENTRY * PFNglShadeModel) (GLenum mode);
static PFNglShadeModel pglShadeModel;
typedef void (APIENTRY * PFNglLightfv) (GLenum light, GLenum pname, GLfloat *params);
static PFNglLightfv pglLightfv;
typedef void (APIENTRY * PFNglLightModelfv) (GLenum pname, GLfloat *params);
static PFNglLightModelfv pglLightModelfv;
typedef void (APIENTRY * PFNglMaterialfv) (GLint face, GLenum pname, GLfloat *params);
static PFNglMaterialfv pglMaterialfv;
typedef void (APIENTRY * PFNglMateriali) (GLint face, GLenum pname, GLint param);
static PFNglMateriali pglMateriali;

/* Raster functions */
typedef void (APIENTRY * PFNglPixelStorei) (GLenum pname, GLint param);
static PFNglPixelStorei pglPixelStorei;
typedef void (APIENTRY  * PFNglReadPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
static PFNglReadPixels pglReadPixels;

/* Texture mapping */
typedef void (APIENTRY * PFNglTexEnvi) (GLenum target, GLenum pname, GLint param);
static PFNglTexEnvi pglTexEnvi;
typedef void (APIENTRY * PFNglTexParameteri) (GLenum target, GLenum pname, GLint param);
static PFNglTexParameteri pglTexParameteri;
typedef void (APIENTRY * PFNglTexImage1D) (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
static PFNglTexImage1D pglTexImage1D;
typedef void (APIENTRY * PFNglTexImage2D) (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
static PFNglTexImage2D pglTexImage2D;

typedef void (APIENTRY * PFNglTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
static PFNglTexSubImage2D pglTexSubImage2D;

/* 1.1 functions */
/* texture objects */ //GL_EXT_texture_object
typedef void (APIENTRY * PFNglGenTextures) (GLsizei n, const GLuint *textures);
static PFNglGenTextures pglGenTextures;
typedef void (APIENTRY * PFNglDeleteTextures) (GLsizei n, const GLuint *textures);
static PFNglDeleteTextures pglDeleteTextures;
typedef void (APIENTRY * PFNglBindTexture) (GLenum target, GLuint texture);
static PFNglBindTexture pglBindTexture;
/* texture mapping */ //GL_EXT_copy_texture
typedef void (APIENTRY * PFNglCopyTexImage2D) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
static PFNglCopyTexImage2D pglCopyTexImage2D;
typedef void (APIENTRY * PFNglCopyTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
static PFNglCopyTexSubImage2D pglCopyTexSubImage2D;

#ifdef USE_FBO_OGL
/* 3.0 functions for framebuffers and renderbuffers */
typedef void (APIENTRY * PFNglGenFramebuffers) (GLsizei n, GLuint *ids);
static PFNglGenFramebuffers pglGenFramebuffers;
typedef void (APIENTRY * PFNglBindFramebuffer) (GLenum target, GLuint framebuffer);
static PFNglBindFramebuffer pglBindFramebuffer;
typedef void (APIENTRY * PFNglDeleteFramebuffers) (GLsizei n, GLuint *ids);
static PFNglDeleteFramebuffers pglDeleteFramebuffers;
typedef void (APIENTRY * PFNglFramebufferTexture2D) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
static PFNglFramebufferTexture2D pglFramebufferTexture2D;
typedef GLenum (APIENTRY * PFNglCheckFramebufferStatus) (GLenum target);
static PFNglCheckFramebufferStatus pglCheckFramebufferStatus;
typedef void (APIENTRY * PFNglGenRenderbuffers) (GLsizei n, GLuint *renderbuffers);
static PFNglGenRenderbuffers pglGenRenderbuffers;
typedef void (APIENTRY * PFNglBindRenderbuffer) (GLenum target, GLuint renderbuffer);
static PFNglBindRenderbuffer pglBindRenderbuffer;
typedef void (APIENTRY * PFNglDeleteRenderbuffers) (GLsizei n, GLuint *renderbuffers);
static PFNglDeleteRenderbuffers pglDeleteRenderbuffers;
typedef void (APIENTRY * PFNglRenderbufferStorage) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
static PFNglRenderbufferStorage pglRenderbufferStorage;
typedef void (APIENTRY * PFNglFramebufferRenderbuffer) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLenum renderbuffer);
static PFNglFramebufferRenderbuffer pglFramebufferRenderbuffer;
#endif // USE_FBO_OGL

#endif //!STATIC_OPENGL

/* 1.2 functions for 3D textures */
typedef void (APIENTRY * PFNglTexImage3D) (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
static PFNglTexImage3D pglTexImage3D;

/* 1.3 functions for multitexturing */
typedef void (APIENTRY *PFNglActiveTexture) (GLenum);
static PFNglActiveTexture pglActiveTexture;
typedef void (APIENTRY *PFNglMultiTexCoord2f) (GLenum, GLfloat, GLfloat);
static PFNglMultiTexCoord2f pglMultiTexCoord2f;
typedef void (APIENTRY *PFNglMultiTexCoord2fv) (GLenum target, const GLfloat *v);
static PFNglMultiTexCoord2fv pglMultiTexCoord2fv;
typedef void (APIENTRY *PFNglClientActiveTexture) (GLenum);
static PFNglClientActiveTexture pglClientActiveTexture;

/* 1.5 functions for buffers */
typedef void (APIENTRY * PFNglGenBuffers) (GLsizei n, GLuint *buffers);
static PFNglGenBuffers pglGenBuffers;
typedef void (APIENTRY * PFNglBindBuffer) (GLenum target, GLuint buffer);
static PFNglBindBuffer pglBindBuffer;
typedef void (APIENTRY * PFNglBufferData) (GLenum target, GLsizei size, const GLvoid *data, GLenum usage);
static PFNglBufferData pglBufferData;
typedef void (APIENTRY * PFNglDeleteBuffers) (GLsizei n, const GLuint *buffers);
static PFNglDeleteBuffers pglDeleteBuffers;

// sky dome needs this
typedef void    (APIENTRY *PFNglColorPointer)       (GLint, GLenum, GLsizei, const GLvoid*);
static PFNglColorPointer pglColorPointer;

/* 1.2 Parms */
/* GL_CLAMP_TO_EDGE_EXT */
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_TEXTURE_MIN_LOD
#define GL_TEXTURE_MIN_LOD 0x813A
#endif
#ifndef GL_TEXTURE_MAX_LOD
#define GL_TEXTURE_MAX_LOD 0x813B
#endif

/* 1.3 GL_TEXTUREi */
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif
#ifndef GL_TEXTURE2
#define GL_TEXTURE2 0x84C2
#endif

boolean SetupGLfunc(void)
{
#ifndef STATIC_OPENGL
#define GETOPENGLFUNC(func, proc) \
	func = GetGLFunc(#proc); \
	if (!func) \
	{ \
		GL_MSG_Warning("failed to get OpenGL function: %s", #proc); \
	} \

	GETOPENGLFUNC(pglClearColor, glClearColor)

	GETOPENGLFUNC(pglClear, glClear)
	GETOPENGLFUNC(pglColorMask, glColorMask)
	GETOPENGLFUNC(pglAlphaFunc, glAlphaFunc)
	GETOPENGLFUNC(pglBlendFunc, glBlendFunc)
	GETOPENGLFUNC(pglCullFace, glCullFace)
	GETOPENGLFUNC(pglPolygonOffset, glPolygonOffset)
	GETOPENGLFUNC(pglScissor, glScissor)
	GETOPENGLFUNC(pglEnable, glEnable)
	GETOPENGLFUNC(pglDisable, glDisable)
	GETOPENGLFUNC(pglGetFloatv, glGetFloatv)
	GETOPENGLFUNC(pglGetIntegerv, glGetIntegerv)
	GETOPENGLFUNC(pglGetString, glGetString)

	GETOPENGLFUNC(pglClearDepth, glClearDepth)
	GETOPENGLFUNC(pglDepthFunc, glDepthFunc)
	GETOPENGLFUNC(pglDepthMask, glDepthMask)
	GETOPENGLFUNC(pglDepthRange, glDepthRange)

	GETOPENGLFUNC(pglMatrixMode, glMatrixMode)
	GETOPENGLFUNC(pglViewport, glViewport)
	GETOPENGLFUNC(pglPushMatrix, glPushMatrix)
	GETOPENGLFUNC(pglPopMatrix, glPopMatrix)
	GETOPENGLFUNC(pglLoadIdentity, glLoadIdentity)
	GETOPENGLFUNC(pglMultMatrixf, glMultMatrixf)
	GETOPENGLFUNC(pglRotatef, glRotatef)
	GETOPENGLFUNC(pglScalef, glScalef)
	GETOPENGLFUNC(pglTranslatef, glTranslatef)

	GETOPENGLFUNC(pglColor4ubv, glColor4ubv)

	GETOPENGLFUNC(pglVertexPointer, glVertexPointer)
	GETOPENGLFUNC(pglNormalPointer, glNormalPointer)
	GETOPENGLFUNC(pglTexCoordPointer, glTexCoordPointer)
	GETOPENGLFUNC(pglDrawArrays, glDrawArrays)
	GETOPENGLFUNC(pglDrawElements, glDrawElements)
	GETOPENGLFUNC(pglEnableClientState, glEnableClientState)
	GETOPENGLFUNC(pglDisableClientState, glDisableClientState)

	GETOPENGLFUNC(pglShadeModel, glShadeModel)
	GETOPENGLFUNC(pglLightfv, glLightfv)
	GETOPENGLFUNC(pglLightModelfv, glLightModelfv)
	GETOPENGLFUNC(pglMaterialfv, glMaterialfv)
	GETOPENGLFUNC(pglMateriali, glMateriali)

	GETOPENGLFUNC(pglPixelStorei, glPixelStorei)
	GETOPENGLFUNC(pglReadPixels, glReadPixels)

	GETOPENGLFUNC(pglTexEnvi, glTexEnvi)
	GETOPENGLFUNC(pglTexParameteri, glTexParameteri)
	GETOPENGLFUNC(pglTexImage1D, glTexImage1D)
	GETOPENGLFUNC(pglTexImage2D, glTexImage2D)
	GETOPENGLFUNC(pglTexSubImage2D, glTexSubImage2D)

	GETOPENGLFUNC(pglGenTextures, glGenTextures)
	GETOPENGLFUNC(pglDeleteTextures, glDeleteTextures)
	GETOPENGLFUNC(pglBindTexture, glBindTexture)

	GETOPENGLFUNC(pglCopyTexImage2D, glCopyTexImage2D)
	GETOPENGLFUNC(pglCopyTexSubImage2D, glCopyTexSubImage2D)

#undef GETOPENGLFUNC
#endif
	return true;
}

typedef void    (APIENTRY *PFNglStencilFuncSeparate)        (GLenum, GLenum, GLint, GLuint);
static PFNglStencilFuncSeparate pglStencilFuncSeparate;

typedef void    (APIENTRY *PFNglStencilOpSeparate)          (GLenum, GLenum, GLenum, GLenum);
static PFNglStencilOpSeparate pglStencilOpSeparate;

typedef GLuint 	(APIENTRY *PFNglCreateShader)		(GLenum);
typedef void 	(APIENTRY *PFNglShaderSource)		(GLuint, GLsizei, const GLchar**, GLint*);
typedef void 	(APIENTRY *PFNglCompileShader)		(GLuint);
typedef void 	(APIENTRY *PFNglGetShaderiv)		(GLuint, GLenum, GLint*);
typedef void 	(APIENTRY *PFNglGetShaderInfoLog)	(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void 	(APIENTRY *PFNglDeleteShader)		(GLuint);
typedef GLuint 	(APIENTRY *PFNglCreateProgram)		(void);
typedef void  	(APIENTRY *PFNglDeleteProgram)		(GLuint);
typedef void 	(APIENTRY *PFNglAttachShader)		(GLuint, GLuint);
typedef void 	(APIENTRY *PFNglLinkProgram)		(GLuint);
typedef void 	(APIENTRY *PFNglGetProgramiv)		(GLuint, GLenum, GLint*);
typedef void 	(APIENTRY *PFNglUseProgram)			(GLuint);
typedef void 	(APIENTRY *PFNglUniform1i)			(GLint, GLint);
typedef void 	(APIENTRY *PFNglUniform1f)			(GLint, GLfloat);
typedef void 	(APIENTRY *PFNglUniform2f)			(GLint, GLfloat, GLfloat);
typedef void 	(APIENTRY *PFNglUniform3f)			(GLint, GLfloat, GLfloat, GLfloat);
typedef void 	(APIENTRY *PFNglUniform4f)			(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void 	(APIENTRY *PFNglUniform1fv)			(GLint, GLsizei, const GLfloat*);
typedef void 	(APIENTRY *PFNglUniform2fv)			(GLint, GLsizei, const GLfloat*);
typedef void 	(APIENTRY *PFNglUniform3fv)			(GLint, GLsizei, const GLfloat*);
typedef GLint 	(APIENTRY *PFNglGetUniformLocation)	(GLuint, const GLchar*);

static PFNglCreateShader pglCreateShader;
static PFNglShaderSource pglShaderSource;
static PFNglCompileShader pglCompileShader;
static PFNglGetShaderiv pglGetShaderiv;
static PFNglGetShaderInfoLog pglGetShaderInfoLog;
static PFNglDeleteShader pglDeleteShader;
static PFNglCreateProgram pglCreateProgram;
static PFNglDeleteProgram pglDeleteProgram;
static PFNglAttachShader pglAttachShader;
static PFNglLinkProgram pglLinkProgram;
static PFNglGetProgramiv pglGetProgramiv;
static PFNglUseProgram pglUseProgram;
static PFNglUniform1i pglUniform1i;
static PFNglUniform1f pglUniform1f;
static PFNglUniform2f pglUniform2f;
static PFNglUniform3f pglUniform3f;
static PFNglUniform4f pglUniform4f;
static PFNglUniform1fv pglUniform1fv;
static PFNglUniform2fv pglUniform2fv;
static PFNglUniform3fv pglUniform3fv;
static PFNglGetUniformLocation pglGetUniformLocation;

// 18032019
static boolean gl_shadersenabled = false;
static INT32 gl_allowshaders = 0;

static INT32 gl_enable_screen_textures = 2;

static GLint gl_portal_stencil_level = 0;

static INT32 gl_portal_mode = HWD_PORTAL_NORMAL;

// 13062019
typedef enum
{
	// lighting
	gluniform_poly_color,
	gluniform_tint_color,
	gluniform_fade_color,
	gluniform_lighting,
	gluniform_fade_start,
	gluniform_fade_end,
	
	// palette rendering
	gluniform_palette_tex, // 1d texture containing a palette
	gluniform_palette_lookup_tex, // 3d texture containing the rgb->index lookup table
	gluniform_lighttable_tex, // 2d texture containing a light table

	// misc.
	gluniform_leveltime,
	
	gluniform_max,
} gluniform_t;

typedef struct gl_shader_s
{
	char *vertex_shader;
	char *fragment_shader;
	GLuint program;
	GLint uniforms[gluniform_max+1];
} gl_shader_t;

static gl_shader_t gl_shaders[HWR_MAXSHADERS];
static gl_shader_t gl_fallback_shader;

// 09102020
typedef struct gl_shaderstate_s
{
	gl_shader_t *current;
	GLuint type;
	GLuint program;
	boolean changed;
} gl_shaderstate_t;
static gl_shaderstate_t gl_shaderstate;

// Shader info
static float shader_leveltime = 0;

// Lactozilla: Shader functions
static boolean Shader_CompileProgram(gl_shader_t *shader, GLint i);
static void Shader_SetUniforms(FSurfaceInfo *Surface, GLRGBAFloat *poly, GLRGBAFloat *tint, GLRGBAFloat *fade);

static GLRGBAFloat shader_defaultcolor = {1.0f, 1.0f, 1.0f, 1.0f};


void SetupGLFunc4(void)
{
	/* 1.2 funcs */
	pglTexImage3D = GetGLFunc("glTexImage3D");
	/* 1.3 funcs */
	pglActiveTexture = GetGLFunc("glActiveTexture");
	pglMultiTexCoord2f = GetGLFunc("glMultiTexCoord2f");
	pglClientActiveTexture = GetGLFunc("glClientActiveTexture");
	pglMultiTexCoord2fv = GetGLFunc("glMultiTexCoord2fv");
	
	/* 1.5 funcs */
	pglGenBuffers = GetGLFunc("glGenBuffers");
	pglBindBuffer = GetGLFunc("glBindBuffer");
	pglBufferData = GetGLFunc("glBufferData");
	pglDeleteBuffers = GetGLFunc("glDeleteBuffers");
	pglColorPointer = GetGLFunc("glColorPointer");
	
	pglStencilFuncSeparate = GetGLFunc("glStencilFuncSeparate");
	pglStencilOpSeparate = GetGLFunc("glStencilOpSeparate");

	pglCreateShader = GetGLFunc("glCreateShader");
	pglShaderSource = GetGLFunc("glShaderSource");
	pglCompileShader = GetGLFunc("glCompileShader");
	pglGetShaderiv = GetGLFunc("glGetShaderiv");
	pglGetShaderInfoLog = GetGLFunc("glGetShaderInfoLog");
	pglDeleteShader = GetGLFunc("glDeleteShader");
	pglCreateProgram = GetGLFunc("glCreateProgram");
	pglDeleteProgram = GetGLFunc("glDeleteProgram");
	pglAttachShader = GetGLFunc("glAttachShader");
	pglLinkProgram = GetGLFunc("glLinkProgram");
	pglGetProgramiv = GetGLFunc("glGetProgramiv");
	pglUseProgram = GetGLFunc("glUseProgram");
	pglUniform1i = GetGLFunc("glUniform1i");
	pglUniform1f = GetGLFunc("glUniform1f");
	pglUniform2f = GetGLFunc("glUniform2f");
	pglUniform3f = GetGLFunc("glUniform3f");
	pglUniform4f = GetGLFunc("glUniform4f");
	pglUniform1fv = GetGLFunc("glUniform1fv");
	pglUniform2fv = GetGLFunc("glUniform2fv");
	pglUniform3fv = GetGLFunc("glUniform3fv");
	pglGetUniformLocation = GetGLFunc("glGetUniformLocation");

#ifdef USE_FBO_OGL
	if (GLFramebuffer_IsFuncAvailible())
	{
		pglGenFramebuffers = GetGLFunc("glGenFramebuffers");
		pglBindFramebuffer = GetGLFunc("glBindFramebuffer");
		pglDeleteFramebuffers = GetGLFunc("glDeleteFramebuffers");
		pglFramebufferTexture2D = GetGLFunc("glFramebufferTexture2D");
		pglCheckFramebufferStatus = GetGLFunc("glCheckFramebufferStatus");
		pglGenRenderbuffers = GetGLFunc("glGenRenderbuffers");
		pglBindRenderbuffer = GetGLFunc("glBindRenderbuffer");
		pglDeleteRenderbuffers = GetGLFunc("glDeleteRenderbuffers");
		pglRenderbufferStorage = GetGLFunc("glRenderbufferStorage");
		pglFramebufferRenderbuffer = GetGLFunc("glFramebufferRenderbuffer");
	}
#endif
}

#ifdef USE_FBO_OGL
static boolean GLFramebuffer_IsFuncAvailible(void)
{
	//this stuff needs atleast OGL 3.0
	if (majorGL < 3)
		return false;

	return((isExtAvailable("GL_ARB_framebuffer_no_attachments",gl_extensions)) && 
	(isExtAvailable("GL_ARB_framebuffer_object",gl_extensions)) && 
	(isExtAvailable("GL_ARB_framebuffer_sRGB",gl_extensions)));

	return false;
}
#endif

EXPORT boolean HWRAPI(InitShaders) (void)
{
	if (!pglUseProgram)
		return false;

	gl_fallback_shader.vertex_shader = Z_StrDup(GLSL_FALLBACK_VERTEX_SHADER);
	gl_fallback_shader.fragment_shader = Z_StrDup(GLSL_FALLBACK_FRAGMENT_SHADER);

	if (!Shader_CompileProgram(&gl_fallback_shader, -1))
	{
		GL_MSG_Error("Failed to compile the fallback shader program!\n");
		return false;
	}

	return true;
}

EXPORT void HWRAPI(LoadShader) (int slot, char *code, hwdshaderstage_t stage)
{
	gl_shader_t *shader;

	if (slot < 0 || slot >= HWR_MAXSHADERS)
		I_Error("LoadShader: Invalid slot %d", slot);

	shader = &gl_shaders[slot];

#define LOADSHADER(source) { \
	if (shader->source) \
		Z_Free(shader->source); \
	shader->source = code; \
	}

	if (stage == HWD_SHADERSTAGE_VERTEX)
		LOADSHADER(vertex_shader)
	else if (stage == HWD_SHADERSTAGE_FRAGMENT)
		LOADSHADER(fragment_shader)
	else
		I_Error("LoadShader: invalid shader stage");

#undef LOADSHADER
}

EXPORT boolean HWRAPI(CompileShader) (int slot)
{
	if (slot < 0 || slot >= HWR_MAXSHADERS)
		I_Error("CompileShader: Invalid slot %d", slot);

	if (Shader_CompileProgram(&gl_shaders[slot], slot))
	{
		return true;
	}
	else
	{
		gl_shaders[slot].program = 0;
		return false;
	}
}

//
// Shader info
// Those are given to the uniforms.
//

EXPORT void HWRAPI(SetShaderInfo) (hwdshaderinfo_t info, INT32 value)
{
	switch (info)
	{
		case HWD_SHADERINFO_LEVELTIME:
			shader_leveltime = value;
			break;
		default:
			break;
	}
}

EXPORT void HWRAPI(SetShader) (int slot)
{
	if (slot == SHADER_NONE)
	{
		UnSetShader();
		return;
	}
	if (gl_allowshaders)
	{
		gl_shader_t *next_shader = &gl_shaders[slot]; // the gl_shader_t we are going to switch to

		if (!next_shader->program)
			next_shader = &gl_fallback_shader; // unusable shader, use fallback instead

		// update gl_shaderstate if an actual shader switch is needed
		if (gl_shaderstate.current != next_shader)
		{
			gl_shaderstate.current = next_shader;
			gl_shaderstate.program = next_shader->program;
			gl_shaderstate.type = slot;
			gl_shaderstate.changed = true;
		}

		gl_shadersenabled = true;

		return;
	}

	gl_shadersenabled = false;
}

EXPORT void HWRAPI(UnSetShader) (void)
{
	if (gl_shadersenabled) // don't repeatedly call glUseProgram if not needed
	{
		gl_shaderstate.current = NULL;
		gl_shaderstate.type = 0;
		gl_shaderstate.program = 0;

		if (pglUseProgram)
			pglUseProgram(0);
	}

	gl_shadersenabled = false;
}


// -----------------+
// SetNoTexture     : Disable texture
// -----------------+
static void SetNoTexture(void)
{
	// Disable texture.
	if (tex_downloaded != NOTEXTURE_NUM)
	{
		if (NOTEXTURE_NUM == 0)
			pglGenTextures(1, &NOTEXTURE_NUM);
		pglBindTexture(GL_TEXTURE_2D, NOTEXTURE_NUM);
		tex_downloaded = NOTEXTURE_NUM;
	}
}

static void GLPerspective(GLfloat fovy, GLfloat aspect)
{
	GLfloat m[4][4] =
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f},
		{ 0.0f, 1.0f, 0.0f, 0.0f},
		{ 0.0f, 0.0f, 1.0f,-1.0f},
		{ 0.0f, 0.0f, 0.0f, 0.0f},
	};
	const GLfloat zNear = NEAR_CLIPPING_PLANE;
	const GLfloat zFar = FAR_CLIPPING_PLANE;
	const GLfloat radians = (GLfloat)(fovy / 2.0f * M_PIl / 180.0f);
	const GLfloat sine = (GLfloat)sin(radians);
	const GLfloat deltaZ = zFar - zNear;
	GLfloat cotangent;

	if ((fabsf((float)deltaZ) < 1.0E-36f) || fpclassify(sine) == FP_ZERO || fpclassify(aspect) == FP_ZERO)
	{
		return;
	}
	cotangent = cosf(radians) / sine;

	m[0][0] = cotangent / aspect;
	m[1][1] = cotangent;
	m[2][2] = -(zFar + zNear) / deltaZ;
	m[3][2] = -2.0f * zNear * zFar / deltaZ;

	pglMultMatrixf(&m[0][0]);
}


// -----------------+
// SetModelView     :
// -----------------+
void SetModelView(GLint w, GLint h)
{
	//GL_DBG_Printf("SetModelView(): %dx%d\n", (int)w, (int)h);

	pglViewport(0, 0, w, h);

	pglMatrixMode(GL_PROJECTION);
	pglLoadIdentity();

	pglMatrixMode(GL_MODELVIEW);
	pglLoadIdentity();

	GLPerspective(fov, ASPECT_RATIO);

	// added for new coronas' code (without depth buffer)
	pglGetIntegerv(GL_VIEWPORT, viewport);
	pglGetFloatv(GL_PROJECTION_MATRIX, projMatrix);
}

// -----------------+
// SetStates        : Set permanent states
// -----------------+
void SetStates(void)
{
	pglEnable(GL_TEXTURE_2D);      // two-dimensional texturing
	pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	pglEnable(GL_ALPHA_TEST);
	pglAlphaFunc(GL_NOTEQUAL, 0.0f);
	pglEnable(GL_BLEND);           // enable color blending

	pglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	pglEnable(GL_STENCIL_TEST);
	pglEnable(GL_DEPTH_TEST);    // check the depth buffer
	pglDepthMask(GL_TRUE);       // enable writing to depth buffer
	pglClearDepth(1.0f);
	pglDepthRange(0.0f, 1.0f);
	pglDepthFunc(GL_LEQUAL);

	// this set CurrentPolyFlags to the actual configuration
	CurrentPolyFlags = 0xffffffff;
	SetBlend(0);

	tex_downloaded = 0;
	SetNoTexture();

	pglPolygonOffset(-1.0f, -1.0f);

	// bp : when no t&l :)
	pglLoadIdentity();
	pglScalef(1.0f, 1.0f, -1.0f);
	pglGetFloatv(GL_MODELVIEW_MATRIX, modelMatrix); // added for new coronas' code (without depth buffer)
}

#ifdef USE_FBO_OGL
void GLFramebuffer_Generate(void)
{
	if (!GLFramebuffer_IsFuncAvailible())
		return;

	// Generate the framebuffer
	if (FramebufferObject == 0)
		pglGenFramebuffers(1, &FramebufferObject);

	if (pglCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
		GLFramebuffer_GenerateAttachments();
}

void GLFramebuffer_Delete(void)
{
	if (!GLFramebuffer_IsFuncAvailible())
		return;

	// Unbind the framebuffer
	pglBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (FramebufferObject)
		pglDeleteFramebuffers(1, &FramebufferObject);

	GLFramebuffer_DeleteAttachments();
	FramebufferObject = 0;
}

void GLFramebuffer_GenerateAttachments(void)
{
	if (!GLFramebuffer_IsFuncAvailible())
		return;

	// Bind the framebuffer
	pglBindFramebuffer(GL_FRAMEBUFFER, FramebufferObject);

	// Generate the framebuffer texture
	if (FramebufferTexture == 0)
	{
		pglGenTextures(1, &FramebufferTexture);
		pglBindTexture(GL_TEXTURE_2D, FramebufferTexture);
		pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screen_width, screen_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		pglBindTexture(GL_TEXTURE_2D, 0);

		// Attach the framebuffer texture to the framebuffer
		pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FramebufferTexture, 0);
	}

	// Generate the renderbuffer
	if (RenderbufferObject == 0)
	{
		pglGenRenderbuffers(1, &RenderbufferObject);

		pglBindRenderbuffer(GL_RENDERBUFFER, RenderbufferObject);
		pglRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, screen_width, screen_height);
		pglFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RenderbufferObject);

		// Clear the renderbuffer
		ClearBuffer(true, true, true, NULL);

		pglBindRenderbuffer(GL_RENDERBUFFER, 0);
	}

	// Unbind the framebuffer
	pglBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLFramebuffer_DeleteAttachments(void)
{
	if (!GLFramebuffer_IsFuncAvailible())
		return;

	// Unbind the framebuffer
	pglBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (FramebufferTexture)
		pglDeleteTextures(1, &FramebufferTexture);

	if (RenderbufferObject)
		pglDeleteRenderbuffers(1, &RenderbufferObject);

	FramebufferTexture = 0;
	RenderbufferObject = 0;
}

void GLFramebuffer_Enable(void)
{
	if (!GLFramebuffer_IsFuncAvailible())
		return;

	if (FramebufferObject == 0)
		GLFramebuffer_Generate();
	else if (FramebufferTexture == 0 || RenderbufferObject == 0)
		GLFramebuffer_GenerateAttachments();

	pglBindFramebuffer(GL_FRAMEBUFFER, FramebufferObject);
	pglBindRenderbuffer(GL_RENDERBUFFER, RenderbufferObject);
}

void GLFramebuffer_Disable(void)
{
	if (!GLFramebuffer_IsFuncAvailible())
		return;

	pglBindFramebuffer(GL_FRAMEBUFFER, 0);
	pglBindRenderbuffer(GL_RENDERBUFFER, 0);
}
#endif
// -----------------+
// DeleteTexture    : Deletes a texture from the GPU and frees its data
// -----------------+
EXPORT void HWRAPI(DeleteTexture) (GLMipmap_t *pTexInfo)
{
	FTextureInfo *head = TexCacheHead;

	if (!pTexInfo)
		return;
	else if (pTexInfo->downloaded)
		pglDeleteTextures(1, (GLuint *)&pTexInfo->downloaded);

	while (head)
	{
		if (head->downloaded == pTexInfo->downloaded)
		{
			if (head->next)
				head->next->prev = head->prev;
			else // no next -> tail is being deleted -> update TexCacheTail
				TexCacheTail = head->prev;
			if (head->prev)
				head->prev->next = head->next;
			else // no prev -> head is being deleted -> update TexCacheHead
				TexCacheHead = head->next;
			free(head);
			break;
		}

		head = head->next;
	}

	pTexInfo->downloaded = 0;
}


// -----------------+
// Flush            : flush OpenGL textures
//                  : Clear list of downloaded mipmaps
// -----------------+
void Flush(void)
{
	//GL_DBG_Printf ("HWR_Flush()\n");

	while (TexCacheHead)
	{
		FTextureInfo *pTexInfo = TexCacheHead;
		GLMipmap_t *texture = pTexInfo->texture;

		if (pTexInfo->downloaded)
		{
			pglDeleteTextures(1, (GLuint *)&pTexInfo->downloaded);
			pTexInfo->downloaded = 0;
		}

		if (texture)
			texture->downloaded = 0;

		TexCacheHead = pTexInfo->next;
		free(pTexInfo);
	}

	TexCacheTail = TexCacheHead = NULL; //Hurdler: well, TexCacheHead is already NULL
	tex_downloaded = 0;

	free(textureBuffer);
	textureBuffer = NULL;
	textureBufferSize = 0;
}


// -----------------+
// isExtAvailable   : Look if an OpenGL extension is available
// Returns          : true if extension available
// -----------------+
INT32 isExtAvailable(const char *extension, const GLubyte *start)
{
	GLubyte         *where, *terminator;

	if (!extension || !start) return 0;
	where = (GLubyte *) strchr(extension, ' ');
	if (where || *extension == '\0')
		return 0;

	for (;;)
	{
		where = (GLubyte *) strstr((const char *) start, extension);
		if (!where)
			break;
		terminator = where + strlen(extension);
		if (where == start || *(where - 1) == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return 1;
		start = terminator;
	}
	return 0;
}


// -----------------+
// Init             : Initialise the OpenGL interface API
// Returns          :
// -----------------+
EXPORT boolean HWRAPI(Init) (void)
{
	return LoadGL();
}


// -----------------+
// SetupGLInfo      : Retreive and store currently loaded OpenGL version
// -----------------+
EXPORT void HWRAPI(SetupGLInfo) (void)
{
	const GLubyte *versionGL = pglGetString(GL_VERSION);
	CONS_Printf("Loaded OpenGL version %s\n", (const char*)versionGL);
	sscanf((const char*)versionGL, "%d.%d", &majorGL, &minorGL);
}


// -----------------+
// ClearMipMapCache : Flush OpenGL textures from memory
// -----------------+
EXPORT void HWRAPI(ClearMipMapCache) (void)
{
	Flush();
}

// Writes screen texture tex into dst_data.
// Pixel format is 24-bit RGB. Row order is top to bottom.
// Dimensions are screen_width * screen_height.
EXPORT void HWRAPI(ReadScreenTexture) (int tex, UINT16 *dst_data)
{
	INT32 i;
	int dst_stride = screen_width * 3; // stride between rows of image data
	GLubyte*top = (GLvoid*)dst_data, *bottom = top + dst_stride * (screen_height - 1);
	GLubyte *row;
	row = malloc(dst_stride);
	if (!row) return;
	// at the time this function is called, generic2 can be found drawn on the framebuffer
	// if some other screen texture is needed, draw it to the framebuffer
	// and draw generic2 back after reading the framebuffer.
	// this hack is for some reason **much** faster than the simple solution of using glGetTexImage.
	if (tex != HWD_SCREENTEXTURE_GENERIC2)
		DrawScreenTexture(tex, NULL, 0);
	pglPixelStorei(GL_PACK_ALIGNMENT, 1);
	pglReadPixels(0, 0, screen_width, screen_height, GL_RGB, GL_UNSIGNED_BYTE, dst_data);
	if (tex != HWD_SCREENTEXTURE_GENERIC2)
		DrawScreenTexture(HWD_SCREENTEXTURE_GENERIC2, NULL, 0);
	// Flip image upside down.
	// In other words, convert OpenGL's "bottom->top" row order into "top->bottom".
	for(i = 0; i < screen_height/2; i++)
	{
		memcpy(row, top, dst_stride);
		memcpy(top, bottom, dst_stride);
		memcpy(bottom, row, dst_stride);
		top += dst_stride;
		bottom -= dst_stride;
	}
	free(row);
}

// -----------------+
// GClipRect        : Defines the 2D hardware clipping window
// -----------------+
EXPORT void HWRAPI(GClipRect) (INT32 minx, INT32 miny, INT32 maxx, INT32 maxy, float nearclip, float farclip)
{
	//GL_DBG_Printf("GClipRect(%d, %d, %d, %d)\n", minx, miny, maxx, maxy);

	pglViewport(minx, screen_height-maxy, maxx-minx, maxy-miny);
	NEAR_CLIPPING_PLANE = nearclip;
	FAR_CLIPPING_PLANE = farclip;

	//pglScissor(minx, screen_height-maxy, maxx-minx, maxy-miny);
	pglMatrixMode(GL_PROJECTION);
	pglLoadIdentity();
	GLPerspective(fov, ASPECT_RATIO);
	pglMatrixMode(GL_MODELVIEW);

	// added for new coronas' code (without depth buffer)
	pglGetIntegerv(GL_VIEWPORT, viewport);
	pglGetFloatv(GL_PROJECTION_MATRIX, projMatrix);
}


// -----------------+
// ClearBuffer      : Clear the color/alpha/depth buffer(s)
// -----------------+
EXPORT void HWRAPI(ClearBuffer) (FBOOLEAN ColorMask,
                                    FBOOLEAN DepthMask,
									FBOOLEAN StencilMask,
                                    FRGBAFloat * ClearColor)
{
	//GL_DBG_Printf("ClearBuffer(%d)\n", alpha);
	GLbitfield ClearMask = 0;

	if (ColorMask)
	{
		if (ClearColor)
			pglClearColor(ClearColor->red,
			              ClearColor->green,
			              ClearColor->blue,
			              ClearColor->alpha);
		ClearMask |= GL_COLOR_BUFFER_BIT;
	}
	if (DepthMask)
	{
		pglClearDepth(1.0f);     //Hurdler: all that are permanen states
		pglDepthRange(0.0f, 1.0f);
		pglDepthFunc(GL_LEQUAL);
		ClearMask |= GL_DEPTH_BUFFER_BIT;
	}

	SetBlend(DepthMask ? PF_Occlude | CurrentPolyFlags : CurrentPolyFlags&~PF_Occlude);

	if (StencilMask)
		ClearMask |= GL_STENCIL_BUFFER_BIT;// looks like sometimes stencil buffer needs clearing? had a problem with random black screens
	pglClear(ClearMask);
	pglEnableClientState(GL_VERTEX_ARRAY); // We always use this one
	pglEnableClientState(GL_TEXTURE_COORD_ARRAY); // And mostly this one, too
}


// -----------------+
// HWRAPI Draw2DLine: Render a 2D line
// -----------------+
EXPORT void HWRAPI(Draw2DLine) (F2DCoord * v1,
                                   F2DCoord * v2,
                                   RGBA_t Color)
{
	//GL_DBG_Printf("DrawLine(): %f %f, %f %f\n", v1->x, v1->y, v2->x, v2->y);
	GLfloat p[12];
	GLfloat dx, dy;
	GLfloat angle;

	// BP: we should reflect the new state in our variable
	//SetBlend(PF_Modulated|PF_NoTexture);

	pglDisable(GL_TEXTURE_2D);

	// This is the preferred, 'modern' way of rendering lines -- creating a polygon.
	if (fabsf(v2->x - v1->x) > FLT_EPSILON)
		angle = (float)atan((v2->y-v1->y)/(v2->x-v1->x));
	else
		angle = (float)N_PI_DEMI;
	dx = (float)sin(angle) / (float)screen_width;
	dy = (float)cos(angle) / (float)screen_height;

	p[0] = v1->x - dx;  p[1] = -(v1->y + dy); p[2] = 1;
	p[3] = v2->x - dx;  p[4] = -(v2->y + dy); p[5] = 1;
	p[6] = v2->x + dx;  p[7] = -(v2->y - dy); p[8] = 1;
	p[9] = v1->x + dx;  p[10] = -(v1->y - dy); p[11] = 1;

	pglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	pglColor4ubv((GLubyte*)&Color.s);
	pglVertexPointer(3, GL_FLOAT, 0, p);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	pglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	pglEnable(GL_TEXTURE_2D);
}

static void Clamp2D(GLenum pname)
{
	pglTexParameteri(GL_TEXTURE_2D, pname, GL_CLAMP); // fallback clamp
#ifdef GL_CLAMP_TO_EDGE
	pglTexParameteri(GL_TEXTURE_2D, pname, GL_CLAMP_TO_EDGE);
#endif
}


// -----------------+
// SetBlend         : Set render mode
// -----------------+
// PF_Masked - we could use an ALPHA_TEST of GL_EQUAL, and alpha ref of 0,
//             is it faster when pixels are discarded ?
EXPORT void HWRAPI(SetBlend) (FBITFIELD PolyFlags)
{
	FBITFIELD Xor;
	Xor = CurrentPolyFlags^PolyFlags;
	if (Xor & (PF_Blending|PF_RemoveYWrap|PF_ForceWrapX|PF_ForceWrapY|PF_Occlude|PF_NoTexture|PF_Modulated|PF_NoDepthTest|PF_Decal|PF_Invisible|PF_NoAlphaTest))
	{
		if (Xor&(PF_Blending)) // if blending mode must be changed
		{
			switch (PolyFlags & PF_Blending) {
				case PF_Translucent & PF_Blending:
					pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // alpha = level of transparency
					pglAlphaFunc(GL_NOTEQUAL, 0.0f);
					break;
				case PF_Masked & PF_Blending:
					// Hurdler: does that mean lighting is only made by alpha src?
					// it sounds ok, but not for polygonsmooth
					pglBlendFunc(GL_SRC_ALPHA, GL_ZERO);                // 0 alpha = holes in texture
					pglAlphaFunc(GL_GREATER, 0.5f);
					break;
				case PF_Additive & PF_Blending:
					pglBlendFunc(GL_SRC_ALPHA, GL_ONE);                 // src * alpha + dest
					pglAlphaFunc(GL_NOTEQUAL, 0.0f);
					break;
				case PF_Environment & PF_Blending:
					pglBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
					pglAlphaFunc(GL_NOTEQUAL, 0.0f);
					break;
				case PF_Substractive & PF_Blending:
					// good for shadow
					// not really but what else ?
					pglBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
					pglAlphaFunc(GL_NOTEQUAL, 0.0f);
					break;
				case PF_Fog & PF_Fog:
					// Sryder: Fog
					// multiplies input colour by input alpha, and destination colour by input colour, then adds them
					pglBlendFunc(GL_SRC_ALPHA, GL_SRC_COLOR);
					pglAlphaFunc(GL_ALWAYS, 0.0f); // Don't discard zero alpha fragments
					break;
				default : // must be 0, otherwise it's an error
					// No blending
					pglBlendFunc(GL_ONE, GL_ZERO);   // the same as no blending
					pglAlphaFunc(GL_GREATER, 0.5f);
					break;
			}
		}
		if (Xor & PF_NoAlphaTest)
		{
			if (PolyFlags & PF_NoAlphaTest)
				pglDisable(GL_ALPHA_TEST);
			else
				pglEnable(GL_ALPHA_TEST);      // discard 0 alpha pixels (holes in texture)
		}

		if (Xor & PF_Decal)
		{
			if (PolyFlags & PF_Decal)
				pglEnable(GL_POLYGON_OFFSET_FILL);
			else
				pglDisable(GL_POLYGON_OFFSET_FILL);
		}
		if (Xor&PF_NoDepthTest)
		{
			if (PolyFlags & PF_NoDepthTest)
				pglDepthFunc(GL_ALWAYS); //pglDisable(GL_DEPTH_TEST);
			else
				pglDepthFunc(GL_LEQUAL); //pglEnable(GL_DEPTH_TEST);
		}

		if (Xor&PF_RemoveYWrap)
		{
			if (PolyFlags & PF_RemoveYWrap)
				Clamp2D(GL_TEXTURE_WRAP_T);
		}

		if (Xor&PF_ForceWrapX)
		{
			if (PolyFlags & PF_ForceWrapX)
				pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		}

		if (Xor&PF_ForceWrapY)
		{
			if (PolyFlags & PF_ForceWrapY)
				pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}

		if (Xor&PF_Modulated)
		{
#if defined (__unix__) || defined (UNIXCOMMON)
			if (oglflags & GLF_NOTEXENV)
			{
				if (!(PolyFlags & PF_Modulated))
					pglColor4ubv(white);
			}
			else
#endif

			// mix texture colour with Surface->PolyColor
			if (PolyFlags & PF_Modulated)
				pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			// colour from texture is unchanged before blending
			else
				pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}

		if (Xor & PF_Occlude) // depth test but (no) depth write
		{
			if (PolyFlags&PF_Occlude)
				pglDepthMask(1);
			else
				pglDepthMask(0);
		}
		////Hurdler: not used if we don't define POLYSKY
		if (Xor & PF_Invisible)
		{
			if (PolyFlags&PF_Invisible)
				pglBlendFunc(GL_ZERO, GL_ONE);         // transparent blending
			else
			{   // big hack: (TODO: manage that better)
				// we test only for PF_Masked because PF_Invisible is only used
				// (for now) with it (yeah, that's crappy, sorry)
				if ((PolyFlags&PF_Blending)==PF_Masked)
					pglBlendFunc(GL_SRC_ALPHA, GL_ZERO);
			}
		}
		if (PolyFlags & PF_NoTexture)
			SetNoTexture();
	}
	CurrentPolyFlags = PolyFlags;
}

static void AllocTextureBuffer(GLMipmap_t *pTexInfo)
{
	size_t size = pTexInfo->width * pTexInfo->height;
	if (size > textureBufferSize)
	{
		textureBuffer = realloc(textureBuffer, size * sizeof(RGBA_t));
		if (textureBuffer == NULL)
			I_Error("AllocTextureBuffer: out of memory allocating %s bytes", sizeu1(size * sizeof(RGBA_t)));
		textureBufferSize = size;
	}
}

// -----------------+
// UpdateTexture    : Updates texture data.
// -----------------+
EXPORT void HWRAPI(UpdateTexture) (GLMipmap_t *pTexInfo)
{
	// Upload a texture
	GLuint num = pTexInfo->downloaded;
	boolean update = true;

	INT32 w = pTexInfo->width, h = pTexInfo->height;
	INT32 i, j;

	const GLubyte *pImgData = (const GLubyte *)pTexInfo->data;
	const GLvoid *ptex = NULL;
	RGBA_t *tex = NULL;

	// Generate a new texture name.
	if (!num)
	{
		pglGenTextures(1, &num);
		pTexInfo->downloaded = num;
		update = false;
	}

	//GL_DBG_Printf("UpdateTexture %d %x\n", (INT32)num, pImgData);

	if ((pTexInfo->format == GL_TEXFMT_P_8) || (pTexInfo->format == GL_TEXFMT_AP_88))
	{
		AllocTextureBuffer(pTexInfo);
		ptex = tex = textureBuffer;

		for (j = 0; j < h; j++)
		{
			for (i = 0; i < w; i++)
			{
				if ((*pImgData == HWR_PATCHES_CHROMAKEY_COLORINDEX) &&
					(pTexInfo->flags & TF_CHROMAKEYED))
				{
					tex[w*j+i].s.red   = 0;
					tex[w*j+i].s.green = 0;
					tex[w*j+i].s.blue  = 0;
					tex[w*j+i].s.alpha = 0;
					pTexInfo->flags |= TF_TRANSPARENT; // there is a hole in it
				}
				else
				{
					tex[w*j+i].s.red   = myPaletteData[*pImgData].s.red;
					tex[w*j+i].s.green = myPaletteData[*pImgData].s.green;
					tex[w*j+i].s.blue  = myPaletteData[*pImgData].s.blue;
					tex[w*j+i].s.alpha = myPaletteData[*pImgData].s.alpha;
				}

				pImgData++;

				if (pTexInfo->format == GL_TEXFMT_AP_88)
				{
					if (!(pTexInfo->flags & TF_CHROMAKEYED))
						tex[w*j+i].s.alpha = *pImgData;
					pImgData++;
				}
			}
		}
	}
	else if (pTexInfo->format == GL_TEXFMT_RGBA)
	{
		// Directly upload the texture data without any kind of conversion.
		ptex = pImgData;
	}
	else if (pTexInfo->format == GL_TEXFMT_ALPHA_INTENSITY_88)
	{
		AllocTextureBuffer(pTexInfo);
		ptex = tex = textureBuffer;

		for (j = 0; j < h; j++)
		{
			for (i = 0; i < w; i++)
			{
				tex[w*j+i].s.red   = *pImgData;
				tex[w*j+i].s.green = *pImgData;
				tex[w*j+i].s.blue  = *pImgData;
				pImgData++;
				tex[w*j+i].s.alpha = *pImgData;
				pImgData++;
			}
		}
	}
	else if (pTexInfo->format == GL_TEXFMT_ALPHA_8) // Used for fade masks
	{
		AllocTextureBuffer(pTexInfo);
		ptex = tex = textureBuffer;

		for (j = 0; j < h; j++)
		{
			for (i = 0; i < w; i++)
			{
				tex[w*j+i].s.red   = 255; // 255 because the fade mask is modulated with the screen texture, so alpha affects it while the colours don't
				tex[w*j+i].s.green = 255;
				tex[w*j+i].s.blue  = 255;
				tex[w*j+i].s.alpha = *pImgData;
				pImgData++;
			}
		}
	}
	else
		GL_MSG_Warning("UpdateTexture: bad format %d\n", pTexInfo->format);

	pglBindTexture(GL_TEXTURE_2D, num);
	tex_downloaded = num;

	// disable texture filtering on any texture that has holes so there's no dumb borders or blending issues
	if (pTexInfo->flags & TF_TRANSPARENT)
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	else
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
	}

	if (pTexInfo->format == GL_TEXFMT_ALPHA_INTENSITY_88)
	{
		//pglTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, ptex);
		if (MipMap)
		{
			pglTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
			pglTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, ptex);

			pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
			if (pTexInfo->flags & TF_TRANSPARENT)
				pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0); // No mippmaps on transparent stuff
			else
				pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 4);
			//pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_LINEAR_MIPMAP_LINEAR);
		}
		else
		{
			if (update)
				pglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, ptex);
			else
				pglTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, ptex);
		}
	}
	else if (pTexInfo->format == GL_TEXFMT_ALPHA_8)
	{
		//pglTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, ptex);
		if (MipMap)
		{
			pglTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
			pglTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, ptex);

			pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
			if (pTexInfo->flags & TF_TRANSPARENT)
				pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0); // No mippmaps on transparent stuff
			else
				pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 4);
			//pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_LINEAR_MIPMAP_LINEAR);
		}
		else
		{
			if (update)
				pglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, ptex);
			else
				pglTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, ptex);
		}
	}
	else
	{
		if (MipMap)
		{
			pglTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
			pglTexImage2D(GL_TEXTURE_2D, 0, textureformatGL, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, ptex);

			// Control the mipmap level of detail
			pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0); // the lower the number, the higer the detail
			if (pTexInfo->flags & TF_TRANSPARENT)
				pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0); // No mippmaps on transparent stuff
			else
				pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 5);
		}
		else
		{
			if (update)
				pglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, ptex);
			else
				pglTexImage2D(GL_TEXTURE_2D, 0, textureformatGL, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, ptex);
		}
	}

	if (pTexInfo->flags & TF_WRAPX)
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	else
		Clamp2D(GL_TEXTURE_WRAP_S);

	if (pTexInfo->flags & TF_WRAPY)
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	else
		Clamp2D(GL_TEXTURE_WRAP_T);

	if (maximumAnisotropy)
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropic_filter);
}


// -----------------+
// SetTexture       : The mipmap becomes the current texture source
// -----------------+
EXPORT void HWRAPI(SetTexture) (GLMipmap_t *pTexInfo)
{
	if (!pTexInfo)
	{
		SetNoTexture();
		return;
	}
	else if (pTexInfo->downloaded)
	{
		if (pTexInfo->downloaded != tex_downloaded)
		{
			pglBindTexture(GL_TEXTURE_2D, pTexInfo->downloaded);
			tex_downloaded = pTexInfo->downloaded;
		}
	}
	else
	{
		FTextureInfo *newTex = calloc(1, sizeof (*newTex));

		UpdateTexture(pTexInfo);

		newTex->texture = pTexInfo;
		newTex->downloaded = (UINT32)pTexInfo->downloaded;
		newTex->width = (UINT32)pTexInfo->width;
		newTex->height = (UINT32)pTexInfo->height;
		newTex->format = (UINT32)pTexInfo->format;

		// insertion at the tail
		if (TexCacheTail)
		{
			newTex->prev = TexCacheTail;
			TexCacheTail->next = newTex;
			TexCacheTail = newTex;
		}
		else // initialization of the linked list
			TexCacheTail = TexCacheHead = newTex;
	}
}

static void Shader_SetUniforms(FSurfaceInfo *Surface, GLRGBAFloat *poly, GLRGBAFloat *tint, GLRGBAFloat *fade)
{
	gl_shader_t *shader = gl_shaderstate.current;

	if (gl_shadersenabled && (shader != NULL) && pglUseProgram)
	{
		if (!shader->program)
		{
			pglUseProgram(0);
			return;
		}

		if (gl_shaderstate.changed)
		{
			pglUseProgram(shader->program);
			gl_shaderstate.changed = false;
		}

		// Color uniforms can be left NULL and will be set to white (1.0f, 1.0f, 1.0f, 1.0f)
		if (poly == NULL)
			poly = &shader_defaultcolor;
		if (tint == NULL)
			tint = &shader_defaultcolor;
		if (fade == NULL)
			fade = &shader_defaultcolor;

		#define UNIFORM_1(uniform, a, function) \
			if (uniform != -1) \
				function (uniform, a);

		#define UNIFORM_2(uniform, a, b, function) \
			if (uniform != -1) \
				function (uniform, a, b);

		#define UNIFORM_3(uniform, a, b, c, function) \
			if (uniform != -1) \
				function (uniform, a, b, c);

		#define UNIFORM_4(uniform, a, b, c, d, function) \
			if (uniform != -1) \
				function (uniform, a, b, c, d);

		// polygon
		UNIFORM_4(shader->uniforms[gluniform_poly_color], poly->red, poly->green, poly->blue, poly->alpha, pglUniform4f);
		UNIFORM_4(shader->uniforms[gluniform_tint_color], tint->red, tint->green, tint->blue, tint->alpha, pglUniform4f);
		UNIFORM_4(shader->uniforms[gluniform_fade_color], fade->red, fade->green, fade->blue, fade->alpha, pglUniform4f);

		if (Surface != NULL)
		{
			UNIFORM_1(shader->uniforms[gluniform_lighting], (GLfloat)Surface->LightInfo.light_level, pglUniform1f);
			UNIFORM_1(shader->uniforms[gluniform_fade_start], (GLfloat)Surface->LightInfo.fade_start, pglUniform1f);
			UNIFORM_1(shader->uniforms[gluniform_fade_end], (GLfloat)Surface->LightInfo.fade_end, pglUniform1f);
		}

		UNIFORM_1(shader->uniforms[gluniform_leveltime], ((float)shader_leveltime) / TICRATE, pglUniform1f);

		#undef UNIFORM_1
		#undef UNIFORM_2
		#undef UNIFORM_3
		#undef UNIFORM_4
	}
}

static boolean Shader_CompileProgram(gl_shader_t *shader, GLint i)
{
	GLuint gl_vertShader = 0;
	GLuint gl_fragShader = 0;
	GLint result;
	const GLchar *vert_shader = shader->vertex_shader;
	const GLchar *frag_shader = shader->fragment_shader;

	if (shader->program)
		pglDeleteProgram(shader->program);

	if (!vert_shader && !frag_shader)
	{
		GL_MSG_Error("Shader_CompileProgram: Missing shaders for shader program %s\n", HWR_GetShaderName(i));
		return false;
	}

	if (vert_shader)
	{
		//
		// Load and compile vertex shader
		//
		gl_vertShader = pglCreateShader(GL_VERTEX_SHADER);
		if (!gl_vertShader)
		{
			GL_MSG_Error("Shader_CompileProgram: Error creating vertex shader %s\n", HWR_GetShaderName(i));
			return false;
		}

		pglShaderSource(gl_vertShader, 1, &vert_shader, NULL);
		pglCompileShader(gl_vertShader);

		// check for compile errors
		pglGetShaderiv(gl_vertShader, GL_COMPILE_STATUS, &result);
		if (result == GL_FALSE)
		{
			GL_MSG_Error("Error compiling vertex shader", gl_vertShader, i);
			pglDeleteShader(gl_vertShader);
			return false;
		}
	}

	if (frag_shader)
	{
		//
		// Load and compile fragment shader
		//
		gl_fragShader = pglCreateShader(GL_FRAGMENT_SHADER);
		if (!gl_fragShader)
		{
			GL_MSG_Error("Shader_CompileProgram: Error creating fragment shader %s\n", HWR_GetShaderName(i));
			pglDeleteShader(gl_vertShader);
			pglDeleteShader(gl_fragShader);
			return false;
		}

		pglShaderSource(gl_fragShader, 1, &frag_shader, NULL);
		pglCompileShader(gl_fragShader);

		// check for compile errors
		pglGetShaderiv(gl_fragShader, GL_COMPILE_STATUS, &result);
		if (result == GL_FALSE)
		{
			GL_MSG_Error("Error compiling fragment shader", gl_fragShader, i);
			pglDeleteShader(gl_vertShader);
			pglDeleteShader(gl_fragShader);
			return false;
		}
	}

	shader->program = pglCreateProgram();
	if (vert_shader)
		pglAttachShader(shader->program, gl_vertShader);
	if (frag_shader)
		pglAttachShader(shader->program, gl_fragShader);
	pglLinkProgram(shader->program);

	// check link status
	pglGetProgramiv(shader->program, GL_LINK_STATUS, &result);

	// delete the shader objects
	if (vert_shader)
		pglDeleteShader(gl_vertShader);
	if (frag_shader)
		pglDeleteShader(gl_fragShader);
	
	// couldn't link?
	if (result != GL_TRUE)
	{
		GL_MSG_Error("Shader_CompileProgram: Error linking shader program %s\n", HWR_GetShaderName(i));
		pglDeleteProgram(shader->program);
		return false;
	}

	// 13062019
#define GETUNI(uniform) pglGetUniformLocation(shader->program, uniform);

	// lighting
	shader->uniforms[gluniform_poly_color] = GETUNI("poly_color");
	shader->uniforms[gluniform_tint_color] = GETUNI("tint_color");
	shader->uniforms[gluniform_fade_color] = GETUNI("fade_color");
	shader->uniforms[gluniform_lighting] = GETUNI("lighting");
	shader->uniforms[gluniform_fade_start] = GETUNI("fade_start");
	shader->uniforms[gluniform_fade_end] = GETUNI("fade_end");

	// palette rendering
	shader->uniforms[gluniform_palette_tex] = GETUNI("palette_tex");
	shader->uniforms[gluniform_palette_lookup_tex] = GETUNI("palette_lookup_tex");
	shader->uniforms[gluniform_lighttable_tex] = GETUNI("lighttable_tex");

	// misc.
	shader->uniforms[gluniform_leveltime] = GETUNI("leveltime");
#undef GETUNI

	// set permanent uniform values
#define UNIFORM_1(uniform, a, function) \
	if (uniform != -1) \
		function (uniform, a);

	pglUseProgram(shader->program);

	// texture unit numbers for the samplers used for palette rendering
	UNIFORM_1(shader->uniforms[gluniform_palette_tex], 2, pglUniform1i);
	UNIFORM_1(shader->uniforms[gluniform_palette_lookup_tex], 1, pglUniform1i);
	UNIFORM_1(shader->uniforms[gluniform_lighttable_tex], 2, pglUniform1i);

	// restore gl shader state
	pglUseProgram(gl_shaderstate.program);
#undef UNIFORM_1

	return true;
}

// code that is common between DrawPolygon and DrawIndexedTriangles
// DrawScreenTexture also can use this function for fancier screen texture drawing
// the corona thing is there too, i have no idea if that stuff works with DrawIndexedTriangles and batching
static void PreparePolygon(FSurfaceInfo *pSurf, FBITFIELD PolyFlags)
{
	static GLRGBAFloat poly = {0,0,0,0};
	static GLRGBAFloat tint = {0,0,0,0};
	static GLRGBAFloat fade = {0,0,0,0};

	SetBlend(PolyFlags);    //TODO: inline (#pragma..)

	if (pSurf)
	{
		// If modulated, mix the surface colour to the texture
		if (CurrentPolyFlags & PF_Modulated)
			pglColor4ubv((GLubyte*)&pSurf->PolyColor.s);

		// If the surface is either modulated or colormapped, or both
		if (CurrentPolyFlags & (PF_Modulated | PF_ColorMapped))
		{
			poly.red   = byte2float[pSurf->PolyColor.s.red];
			poly.green = byte2float[pSurf->PolyColor.s.green];
			poly.blue  = byte2float[pSurf->PolyColor.s.blue];
			poly.alpha = byte2float[pSurf->PolyColor.s.alpha];
		}

		// Only if the surface is colormapped
		if (CurrentPolyFlags & PF_ColorMapped)
		{
			tint.red   = byte2float[pSurf->TintColor.s.red];
			tint.green = byte2float[pSurf->TintColor.s.green];
			tint.blue  = byte2float[pSurf->TintColor.s.blue];
			tint.alpha = byte2float[pSurf->TintColor.s.alpha];

			fade.red   = byte2float[pSurf->FadeColor.s.red];
			fade.green = byte2float[pSurf->FadeColor.s.green];
			fade.blue  = byte2float[pSurf->FadeColor.s.blue];
			fade.alpha = byte2float[pSurf->FadeColor.s.alpha];

			if (pSurf->LightTableId && pSurf->LightTableId != lt_downloaded)
			{
				pglActiveTexture(GL_TEXTURE2);
				pglBindTexture(GL_TEXTURE_2D, pSurf->LightTableId);
				pglActiveTexture(GL_TEXTURE0);
				lt_downloaded = pSurf->LightTableId;
			}
		}
	}

	Shader_SetUniforms(pSurf, &poly, &tint, &fade);
}

// -----------------+
// DrawPolygon      : Render a polygon, set the texture, set render mode
// -----------------+
EXPORT void HWRAPI(DrawPolygon) (FSurfaceInfo *pSurf, FOutVector *pOutVerts, FUINT iNumPts, FBITFIELD PolyFlags)
{
	PreparePolygon(pSurf, PolyFlags);

	pglVertexPointer(3, GL_FLOAT, sizeof(FOutVector), &pOutVerts[0].x);
	pglTexCoordPointer(2, GL_FLOAT, sizeof(FOutVector), &pOutVerts[0].s);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, iNumPts);

	if (PolyFlags & PF_RemoveYWrap)
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	if (PolyFlags & PF_ForceWrapX)
		Clamp2D(GL_TEXTURE_WRAP_S);

	if (PolyFlags & PF_ForceWrapY)
		Clamp2D(GL_TEXTURE_WRAP_T);
}

EXPORT void HWRAPI(DrawIndexedTriangles) (FSurfaceInfo *pSurf, FOutVector *pOutVerts, FUINT iNumPts, FBITFIELD PolyFlags, unsigned int *IndexArray)
{
	PreparePolygon(pSurf, PolyFlags);

	pglVertexPointer(3, GL_FLOAT, sizeof(FOutVector), &pOutVerts[0].x);
	pglTexCoordPointer(2, GL_FLOAT, sizeof(FOutVector), &pOutVerts[0].s);
	pglDrawElements(GL_TRIANGLES, iNumPts, GL_UNSIGNED_INT, IndexArray);

	// the DrawPolygon variant of this has some code about polyflags and wrapping here but havent noticed any problems from omitting it?
}

// Sky dome code, taken/backported from SRB2

typedef struct vbo_vertex_s
{
	float x, y, z;
	float u, v;
	unsigned char r, g, b, a;
} vbo_vertex_t;

typedef struct
{
	int mode;
	int vertexcount;
	int vertexindex;
	int use_texture;
} GLSkyLoopDef;

typedef struct
{
	unsigned int id;
	int rows, columns;
	int loopcount;
	GLSkyLoopDef *loops;
	vbo_vertex_t *data;
} GLSkyVBO;

static const boolean gl_ext_arb_vertex_buffer_object = true;

#define NULL_VBO_VERTEX ((vbo_vertex_t*)NULL)
#define sky_vbo_x (gl_ext_arb_vertex_buffer_object ? &NULL_VBO_VERTEX->x : &vbo->data[0].x)
#define sky_vbo_u (gl_ext_arb_vertex_buffer_object ? &NULL_VBO_VERTEX->u : &vbo->data[0].u)
#define sky_vbo_r (gl_ext_arb_vertex_buffer_object ? &NULL_VBO_VERTEX->r : &vbo->data[0].r)

// The texture offset to be applied to the texture coordinates in SkyVertex().
static int rows, columns;
static signed char yflip;
static int texw, texh;
static boolean foglayer;
static float delta = 0.0f;

static int gl_sky_detail = 16;

static INT32 lasttex = -1;

#define MAP_COEFF 128.0f

static void SkyVertex(vbo_vertex_t *vbo, int r, int c)
{
	const float radians = (float)(M_PIl / 180.0f);
	const float scale = 10000.0f;
	const float maxSideAngle = 60.0f;

	float topAngle = (c / (float)columns * 360.0f);
	float sideAngle = (maxSideAngle * (rows - r) / rows);
	float height = (float)(sin(sideAngle * radians));
	float realRadius = (float)(scale * cos(sideAngle * radians));
	float x = (float)(realRadius * cos(topAngle * radians));
	float y = (!yflip) ? scale * height : -scale * height;
	float z = (float)(realRadius * sin(topAngle * radians));
	float timesRepeat = (4 * (256.0f / texw));
	if (fpclassify(timesRepeat) == FP_ZERO)
		timesRepeat = 1.0f;

	if (!foglayer)
	{
		vbo->r = 255;
		vbo->g = 255;
		vbo->b = 255;
		vbo->a = (r == 0 ? 0 : 255);

		// And the texture coordinates.
		//vbo->u = (-timesRepeat * c / (float)columns);
		vbo->u = (timesRepeat * c / (float)columns);// TEST
		if (!yflip)	// Flipped Y is for the lower hemisphere.
			vbo->v = (r / (float)rows) + 0.5f;
		else
			vbo->v = 1.0f + ((rows - r) / (float)rows) + 0.5f;
	}

	if (r != 4)
	{
		y += 300.0f;
	}

	// And finally the vertex.
	vbo->x = x;
	vbo->y = y + delta;
	vbo->z = z;
}

static GLSkyVBO sky_vbo;

static void gld_BuildSky(int row_count, int col_count)
{
	int c, r;
	vbo_vertex_t *vertex_p;
	int vertex_count = 2 * row_count * (col_count * 2 + 2) + col_count * 2;

	GLSkyVBO *vbo = &sky_vbo;

	if ((vbo->columns != col_count) || (vbo->rows != row_count))
	{
		free(vbo->loops);
		free(vbo->data);
		memset(vbo, 0, sizeof(&vbo));
	}

	if (!vbo->data)
	{
		memset(vbo, 0, sizeof(&vbo));
		vbo->loops = malloc((row_count * 2 + 2) * sizeof(vbo->loops[0]));
		// create vertex array
		vbo->data = malloc(vertex_count * sizeof(vbo->data[0]));
	}

	vbo->columns = col_count;
	vbo->rows = row_count;

	vertex_p = &vbo->data[0];
	vbo->loopcount = 0;

	for (yflip = 0; yflip < 2; yflip++)
	{
		vbo->loops[vbo->loopcount].mode = GL_TRIANGLE_FAN;
		vbo->loops[vbo->loopcount].vertexindex = vertex_p - &vbo->data[0];
		vbo->loops[vbo->loopcount].vertexcount = col_count;
		vbo->loops[vbo->loopcount].use_texture = false;
		vbo->loopcount++;

		delta = 0.0f;
		foglayer = true;
		for (c = 0; c < col_count; c++)
		{
			SkyVertex(vertex_p, 1, c);
			vertex_p->r = 255;
			vertex_p->g = 255;
			vertex_p->b = 255;
			vertex_p->a = 255;
			vertex_p++;
		}
		foglayer = false;

		delta = (yflip ? 5.0f : -5.0f) / MAP_COEFF;

		for (r = 0; r < row_count; r++)
		{
			vbo->loops[vbo->loopcount].mode = GL_TRIANGLE_STRIP;
			vbo->loops[vbo->loopcount].vertexindex = vertex_p - &vbo->data[0];
			vbo->loops[vbo->loopcount].vertexcount = 2 * col_count + 2;
			vbo->loops[vbo->loopcount].use_texture = true;
			vbo->loopcount++;

			for (c = 0; c <= col_count; c++)
			{
				SkyVertex(vertex_p++, r + (yflip ? 1 : 0), (c ? c : 0));
				SkyVertex(vertex_p++, r + (yflip ? 0 : 1), (c ? c : 0));
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

static void RenderDome(INT32 skytexnum)
{
	int i, j;
	int vbosize;
	GLSkyVBO *vbo = &sky_vbo;

	rows = 4;
	columns = 4 * gl_sky_detail;

	vbosize = 2 * rows * (columns * 2 + 2) + columns * 2;

	// Build the sky dome! Yes!
	if (lasttex != skytexnum)
	{
		// delete VBO when already exists
		if (gl_ext_arb_vertex_buffer_object)
		{
			if (vbo->id)
				pglDeleteBuffers(1, &vbo->id);
		}

		lasttex = skytexnum;
		gld_BuildSky(rows, columns);

		if (gl_ext_arb_vertex_buffer_object)
		{
			// generate a new VBO and get the associated ID
			pglGenBuffers(1, &vbo->id);

			// bind VBO in order to use
			pglBindBuffer(GL_ARRAY_BUFFER, vbo->id);

			// upload data to VBO
			pglBufferData(GL_ARRAY_BUFFER, vbosize * sizeof(vbo->data[0]), vbo->data, GL_STATIC_DRAW);
		}
	}

	// bind VBO in order to use
	if (gl_ext_arb_vertex_buffer_object)
		pglBindBuffer(GL_ARRAY_BUFFER, vbo->id);

	// activate and specify pointers to arrays
	pglVertexPointer(3, GL_FLOAT, sizeof(vbo->data[0]), sky_vbo_x);
	pglTexCoordPointer(2, GL_FLOAT, sizeof(vbo->data[0]), sky_vbo_u);
	pglColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vbo->data[0]), sky_vbo_r);

	// activate color arrays
	pglEnableClientState(GL_COLOR_ARRAY);

	// set transforms
	pglScalef(1.0f, (float)texh / 230.0f, 1.0f);
	pglRotatef(270.0f, 0.0f, 1.0f, 0.0f);

	for (j = 0; j < 2; j++)
	{
		for (i = 0; i < vbo->loopcount; i++)
		{
			GLSkyLoopDef *loop = &vbo->loops[i];

			if (j == 0 ? loop->use_texture : !loop->use_texture)
				continue;

			pglDrawArrays(loop->mode, loop->vertexindex, loop->vertexcount);
		}
	}

	pglScalef(1.0f, 1.0f, 1.0f);
	pglColor4ubv(white);

	// bind with 0, so, switch back to normal pointer operation
	if (gl_ext_arb_vertex_buffer_object)
		pglBindBuffer(GL_ARRAY_BUFFER, 0);

	// deactivate color array
	pglDisableClientState(GL_COLOR_ARRAY);
}

EXPORT void HWRAPI(RenderSkyDome) (INT32 tex, INT32 texture_width, INT32 texture_height, FTransform transform)
{
	SetBlend(PF_Translucent|PF_NoDepthTest|PF_Modulated);
	SetTransform(&transform);
	texw = texture_width;
	texh = texture_height;
	RenderDome(tex);
	SetBlend(0);
}

// ==========================================================================
//
// ==========================================================================
EXPORT void HWRAPI(SetSpecialState) (hwdspecialstate_t IdState, INT32 Value)
{
	switch (IdState)
	{
		case HWD_SET_SHADERS:
			gl_allowshaders = Value;
			break;
#ifdef USE_FBO_OGL
		case HWD_SET_FRAMEBUFFER:
			FrameBufferEnabled = Value ? GL_TRUE : GL_FALSE;
			supportFBO = GLFramebuffer_IsFuncAvailible();

			if (!supportFBO)
			{
				FrameBufferEnabled = GL_FALSE;
				CV_Set(&cv_grframebuffer, "Off");
			}
			break;
#endif
		case HWD_SET_TEXTUREFILTERMODE:
			switch (Value)
			{
				case HWD_SET_TEXTUREFILTER_TRILINEAR:
					min_filter = GL_LINEAR_MIPMAP_LINEAR;
					mag_filter = GL_LINEAR;
					MipMap = GL_TRUE;
					break;
				case HWD_SET_TEXTUREFILTER_BILINEAR:
					min_filter = mag_filter = GL_LINEAR;
					MipMap = GL_FALSE;
					break;
				case HWD_SET_TEXTUREFILTER_POINTSAMPLED:
					min_filter = mag_filter = GL_NEAREST;
					MipMap = GL_FALSE;
					break;
				case HWD_SET_TEXTUREFILTER_MIXED1:
					min_filter = GL_NEAREST;
					mag_filter = GL_LINEAR;
					MipMap = GL_FALSE;
					break;
				case HWD_SET_TEXTUREFILTER_MIXED2:
					min_filter = GL_LINEAR;
					mag_filter = GL_NEAREST;
					MipMap = GL_FALSE;
					break;
				case HWD_SET_TEXTUREFILTER_MIXED3:
					min_filter = GL_LINEAR_MIPMAP_LINEAR;
					mag_filter = GL_NEAREST;
					MipMap = GL_TRUE;
					break;
				default:
					mag_filter = GL_LINEAR;
					min_filter = GL_NEAREST;
			}
			if (!supportMipMap)
			{
				MipMap = GL_FALSE;
				min_filter = GL_LINEAR;
			}

			Flush(); //??? if we want to change filter mode by texture, remove this
			break;
			
		case HWD_SET_MSAA:
			if (Value)
			{
				pglEnable(GL_MULTISAMPLE);
				if (Value == 2)
					pglEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
			}
			break;

		case HWD_SET_TEXTUREANISOTROPICMODE:
			anisotropic_filter = min(Value,maximumAnisotropy);
			if (maximumAnisotropy)
				Flush(); //??? if we want to change filter mode by texture, remove this
			break;

		case HWD_SET_SCREEN_TEXTURES:
			gl_enable_screen_textures = Value;
			break;

		case HWD_SET_PORTAL_MODE:
			gl_portal_mode = Value;
			switch (Value)
			{
				case HWD_PORTAL_NORMAL:
					// regular drawing, only to current level of stencil
					pglEnable(GL_TEXTURE_2D);
					pglEnable(GL_DEPTH_TEST);
					pglEnable(GL_BLEND);
					pglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
					pglStencilFuncSeparate(GL_FRONT_AND_BACK, GL_EQUAL, gl_portal_stencil_level, 0xFF);
					pglStencilOpSeparate(GL_FRONT_AND_BACK, GL_KEEP, GL_KEEP, GL_KEEP);
					pglDepthMask(GL_TRUE);
					break;
				case HWD_PORTAL_STENCIL_SEGS:
					// draw only to stencil, only to current level of stencil, incrementing when drawing
					pglDisable(GL_TEXTURE_2D);
					pglDisable(GL_DEPTH_TEST);
					pglDisable(GL_BLEND);
					pglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
					pglStencilFuncSeparate(GL_FRONT_AND_BACK, GL_EQUAL, gl_portal_stencil_level, 0xFF);
					pglStencilOpSeparate(GL_FRONT_AND_BACK, GL_KEEP, GL_INCR, GL_INCR);// dont think z-buffer is used at this point
					pglDepthMask(GL_FALSE);
					break;
				case HWD_PORTAL_STENCIL_REVERSE_SEGS:
					// draw only to stencil, only to current level of stencil, incrementing when drawing
					pglDisable(GL_TEXTURE_2D);
					pglDisable(GL_DEPTH_TEST);
					pglDisable(GL_BLEND);
					pglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
					pglStencilFuncSeparate(GL_FRONT_AND_BACK, GL_EQUAL, gl_portal_stencil_level+1, 0xFF);
					pglStencilOpSeparate(GL_FRONT_AND_BACK, GL_KEEP, GL_DECR, GL_DECR);// dont think z-buffer is used at this point
					pglDepthMask(GL_FALSE);
					break;
				case HWD_PORTAL_DEPTH_SEGS:
					// draw only to depth, only to current level of stencil
					pglDisable(GL_TEXTURE_2D);
					pglEnable(GL_DEPTH_TEST);
					pglDisable(GL_BLEND);
					pglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
					pglStencilFuncSeparate(GL_FRONT_AND_BACK, GL_EQUAL, gl_portal_stencil_level, 0xFF);
					pglStencilOpSeparate(GL_FRONT_AND_BACK, GL_KEEP, GL_KEEP, GL_KEEP);
					pglDepthMask(GL_TRUE);
					break;
				case HWD_PORTAL_SKY_STENCIL_SEGS:
					// draw only to stencil, only to current level of stencil, incrementing when drawing
					// same as HWD_PORTAL_STENCIL_SEGS except depth testing is enabled
					// this is used to mark all visible skywall pixels to the stencil buffer
					pglDisable(GL_TEXTURE_2D);
					pglEnable(GL_DEPTH_TEST);
					pglDisable(GL_BLEND);
					pglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
					pglStencilFuncSeparate(GL_FRONT_AND_BACK, GL_EQUAL, gl_portal_stencil_level, 0xFF);
					pglStencilOpSeparate(GL_FRONT_AND_BACK, GL_KEEP, GL_KEEP, GL_INCR);
					pglDepthMask(GL_TRUE);// not sure about this and does it matter
					break;
				default:
					I_Error("Bad value in HWD_SET_PORTAL_MODE");
			}
			break;

		case HWD_SET_STENCIL_LEVEL:
			gl_portal_stencil_level = Value;
			break;

		default:
			break;
	}
}

static float *vertBuffer = NULL;
static float *normBuffer = NULL;
static size_t lerpBufferSize = 0;
static short *vertTinyBuffer = NULL;
static char *normTinyBuffer = NULL;
static size_t lerpTinyBufferSize = 0;

// Static temporary buffer for doing frame interpolation
// 'size' is the vertex size
static void AllocLerpBuffer(size_t size)
{
	if (lerpBufferSize >= size)
		return;

	if (vertBuffer != NULL)
		free(vertBuffer);

	if (normBuffer != NULL)
		free(normBuffer);

	lerpBufferSize = size;
	vertBuffer = malloc(lerpBufferSize);
	normBuffer = malloc(lerpBufferSize);
}

// Static temporary buffer for doing frame interpolation
// 'size' is the vertex size
static void AllocLerpTinyBuffer(size_t size)
{
	if (lerpTinyBufferSize >= size)
		return;

	if (vertTinyBuffer != NULL)
		free(vertTinyBuffer);

	if (normTinyBuffer != NULL)
		free(normTinyBuffer);

	lerpTinyBufferSize = size;
	vertTinyBuffer = malloc(lerpTinyBufferSize);
	normTinyBuffer = malloc(lerpTinyBufferSize / 2);
}

#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif

static void CreateModelVBO(mesh_t *mesh, mdlframe_t *frame)
{
	int bufferSize = sizeof(vbo64_t)*mesh->numTriangles * 3;
	vbo64_t *buffer = (vbo64_t*)malloc(bufferSize);
	vbo64_t *bufPtr = buffer;

	float *vertPtr = frame->vertices;
	float *normPtr = frame->normals;
	float *tanPtr = frame->tangents;
	float *uvPtr = mesh->uvs;
	float *lightPtr = mesh->lightuvs;
	char *colorPtr = frame->colors;

	int i;
	for (i = 0; i < mesh->numTriangles * 3; i++)
	{
		bufPtr->x = *vertPtr++;
		bufPtr->y = *vertPtr++;
		bufPtr->z = *vertPtr++;

		bufPtr->nx = *normPtr++;
		bufPtr->ny = *normPtr++;
		bufPtr->nz = *normPtr++;

		bufPtr->s0 = *uvPtr++;
		bufPtr->t0 = *uvPtr++;

		if (tanPtr != NULL)
		{
			bufPtr->tan0 = *tanPtr++;
			bufPtr->tan1 = *tanPtr++;
			bufPtr->tan2 = *tanPtr++;
		}

		if (lightPtr != NULL)
		{
			bufPtr->s1 = *lightPtr++;
			bufPtr->t1 = *lightPtr++;
		}

		if (colorPtr)
		{
			bufPtr->r = *colorPtr++;
			bufPtr->g = *colorPtr++;
			bufPtr->b = *colorPtr++;
			bufPtr->a = *colorPtr++;
		}
		else
		{
			bufPtr->r = 255;
			bufPtr->g = 255;
			bufPtr->b = 255;
			bufPtr->a = 255;
		}

		bufPtr++;
	}

	pglGenBuffers(1, &frame->vboID);
	pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);
	pglBufferData(GL_ARRAY_BUFFER, bufferSize, buffer, GL_STATIC_DRAW);
	free(buffer);

	// Don't leave the array buffer bound to the model,
	// since this is called mid-frame
	pglBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void CreateModelVBOTiny(mesh_t *mesh, tinyframe_t *frame)
{
	int bufferSize = sizeof(vbotiny_t)*mesh->numTriangles * 3;
	vbotiny_t *buffer = (vbotiny_t*)malloc(bufferSize);
	vbotiny_t *bufPtr = buffer;

	short *vertPtr = frame->vertices;
	char *normPtr = frame->normals;
	float *uvPtr = mesh->uvs;
	char *tanPtr = frame->tangents;

	int i;
	for (i = 0; i < mesh->numVertices; i++)
	{
		bufPtr->x = *vertPtr++;
		bufPtr->y = *vertPtr++;
		bufPtr->z = *vertPtr++;

		bufPtr->nx = *normPtr++;
		bufPtr->ny = *normPtr++;
		bufPtr->nz = *normPtr++;

		bufPtr->s0 = *uvPtr++;
		bufPtr->t0 = *uvPtr++;

		if (tanPtr)
		{
			bufPtr->tanx = *tanPtr++;
			bufPtr->tany = *tanPtr++;
			bufPtr->tanz = *tanPtr++;
		}

		bufPtr++;
	}

	pglGenBuffers(1, &frame->vboID);
	pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);
	pglBufferData(GL_ARRAY_BUFFER, bufferSize, buffer, GL_STATIC_DRAW);
	free(buffer);

	// Don't leave the array buffer bound to the model,
	// since this is called mid-frame
	pglBindBuffer(GL_ARRAY_BUFFER, 0);
}

EXPORT void HWRAPI(CreateModelVBOs) (model_t *model)
{
	int i;
	for (i = 0; i < model->numMeshes; i++)
	{
		mesh_t *mesh = &model->meshes[i];

		if (mesh->frames)
		{
			int j;
			for (j = 0; j < model->meshes[i].numFrames; j++)
			{
				mdlframe_t *frame = &mesh->frames[j];
				if (frame->vboID)
					pglDeleteBuffers(1, &frame->vboID);
				frame->vboID = 0;
				CreateModelVBO(mesh, frame);
			}
		}
		else if (mesh->tinyframes)
		{
			int j;
			for (j = 0; j < model->meshes[i].numFrames; j++)
			{
				tinyframe_t *frame = &mesh->tinyframes[j];
				if (frame->vboID)
					pglDeleteBuffers(1, &frame->vboID);
				frame->vboID = 0;
				CreateModelVBOTiny(mesh, frame);
			}
		}
	}
}

#define BUFFER_OFFSET(i) ((char*)(i))

static void DrawModelEx(model_t *model, INT32 frameIndex, float duration, float tics, INT32 nextFrameIndex, FTransform *pos, float hscale, float vscale, UINT8 flipped, UINT8 hflipped, FSurfaceInfo *Surface)
{
	static GLRGBAFloat poly = {0,0,0,0};
	static GLRGBAFloat tint = {0,0,0,0};
	static GLRGBAFloat fade = {0,0,0,0};

	float pol = 0.0f;
	float scalex, scaley, scalez;

	boolean useTinyFrames;

	int i;

	// Affect input model scaling
	hscale *= 0.5f;
	vscale *= 0.5f;
	scalex = hscale;
	scaley = vscale;
	scalez = hscale;
	
	if (duration > 0.0 && tics >= 0.0) // don't interpolate if instantaneous or infinite in length
	{
		float newtime = (duration - tics); // + 1;

		pol = newtime / duration;

		if (pol > 1.0f)
			pol = 1.0f;

		if (pol < 0.0f)
			pol = 0.0f;
	}

	poly.red    = byte2float[Surface->PolyColor.s.red];
	poly.green  = byte2float[Surface->PolyColor.s.green];
	poly.blue   = byte2float[Surface->PolyColor.s.blue];
	poly.alpha  = byte2float[Surface->PolyColor.s.alpha];

	if (poly.alpha < 1)
		SetBlend(PF_Translucent|PF_Modulated);
	else
		SetBlend(PF_Masked|PF_Modulated|PF_Occlude);

	pglColor4ubv((GLubyte*)&Surface->PolyColor.s);

	tint.red    = byte2float[Surface->TintColor.s.red];
	tint.green  = byte2float[Surface->TintColor.s.green];
	tint.blue   = byte2float[Surface->TintColor.s.blue];
	tint.alpha  = byte2float[Surface->TintColor.s.alpha];

	fade.red   = byte2float[Surface->FadeColor.s.red];
	fade.green = byte2float[Surface->FadeColor.s.green];
	fade.blue  = byte2float[Surface->FadeColor.s.blue];
	fade.alpha = byte2float[Surface->FadeColor.s.alpha];
	
	if (Surface->LightTableId && Surface->LightTableId != lt_downloaded)
	{
		pglActiveTexture(GL_TEXTURE2);
		pglBindTexture(GL_TEXTURE_2D, Surface->LightTableId);
		pglActiveTexture(GL_TEXTURE0);
		lt_downloaded = Surface->LightTableId;
	}

	Shader_SetUniforms(Surface, &poly, &tint, &fade);

	pglEnable(GL_CULL_FACE);
	pglEnable(GL_NORMALIZE);

#ifdef USE_FTRANSFORM_MIRROR
	// flipped is if the object is vertically flipped
	// hflipped is if the object is horizontally flipped
	// pos->flip is if the screen is flipped vertically
	// pos->mirror is if the screen is flipped horizontally
	// XOR all the flips together to figure out what culling to use!
	{
		boolean reversecull = (flipped ^ hflipped ^ pos->flip ^ pos->mirror);
		if (reversecull)
			pglCullFace(GL_FRONT);
		else
			pglCullFace(GL_BACK);
	}
#else
	// pos->flip is if the screen is flipped too
	if (flipped ^ hflipped ^ pos->flip) // If one or three of these are active, but not two, invert the model's culling
		pglCullFace(GL_FRONT);
	else
		pglCullFace(GL_BACK);
#endif

	pglPushMatrix(); // should be the same as glLoadIdentity
	pglTranslatef(pos->x, pos->z, pos->y);
	if (flipped)
		scaley = -scaley;
	if (hflipped)
		scalez = -scalez;
#ifdef USE_FTRANSFORM_ANGLEZ
	pglRotatef(pos->anglez2, 0.0f, 0.0f, -1.0f); // rotate by slope from Kart
#endif
	pglRotatef(pos->anglex2, -1.0f, 0.0f, 0.0f);
	pglRotatef(pos->angley, 0.0f, -1.0f, 0.0f);
	
	if (pos->roll)
	{
		float roll = (1.0f * pos->rollflip);
		pglTranslatef(pos->centerx, pos->centery, 0);

		// rotate model for pitch and roll
		pglRotatef(pos->anglex, 1.0f, 0.0f, 0.0f);
#ifdef USE_FTRANSFORM_ANGLEZ
		pglRotatef(pos->anglez, 0.0f, 0.0f, -1.0f);
#endif

		if (pos->rotaxis == 2) // Z
			pglRotatef(pos->rollangle, 0.0f, 0.0f, roll);
		else if (pos->rotaxis == 1) // Y
			pglRotatef(pos->rollangle, 0.0f, roll, 0.0f);
		else // X
			pglRotatef(pos->rollangle, roll, 0.0f, 0.0f);
			
		pglTranslatef(-pos->centerx, -pos->centery, 0);
	}

	pglScalef(scalex, scaley, scalez);

	useTinyFrames = model->meshes[0].tinyframes != NULL;

	if (useTinyFrames)
		pglScalef(1 / 64.0f, 1 / 64.0f, 1 / 64.0f);

	pglEnableClientState(GL_NORMAL_ARRAY);

	for (i = 0; i < model->numMeshes; i++)
	{
		mesh_t *mesh = &model->meshes[i];

		if (useTinyFrames)
		{
			tinyframe_t *frame = &mesh->tinyframes[frameIndex % mesh->numFrames];
			tinyframe_t *nextframe = NULL;

			if (nextFrameIndex != -1)
				nextframe = &mesh->tinyframes[nextFrameIndex % mesh->numFrames];

			if (!nextframe || fpclassify(pol) == FP_ZERO)
			{
				pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);
				pglVertexPointer(3, GL_SHORT, sizeof(vbotiny_t), BUFFER_OFFSET(0));
				pglNormalPointer(GL_BYTE, sizeof(vbotiny_t), BUFFER_OFFSET(sizeof(short)*3));
				pglTexCoordPointer(2, GL_FLOAT, sizeof(vbotiny_t), BUFFER_OFFSET(sizeof(short) * 3 + sizeof(char) * 6));
				pglDrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
				pglBindBuffer(GL_ARRAY_BUFFER, 0);
			}
			else
			{
				short *vertPtr;
				char *normPtr;
				int j = 0;

				// Dangit, I soooo want to do this in a GLSL shader...
				AllocLerpTinyBuffer(mesh->numVertices * sizeof(short) * 3);
				vertPtr = vertTinyBuffer;
				normPtr = normTinyBuffer;

				for (j = 0; j < mesh->numVertices * 3; j++)
				{
					// Interpolate
					*vertPtr++ = (short)(frame->vertices[j] + (pol * (nextframe->vertices[j] - frame->vertices[j])));
					*normPtr++ = (char)(frame->normals[j] + (pol * (nextframe->normals[j] - frame->normals[j])));
				}

				pglVertexPointer(3, GL_SHORT, 0, vertTinyBuffer);
				pglNormalPointer(GL_BYTE, 0, normTinyBuffer);
				pglTexCoordPointer(2, GL_FLOAT, 0, mesh->uvs);
				pglDrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
			}
		}
		else
		{
			mdlframe_t *frame = &mesh->frames[frameIndex % mesh->numFrames];
			mdlframe_t *nextframe = NULL;

			if (nextFrameIndex != -1)
				nextframe = &mesh->frames[nextFrameIndex % mesh->numFrames];

			if (!nextframe || fpclassify(pol) == FP_ZERO)
			{
				// Zoom! Take advantage of just shoving the entire arrays to the GPU.
				pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);
				pglVertexPointer(3, GL_FLOAT, sizeof(vbo64_t), BUFFER_OFFSET(0));
				pglNormalPointer(GL_FLOAT, sizeof(vbo64_t), BUFFER_OFFSET(sizeof(float) * 3));
				pglTexCoordPointer(2, GL_FLOAT, sizeof(vbo64_t), BUFFER_OFFSET(sizeof(float) * 6));

				pglDrawArrays(GL_TRIANGLES, 0, mesh->numTriangles * 3);
				pglBindBuffer(GL_ARRAY_BUFFER, 0);
			}
			else
			{
				float *vertPtr;
				float *normPtr;
				int j = 0;

				// Dangit, I soooo want to do this in a GLSL shader...
				AllocLerpBuffer(mesh->numVertices * sizeof(float) * 3);
				vertPtr = vertBuffer;
				normPtr = normBuffer;

				for (j = 0; j < mesh->numVertices * 3; j++)
				{
					// Interpolate
					*vertPtr++ = frame->vertices[j] + (pol * (nextframe->vertices[j] - frame->vertices[j]));
					*normPtr++ = frame->normals[j] + (pol * (nextframe->normals[j] - frame->normals[j]));
				}

				pglVertexPointer(3, GL_FLOAT, 0, vertBuffer);
				pglNormalPointer(GL_FLOAT, 0, normBuffer);
				pglTexCoordPointer(2, GL_FLOAT, 0, mesh->uvs);
				pglDrawArrays(GL_TRIANGLES, 0, mesh->numVertices);
			}
		}
	}

	pglDisableClientState(GL_NORMAL_ARRAY);

	pglPopMatrix(); // should be the same as glLoadIdentity
	pglDisable(GL_CULL_FACE);
	pglDisable(GL_NORMALIZE);
}

// -----------------+
// HWRAPI DrawModel : Draw a model
// -----------------+
EXPORT void HWRAPI(DrawModel) (model_t *model, INT32 frameIndex, float duration, float tics, INT32 nextFrameIndex, FTransform *pos, float hscale, float vscale, UINT8 flipped, UINT8 hflipped, FSurfaceInfo *Surface)
{
	DrawModelEx(model, frameIndex, duration, tics, nextFrameIndex, pos, hscale, vscale, flipped, hflipped, Surface);
}

// -----------------+
// SetTransform     :
// -----------------+
EXPORT void HWRAPI(SetTransform) (FTransform *stransform)
{
	static boolean special_splitscreen;
	GLdouble used_fov;
	boolean shearing = false;

	pglLoadIdentity();

	if (stransform)
	{
		used_fov = stransform->fovxangle;
		shearing = stransform->shearing;
		// keep a trace of the transformation for md2
		memcpy(&md2_transform, stransform, sizeof (md2_transform));

#ifdef USE_FTRANSFORM_MIRROR
		// mirroring from Kart
		if (stransform->mirror)
			pglScalef(-stransform->scalex, stransform->scaley, -stransform->scalez);
		else
#endif
		if (stransform->flip)
			pglScalef(stransform->scalex, -stransform->scaley, -stransform->scalez);
		else
			pglScalef(stransform->scalex, stransform->scaley, -stransform->scalez);

		if (stransform->roll)
			pglRotatef(stransform->rollangle, 0.0f, 0.0f, 1.0f);
		pglRotatef(stransform->anglex       , 1.0f, 0.0f, 0.0f);
		pglRotatef(stransform->angley+270.0f, 0.0f, 1.0f, 0.0f);
		pglTranslatef(-stransform->x, -stransform->z, -stransform->y);

		special_splitscreen = (stransform->splitscreen == 1);
	}
	else
	{
		//Hurdler: is "fov" correct?
		used_fov = fov;
		pglScalef(1.0f, 1.0f, -1.0f);
	}

	pglMatrixMode(GL_PROJECTION);
	pglLoadIdentity();

	// jimita 14042019
	// Simulate Software's y-shearing
	// https://zdoom.org/wiki/Y-shearing
	if (shearing)
	{
		float dy = FIXED_TO_FLOAT(AIMINGTODY((stransform->viewaiming)) * 2)* ((float)vid.width / vid.height) / ((float)BASEVIDWIDTH / BASEVIDHEIGHT); //screen_width/BASEVIDWIDTH;
		if (stransform->flip)
			dy *= -1.0f;
		pglTranslatef(0.0f, -dy/BASEVIDHEIGHT, 0.0f);
	}

	if (special_splitscreen)
	{
		used_fov = (float)(atan(tan(used_fov*M_PI/360)*0.8)*360/M_PI);
		GLPerspective((GLfloat)used_fov, 2*ASPECT_RATIO);
	}
	else
		GLPerspective((GLfloat)used_fov, ASPECT_RATIO);

	pglGetFloatv(GL_PROJECTION_MATRIX, projMatrix); // added for new coronas' code (without depth buffer)
	pglMatrixMode(GL_MODELVIEW);

	pglGetFloatv(GL_MODELVIEW_MATRIX, modelMatrix); // added for new coronas' code (without depth buffer)
}

EXPORT INT32  HWRAPI(GetTextureUsed) (void)
{
	FTextureInfo *tmp = TexCacheHead;
	INT32 res = 0;

	while (tmp)
	{
		// Figure out the correct bytes-per-pixel for this texture
		// I don't know which one the game actually _uses_ but this
		// follows format2bpp in hw_cache.c
		int bpp = 1;
		int format = tmp->format;
		if (format == GL_TEXFMT_RGBA)
			bpp = 4;
		else if (format == GL_TEXFMT_ALPHA_INTENSITY_88 || format == GL_TEXFMT_AP_88)
			bpp = 2;

		// Add it up!
		res += tmp->height*tmp->width*bpp;
		tmp = tmp->next;
	}
	
	return res;
}

EXPORT void HWRAPI(PostImgRedraw) (float points[SCREENVERTS][SCREENVERTS][2])
{
	INT32 x, y;
	float float_x, float_y, float_nextx, float_nexty;
	float xfix, yfix;
	INT32 texsize = 512;

	const float blackBack[16] =
	{
		-16.0f, -16.0f, 6.0f,
		-16.0f, 16.0f, 6.0f,
		16.0f, 16.0f, 6.0f,
		16.0f, -16.0f, 6.0f
	};
	
	if (gl_enable_screen_textures != 2) 
		return;

	// look for power of two that is large enough for the screen
	while (texsize < screen_width || texsize < screen_height)
		texsize <<= 1;

	// X/Y stretch fix for all resolutions(!)
	xfix = (float)(texsize)/((float)((screen_width)/(float)(SCREENVERTS-1)));
	yfix = (float)(texsize)/((float)((screen_height)/(float)(SCREENVERTS-1)));

	pglDisable(GL_DEPTH_TEST);
	pglDisable(GL_BLEND);

	// Draw a black square behind the screen texture,
	// so nothing shows through the edges
	pglColor4ubv(white);

	pglVertexPointer(3, GL_FLOAT, 0, blackBack);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	pglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	for(x=0;x<SCREENVERTS-1;x++)
	{
		for(y=0;y<SCREENVERTS-1;y++)
		{
			float stCoords[8];
			float vertCoords[12];

			// Used for texture coordinates
			// Annoying magic numbers to scale the square texture to
			// a non-square screen..
			float_x = (float)(x/(xfix));
			float_y = (float)(y/(yfix));
			float_nextx = (float)(x+1)/(xfix);
			float_nexty = (float)(y+1)/(yfix);

			stCoords[0] = float_x;
			stCoords[1] = float_y;
			stCoords[2] = float_x;
			stCoords[3] = float_nexty;
			stCoords[4] = float_nextx;
			stCoords[5] = float_nexty;
			stCoords[6] = float_nextx;
			stCoords[7] = float_y;

			pglTexCoordPointer(2, GL_FLOAT, 0, stCoords);

			vertCoords[0] = points[x][y][0];
			vertCoords[1] = points[x][y][1];
			vertCoords[2] = 4.4f;
			vertCoords[3] = points[x][y + 1][0];
			vertCoords[4] = points[x][y + 1][1];
			vertCoords[5] = 4.4f;
			vertCoords[6] = points[x + 1][y + 1][0];
			vertCoords[7] = points[x + 1][y + 1][1];
			vertCoords[8] = 4.4f;
			vertCoords[9] = points[x + 1][y][0];
			vertCoords[10] = points[x + 1][y][1];
			vertCoords[11] = 4.4f;

			pglVertexPointer(3, GL_FLOAT, 0, vertCoords);

			pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}
	}

	pglEnable(GL_DEPTH_TEST);
	pglEnable(GL_BLEND);
}

// Sryder:	This needs to be called whenever the screen changes resolution in order to reset the screen textures to use
//			a new size
EXPORT void HWRAPI(FlushScreenTextures) (void)
{
	int i;
	pglDeleteTextures(NUMSCREENTEXTURES, screenTextures);
	for (i = 0; i < NUMSCREENTEXTURES; i++)
		screenTextures[i] = 0;
}

EXPORT void HWRAPI(DrawScreenTexture)(int tex, FSurfaceInfo *surf, FBITFIELD polyflags)
{
	float xfix, yfix;
	INT32 texsize = 512;

	const float screenVerts[12] =
	{
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f
	};

	float fix[8];
	
	if (gl_enable_screen_textures != 2) 
		return;

	// look for power of two that is large enough for the screen
	while (texsize < screen_width || texsize < screen_height)
		texsize <<= 1;

	xfix = 1/((float)(texsize)/((float)((screen_width))));
	yfix = 1/((float)(texsize)/((float)((screen_height))));

	// const float screenVerts[12]

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	pglClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	pglBindTexture(GL_TEXTURE_2D, screenTextures[tex]);
	
	PreparePolygon(surf, surf ? polyflags : (PF_NoDepthTest));
	if (!surf)
		pglColor4ubv(white);

	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, screenVerts);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	tex_downloaded = screenTextures[tex];
}


// Do screen fades!
EXPORT void HWRAPI(DoScreenWipe)(int wipeStart, int wipeEnd)
{
	INT32 texsize = 512;
	float xfix, yfix;

	INT32 fademaskdownloaded = tex_downloaded; // the fade mask that has been set

	const float screenVerts[12] =
	{
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f
	};

	float fix[8];

	const float defaultST[8] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};

	if (!gl_enable_screen_textures) 
		return;

	// look for power of two that is large enough for the screen
	while (texsize < screen_width || texsize < screen_height)
		texsize <<= 1;

	xfix = 1/((float)(texsize)/((float)((screen_width))));
	yfix = 1/((float)(texsize)/((float)((screen_height))));

	// const float screenVerts[12]

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	pglClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	SetBlend(PF_Modulated|PF_NoDepthTest);
	pglEnable(GL_TEXTURE_2D);

	// Draw the original screen
	pglBindTexture(GL_TEXTURE_2D, screenTextures[wipeStart]);
	pglColor4ubv(white);
	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, screenVerts);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	SetBlend(PF_Modulated|PF_Translucent|PF_NoDepthTest);

	// Draw the end screen that fades in
	pglActiveTexture(GL_TEXTURE0);
	pglEnable(GL_TEXTURE_2D);
	pglBindTexture(GL_TEXTURE_2D, screenTextures[wipeEnd]);
	pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	pglActiveTexture(GL_TEXTURE1);
	pglEnable(GL_TEXTURE_2D);
	pglBindTexture(GL_TEXTURE_2D, fademaskdownloaded);

	pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	// const float defaultST[8]

	pglClientActiveTexture(GL_TEXTURE0);
	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, screenVerts);
	pglClientActiveTexture(GL_TEXTURE1);
	pglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	pglTexCoordPointer(2, GL_FLOAT, 0, defaultST);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	pglDisable(GL_TEXTURE_2D); // disable the texture in the 2nd texture unit
	pglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	pglActiveTexture(GL_TEXTURE0);
	pglClientActiveTexture(GL_TEXTURE0);
	tex_downloaded = screenTextures[wipeEnd];
}

EXPORT void HWRAPI(RenderVhsEffect) (fixed_t upbary, fixed_t downbary, UINT8 updistort, UINT8 downdistort, UINT8 barsize)
{
	INT32 texsize = 512;
	float xfix, yfix;
	float fix[8];
	GLubyte color[4] = {255, 255, 255, 255};
	float i;

	float screenVerts[12] =
	{
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f
	};

	if (gl_enable_screen_textures != 2) 
		return;

	// look for power of two that is large enough for the screen
	while (texsize < screen_width || texsize < screen_height)
		texsize <<= 1;

	xfix = 1/((float)(texsize)/((float)((screen_width))));
	yfix = 1/((float)(texsize)/((float)((screen_height))));

	// Slight fuzziness
	MakeScreenTexture(HWD_SCREENTEXTURE_GENERIC1);
	SetBlend(PF_Modulated|PF_Translucent|PF_NoDepthTest);
	//pglBindTexture(GL_TEXTURE_2D, screentexture);

	for (i = 0; i < 1; i += 2.f/vid.height)
	{
		fix[2] = (float)(rand() % 128) / -22000 * xfix;
		fix[0] = fix[2];
		fix[6] = fix[0] + xfix;
		fix[4] = fix[2] + xfix;
		fix[1] = fix[7] = i*yfix;
		fix[3] = fix[5] = (i+0.015)*yfix;

		screenVerts[1] = screenVerts[10] = 2*i - 1.0f;
		screenVerts[4] = screenVerts[7] = screenVerts[1] + 0.03;

		pglColor4ubv(color);

		pglTexCoordPointer(2, GL_FLOAT, 0, fix);
		pglVertexPointer(3, GL_FLOAT, 0, screenVerts);
		pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

	// Upward bar
	MakeScreenTexture(HWD_SCREENTEXTURE_GENERIC1);
	//pglBindTexture(GL_TEXTURE_2D, screentexture);
	color[0] = color[1] = color[2] = 190;
	color[3] = 250;
	pglColor4ubv(color);

	fix[0] = 0.0f;
	fix[6] = xfix;
	fix[2] = (float)updistort / screen_width * xfix;
	fix[4] = fix[2] + fix[6];

	screenVerts[1] = screenVerts[10] = 2.0f*upbary/screen_height - 1.0f;
	screenVerts[4] = screenVerts[7] = screenVerts[1] + (float)barsize/screen_height;

	fix[1] = fix[7] = (float)upbary/screen_height * yfix;
	fix[3] = fix[5] = fix[1] + (float)barsize/2/screen_height * yfix;

	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, screenVerts);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	fix[1] = fix[7] += (fix[3] - fix[7])*2;
	screenVerts[1] = screenVerts[10] += (screenVerts[4] - screenVerts[1])*2;
	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, screenVerts);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Downward bar
	MakeScreenTexture(HWD_SCREENTEXTURE_GENERIC1);
	//pglBindTexture(GL_TEXTURE_2D, screentexture);

	fix[0] = 0.0f;
	fix[6] = xfix;
	fix[2] = (float)downdistort / screen_width * -xfix;
	fix[4] = fix[2] + fix[6];

	screenVerts[1] = screenVerts[10] = 2.0f*downbary/screen_height - 1.0f;
	screenVerts[4] = screenVerts[7] = screenVerts[1] + (float)barsize/screen_height;

	fix[1] = fix[7] = (float)downbary/screen_height * yfix;
	fix[3] = fix[5] = fix[1] + (float)barsize/2/screen_height * yfix;

	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, screenVerts);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	fix[1] = fix[7] += (fix[3] - fix[7])*2;
	screenVerts[1] = screenVerts[10] += (screenVerts[4] - screenVerts[1])*2;
	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, screenVerts);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// Create a texture from the screen.
EXPORT void HWRAPI(MakeScreenTexture) (int tex)
{
	INT32 texsize = 512;
	boolean firstTime = (screenTextures[tex] == 0);
	
	if (!gl_enable_screen_textures)
		return;

	// look for power of two that is large enough for the screen
	while (texsize < screen_width || texsize < screen_height)
		texsize <<= 1;

	// Create screen texture
	if (firstTime)
		pglGenTextures(1, &screenTextures[tex]);
	pglBindTexture(GL_TEXTURE_2D, screenTextures[tex]);

	if (firstTime)
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		Clamp2D(GL_TEXTURE_WRAP_S);
		Clamp2D(GL_TEXTURE_WRAP_T);
		pglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texsize, texsize, 0);
	}
	else
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);

	tex_downloaded = screenTextures[tex];
}

EXPORT void HWRAPI(DrawScreenFinalTexture)(int tex, INT32 width, INT32 height)
{
	float xfix, yfix;
	float origaspect, newaspect;
	float xoff = 1, yoff = 1; // xoffset and yoffset for the polygon to have black bars around the screen
	FRGBAFloat clearColour;
	INT32 texsize = 512;

	float off[12];
	float fix[8];
	
	if (gl_enable_screen_textures != 2)
		return;

	// look for power of two that is large enough for the screen
	while (texsize < screen_width || texsize < screen_height)
		texsize <<= 1;

	xfix = 1/((float)(texsize)/((float)((screen_width))));
	yfix = 1/((float)(texsize)/((float)((screen_height))));

	origaspect = (float)screen_width / screen_height;
	newaspect = (float)width / height;
	if (origaspect < newaspect)
	{
		xoff = origaspect / newaspect;
		yoff = 1;
	}
	else if (origaspect > newaspect)
	{
		xoff = 1;
		yoff = newaspect / origaspect;
	}

	// float off[12];
	off[0] = -xoff;
	off[1] = -yoff;
	off[2] = 1.0f;
	off[3] = -xoff;
	off[4] = yoff;
	off[5] = 1.0f;
	off[6] = xoff;
	off[7] = yoff;
	off[8] = 1.0f;
	off[9] = xoff;
	off[10] = -yoff;
	off[11] = 1.0f;

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	pglViewport(0, 0, width, height);

	clearColour.red = clearColour.green = clearColour.blue = 0;
	clearColour.alpha = 1;
	ClearBuffer(true, false, false, &clearColour);
	SetBlend(PF_NoDepthTest);

	pglBindTexture(GL_TEXTURE_2D, screenTextures[tex]);
	
	if (HWR_ShouldUsePaletteRendering())
		pglUseProgram(gl_shaders[SHADER_PALETTE_POSTPROCESS].program); // palette postprocess shader

	pglColor4ubv(white);

	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, off);

	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	
	if (HWR_ShouldUsePaletteRendering())
		pglUseProgram(0);

	tex_downloaded = screenTextures[tex];
}

EXPORT void HWRAPI(SetPaletteLookup)(UINT8 *lut)
{
	GLenum internalFormat;
	if (gl_version[0] == '1' || gl_version[0] == '2')
	{
		// if the OpenGL version is below 3.0, then the GL_R8 format may not be available.
		// so use GL_LUMINANCE8 instead to get a single component 8-bit format
		// (it is possible to have access to shaders even in some OpenGL 1.x systems,
		// so palette rendering can still possibly be achieved there)
		internalFormat = GL_LUMINANCE8;
	}
	else
	{
		internalFormat = GL_R8;
	}
	
	if (!paletteLookupTex)
		pglGenTextures(1, &paletteLookupTex);
	pglActiveTexture(GL_TEXTURE1);
	pglBindTexture(GL_TEXTURE_3D, paletteLookupTex);
	pglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	pglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	pglTexImage3D(GL_TEXTURE_3D, 0, internalFormat, HWR_PALETTE_LUT_SIZE, HWR_PALETTE_LUT_SIZE, HWR_PALETTE_LUT_SIZE,
		0, GL_RED, GL_UNSIGNED_BYTE, lut); // should there be a safety guard here aswell as the old one had? cant really test since i dont have hardware which doesent support 3D textures
	pglActiveTexture(GL_TEXTURE0);
}

EXPORT UINT32 HWRAPI(CreateLightTable)(RGBA_t *hw_lighttable)
{
	LTListItem *item = malloc(sizeof(LTListItem));
	if (!LightTablesTail)
	{
		LightTablesHead = LightTablesTail = item;
	}
	else
	{
		LightTablesTail->next = item;
		LightTablesTail = item;
	}
	item->next = NULL;
	pglGenTextures(1, &item->id);
	pglBindTexture(GL_TEXTURE_2D, item->id);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, hw_lighttable);

	// restore previously bound texture
	pglBindTexture(GL_TEXTURE_2D, tex_downloaded);

	return item->id;
}

// Delete light table textures, ids given before become invalid and must not be used.
EXPORT void HWRAPI(ClearLightTables)(void)
{
	while (LightTablesHead)
	{
		LTListItem *item = LightTablesHead;
		pglDeleteTextures(1, (GLuint *)&item->id);
		LightTablesHead = item->next;
		free(item);
	}

	LightTablesTail = NULL;
	
	// we no longer have a bound light table (if we had one), we just deleted it!
	lt_downloaded = 0;
}

// This palette is used for the palette rendering postprocessing step.
EXPORT void HWRAPI(SetScreenPalette)(RGBA_t *palette)
{
	if (memcmp(screenPalette, palette, sizeof(screenPalette)))
	{
		memcpy(screenPalette, palette, sizeof(screenPalette));
		if (!screenPaletteTex)
			pglGenTextures(1, &screenPaletteTex);
		pglActiveTexture(GL_TEXTURE2);
		pglBindTexture(GL_TEXTURE_1D, screenPaletteTex);
		pglTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		pglTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		pglTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, palette);
		pglActiveTexture(GL_TEXTURE0);
	}
}

#endif //HWRENDER
