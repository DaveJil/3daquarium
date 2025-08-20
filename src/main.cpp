#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>

#ifdef __APPLE__
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl3.h>
#else
  #error "This setup is macOS-only. For Windows/Linux, use the GLEW/GLAD path."
#endif
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

static int SCR_W = 1280, SCR_H = 720;

static float camYaw = -90.0f, camPitch = -5.0f;
static glm::vec3 camPos(0.0f, 1.2f, 4.0f);
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
    float sensitivity = 0.1f;
    xoffset *= sensitivity; yoffset *= sensitivity;
    camYaw += xoffset; camPitch += yoffset;
    camPitch = std::clamp(camPitch, -89.0f, 89.0f);
    glm::vec3 front;
    front.x = cos(glm::radians(camYaw)) * cos(glm::radians(camPitch));
    front.y = sin(glm::radians(camPitch));
    front.z = sin(glm::radians(camYaw)) * cos(glm::radians(camPitch));
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

static GLuint compileShader(GLenum type, const char* src, const char* name) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]; glGetShaderInfoLog(s, 2048, nullptr, log);
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
        char log[2048]; glGetProgramInfoLog(p, 2048, nullptr, log);
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

struct Mesh { GLuint vao=0, vbo=0, ebo=0; GLsizei idxCount=0; };

// ---- Tank walls (inward normals) ----
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

// ---- Water surface (pos+uv) ----
static Mesh makeWaterPlane(int nx=120, int nz=120, float sx=3.0f, float sz=1.6f, float y=0.4f) {
    struct V { glm::vec3 p; glm::vec2 uv; };
    std::vector<V> v; v.reserve((nx+1)*(nz+1));
    for (int z=0; z<=nz; ++z) {
        for (int x=0; x<=nx; ++x) {
            float u = (float)x/nx, w=(float)z/nz;
            v.push_back({ glm::vec3((u-0.5f)*sx, y, (w-0.5f)*sz), glm::vec2(u,w) });
        }
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

// ---- Floor (solid plane with normals) ----
static Mesh makeFloor(float sx=3.0f, float sz=1.6f, float y=-0.85f) {
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

// ---- Simple low-poly fish mesh (local X = 0..1 along body) ----
static Mesh makeFishMesh() {
    // positions + normals for a wedge body and tail fins
    struct V { glm::vec3 p,n; };
    std::vector<V> v;
    std::vector<unsigned> idx;

    auto addTri = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c){
        glm::vec3 n = glm::normalize(glm::cross(b-a, c-a));
        unsigned base = (unsigned)v.size();
        v.push_back({a,n}); v.push_back({b,n}); v.push_back({c,n});
        idx.insert(idx.end(), {base, base+1, base+2});
    };

    // Body (a tiny prism)
    float H = 0.08f, W = 0.06f, L = 1.0f;
    glm::vec3 head = {0.0f, 0.0f, 0.0f};
    glm::vec3 b1 = {0.6f,  H,  0.0f};
    glm::vec3 b2 = {0.6f, -H,  0.0f};
    glm::vec3 b3 = {0.6f,  0.0f,  W};
    glm::vec3 b4 = {0.6f,  0.0f, -W};

    // sides
    addTri(head, b1, b3);
    addTri(head, b3, b2);
    addTri(head, b2, b4);
    addTri(head, b4, b1);

    // Tail (two triangles at x=L)
    glm::vec3 tL = {L,  0.0f,  0.0f};
    glm::vec3 tU = {L,  0.12f, 0.05f};
    glm::vec3 tD = {L, -0.12f, 0.05f};
    glm::vec3 tU2= {L,  0.12f,-0.05f};
    glm::vec3 tD2= {L, -0.12f,-0.05f};
    addTri(b3, tU, tD);
    addTri(b4, tU2, tD2);

    // Dorsal + ventral fins (small)
    addTri({0.3f, H, 0.0f}, {0.45f, H+0.08f, 0.0f}, {0.4f, H, 0.02f});
    addTri({0.3f,-H, 0.0f}, {0.45f,-H-0.08f,0.0f}, {0.4f,-H, 0.02f});

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

// ---------------- INSTANCE + PARTICLE DATA ----------------
struct Fish { glm::vec3 pos, vel; float phase, scale; };
static std::vector<Fish> fish;
static GLuint fishInstanceVBO = 0;   // holds iPos(3) + iDir(3) + iPhaseScale(2)
static Mesh fishMesh;

static const int N_FISH = 120;
static const glm::vec3 TANK_EXTENTS = {3.5f*0.5f - 0.05f, 0.8f, 1.6f*0.5f - 0.05f}; // inner bounds
static float waterY = 0.4f;

static std::mt19937 rng(1337);
static std::uniform_real_distribution<float> urand(-1.0f, 1.0f);
static std::uniform_real_distribution<float> urand01(0.0f, 1.0f);

static void initFish() {
    fish.resize(N_FISH);
    for (int i=0;i<N_FISH;++i) {
        fish[i].pos = glm::vec3(urand(rng)*TANK_EXTENTS.x*0.9f,
                                urand01(rng)*0.8f - 0.4f,
                                urand(rng)*TANK_EXTENTS.z*0.9f);
        glm::vec3 dir = glm::normalize(glm::vec3(urand(rng), urand(rng)*0.2f, urand(rng)));
        fish[i].vel = dir * (0.6f + urand01(rng)*0.8f);
        fish[i].phase = urand01(rng) * 6.28318f;
        fish[i].scale = 0.7f + urand01(rng)*0.8f; // different sizes
    }
    glGenBuffers(1, &fishInstanceVBO);
}

// simple schooling-ish motion
static void updateFish(float dt) {
    const float maxSpeed = 1.6f;
    const float neighborDist2 = 0.25f;   // squared
    const float avoidDist2    = 0.04f;

    for (int i=0;i<N_FISH;++i) {
        glm::vec3 pos = fish[i].pos;
        glm::vec3 vel = fish[i].vel;

        // neighborhood
        glm::vec3 align(0), coh(0), sep(0);
        int count = 0;
        for (int j=0;j<N_FISH;++j) if (j!=i) {
            glm::vec3 d = fish[j].pos - pos;
            float d2 = glm::dot(d,d);
            if (d2 < neighborDist2) {
                align += fish[j].vel;
                coh   += fish[j].pos;
                count++;
                if (d2 < avoidDist2) sep -= d * (0.05f / std::max(d2, 1e-4f));
            }
        }
        if (count>0) {
            align = glm::normalize(align / (float)count) * 0.6f;
            coh   = (coh / (float)count) - pos;
        }

        // boundaries (keep inside tank, below water)
        glm::vec3 steer(0);
        glm::vec3 limit = TANK_EXTENTS;
        if (pos.x >  limit.x) steer.x -= (pos.x - limit.x)*2.0f;
        if (pos.x < -limit.x) steer.x += (-limit.x - pos.x)*2.0f;
        if (pos.z >  limit.z) steer.z -= (pos.z - limit.z)*2.0f;
        if (pos.z < -limit.z) steer.z += (-limit.z - pos.z)*2.0f;

        float minY = -0.8f, maxY = waterY - 0.05f;
        if (pos.y > maxY) steer.y -= (pos.y - maxY)*3.0f;
        if (pos.y < minY) steer.y += (minY - pos.y)*3.0f;

        // random wander
        glm::vec3 jitter(urand(rng)*0.2f, urand(rng)*0.1f, urand(rng)*0.2f);

        // combine
        vel += align*0.6f + coh*0.25f + sep*0.9f + steer*1.2f + jitter*0.3f;
        float s = glm::length(vel);
        if (s > maxSpeed) vel = vel * (maxSpeed / s);

        pos += vel * dt;
        fish[i].pos = pos;
        fish[i].vel = vel;
    }
}

// ---- Bubbles (as point sprites) ----
static const int N_BUB = 80;
static std::vector<glm::vec3> bubblePos;
static GLuint bubbleVBO = 0;
static GLuint bubbleVAO = 0;

static void initBubbles() {
    bubblePos.resize(N_BUB);
    for (int i=0;i<N_BUB;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.8f;
        float z = urand(rng)*TANK_EXTENTS.z*0.6f;
        float y = -0.85f + urand01(rng)*0.2f;
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
        bubblePos[i].y += (0.25f + 0.15f*urand01(rng)) * dt;
        bubblePos[i].x += 0.05f * sin(glfwGetTime()*2.0 + i*0.37) * dt; // small wobble
        if (bubblePos[i].y > waterY - 0.02f) {
            bubblePos[i].y = -0.85f + urand01(rng)*0.1f;
            bubblePos[i].x = urand(rng)*TANK_EXTENTS.x*0.8f;
            bubblePos[i].z = urand(rng)*TANK_EXTENTS.z*0.6f;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER,bubbleVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, bubblePos.size()*sizeof(glm::vec3), bubblePos.data());
}

// --------------- MAIN ---------------
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

    // programs
    std::string vs_basic = loadFile("shaders/basic.vert");
    std::string fs_basic = loadFile("shaders/basic.frag");
    std::string vs_water = loadFile("shaders/water.vert");
    std::string fs_water = loadFile("shaders/water.frag");
    std::string vs_fish  = loadFile("shaders/fish.vert");
    std::string fs_fish  = loadFile("shaders/fish.frag");
    std::string vs_bub   = loadFile("shaders/bubbles.vert");
    std::string fs_bub   = loadFile("shaders/bubbles.frag");

    GLuint progBasic = linkProgram(
        compileShader(GL_VERTEX_SHADER, vs_basic.c_str(), "basic.vert"),
        compileShader(GL_FRAGMENT_SHADER, fs_basic.c_str(), "basic.frag"),
        "progBasic");

    GLuint progWater = linkProgram(
        compileShader(GL_VERTEX_SHADER, vs_water.c_str(), "water.vert"),
        compileShader(GL_FRAGMENT_SHADER, fs_water.c_str(), "water.frag"),
        "progWater");

    GLuint progFish = linkProgram(
        compileShader(GL_VERTEX_SHADER, vs_fish.c_str(), "fish.vert"),
        compileShader(GL_FRAGMENT_SHADER, fs_fish.c_str(), "fish.frag"),
        "progFish");

    GLuint progBub = linkProgram(
        compileShader(GL_VERTEX_SHADER, vs_bub.c_str(), "bubbles.vert"),
        compileShader(GL_FRAGMENT_SHADER, fs_bub.c_str(), "bubbles.frag"),
        "progBub");

    // geometry
    Mesh tank = makeBox(3.5f, 2.0f, 1.6f);
    Mesh water = makeWaterPlane();
    Mesh floor = makeFloor();
    fishMesh   = makeFishMesh();

    // fish instance buffer (iPos, iDir, iPhaseScale)
    initFish();
    glBindVertexArray(fishMesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, fishInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, N_FISH*(sizeof(float)*8), nullptr, GL_DYNAMIC_DRAW);
    // iPos (3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)0);
    glVertexAttribDivisor(3,1);
    // iDir (3)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4,3,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)(sizeof(float)*3));
    glVertexAttribDivisor(4,1);
    // iPhaseScale (2)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5,2,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)(sizeof(float)*6));
    glVertexAttribDivisor(5,1);
    glBindVertexArray(0);

    // bubbles
    initBubbles();

    auto uLoc = [&](GLuint p, const char* n){ return glGetUniformLocation(p, n); };

    float last = (float)glfwGetTime();
    while (!glfwWindowShouldClose(win)) {
        float now=(float)glfwGetTime(), dt=now-last; last=now;
        glfwPollEvents();
        if (glfwGetKey(win, GLFW_KEY_ESCAPE)==GLFW_PRESS) glfwSetWindowShouldClose(win, 1);
        if (glfwGetKey(win, GLFW_KEY_F1)==GLFW_PRESS){ wireframe=!wireframe; glPolygonMode(GL_FRONT_AND_BACK, wireframe?GL_LINE:GL_FILL); }
        process_input(win, dt);

        updateFish(dt);
        updateBubbles(dt);

        // upload instance data
        std::vector<float> inst; inst.resize(N_FISH*8);
        for (int i=0;i<N_FISH;++i) {
            glm::vec3 dir = glm::length(fish[i].vel)>1e-6f ? glm::normalize(fish[i].vel) : glm::vec3(0,0,-1);
            int o = i*8;
            inst[o+0]=fish[i].pos.x; inst[o+1]=fish[i].pos.y; inst[o+2]=fish[i].pos.z;
            inst[o+3]=dir.x;         inst[o+4]=dir.y;         inst[o+5]=dir.z;
            inst[o+6]=fish[i].phase; inst[o+7]=fish[i].scale;
            fish[i].phase += dt * 3.5f;
        }
        glBindBuffer(GL_ARRAY_BUFFER, fishInstanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, inst.size()*sizeof(float), inst.data());

        glClearColor(0.02f,0.05f,0.09f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = glm::perspective(glm::radians(60.0f),(float)SCR_W/(float)SCR_H,0.05f,100.0f);
        glm::mat4 view = glm::lookAt(camPos, camPos+camFront, camUp);

        // Tank walls
        glUseProgram(progBasic);
        glm::vec3 ldir = glm::normalize(glm::vec3(-1.0f,-1.5f,-0.4f));
        glUniformMatrix4fv(uLoc(progBasic,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(uLoc(progBasic,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(uLoc(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(glm::mat4(1.0f)));
        glUniform3f(uLoc(progBasic,"uColor"), 0.15f,0.35f,0.6f);
        glUniform3f(uLoc(progBasic,"uLightDir"), ldir.x,ldir.y,ldir.z);
        glUniform3f(uLoc(progBasic,"uViewPos"), camPos.x, camPos.y, camPos.z);
        glBindVertexArray(tank.vao);
        glDrawElements(GL_TRIANGLES, tank.idxCount, GL_UNSIGNED_INT, 0);

        // Floor (sand)
        glUniform3f(uLoc(progBasic,"uColor"), 0.75f, 0.68f, 0.45f);
        glBindVertexArray(floor.vao);
        glDrawElements(GL_TRIANGLES, floor.idxCount, GL_UNSIGNED_INT, 0);

        // Water surface
        glUseProgram(progWater);
        glUniformMatrix4fv(uLoc(progWater,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(uLoc(progWater,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(uLoc(progWater,"uModel"),1,GL_FALSE,glm::value_ptr(glm::mat4(1.0f)));
        glUniform1f(uLoc(progWater,"uTime"), now);
        glUniform3f(uLoc(progWater,"uDeepColor"), 0.0f, 0.25f, 0.45f);
        glUniform3f(uLoc(progWater,"uShallowColor"), 0.1f, 0.6f, 0.8f);
        glBindVertexArray(water.vao);
        glDisable(GL_CULL_FACE);
        glDrawElements(GL_TRIANGLES, water.idxCount, GL_UNSIGNED_INT, 0);
        glEnable(GL_CULL_FACE);

        // Fish (instanced)
        glUseProgram(progFish);
        glUniformMatrix4fv(uLoc(progFish,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(uLoc(progFish,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniform3f(uLoc(progFish,"uLightDir"), ldir.x, ldir.y, ldir.z);
        glUniform3f(uLoc(progFish,"uViewPos"), camPos.x, camPos.y, camPos.z);
        glUniform1f(uLoc(progFish,"uTime"), now);
        glBindVertexArray(fishMesh.vao);
        glDrawElementsInstanced(GL_TRIANGLES, fishMesh.idxCount, GL_UNSIGNED_INT, 0, N_FISH);

        // Bubbles
        glUseProgram(progBub);
        glUniformMatrix4fv(uLoc(progBub,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(uLoc(progBub,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glBindVertexArray(bubbleVAO);
        glDrawArrays(GL_POINTS, 0, N_BUB);

        glfwSwapBuffers(win);
    }
    glfwTerminate();
    return 0;
}
