#include "gpu.h"
#include <stdio.h>
#include <stdlib.h>

static char error[1024];
const char *gpu_error(void) { return error; }

#if LIFE_HAVE_GPU
#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <X11/Xutil.h>

struct gpu {
    Display *display;
    Window window;
    GLXContext context;
    GLuint step, draw, states[2], visited, counters, caption, vao;
    int columns, rows, current;
    unsigned frame;
    bool double_buffer;
};

/* Same state byte as the CPU: 0..31 trail, 32..63 live age. Out-of-grid
 * threads participate in every barrier. Only five counters leave the GPU. */
static const char compute_source[] =
    "#version 430 core\n"
    "layout(local_size_x=8,local_size_y=8) in;\n"
    "layout(r8ui,binding=0) readonly uniform uimage2D before;\n"
    "layout(r8ui,binding=1) writeonly uniform uimage2D after;\n"
    "layout(r8ui,binding=2) uniform uimage2D visited;\n"
    "layout(std430,binding=0) buffer Counters { uint population,changes,fresh,hash0,hash1; };\n"
    "uniform uint trail; uniform bool fade,sample_world;\n"
    "shared uvec4 sums[64]; shared uint hashes[64];\n"
    "uint mixbits(uint x) { x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; return x^(x>>16); }\n"
    "void main() {\n"
    " ivec2 p=ivec2(gl_GlobalInvocationID.xy),dim=imageSize(before);\n"
    " uint lane=gl_LocalInvocationIndex; uvec4 s=uvec4(0); uint h=0;\n"
    " if(all(lessThan(p,dim))) {\n"
    "  uint old=imageLoad(before,p).r; bool alive=old>=32u; uint neighbors=0u;\n"
    "  if(!fade) for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {\n"
    "   if(x==0 && y==0) continue;\n"
    "   ivec2 q=(p+ivec2(x,y)+dim)%dim; neighbors+=uint(imageLoad(before,q).r>=32u);\n"
    "  }\n"
    "  bool live=!fade && (neighbors==3u || (alive && neighbors==2u));\n"
    "  uint value=live ? (alive ? min(old+1u,63u) : 32u) : (alive ? trail : (old>0u ? old-1u : 0u));\n"
    "  imageStore(after,p,uvec4(value)); uint fresh_cell=0u;\n"
    "  if(sample_world && live && imageLoad(visited,p).r==0u) { imageStore(visited,p,uvec4(1)); fresh_cell=1u; }\n"
    "  uint index=uint(p.y*dim.x+p.x)+1u;\n"
    "  s=uvec4(uint(live),uint(live!=alive),fresh_cell,live ? mixbits(index) : 0u);\n"
    "  h=live ? mixbits(index^0x9e3779b9u) : 0u;\n"
    " }\n"
    " sums[lane]=s; hashes[lane]=h; barrier();\n"
    " for(uint n=32u;n>0u;n>>=1u) {\n"
    "  if(lane<n) { sums[lane]+=sums[lane+n]; hashes[lane]^=hashes[lane+n]; } barrier();\n"
    " }\n"
    " if(lane==0u) { atomicAdd(population,sums[0].x); atomicAdd(changes,sums[0].y);\n"
    "  atomicAdd(fresh,sums[0].z); atomicAdd(hash0,sums[0].w); atomicXor(hash1,hashes[0]); }\n"
    "}\n";

static const char vertex_source[] =
    "#version 430 core\n"
    "void main() { vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2); gl_Position=vec4(p*2.0-1.0,0,1); }\n";
static const char fragment_source[] =
    "#version 430 core\n"
    "uniform usampler2D world; uniform sampler2D caption;\n"
    "uniform vec3 palette[64]; uniform ivec2 window_size,origin;\n"
    "uniform int cell_size,grid_height; uniform bool rounded,fancy; uniform float phase; out vec4 color;\n"
    /* Probe the existing world; bounds checks are explicit for integer
     * texelFetch. The plume is a display effect, never a simulation input. */
    "float ember(vec2 p) {\n"
    " vec2 cell=p/float(cell_size); ivec2 at=ivec2(floor(cell));\n"
    " if(any(lessThan(at,ivec2(0))) || any(greaterThanEqual(at,textureSize(world,0)))) return 0.0;\n"
    " uint v=texelFetch(world,at,0).r;\n"
    " float heat=v>=32u ? mix(1.0,0.55,float(v-32u)/31.0) : float(v)*0.012;\n"
    " if(cell_size>=5) {\n"
    "  vec2 d=abs(fract(cell)-vec2(0.5));\n"
    "  heat*=1.0-smoothstep(0.22,0.5,d.x);\n"
    "  heat*=1.0-smoothstep(0.25,0.5,d.y);\n"
    " } return heat;\n"
    "}\n"
    "vec3 fire(uint value,float flicker) {\n"
    " if(value>=32u) {\n"
    "  float age=float(value-32u)/31.0;\n"
    "  return mix(vec3(1.0,0.91,0.65),vec3(1.0,0.43,0.12),age)*(0.94+0.06*flicker);\n"
    " } float t=float(value)/31.0;\n"
    " return palette[0]+vec3(0.85,0.19,0.025)*t*t;\n"
    "}\n"
    "void main() {\n"
    " ivec2 p=ivec2(gl_FragCoord.xy); p.y=window_size.y-1-p.y; vec3 rgb=palette[0];\n"
    " if(p.y>=grid_height) {\n"
    "  float ink=texelFetch(caption,ivec2(p.x,p.y-grid_height),0).r;\n"
    "  rgb=mix(rgb,fancy ? vec3(0.72,0.47,0.25) : palette[45],ink);\n"
    " } else {\n"
    "  ivec2 q=p-origin,extent=textureSize(world,0)*cell_size;\n"
    "  if(all(greaterThanEqual(q,ivec2(0))) && all(lessThan(q,extent))) {\n"
    "   bool gap=cell_size>=5 && (q.x%cell_size==cell_size-1 || q.y%cell_size==cell_size-1);\n"
    "   uint value=gap ? 0u : texelFetch(world,q/cell_size,0).r;\n"
    "   float coverage=gap ? 0.0 : 1.0; rgb=palette[value];\n"
    /* A rounded box evaluated in pixel space: one texture read, no extra
     * render target or pass. Fine worlds keep their exact square pixels. */
    "   if(rounded && cell_size>=5 && value!=0u) {\n"
    "    float half_width=float(cell_size-1)*0.5,radius=half_width*0.44;\n"
    "    vec2 local=vec2(q%cell_size)+vec2(0.5)-vec2(half_width);\n"
    "    vec2 d=abs(local)-vec2(half_width-radius);\n"
    "    float distance=length(max(d,vec2(0)))+min(max(d.x,d.y),0.0)-radius;\n"
    "    coverage=1.0-smoothstep(-0.5,0.5,distance);\n"
    "   }\n"
    "   if(fancy) {\n"
    "    float height=clamp(float(cell_size)*1.2,3.0,32.0);\n"
    "    vec2 pos=vec2(q)+vec2(0.5);\n"
    "    float wave=sin(pos.x/(height*2.7)+pos.y/(height*1.6)-phase*13.0);\n"
    "    float flicker=0.5+0.5*sin(pos.x/(height*1.3)+pos.y/height-phase*23.0);\n"
    "    float sway=wave*height*0.18;\n"
    "    float glow=0.44*ember(pos+vec2(sway,height*0.28));\n"
    "    glow+=0.28*ember(pos+vec2(sway*1.6,height*0.65));\n"
    "    glow+=0.16*ember(pos+vec2(sway*2.0,height*1.1));\n"
    "    glow*=0.8+0.2*flicker;\n"
    "    vec3 plume=palette[0]+vec3(0.62,0.17,0.02)*glow;\n"
    "    rgb=mix(plume,fire(value,flicker),value==0u ? 0.0 : coverage);\n"
    "   } else rgb=mix(palette[0],rgb,coverage);\n"
    "  }\n"
    " } color=vec4(rgb,1);\n"
    "}\n";

static GLuint
shader(GLenum type, const char *source)
{
    GLuint object = glCreateShader(type);
    glShaderSource(object, 1, &source, NULL);
    glCompileShader(object);
    GLint ok;
    glGetShaderiv(object, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(object, sizeof(error), NULL, error);
        glDeleteShader(object);
        return 0;
    }
    return object;
}

static GLuint
program(const char *compute)
{
    GLuint a = shader(compute ? GL_COMPUTE_SHADER : GL_VERTEX_SHADER, compute ? compute : vertex_source);
    if (!a) return 0;
    GLuint b = compute ? 0 : shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!compute && !b) { glDeleteShader(a); return 0; }
    GLuint object = glCreateProgram();
    glAttachShader(object, a);
    if (b) glAttachShader(object, b);
    glLinkProgram(object);
    glDeleteShader(a);
    if (b) glDeleteShader(b);
    GLint ok;
    glGetProgramiv(object, GL_LINK_STATUS, &ok);
    if (!ok) {
        glGetProgramInfoLog(object, sizeof(error), NULL, error);
        glDeleteProgram(object);
        return 0;
    }
    return object;
}

static bool
gl_ok(const char *operation)
{
    GLenum code = glGetError();
    if (code == GL_NO_ERROR) return true;
    snprintf(error, sizeof(error), "%s (OpenGL error 0x%x)", operation, code);
    return false;
}

static bool context_error;
static int
catch_context_error(Display *display, XErrorEvent *event)
{
    (void)display; (void)event;
    context_error = true;
    return 0;
}

struct gpu *
gpu_open(Display *display, Window window)
{
    snprintf(error, sizeof(error), "GPU mode needs GLX and OpenGL 4.3; use --cpu on this display");
    int opcode, event, code;
    if (!XQueryExtension(display, "GLX", &opcode, &event, &code)) return NULL;
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(display, window, &attributes)) return NULL;
    int screen = XScreenNumberOfScreen(attributes.screen);
    if (!epoxy_has_glx_extension(display, screen, "GLX_ARB_create_context")) return NULL;
    int count;
    GLXFBConfig *configs = glXGetFBConfigs(display, screen, &count), chosen = NULL;
    bool double_buffer = false;
    for (int i = 0; configs && i < count; ++i) {
        int visual, drawable, render, samples, buffered;
        glXGetFBConfigAttrib(display, configs[i], GLX_VISUAL_ID, &visual);
        glXGetFBConfigAttrib(display, configs[i], GLX_DRAWABLE_TYPE, &drawable);
        glXGetFBConfigAttrib(display, configs[i], GLX_RENDER_TYPE, &render);
        glXGetFBConfigAttrib(display, configs[i], GLX_SAMPLES, &samples);
        glXGetFBConfigAttrib(display, configs[i], GLX_DOUBLEBUFFER, &buffered);
        if ((VisualID)visual == XVisualIDFromVisual(attributes.visual) &&
            (drawable & GLX_WINDOW_BIT) && (render & GLX_RGBA_BIT) && !samples) {
            chosen = configs[i]; double_buffer = buffered != 0;
            if (double_buffer) break;
        }
    }
    if (!chosen) { if (configs) XFree(configs); return NULL; }
    struct gpu *gpu = calloc(1, sizeof(*gpu));
    if (!gpu) { XFree(configs); return NULL; }
    gpu->display = display; gpu->window = window; gpu->double_buffer = double_buffer;
    const int request[] = {GLX_CONTEXT_MAJOR_VERSION_ARB, 4, GLX_CONTEXT_MINOR_VERSION_ARB, 3,
                          GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB, None};
    XSync(display, False);
    int (*previous)(Display *, XErrorEvent *) = XSetErrorHandler(catch_context_error);
    context_error = false;
    gpu->context = glXCreateContextAttribsARB(display, chosen, NULL, True, request);
    XFree(configs);
    XSync(display, False);
    bool current = !context_error && gpu->context && glXMakeCurrent(display, window, gpu->context);
    XSync(display, False);
    XSetErrorHandler(previous);
    if (!current || context_error) {
        if (gpu->context) glXDestroyContext(display, gpu->context);
        free(gpu); return NULL;
    }
    if (epoxy_has_glx_extension(display, screen, "GLX_EXT_swap_control"))
        glXSwapIntervalEXT(display, window, 0);
    else if (epoxy_has_glx_extension(display, screen, "GLX_MESA_swap_control"))
        glXSwapIntervalMESA(0);
    glDisable(GL_DITHER);
    gpu->step = program(compute_source);
    if (gpu->step) gpu->draw = program(NULL);
    if (!gpu->step || !gpu->draw) { gpu_close(gpu); return NULL; }
    glGenTextures(2, gpu->states);
    glGenTextures(1, &gpu->visited);
    glGenTextures(1, &gpu->caption);
    glGenBuffers(1, &gpu->counters);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu->counters);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 5 * sizeof(GLuint), NULL, GL_DYNAMIC_READ);
    glGenVertexArrays(1, &gpu->vao);
    glUseProgram(gpu->draw);
    glUniform1i(glGetUniformLocation(gpu->draw, "world"), 0);
    glUniform1i(glGetUniformLocation(gpu->draw, "caption"), 1);
    if (!gl_ok("initializing GPU")) { gpu_close(gpu); return NULL; }
    return gpu;
}

const char *gpu_renderer(const struct gpu *gpu)
{
    (void)gpu;
    return (const char *)glGetString(GL_RENDERER);
}

void gpu_palette(struct gpu *gpu, const float *rgb)
{
    glUseProgram(gpu->draw);
    glUniform3fv(glGetUniformLocation(gpu->draw, "palette"), LIFE_COLORS, rgb);
}

static void
texture(GLuint name, int columns, int rows, const uint8_t *data)
{
    glBindTexture(GL_TEXTURE_2D, name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, columns, rows, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, data);
}

bool
gpu_seed(struct gpu *gpu, const struct life *life, const uint8_t *visited)
{
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    glActiveTexture(GL_TEXTURE0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, life->stride);
    texture(gpu->states[0], life->columns, life->rows, life->cells + life->stride + 1);
    texture(gpu->states[1], life->columns, life->rows, NULL);
    texture(gpu->visited, life->columns, life->rows, visited + life->stride + 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gpu->columns = life->columns; gpu->rows = life->rows; gpu->current = 0;
    gpu->frame = 0;
    return gl_ok("allocating GPU world; try a larger --size");
}

bool
gpu_step(struct gpu *gpu, unsigned trail, bool fade, bool sample)
{
    glUseProgram(gpu->step);
    glUniform1ui(glGetUniformLocation(gpu->step, "trail"), trail < LIFE_ALIVE ? trail : LIFE_ALIVE - 1);
    glUniform1i(glGetUniformLocation(gpu->step, "fade"), fade);
    glUniform1i(glGetUniformLocation(gpu->step, "sample_world"), sample);
    glBindImageTexture(0, gpu->states[gpu->current], 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8UI);
    glBindImageTexture(1, gpu->states[1 - gpu->current], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8UI);
    glBindImageTexture(2, gpu->visited, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R8UI);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gpu->counters);
    const GLuint zero = 0;
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    glDispatchCompute((GLuint)(gpu->columns + 7) / 8, (GLuint)(gpu->rows + 7) / 8, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT |
                    GL_BUFFER_UPDATE_BARRIER_BIT);
    gpu->current = 1 - gpu->current;
    ++gpu->frame;
    return gl_ok("advancing GPU world");
}

bool
gpu_observe(struct gpu *gpu, struct gpu_stats *stats)
{
    /* Read on the next frame, after the normal frame-rate sleep. In the usual
     * case the GPU has finished, so this does not stall on a just-issued step. */
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu->counters);
    GLuint result[5];
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(result), result);
    stats->population = result[0]; stats->changes = result[1]; stats->fresh = result[2];
    stats->hash = ((uint64_t)result[3] << 32) | result[4];
    return gl_ok("reading GPU counters");
}

bool
gpu_caption(struct gpu *gpu, Pixmap bitmap, int width, int height)
{
    XImage *image = XGetImage(gpu->display, bitmap, 0, 0, (unsigned)width, (unsigned)height, 1, XYPixmap);
    uint8_t *pixels = malloc((size_t)width * (size_t)height);
    if (!image || !pixels) {
        if (image) XDestroyImage(image);
        free(pixels); snprintf(error, sizeof(error), "cannot allocate GPU caption"); return false;
    }
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            pixels[y * width + x] = XGetPixel(image, x, y) ? 255 : 0;
    XDestroyImage(image);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gpu->caption);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
    free(pixels);
    return gl_ok("uploading GPU caption");
}

void
gpu_draw(struct gpu *gpu, int width, int height, int grid_height, int size, int left, int top,
         bool rounded, bool fancy)
{
    glUseProgram(gpu->draw);
    glUniform2i(glGetUniformLocation(gpu->draw, "window_size"), width, height);
    glUniform2i(glGetUniformLocation(gpu->draw, "origin"), left, top);
    glUniform1i(glGetUniformLocation(gpu->draw, "cell_size"), size);
    glUniform1i(glGetUniformLocation(gpu->draw, "grid_height"), grid_height);
    glUniform1i(glGetUniformLocation(gpu->draw, "rounded"), rounded);
    glUniform1i(glGetUniformLocation(gpu->draw, "fancy"), fancy);
    /* A periodic simulation clock: expose/zoom and hidden time cannot make
     * the flames jump, and long sessions do not lose float precision. */
    glUniform1f(glGetUniformLocation(gpu->draw, "phase"),
                (float)(gpu->frame % 960) * (6.28318530718f / 960));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gpu->states[gpu->current]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gpu->caption);
    glViewport(0, 0, width, height);
    glBindVertexArray(gpu->vao);
    glDrawBuffer(gpu->double_buffer ? GL_BACK : GL_FRONT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (gpu->double_buffer) glXSwapBuffers(gpu->display, gpu->window);
    else glFlush();
}

void
gpu_close(struct gpu *gpu)
{
    if (!gpu) return;
    glDeleteProgram(gpu->step); glDeleteProgram(gpu->draw);
    glDeleteTextures(2, gpu->states); glDeleteTextures(1, &gpu->visited);
    glDeleteTextures(1, &gpu->caption); glDeleteBuffers(1, &gpu->counters);
    glDeleteVertexArrays(1, &gpu->vao);
    glXMakeCurrent(gpu->display, None, NULL);
    glXDestroyContext(gpu->display, gpu->context);
    free(gpu);
}
#else
/* The original Xlib build needs no OpenGL headers or libraries. */
struct gpu *gpu_open(Display *display, Window window)
{
    (void)display; (void)window;
    snprintf(error, sizeof(error), "GPU support not built; install libepoxy and rebuild, or use --cpu");
    return NULL;
}
const char *gpu_renderer(const struct gpu *gpu) { (void)gpu; return "unavailable"; }
void gpu_palette(struct gpu *gpu, const float *rgb) { (void)gpu; (void)rgb; }
bool gpu_seed(struct gpu *gpu, const struct life *life, const uint8_t *visited)
{ (void)gpu; (void)life; (void)visited; return false; }
bool gpu_step(struct gpu *gpu, unsigned trail, bool fade, bool sample)
{ (void)gpu; (void)trail; (void)fade; (void)sample; return false; }
bool gpu_observe(struct gpu *gpu, struct gpu_stats *stats)
{ (void)gpu; (void)stats; return false; }
bool gpu_caption(struct gpu *gpu, Pixmap bitmap, int width, int height)
{ (void)gpu; (void)bitmap; (void)width; (void)height; return false; }
void gpu_draw(struct gpu *gpu, int width, int height, int grid_height, int size, int left, int top,
              bool rounded, bool fancy)
{ (void)gpu; (void)width; (void)height; (void)grid_height; (void)size; (void)left; (void)top; (void)rounded; (void)fancy; }
void gpu_close(struct gpu *gpu) { (void)gpu; }
#endif
