#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>

#ifdef __APPLE__
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl3.h>   // macOS OpenGL 4.1 core
#else
  #error "This setup is macOS-only. For Windows/Linux, use the GLEW/GLAD path."
#endif
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ------------------------- Window & Camera -------------------------
static int SCR_W = 1280, SCR_H = 720;

static float camYaw = -90.0f, camPitch = -5.0f;
static glm::vec3 camPos(0.0f, 0.1f, 0.0f);
static glm::vec3 camFront(0.0f, 0.0f, -1.0f);
static glm::vec3 camUp(0.0f, 1.0f, 0.0f);
static bool firstMouse = true;
static double lastX = SCR_W * 0.5, lastY = SCR_H * 0.5;
static bool wireframe = false;

static void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    SCR_W = w; SCR_H = h;
    glViewport(0, 0, w, h);
}
static void mouse_callback(GLFWwindow*, double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = (float)(xpos - lastX);
    float yoffset = (float)(lastY - ypos);
    lastX = xpos; lastY = ypos;
    float sensitivity = 0.12f;
    xoffset *= sensitivity; yoffset *= sensitivity;
    camYaw += xoffset; camPitch += yoffset;
    camPitch = std::clamp(camPitch, -89.0f, 89.0f);
    glm::vec3 front;
    front.x = std::cos(glm::radians(camYaw)) * std::cos(glm::radians(camPitch));
    front.y = std::sin(glm::radians(camPitch));
    front.z = std::sin(glm::radians(camYaw)) * std::cos(glm::radians(camPitch));
    camFront = glm::normalize(front);
}
static void process_input(GLFWwindow* win, float dt) {
    float speed = (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 6.0f : 3.0f;
    float vel = speed * dt;
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) camPos += camFront * vel;
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) camPos -= camFront * vel;
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) camPos -= glm::normalize(glm::cross(camFront, camUp)) * vel;
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) camPos += glm::normalize(glm::cross(camFront, camUp)) * vel;
    if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS) camPos.y -= vel;
    if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS) camPos.y += vel;
}

// ------------------------- Utilities -------------------------
static GLuint compileShader(GLenum type, const char* src, const char* name) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096]; glGetShaderInfoLog(s, 4096, nullptr, log);
        std::cerr << "Shader error in " << name << ":\n" << log << "\n";
    }
    return s;
}
static GLuint linkProgram(GLuint vs, GLuint fs, const char* name) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096]; glGetProgramInfoLog(p, 4096, nullptr, log);
        std::cerr << "Link error in " << name << ":\n" << log << "\n";
    }
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}
static std::string loadFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { std::cerr << "Cannot open " << path << "\n"; return ""; }
    fseek(f, 0, SEEK_END); long len = ftell(f); rewind(f);
    std::string s; s.resize(len);
    fread(s.data(), 1, len, f);
    fclose(f);
    return s;
}

// ------------------------- Geometry Helpers -------------------------
struct Mesh { GLuint vao=0, vbo=0, ebo=0; GLsizei idxCount=0; };

// Tank walls (inward-facing normals)
static Mesh makeBox(float w, float h, float d) {
    float x=w*0.5f, y=h*0.5f, z=d*0.5f;
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> i;
    auto addQuad=[&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n){
        size_t base=v.size();
        v.push_back({a,n}); v.push_back({b,n}); v.push_back({c,n}); v.push_back({d,n});
        i.insert(i.end(), { (unsigned)base,(unsigned)base+2,(unsigned)base+1,
                            (unsigned)base,(unsigned)base+3,(unsigned)base+2 });
    };
    addQuad({-x,-y, z},{ x,-y, z},{ x, y, z},{-x, y, z}, { 0, 0,-1});
    addQuad({ x,-y,-z},{-x,-y,-z},{-x, y,-z},{ x, y,-z}, { 0, 0, 1});
    addQuad({-x,-y,-z},{-x,-y, z},{-x, y, z},{-x, y,-z}, { 1, 0, 0});
    addQuad({ x,-y, z},{ x,-y,-z},{ x, y,-z},{ x, y, z}, {-1, 0, 0});
    addQuad({-x, y, z},{ x, y, z},{ x, y,-z},{-x, y,-z}, { 0,-1, 0}); // ceiling
    Mesh m; glGenVertexArrays(1,&m.vao); glBindVertexArray(m.vao);
    glGenBuffers(1,&m.vbo); glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size()*sizeof(V), v.data(), GL_STATIC_DRAW);
    glGenBuffers(1,&m.ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, i.size()*sizeof(unsigned), i.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,n));
    m.idxCount=(GLsizei)i.size();
    glBindVertexArray(0);
    return m;
}

// Water plane (position + UV in attrib 0/2)
static Mesh makeWaterPlane(int nx=120, int nz=120, float sx=3.2f, float sz=1.8f, float y=0.45f) {
    struct V { glm::vec3 p; glm::vec2 uv; };
    std::vector<V> v; v.reserve((nx+1)*(nz+1));
    for (int z=0; z<=nz; ++z) for (int x=0; x<=nx; ++x) {
        float u = (float)x/nx, w=(float)z/nz;
        v.push_back({ glm::vec3((u-0.5f)*sx, y, (w-0.5f)*sz), glm::vec2(u,w) });
    }
    std::vector<unsigned> idx; idx.reserve(nx*nz*6);
    for (int z=0; z<nz; ++z) for (int x=0; x<nx; ++x) {
        unsigned a = z*(nx+1)+x, b=a+1, c=a+(nx+1), d=c+1;
        idx.insert(idx.end(), {a,c,b, b,c,d});
    }
    Mesh m; glGenVertexArrays(1,&m.vao); glBindVertexArray(m.vao);
    glGenBuffers(1,&m.vbo); glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size()*sizeof(V), v.data(), GL_STATIC_DRAW);
    glGenBuffers(1,&m.ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,uv));
    m.idxCount=(GLsizei)idx.size();
    glBindVertexArray(0);
    return m;
}

// Floor plane (with normals)
static Mesh makeFloor(float sx=3.2f, float sz=1.8f, float y=-0.9f) {
    struct V { glm::vec3 p,n; };
    std::vector<V> v = {
        {{-sx*0.5f,y,-sz*0.5f},{0,1,0}},
        {{ sx*0.5f,y,-sz*0.5f},{0,1,0}},
        {{ sx*0.5f,y, sz*0.5f},{0,1,0}},
        {{-sx*0.5f,y, sz*0.5f},{0,1,0}},
    };
    std::vector<unsigned> i = {0,1,2, 0,2,3};
    Mesh m; glGenVertexArrays(1,&m.vao); glBindVertexArray(m.vao);
    glGenBuffers(1,&m.vbo); glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size()*sizeof(V), v.data(), GL_STATIC_DRAW);
    glGenBuffers(1,&m.ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, i.size()*sizeof(unsigned), i.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,n));
    m.idxCount=(GLsizei)i.size();
    glBindVertexArray(0);
    return m;
}

// Rounded fusiform fish mesh (base shape)
static Mesh makeFishMesh() {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;

    auto push = [&](const glm::vec3& p, const glm::vec3& n){ v.push_back({p, glm::normalize(n)}); };

    const int   segX = 22;        // length segments
    const int   segR = 18;        // radial segments
    const float rMax = 0.06f;     // slimmer by default (we stretch per-instance)
    const float zFlatten = 0.65f; // thinner left-right

    for (int i=0; i<=segX; ++i) {
        float t = (float)i / (float)segX;
        float tClamped = std::clamp(t*1.02f, 0.0f, 1.0f);
        float r = rMax * std::pow(std::sin(3.14159f * tClamped), 0.75f);
        if (i == 0) r *= 0.6f; // nose

        for (int j=0; j<=segR; ++j) {
            float a = (2.0f * 3.14159f) * (float)j / (float)segR;
            float cy = std::cos(a), sy = std::sin(a);
            glm::vec3 p = { t, r * cy, zFlatten * r * sy };
            glm::vec3 n = { 0.0f, cy, (1.0f / zFlatten) * sy };
            push(p, n);
        }
    }
    int ring = segR + 1;
    for (int i=0; i<segX; ++i) for (int j=0; j<segR; ++j) {
        unsigned a = i*ring + j, b=a+1, c=(i+1)*ring + j, d=c+1;
        idx.insert(idx.end(), {a,c,b, b,c,d});
    }
    // nose cap
    {
        glm::vec3 nose = {0.0f, 0.0f, 0.0f};
        unsigned baseCenter = (unsigned)v.size();
        push(nose, glm::vec3(-1,0,0));
        for (int j=0; j<segR; ++j) { unsigned a=j, b=j+1; idx.insert(idx.end(), {baseCenter, b, a}); }
    }
    // simple tail fin each side
    {
        float x = 1.05f;
        glm::vec3 tU = {x,  0.14f,  0.0f}, tD = {x, -0.14f,  0.0f};
        glm::vec3 baseL = {1.0f,  0.04f,  0.02f}, baseR = {1.0f, -0.04f,  0.02f};
        glm::vec3 baseL2= {1.0f,  0.04f, -0.02f}, baseR2= {1.0f, -0.04f, -0.02f};
        unsigned s = (unsigned)v.size();
        push(tU,{0,0, 1}); push(tD,{0,0, 1}); push(baseL,{0,0, 1}); push(baseR,{0,0, 1});
        push(tU,{0,0,-1}); push(tD,{0,0,-1}); push(baseL2,{0,0,-1}); push(baseR2,{0,0,-1});
        idx.insert(idx.end(), {s+2,s+0,s+1,  s+2,s+1,s+3});
        idx.insert(idx.end(), {s+5,s+7,s+4,  s+5,s+6,s+7});
    }

    Mesh m; glGenVertexArrays(1,&m.vao); glBindVertexArray(m.vao);
    glGenBuffers(1,&m.vbo); glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size()*sizeof(V), v.data(), GL_STATIC_DRAW);
    glGenBuffers(1,&m.ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,n));
    m.idxCount = (GLsizei)idx.size();
    glBindVertexArray(0);
    return m;
}

// Simple vertical strip for a plant (many segments, sway in shader)
static Mesh makePlantStrip(int segments = 10, float height = 0.5f, float width = 0.02f) {
    struct V { glm::vec3 p,n; };
    std::vector<V> v;
    std::vector<unsigned> idx;
    // Two columns of vertices along Y
    for (int i=0;i<=segments;++i) {
        float t = (float)i/segments;
        float y = t * height;
        float w = width * (0.7f + 0.3f * (1.0f - t)); // narrower near tip
        // left/right
        v.push_back({glm::vec3(-w*0.5f, y, 0.0f), glm::vec3(0,0,1)});
        v.push_back({glm::vec3( w*0.5f, y, 0.0f), glm::vec3(0,0,1)});
        if (i<segments) {
            unsigned base = i*2;
            idx.insert(idx.end(), {base, base+2, base+1,  base+1, base+2, base+3});
        }
    }
    Mesh m; glGenVertexArrays(1,&m.vao); glBindVertexArray(m.vao);
    glGenBuffers(1,&m.vbo); glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size()*sizeof(V), v.data(), GL_STATIC_DRAW);
    glGenBuffers(1,&m.ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,n));
    m.idxCount=(GLsizei)idx.size();
    glBindVertexArray(0);
    return m;
}

// A simple dome "rock" (hemisphere)
static Mesh makeRockDome(int rings=10, int sectors=16, float radius=0.18f) {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    for (int r=0;r<=rings;++r) {
        float vr = (float)r/rings;           // 0..1
        float phi = (vr*0.5f)*3.14159f;      // 0..pi/2
        for (int s=0;s<=sectors;++s) {
            float vs = (float)s/sectors;
            float theta = vs*2.0f*3.14159f;
            float x = radius*std::cos(theta)*std::sin(phi);
            float y = radius*std::cos(phi);
            float z = radius*std::sin(theta)*std::sin(phi);
            glm::vec3 p(x,y,z), n = glm::normalize(p);
            v.push_back({p,n});
        }
    }
    int ring = sectors+1;
    for (int r=0;r<rings;++r) for (int s=0;s<sectors;++s) {
        unsigned a=r*ring+s, b=a+1, c=(r+1)*ring+s, d=c+1;
        idx.insert(idx.end(), {a,c,b, b,c,d});
    }
    Mesh m; glGenVertexArrays(1,&m.vao); glBindVertexArray(m.vao);
    glGenBuffers(1,&m.vbo); glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size()*sizeof(V), v.data(), GL_STATIC_DRAW);
    glGenBuffers(1,&m.ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,n));
    m.idxCount=(GLsizei)idx.size();
    glBindVertexArray(0);
    return m;
}

// ------------------------- Species & Instances -------------------------
struct FishInst { glm::vec3 pos, vel; float phase; float scale; glm::vec3 stretch; glm::vec3 color; };

static std::vector<FishInst> minis, darters, loaches;
static GLuint vboMinis=0, vboDarters=0, vboLoaches=0;

static Mesh fishMesh, plantMesh, tankMesh, floorMesh, waterMesh, rockMesh;

static const glm::vec3 TANK_EXTENTS = {3.5f*0.5f - 0.05f, 0.9f, 1.8f*0.5f - 0.05f}; // inner bounds
static float waterY = 0.45f;

// counts (reduced clustering)
static int N_MINIS   = 60;  // small school
static int N_DARTERS = 16;  // roamers
static int N_LOACHES = 10;  // bottom huggers

// plants & rocks
static int N_PLANTS = 55;
static int N_ROCKS  = 6;
static GLuint plantVBO=0, plantVAO=0; // per-instance attribs
static std::vector<glm::vec3> plantPos;
static std::vector<glm::vec2> plantHP;   // height, phase
static std::vector<glm::vec3> plantColor;

static std::vector<glm::vec4> rocks; // x,y,z,radius (we place dome with model matrix via uniform)

// RNG
static std::mt19937 rng(2025);
static std::uniform_real_distribution<float> urand(-1.0f, 1.0f);
static std::uniform_real_distribution<float> urand01(0.0f, 1.0f);

// ---- Fish initializers ----
static void initSpecies(std::vector<FishInst>& v, int count,
                        glm::vec3 baseColor, glm::vec3 varyColor,
                        glm::vec3 stretchMean, glm::vec3 stretchVar,
                        float speedMin, float speedMax,
                        float yMin, float yMax) {
    v.resize(count);
    for (int i=0;i<count;++i) {
        glm::vec3 p(urand(rng)*TANK_EXTENTS.x*0.9f,
                    yMin + urand01(rng)*(yMax-yMin),
                    urand(rng)*TANK_EXTENTS.z*0.9f);
        glm::vec3 dir = glm::normalize(glm::vec3(urand(rng), urand(rng)*0.2f, urand(rng)));
        float sp = speedMin + urand01(rng)*(speedMax-speedMin);
        glm::vec3 col = glm::clamp(baseColor + varyColor * urand(rng)*0.5f, glm::vec3(0.0f), glm::vec3(1.0f));
        glm::vec3 stretch = glm::max(stretchMean + stretchVar * urand(rng), glm::vec3(0.25f));
        v[i] = { p, dir*sp, urand01(rng)*6.28318f, 0.55f + urand01(rng)*0.6f, stretch, col };
    }
}

static void initPlantsAndRocks() {
    // plants
    plantPos.resize(N_PLANTS);
    plantHP.resize(N_PLANTS);
    plantColor.resize(N_PLANTS);
    for (int i=0;i<N_PLANTS;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.9f;
        float z = urand(rng)*TANK_EXTENTS.z*0.9f;
        float h = 0.35f + urand01(rng)*0.45f;
        float phase = urand01(rng)*6.28318f;
        glm::vec3 col = glm::vec3(0.2f, 0.5f + urand01(rng)*0.4f, 0.2f);
        plantPos[i]   = glm::vec3(x, -0.9f, z);
        plantHP[i]    = glm::vec2(h, phase);
        plantColor[i] = col;
    }
    // VAO/VBO for plant instances (pos3 + hp2 + color3)
    glGenVertexArrays(1, &plantVAO);
    glBindVertexArray(plantVAO);
    glGenBuffers(1, &plantVBO);
    glBindBuffer(GL_ARRAY_BUFFER, plantVBO);
    glBufferData(GL_ARRAY_BUFFER, N_PLANTS*(sizeof(float)*8), nullptr, GL_DYNAMIC_DRAW);
    // layout: 8=pos3, 9=hp2, 10=color3 (instanced)
    glEnableVertexAttribArray(8);  glVertexAttribPointer(8,3,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)0);
    glVertexAttribDivisor(8,1);
    glEnableVertexAttribArray(9);  glVertexAttribPointer(9,2,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)(sizeof(float)*3));
    glVertexAttribDivisor(9,1);
    glEnableVertexAttribArray(10); glVertexAttribPointer(10,3,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)(sizeof(float)*5));
    glVertexAttribDivisor(10,1);
    glBindVertexArray(0);

    // rocks (random domes)
    rocks.resize(N_ROCKS);
    for (int i=0;i<N_ROCKS;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.75f;
        float z = urand(rng)*TANK_EXTENTS.z*0.75f;
        float r = 0.08f + urand01(rng)*0.18f;
        rocks[i] = glm::vec4(x, -0.9f, z, r);
    }
}

// ---- Fish VAO/VBO setup (shared mesh, per-species instance VBOs) ----
static void setupFishInstancing(GLuint &instVBO, const Mesh& m, int count) {
    if (!instVBO) glGenBuffers(1, &instVBO);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, instVBO);
    // per-instance: iPos(3), iDir(3), iPhase(1), iScale(1), iStretch(3), iColor(3) = 14 floats
    glBufferData(GL_ARRAY_BUFFER, count * (sizeof(float)*14), nullptr, GL_DYNAMIC_DRAW);
    // iPos
    glEnableVertexAttribArray(3); glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,sizeof(float)*14,(void*)0);                 glVertexAttribDivisor(3,1);
    // iDir
    glEnableVertexAttribArray(4); glVertexAttribPointer(4,3,GL_FLOAT,GL_FALSE,sizeof(float)*14,(void*)(sizeof(float)*3));  glVertexAttribDivisor(4,1);
    // iPhaseScale
    glEnableVertexAttribArray(5); glVertexAttribPointer(5,2,GL_FLOAT,GL_FALSE,sizeof(float)*14,(void*)(sizeof(float)*6));  glVertexAttribDivisor(5,1);
    // iStretch
    glEnableVertexAttribArray(6); glVertexAttribPointer(6,3,GL_FLOAT,GL_FALSE,sizeof(float)*14,(void*)(sizeof(float)*8));  glVertexAttribDivisor(6,1);
    // iColor
    glEnableVertexAttribArray(7); glVertexAttribPointer(7,3,GL_FLOAT,GL_FALSE,sizeof(float)*14,(void*)(sizeof(float)*11)); glVertexAttribDivisor(7,1);
    glBindVertexArray(0);
}

// ---- Species updates (different behaviors) ----
static void updateMinis(float dt) {
    // Light schooling, less clumpy (smaller neighbor radius, stronger separation, personal drift)
    const float maxSpeed = 1.4f;
    const float neighborDist2 = 0.18f;   // smaller
    const float avoidDist2    = 0.06f;   // larger repulsion zone

    for (auto &f : minis) {
        glm::vec3 pos=f.pos, vel=f.vel;
        glm::vec3 align(0), coh(0), sep(0);
        int count = 0;
        for (auto &o : minis) {
            if (&o==&f) continue;
            glm::vec3 d = o.pos - pos; float d2 = glm::dot(d,d);
            if (d2 < neighborDist2) {
                align += o.vel; coh += o.pos; ++count;
                if (d2 < avoidDist2) sep -= d * (0.2f / std::max(d2, 1e-4f));
            }
        }
        if (count>0) { align = glm::normalize(align/(float)count) * 0.6f; coh = (coh/(float)count) - pos; }
        // gentle individual drift target
        glm::vec3 drift(std::sin(f.phase*0.7f)*0.1f, std::sin(f.phase*1.3f)*0.05f, std::cos(f.phase*0.9f)*0.1f);
        // bounds
        glm::vec3 steer(0); glm::vec3 lim=TANK_EXTENTS; float minY=-0.75f, maxY=waterY-0.08f;
        if (pos.x> lim.x) steer.x -= (pos.x-lim.x)*2.2f; if (pos.x<-lim.x) steer.x += (-lim.x-pos.x)*2.2f;
        if (pos.z> lim.z) steer.z -= (pos.z-lim.z)*2.2f; if (pos.z<-lim.z) steer.z += (-lim.z-pos.z)*2.2f;
        if (pos.y> maxY)  steer.y -= (pos.y-maxY)*3.2f; if (pos.y< minY)  steer.y += (minY-pos.y)*3.2f;

        glm::vec3 jitter(urand(rng)*0.15f, urand(rng)*0.08f, urand(rng)*0.15f);
        vel += align*0.45f + coh*0.18f + sep*1.15f + steer*1.2f + drift*0.3f + jitter*0.25f;
        float s=glm::length(vel); if (s>maxSpeed) vel*= (maxSpeed/s);
        pos += vel*dt; f.pos=pos; f.vel=vel; f.phase += dt*3.2f;
    }
}
static void updateDarters(float dt) {
    // Free roam with curved paths and occasional bursts
    for (auto &f : darters) {
        glm::vec3 pos=f.pos, vel=f.vel;
        glm::vec3 wobble(std::sin(glfwGetTime()*1.7f + f.phase)*0.25f,
                         std::sin(glfwGetTime()*0.9f + f.phase*1.3f)*0.1f,
                         std::cos(glfwGetTime()*1.4f + f.phase)*0.25f);
        vel += wobble * 0.6f * dt * 2.0f;
        // burst
        if (urand01(rng) < 0.005f) vel *= 1.6f;
        // keep inside
        glm::vec3 lim=TANK_EXTENTS; float minY=-0.7f, maxY=waterY-0.06f;
        if (pos.x> lim.x) vel.x -= (pos.x-lim.x)*2.0f; if (pos.x<-lim.x) vel.x += (-lim.x-pos.x)*2.0f;
        if (pos.z> lim.z) vel.z -= (pos.z-lim.z)*2.0f; if (pos.z<-lim.z) vel.z += (-lim.z-pos.z)*2.0f;
        if (pos.y> maxY)  vel.y -= (pos.y-maxY)*2.0f; if (pos.y< minY)  vel.y += (minY-pos.y)*2.0f;

        float s=glm::length(vel); float maxSpeed=1.9f; if (s>maxSpeed) vel*= (maxSpeed/s);
        pos += vel*dt; f.pos=pos; f.vel=vel; f.phase += dt*2.6f;
    }
}
static void updateLoaches(float dt) {
    // Bottom huggers — mostly xz movement with slight y wiggle
    for (auto &f : loaches) {
        glm::vec3 pos=f.pos, vel=f.vel;
        vel.y = std::sin(glfwGetTime()*2.0f + f.phase)*0.02f; // gentle up/down
        // prefer along walls & floor
        glm::vec3 lim=TANK_EXTENTS; float minY=-0.86f, maxY=-0.55f;
        if (pos.x> lim.x) vel.x -= (pos.x-lim.x)*3.0f; if (pos.x<-lim.x) vel.x += (-lim.x-pos.x)*3.0f;
        if (pos.z> lim.z) vel.z -= (pos.z-lim.z)*3.0f; if (pos.z<-lim.z) vel.z += (-lim.z-pos.z)*3.0f;
        if (pos.y> maxY)  vel.y -= (pos.y-maxY)*3.0f; if (pos.y< minY)  vel.y += (minY-pos.y)*3.0f;

        // small random turns
        glm::vec3 jitter(urand(rng)*0.12f, 0.0f, urand(rng)*0.12f);
        vel += jitter*0.4f;
        float s=glm::length(vel); float maxSpeed=1.0f; if (s>maxSpeed) vel*= (maxSpeed/s);
        pos += vel*dt; f.pos=pos; f.vel=vel; f.phase += dt*3.8f;
    }
}

// ------------------------- Bubbles as before -------------------------
static const int N_BUB = 90;
static std::vector<glm::vec3> bubblePos;
static GLuint bubbleVBO = 0;
static GLuint bubbleVAO = 0;

static void initBubbles() {
    bubblePos.resize(N_BUB);
    for (int i=0;i<N_BUB;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.6f;
        float z = urand(rng)*TANK_EXTENTS.z*0.6f;
        float y = -0.9f + urand01(rng)*0.2f;
        bubblePos[i] = glm::vec3(x,y,z);
    }
    glGenVertexArrays(1,&bubbleVAO);
    glBindVertexArray(bubbleVAO);
    glGenBuffers(1,&bubbleVBO);
    glBindBuffer(GL_ARRAY_BUFFER,bubbleVBO);
    glBufferData(GL_ARRAY_BUFFER, bubblePos.size()*sizeof(glm::vec3), bubblePos.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(glm::vec3),(void*)0);
    glBindVertexArray(0);
}
static void updateBubbles(float dt) {
    for (int i=0;i<N_BUB;++i) {
        bubblePos[i].y += (0.28f + 0.18f*urand01(rng)) * dt;
        bubblePos[i].x += 0.06f * std::sin(glfwGetTime()*2.2f + i*0.31) * dt;
        if (bubblePos[i].y > waterY - 0.02f) {
            bubblePos[i].y = -0.9f + urand01(rng)*0.12f;
            bubblePos[i].x = urand(rng)*TANK_EXTENTS.x*0.6f;
            bubblePos[i].z = urand(rng)*TANK_EXTENTS.z*0.6f;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER,bubbleVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, bubblePos.size()*sizeof(glm::vec3), bubblePos.data());
}

// ------------------------- Main -------------------------
int main() {
    if (!glfwInit()) { std::cerr<<"GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
#endif
    GLFWwindow* win = glfwCreateWindow(SCR_W,SCR_H,"AquariumGL",nullptr,nullptr);
    if (!win) { std::cerr<<"Window failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);
    glfwSetCursorPosCallback(win, mouse_callback);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Load shaders
    std::string vs_basic = loadFile("shaders/basic.vert");
    std::string fs_basic = loadFile("shaders/basic.frag");   // now has fog & caustics
    std::string vs_water = loadFile("shaders/water.vert");
    std::string fs_water = loadFile("shaders/water.frag");
    std::string vs_fish  = loadFile("shaders/fish.vert");    // now supports per-inst color/stretch
    std::string fs_fish  = loadFile("shaders/fish.frag");    // now has fog, uses vColor
    std::string vs_bub   = loadFile("shaders/bubbles.vert");
    std::string fs_bub   = loadFile("shaders/bubbles.frag");
    std::string vs_plant = loadFile("shaders/plant.vert");
    std::string fs_plant = loadFile("shaders/plant.frag");

    GLuint progBasic = linkProgram(
        compileShader(GL_VERTEX_SHADER,   vs_basic.c_str(), "basic.vert"),
        compileShader(GL_FRAGMENT_SHADER, fs_basic.c_str(), "basic.frag"),
        "progBasic");

    GLuint progWater = linkProgram(
        compileShader(GL_VERTEX_SHADER,   vs_water.c_str(), "water.vert"),
        compileShader(GL_FRAGMENT_SHADER, fs_water.c_str(), "water.frag"),
        "progWater");

    GLuint progFish = linkProgram(
        compileShader(GL_VERTEX_SHADER,   vs_fish.c_str(), "fish.vert"),
        compileShader(GL_FRAGMENT_SHADER, fs_fish.c_str(), "fish.frag"),
        "progFish");

    GLuint progBub = linkProgram(
        compileShader(GL_VERTEX_SHADER,   vs_bub.c_str(), "bubbles.vert"),
        compileShader(GL_FRAGMENT_SHADER, fs_bub.c_str(), "bubbles.frag"),
        "progBub");

    GLuint progPlant = linkProgram(
        compileShader(GL_VERTEX_SHADER,   vs_plant.c_str(), "plant.vert"),
        compileShader(GL_FRAGMENT_SHADER, fs_plant.c_str(), "plant.frag"),
        "progPlant");

    // Geometry
    tankMesh  = makeBox(3.5f, 2.1f, 1.8f);
    floorMesh = makeFloor();
    waterMesh = makeWaterPlane();
    fishMesh  = makeFishMesh();
    plantMesh = makePlantStrip(12, 0.6f, 0.027f);
    rockMesh  = makeRockDome();

    // Species init
    initSpecies(minis,   N_MINIS,   /*color*/ {0.2f,0.7f,0.9f}, {0.2f,0.2f,0.2f}, /*stretch*/ {1.1f,0.7f,0.8f}, {0.2f,0.15f,0.15f}, 0.55f,1.0f, -0.5f, waterY-0.1f);
    initSpecies(darters, N_DARTERS, /*color*/ {0.95f,0.55f,0.25f}, {0.3f,0.25f,0.25f}, {1.3f,0.9f,1.0f}, {0.25f,0.1f,0.2f}, 0.8f,1.6f, -0.5f, waterY-0.07f);
    initSpecies(loaches, N_LOACHES, /*color*/ {0.55f,0.65f,0.35f}, {0.25f,0.25f,0.2f}, {1.2f,0.5f,0.9f}, {0.2f,0.1f,0.15f}, 0.4f,0.9f, -0.86f, -0.55f);
    setupFishInstancing(vboMinis,   fishMesh, N_MINIS);
    setupFishInstancing(vboDarters, fishMesh, N_DARTERS);
    setupFishInstancing(vboLoaches, fishMesh, N_LOACHES);

    initPlantsAndRocks();
    initBubbles();

    auto uLoc = [&](GLuint p, const char* n){ return glGetUniformLocation(p, n); };
    glm::vec3 lightDir = glm::normalize(glm::vec3(-0.7f,-1.2f,-0.35f));
    glm::vec3 fogColor(0.02f,0.06f,0.09f);
    float fogNear = 1.5f, fogFar = 7.0f;

    float last = (float)glfwGetTime();
    while (!glfwWindowShouldClose(win)) {
        float now=(float)glfwGetTime(), dt=now-last; last=now;
        glfwPollEvents();
        if (glfwGetKey(win, GLFW_KEY_ESCAPE)==GLFW_PRESS) glfwSetWindowShouldClose(win, 1);
        if (glfwGetKey(win, GLFW_KEY_F1)==GLFW_PRESS){ wireframe=!wireframe; glPolygonMode(GL_FRONT_AND_BACK, wireframe?GL_LINE:GL_FILL); }
        process_input(win, dt);

        updateMinis(dt);
        updateDarters(dt);
        updateLoaches(dt);
        updateBubbles(dt);

        // upload plant instances
        {
            std::vector<float> data; data.resize(N_PLANTS*8);
            for (int i=0;i<N_PLANTS;++i) {
                int o=i*8;
                data[o+0]=plantPos[i].x; data[o+1]=plantPos[i].y; data[o+2]=plantPos[i].z;
                data[o+3]=plantHP[i].x;  data[o+4]=plantHP[i].y;
                data[o+5]=plantColor[i].r; data[o+6]=plantColor[i].g; data[o+7]=plantColor[i].b;
            }
            glBindVertexArray(plantVAO);
            glBindBuffer(GL_ARRAY_BUFFER, plantVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, data.size()*sizeof(float), data.data());
            glBindVertexArray(0);
        }

        // upload fish per species
        auto uploadFish = [&](const std::vector<FishInst>& species, GLuint vbo){
            std::vector<float> inst; inst.resize(species.size()*14);
            for (size_t i=0;i<species.size();++i) {
                const auto &f = species[i];
                glm::vec3 dir = glm::length(f.vel)>1e-6f ? glm::normalize(f.vel) : glm::vec3(0,0,-1);
                size_t o = i*14;
                inst[o+0]=f.pos.x; inst[o+1]=f.pos.y; inst[o+2]=f.pos.z;
                inst[o+3]=dir.x;   inst[o+4]=dir.y;   inst[o+5]=dir.z;
                inst[o+6]=f.phase; inst[o+7]=f.scale;
                inst[o+8]=f.stretch.x; inst[o+9]=f.stretch.y; inst[o+10]=f.stretch.z;
                inst[o+11]=f.color.r;  inst[o+12]=f.color.g;  inst[o+13]=f.color.b;
            }
            glBindVertexArray(fishMesh.vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, inst.size()*sizeof(float), inst.data());
            glBindVertexArray(0);
        };
        uploadFish(minis, vboMinis);
        uploadFish(darters, vboDarters);
        uploadFish(loaches, vboLoaches);

        glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = glm::perspective(glm::radians(60.0f),(float)SCR_W/(float)SCR_H,0.05f,100.0f);
        glm::mat4 view = glm::lookAt(camPos, camPos+camFront, camUp);

        // ---------- Opaque: floor ----------
        glUseProgram(progBasic);
        glUniformMatrix4fv(uLoc(progBasic,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(uLoc(progBasic,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(uLoc(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(glm::mat4(1.0f)));
        glUniform3f(uLoc(progBasic,"uLightDir"), lightDir.x,lightDir.y,lightDir.z);
        glUniform3f(uLoc(progBasic,"uViewPos"), camPos.x, camPos.y, camPos.z);
        glUniform3f(uLoc(progBasic,"uBaseColor"), 0.75f, 0.68f, 0.45f); // sand
        glUniform3f(uLoc(progBasic, "uFogColor"), fogColor.r, fogColor.g, fogColor.b);
        glUniform1f(uLoc(progBasic, "uFogNear"),  fogNear);
        glUniform1f(uLoc(progBasic, "uFogFar"),   fogFar);
        glUniform1f(uLoc(progBasic, "uTime"),     now);
        glUniform1i(uLoc(progBasic, "uApplyCaustics"), 1);
        glBindVertexArray(floorMesh.vao);
        glDrawElements(GL_TRIANGLES, floorMesh.idxCount, GL_UNSIGNED_INT, 0);

        // ---------- Opaque: rocks ----------
        glUniform1i(uLoc(progBasic, "uApplyCaustics"), 0); // use solid color, small caustics also fine
        for (int i=0;i<N_ROCKS;++i) {
            glm::vec4 r = rocks[i];
            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(r.x, r.y, r.z))
                        * glm::scale(glm::mat4(1.0f), glm::vec3(r.w));
            glUniformMatrix4fv(uLoc(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(M));
            glUniform3f(uLoc(progBasic,"uBaseColor"), 0.35f+0.1f*urand01(rng), 0.32f, 0.28f);
            glBindVertexArray(rockMesh.vao);
            glDrawElements(GL_TRIANGLES, rockMesh.idxCount, GL_UNSIGNED_INT, 0);
        }

        // ---------- Tank (blue-ish glass walls rendered opaque for simplicity) ----------
        glUniformMatrix4fv(uLoc(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(glm::mat4(1.0f)));
        glUniform3f(uLoc(progBasic,"uBaseColor"), 0.12f,0.28f,0.45f);
        glBindVertexArray(tankMesh.vao);
        glDrawElements(GL_TRIANGLES, tankMesh.idxCount, GL_UNSIGNED_INT, 0);

        // ---------- Plants (instanced, swaying) ----------
        glUseProgram(progPlant);
        glUniformMatrix4fv(uLoc(progPlant,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(uLoc(progPlant,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniform1f(uLoc(progPlant,"uTime"), now);
        glUniform3f(uLoc(progPlant,"uLightDir"), lightDir.x,lightDir.y,lightDir.z);
        glUniform3f(uLoc(progPlant,"uViewPos"), camPos.x, camPos.y, camPos.z);
        glUniform3f(uLoc(progPlant, "uFogColor"), fogColor.r, fogColor.g, fogColor.b);
        glUniform1f(uLoc(progPlant, "uFogNear"),  fogNear);
        glUniform1f(uLoc(progPlant, "uFogFar"),   fogFar);
        // bind mesh & also bind plant instance VBO/VAO attributes onto mesh VAO
        glBindVertexArray(plantMesh.vao);
        // temporarily attach instance attributes from plantVAO onto this VAO
        glBindBuffer(GL_ARRAY_BUFFER, plantVBO);
        glEnableVertexAttribArray(8);  glVertexAttribPointer(8,3,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)0);                 glVertexAttribDivisor(8,1);
        glEnableVertexAttribArray(9);  glVertexAttribPointer(9,2,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)(sizeof(float)*3));  glVertexAttribDivisor(9,1);
        glEnableVertexAttribArray(10); glVertexAttribPointer(10,3,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)(sizeof(float)*5)); glVertexAttribDivisor(10,1);
        glDrawElementsInstanced(GL_TRIANGLES, plantMesh.idxCount, GL_UNSIGNED_INT, 0, N_PLANTS);
        // disable if you like (not strictly necessary)
        glBindVertexArray(0);

        // ---------- Fish (three species) ----------
        auto drawSpecies = [&](const std::vector<FishInst>& v, GLuint vbo){
            glBindVertexArray(fishMesh.vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            // attributes already configured in setupFishInstancing()
            glUseProgram(progFish);
            glUniformMatrix4fv(uLoc(progFish,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
            glUniformMatrix4fv(uLoc(progFish,"uView"),1,GL_FALSE,glm::value_ptr(view));
            glUniform3f(uLoc(progFish,"uLightDir"), lightDir.x, lightDir.y, lightDir.z);
            glUniform3f(uLoc(progFish,"uViewPos"), camPos.x, camPos.y, camPos.z);
            glUniform1f(uLoc(progFish,"uTime"), now);
            glUniform3f(uLoc(progFish, "uFogColor"), fogColor.r, fogColor.g, fogColor.b);
            glUniform1f(uLoc(progFish, "uFogNear"),  fogNear);
            glUniform1f(uLoc(progFish, "uFogFar"),   fogFar);
            glDrawElementsInstanced(GL_TRIANGLES, fishMesh.idxCount, GL_UNSIGNED_INT, 0, (GLsizei)v.size());
            glBindVertexArray(0);
        };
        drawSpecies(minis, vboMinis);
        drawSpecies(darters, vboDarters);
        drawSpecies(loaches, vboLoaches);

        // ---------- Bubbles ----------
        glUseProgram(progBub);
        glUniformMatrix4fv(uLoc(progBub,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(uLoc(progBub,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glBindVertexArray(bubbleVAO);
        glDrawArrays(GL_POINTS, 0, N_BUB);

        // ---------- Water LAST ----------
        glUseProgram(progWater);
        glUniformMatrix4fv(uLoc(progWater,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(uLoc(progWater,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(uLoc(progWater,"uModel"),1,GL_FALSE,glm::value_ptr(glm::mat4(1.0f)));
        glUniform1f(uLoc(progWater,"uTime"), now);
        glUniform3f(uLoc(progWater,"uDeepColor"), 0.0f, 0.25f, 0.45f);
        glUniform3f(uLoc(progWater,"uShallowColor"), 0.1f, 0.6f, 0.8f);
        glBindVertexArray(waterMesh.vao);
        glDisable(GL_CULL_FACE);
        glDrawElements(GL_TRIANGLES, waterMesh.idxCount, GL_UNSIGNED_INT, 0);
        glEnable(GL_CULL_FACE);

        glfwSwapBuffers(win);
    }
    glfwTerminate();
    return 0;
}
