#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>

#ifdef __APPLE__
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl3.h>
#else
  #error "This setup targets macOS OpenGL 4.1 core profile."
#endif
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ===========================================================
// Window/camera/controls
// ===========================================================
static int SCR_W = 1280, SCR_H = 720;

static float camYaw = -90.0f, camPitch = -5.0f;
static glm::vec3 camPos(0.0f, 0.20f, 0.6f);
static glm::vec3 camFront(0.0f, 0.0f, -1.0f);
static glm::vec3 camUp(0.0f, 1.0f, 0.0f);
static bool firstMouse = true;
static double lastX = SCR_W * 0.5, lastY = SCR_H * 0.5;
static bool wireframe = false;

// Camera modes and controls
static bool orbitMode = false;
static float orbitRadius = 3.0f;
static float orbitAngle = 0.0f;
static glm::vec3 orbitCenter(0.0f, 0.0f, 0.0f);
static bool paused = false;
static float timeScale = 1.0f;

// ===========================================================
// HDR render targets & screen triangle
// ===========================================================
static GLuint hdrFBO = 0, hdrColorTex = 0, hdrDepthRBO = 0, opaqueCopyTex = 0;
static GLuint screenVAO = 0;

static void createOrResizeHDR() {
    if (!hdrFBO) glGenFramebuffers(1, &hdrFBO);
    if (!hdrColorTex) glGenTextures(1, &hdrColorTex);
    if (!opaqueCopyTex) glGenTextures(1, &opaqueCopyTex);
    if (!hdrDepthRBO) glGenRenderbuffers(1, &hdrDepthRBO);

    glBindTexture(GL_TEXTURE_2D, hdrColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_W, SCR_H, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, opaqueCopyTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_W, SCR_H, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindRenderbuffer(GL_RENDERBUFFER, hdrDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_W, SCR_H);

    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrColorTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, hdrDepthRBO);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "HDR FBO incomplete!\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ===========================================================
// Input
// ===========================================================
static void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    SCR_W = w; SCR_H = h;
    glViewport(0, 0, SCR_W, SCR_H);
    createOrResizeHDR();
}
static void mouse_callback(GLFWwindow*, double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = float(xpos - lastX);
    float yoffset = float(lastY - ypos);
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

// ===========================================================
// Utils
// ===========================================================
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

// ===========================================================
// Geometry
// ===========================================================
struct Mesh { GLuint vao=0, vbo=0, ebo=0; GLsizei idxCount=0; };

// Create a proper fish mesh with good visibility
static Mesh createFishMesh() {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    auto push = [&](const glm::vec3& p, const glm::vec3& n){ v.push_back({p, glm::normalize(n)}); };

    const int segX=24, segR=16;
    const float rMax=0.08f, zFlatten=0.7f; // Larger and more visible
    for (int i=0; i<=segX; ++i) {
        float t = (float)i / (float)segX;
        float r = rMax * std::pow(std::sin(3.14159f * std::clamp(t*1.02f, 0.0f, 1.0f)), 0.75f);
        if (i == 0) r *= 0.5f; // Head
        if (i > segX * 0.8f) r *= 0.6f; // Tail taper
        for (int j=0; j<=segR; ++j) {
            float a = (2.0f * 3.14159f) * (float)j / (float)segR;
            float cy = std::cos(a), sy = std::sin(a);
            glm::vec3 p = { t * 0.25f, r * cy, zFlatten * r * sy }; // Scale down length
            glm::vec3 n = { 0.0f, cy, (1.0f / zFlatten) * sy };
            push(p, n);
        }
    }
    int ring = segR + 1;
    for (int i=0; i<segX; ++i) for (int j=0; j<segR; ++j) {
        unsigned a = i*ring + j, b=a+1, c=(i+1)*ring + j, d=c+1;
        idx.insert(idx.end(), {a,c,b, b,c,d});
    }

    // Add nose cap
    glm::vec3 nose = {0.0f, 0.0f, 0.0f};
    unsigned baseCenter = (unsigned)v.size();
    v.push_back({nose, glm::vec3(-1,0,0)});
    for (int j=0; j<segR; ++j) { 
        unsigned a=j, b=(j+1)%segR; 
        idx.insert(idx.end(), {baseCenter, a, b}); 
    }
    
    // Add tail fin
    float x = 0.26f;
    glm::vec3 tU = {x,  0.12f,  0.0f}, tD = {x, -0.12f,  0.0f};
    glm::vec3 baseL = {0.22f,  0.03f,  0.02f}, baseR = {0.22f, -0.03f,  0.02f};
    glm::vec3 baseL2= {0.22f,  0.03f, -0.02f}, baseR2= {0.22f, -0.03f, -0.02f};
    unsigned s = (unsigned)v.size();
    v.push_back({tU,{0,0, 1}}); v.push_back({tD,{0,0, 1}}); v.push_back({baseL,{0,0, 1}}); v.push_back({baseR,{0,0, 1}});
    v.push_back({tU,{0,0,-1}}); v.push_back({tD,{0,0,-1}}); v.push_back({baseL2,{0,0,-1}}); v.push_back({baseR2,{0,0,-1}});
    idx.insert(idx.end(), {s+2,s+0,s+1,  s+2,s+1,s+3});
    idx.insert(idx.end(), {s+5,s+7,s+4,  s+5,s+6,s+7});

    Mesh m; glGenVertexArrays(1,&m.vao); glBindVertexArray(m.vao);
    glGenBuffers(1,&m.vbo); glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size()*sizeof(V), v.data(), GL_STATIC_DRAW);
    glGenBuffers(1,&m.ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,n));
    
    // Set up instancing attributes
    glEnableVertexAttribArray(3); glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,15*sizeof(float),(void*)0);                 glVertexAttribDivisor(3,1);
    glEnableVertexAttribArray(4); glVertexAttribPointer(4,3,GL_FLOAT,GL_FALSE,15*sizeof(float),(void*)(3*sizeof(float)));  glVertexAttribDivisor(4,1);
    glEnableVertexAttribArray(5); glVertexAttribPointer(5,2,GL_FLOAT,GL_FALSE,15*sizeof(float),(void*)(6*sizeof(float)));  glVertexAttribDivisor(5,1);
    glEnableVertexAttribArray(6); glVertexAttribPointer(6,3,GL_FLOAT,GL_FALSE,15*sizeof(float),(void*)(8*sizeof(float)));  glVertexAttribDivisor(6,1);
    glEnableVertexAttribArray(7); glVertexAttribPointer(7,3,GL_FLOAT,GL_FALSE,15*sizeof(float),(void*)(11*sizeof(float))); glVertexAttribDivisor(7,1);
    glEnableVertexAttribArray(8); glVertexAttribPointer(8,1,GL_FLOAT,GL_FALSE,15*sizeof(float),(void*)(14*sizeof(float))); glVertexAttribDivisor(8,1);
    
    m.idxCount=(GLsizei)idx.size();
    glBindVertexArray(0); return m;
}

// Simple OBJ loader with better error reporting
static Mesh loadOBJModel(const std::string& filename) {
    std::cout << "Attempting to load: " << filename << std::endl;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "ERROR: Failed to open OBJ file: " << filename << std::endl;
        std::cerr << "Current working directory should contain: fish.obj, koi_fish.obj, bream_fish__dorade_royale.obj, fish_animated.obj" << std::endl;
        std::cerr << "Using fallback procedural mesh instead." << std::endl;
        return createFishMesh(); // Use our procedural fish mesh as fallback
    }
    
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<unsigned> indices;
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string type;
        iss >> type;
        
        if (type == "v") {
            float x, y, z;
            iss >> x >> y >> z;
            positions.push_back(glm::vec3(x, y, z));
        } else if (type == "vn") {
            float x, y, z;
            iss >> x >> y >> z;
            normals.push_back(glm::normalize(glm::vec3(x, y, z)));
        } else if (type == "f") {
            std::string vertex;
            for (int i = 0; i < 3; ++i) {
                iss >> vertex;
                std::istringstream viss(vertex);
                std::string index_str;
                std::vector<int> indices_vertex;
                
                while (std::getline(viss, index_str, '/')) {
                    if (index_str.empty()) {
                        indices_vertex.push_back(0);
                    } else {
                        indices_vertex.push_back(std::stoi(index_str) - 1);
                    }
                }
                
                while (indices_vertex.size() < 3) {
                    indices_vertex.push_back(0);
                }
                
                indices.push_back(indices_vertex[0]);
            }
        }
    }
    
    if (positions.empty()) {
        std::cerr << "ERROR: No vertices found in OBJ file: " << filename << std::endl;
        return createFishMesh();
    }
    
    // Scale to appropriate size for aquarium - make them quite large and visible
    float scale = 0.15f; // Larger scale for better visibility
    
    std::cout << "SUCCESS: Loaded " << filename << " with " << positions.size() << " vertices, " << indices.size() << " indices" << std::endl;
    
    struct V { glm::vec3 p, n; };
    std::vector<V> vertices;
    
    for (size_t i = 0; i < positions.size(); ++i) {
        V v;
        v.p = positions[i] * scale;
        v.n = (i < normals.size()) ? normals[i] : glm::vec3(0, 1, 0);
        vertices.push_back(v);
    }
    
    Mesh m;
    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);
    
    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(V), vertices.data(), GL_STATIC_DRAW);
    
    if (!indices.empty()) {
        glGenBuffers(1, &m.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);
        m.idxCount = (GLsizei)indices.size();
    } else {
        m.idxCount = (GLsizei)vertices.size();
    }
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, n));
    
    // Set up instancing attributes
    glEnableVertexAttribArray(3); glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,15*sizeof(float),(void*)0);                 glVertexAttribDivisor(3,1);
    glEnableVertexAttribArray(4); glVertexAttribPointer(4,3,GL_FLOAT,GL_FALSE,15*sizeof(float),(void*)(3*sizeof(float)));  glVertexAttribDivisor(4,1);
    glEnableVertexAttribArray(5); glVertexAttribPointer(5,2,GL_FLOAT,GL_FALSE,15*sizeof(float),(void*)(6*sizeof(float)));  glVertexAttribDivisor(5,1);
    glEnableVertexAttribArray(6); glVertexAttribPointer(6,3,GL_FLOAT,GL_FALSE,15*sizeof(float),(void*)(8*sizeof(float)));  glVertexAttribDivisor(6,1);
    glEnableVertexAttribArray(7); glVertexAttribPointer(7,3,GL_FLOAT,GL_FALSE,15*sizeof(float),(void*)(11*sizeof(float))); glVertexAttribDivisor(7,1);
    glEnableVertexAttribArray(8); glVertexAttribPointer(8,1,GL_FLOAT,GL_FALSE,15*sizeof(float),(void*)(14*sizeof(float))); glVertexAttribDivisor(8,1);
    
    glBindVertexArray(0);
    return m;
}

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
    addQuad({-x, y, z},{ x, y, z},{ x, y,-z},{-x, y,-z}, { 0,-1, 0});
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

static Mesh makeGlassTank(float w, float h, float d, float thickness = 0.05f) {
    float x=w*0.5f, y=h*0.5f, z=d*0.5f;
    float t=thickness;
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> i;
    auto addQuad=[&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n){
        size_t base=v.size();
        v.push_back({a,n}); v.push_back({b,n}); v.push_back({c,n}); v.push_back({d,n});
        i.insert(i.end(), { (unsigned)base,(unsigned)base+2,(unsigned)base+1,
                            (unsigned)base,(unsigned)base+3,(unsigned)base+2 });
    };
    
    // Bottom glass panel (exterior)
    addQuad({-x,-y-t,-z},{ x,-y-t,-z},{ x,-y-t, z},{-x,-y-t, z}, { 0, 1, 0});
    // Bottom glass panel (interior)  
    addQuad({-x,-y,-z},{-x,-y, z},{ x,-y, z},{ x,-y,-z}, { 0,-1, 0});
    
    // Left wall (exterior)
    addQuad({-x-t,-y-t,-z},{-x-t, y,-z},{-x-t, y, z},{-x-t,-y-t, z}, { 1, 0, 0});
    // Left wall (interior)
    addQuad({-x,-y,-z},{-x,-y, z},{-x, y, z},{-x, y,-z}, {-1, 0, 0});
    
    // Right wall (exterior)
    addQuad({ x+t,-y-t, z},{ x+t, y, z},{ x+t, y,-z},{ x+t,-y-t,-z}, {-1, 0, 0});
    // Right wall (interior)
    addQuad({ x,-y, z},{ x, y, z},{ x, y,-z},{ x,-y,-z}, { 1, 0, 0});
    
    // Front wall (exterior)
    addQuad({-x-t,-y-t, z-t},{ x+t,-y-t, z-t},{ x+t, y, z-t},{-x-t, y, z-t}, { 0, 0,-1});
    // Front wall (interior)
    addQuad({-x,-y, z},{ x,-y, z},{ x, y, z},{-x, y, z}, { 0, 0, 1});
    
    // Back wall (exterior)
    addQuad({ x+t,-y-t,-z-t},{-x-t,-y-t,-z-t},{-x-t, y,-z-t},{ x+t, y,-z-t}, { 0, 0, 1});
    // Back wall (interior)
    addQuad({ x,-y,-z},{-x,-y,-z},{-x, y,-z},{ x, y,-z}, { 0, 0,-1});
    
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

static Mesh makeWaterVolume(float w, float h, float d, float waterLevel = 0.9f) {
    float x=w*0.5f*0.95f, y=h*waterLevel*0.5f, z=d*0.5f*0.95f; // Slightly smaller than tank interior
    float bottom = -h*0.5f + 0.02f; // Just above tank bottom
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> i;
    auto addQuad=[&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n){
        size_t base=v.size();
        v.push_back({a,n}); v.push_back({b,n}); v.push_back({c,n}); v.push_back({d,n});
        i.insert(i.end(), { (unsigned)base,(unsigned)base+1,(unsigned)base+2,
                            (unsigned)base,(unsigned)base+2,(unsigned)base+3 });
    };
    
    // Water volume faces (all inward-facing normals for proper transparency)
    addQuad({-x,bottom,-z},{ x,bottom,-z},{ x,bottom, z},{-x,bottom, z}, { 0, 1, 0}); // Bottom
    addQuad({-x,bottom,-z},{-x, y,-z},{-x, y, z},{-x,bottom, z}, { 1, 0, 0}); // Left
    addQuad({ x,bottom, z},{ x, y, z},{ x, y,-z},{ x,bottom,-z}, {-1, 0, 0}); // Right  
    addQuad({-x,bottom, z},{-x, y, z},{ x, y, z},{ x,bottom, z}, { 0, 0,-1}); // Front
    addQuad({ x,bottom,-z},{ x, y,-z},{-x, y,-z},{-x,bottom,-z}, { 0, 0, 1}); // Back
    addQuad({-x, y, z},{ x, y, z},{ x, y,-z},{-x, y,-z}, { 0,-1, 0}); // Top (water surface)
    
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

static Mesh makeTankBase(float w, float h, float d) {
    float bw = w * 1.3f, bh = h * 0.15f, bd = d * 1.3f; // Base is wider and shorter
    float x=bw*0.5f, y=bh*0.5f, z=bd*0.5f;
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> i;
    auto addQuad=[&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n){
        size_t base=v.size();
        v.push_back({a,n}); v.push_back({b,n}); v.push_back({c,n}); v.push_back({d,n});
        i.insert(i.end(), { (unsigned)base,(unsigned)base+2,(unsigned)base+1,
                            (unsigned)base,(unsigned)base+3,(unsigned)base+2 });
    };
    
    // All 6 faces of the base
    addQuad({-x,-y, z},{ x,-y, z},{ x, y, z},{-x, y, z}, { 0, 0,-1}); // Front
    addQuad({ x,-y,-z},{-x,-y,-z},{-x, y,-z},{ x, y,-z}, { 0, 0, 1}); // Back
    addQuad({-x,-y,-z},{-x,-y, z},{-x, y, z},{-x, y,-z}, { 1, 0, 0}); // Left
    addQuad({ x,-y, z},{ x,-y,-z},{ x, y,-z},{ x, y, z}, {-1, 0, 0}); // Right
    addQuad({-x, y, z},{ x, y, z},{ x, y,-z},{-x, y,-z}, { 0,-1, 0}); // Top
    addQuad({-x,-y,-z},{ x,-y,-z},{ x,-y, z},{-x,-y, z}, { 0, 1, 0}); // Bottom
    
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
    glBindVertexArray(0); return m;
}
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
    glBindVertexArray(0); return m;
}

static Mesh makePlantStrip(int segments = 12, float height = 0.6f, float width = 0.027f) {
    struct V { glm::vec3 p,n; };
    std::vector<V> v;
    std::vector<unsigned> idx;
    
    // Create 4 strips at different angles for 3D appearance
    const int numStrips = 4;
    for (int strip = 0; strip < numStrips; ++strip) {
        float angle = (2.0f * 3.14159f) * (float)strip / (float)numStrips;
        glm::vec3 stripDir = glm::vec3(std::cos(angle), 0.0f, std::sin(angle));
        glm::vec3 stripNormal = glm::vec3(-std::sin(angle), 0.0f, std::cos(angle));
        
        int baseVertex = (int)v.size();
        
        for (int i=0;i<=segments;++i) {
            float t = (float)i/segments;
            float y = t * height;
            float w = width * (0.7f + 0.3f * (1.0f - t));
            float sway = 0.05f * std::sin(t * 6.0f) * t; // Natural plant sway
            
            glm::vec3 offset = stripDir * w * 0.5f;
            glm::vec3 swayOffset = glm::vec3(sway, 0.0f, sway * 0.5f);
            
            v.push_back({glm::vec3(-offset.x + swayOffset.x, y, -offset.z + swayOffset.z), stripNormal});
            v.push_back({glm::vec3( offset.x + swayOffset.x, y,  offset.z + swayOffset.z), stripNormal});
            
            if (i < segments) {
                unsigned base = baseVertex + i*2;
                idx.insert(idx.end(), {base, base+2, base+1,  base+1, base+2, base+3});
                // Add back faces for proper 3D appearance
                idx.insert(idx.end(), {base+1, base+2, base,  base+3, base+2, base+1});
            }
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
    glBindVertexArray(0); return m;
}
static Mesh makeRockDome(int rings=12, int sectors=18, float radius=0.22f) {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    for (int r=0;r<=rings;++r) {
        float vr = (float)r/rings;
        float phi = (vr*0.5f)*3.14159f;
        for (int s=0;s<=sectors;++s) {
            float vs = (float)s/sectors;
            float theta = vs*2.0f*3.14159f;
            float x = radius*std::cos(theta)*std::sin(phi);
            float y = radius*std::cos(phi);
            float z = radius*std::sin(theta)*std::sin(phi);
            glm::vec3 p(x,y,z), n = glm::normalize(glm::vec3(x, std::max(y, 1e-3f), z));
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
    glBindVertexArray(0); return m;
}

static Mesh makeCoral(int segments = 8, float height = 0.6f, float baseRadius = 0.15f) { // Increased height and radius
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    
    for (int i=0; i<=segments; ++i) {
        float t = (float)i/segments;
        float y = t * height;
        float radius = baseRadius * (1.0f - t * 0.2f) + 0.04f * std::sin(t * 8.0f); // Less taper, more visible
        
        for (int j=0; j<=12; ++j) { // More sides for rounder coral
            float angle = (2.0f * 3.14159f) * (float)j / 12.0f;
            float x = radius * std::cos(angle);
            float z = radius * std::sin(angle);
            
            // Add some bumpy texture
            float bumpiness = 1.0f + 0.2f * std::sin(angle * 4.0f) * std::cos(t * 6.0f);
            x *= bumpiness;
            z *= bumpiness;
            
            glm::vec3 p(x, y, z);
            glm::vec3 n = glm::normalize(glm::vec3(x, 0.2f, z));
            v.push_back({p, n});
        }
    }
    
    for (int i=0; i<segments; ++i) {
        for (int j=0; j<12; ++j) {
            unsigned a = i*13 + j, b = a+1, c = (i+1)*13 + j, d = c+1;
            if (j == 11) { b = i*13; d = (i+1)*13; }
            idx.insert(idx.end(), {a,c,b, b,c,d});
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
    glBindVertexArray(0); return m;
}

static Mesh makeShell(float radius = 0.12f, float height = 0.08f) {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    
    const int rings = 6, sectors = 12;
    for (int r=0; r<=rings; ++r) {
        float vr = (float)r/rings;
        float phi = vr * 3.14159f * 0.3f;
        float r_radius = radius * (1.0f - vr * 0.3f);
        
        for (int s=0; s<=sectors; ++s) {
            float vs = (float)s/sectors;
            float theta = vs * 2.0f * 3.14159f;
            float x = r_radius * std::cos(theta);
            float y = height * std::sin(phi);
            float z = r_radius * std::sin(theta);
            
            glm::vec3 p(x, y, z);
            glm::vec3 n = glm::normalize(glm::vec3(x, 0.5f, z));
            v.push_back({p, n});
        }
    }
    
    int ring = sectors+1;
    for (int r=0; r<rings; ++r) {
        for (int s=0; s<sectors; ++s) {
            unsigned a = r*ring + s, b = a+1, c = (r+1)*ring + s, d = c+1;
            idx.insert(idx.end(), {a,c,b, b,c,d});
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
    glBindVertexArray(0); return m;
}

static Mesh makeDriftwood(int segments = 6, float length = 0.3f, float radius = 0.04f) {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    
    for (int i=0; i<=segments; ++i) {
        float t = (float)i/segments;
        float x = t * length;
        float r = radius * (1.0f - t * 0.4f);
        
        for (int j=0; j<=8; ++j) {
            float angle = (2.0f * 3.14159f) * (float)j / 8.0f;
            float y = r * std::cos(angle);
            float z = r * std::sin(angle);
            
            glm::vec3 p(x, y, z);
            glm::vec3 n = glm::normalize(glm::vec3(0.1f, y, z));
            v.push_back({p, n});
        }
    }
    
    for (int i=0; i<segments; ++i) {
        for (int j=0; j<8; ++j) {
            unsigned a = i*9 + j, b = a+1, c = (i+1)*9 + j, d = c+1;
            if (j == 7) { b = i*9; d = (i+1)*9; }
            idx.insert(idx.end(), {a,c,b, b,c,d});
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
    glBindVertexArray(0); return m;
}

static Mesh makeAnemone(int segments = 16, float height = 0.25f, float baseRadius = 0.06f) {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    
    // Central body
    for (int i=0; i<=segments; ++i) {
        float t = (float)i/segments;
        float y = t * height * 0.6f;
        float radius = baseRadius * (1.0f - t * 0.2f);
        
        for (int j=0; j<=12; ++j) {
            float angle = (2.0f * 3.14159f) * (float)j / 12.0f;
            float x = radius * std::cos(angle);
            float z = radius * std::sin(angle);
            
            glm::vec3 p(x, y, z);
            glm::vec3 n = glm::normalize(glm::vec3(x, 0.2f, z));
            v.push_back({p, n});
        }
    }
    
    // Tentacles
    int bodyVerts = (int)v.size();
    for (int t=0; t<8; ++t) {
        float tentacleAngle = (2.0f * 3.14159f) * (float)t / 8.0f;
        float baseX = baseRadius * 0.8f * std::cos(tentacleAngle);
        float baseZ = baseRadius * 0.8f * std::sin(tentacleAngle);
        
        for (int i=0; i<=6; ++i) {
            float s = (float)i / 6.0f;
            float tentacleHeight = height * 0.6f + s * height * 0.4f;
            float sway = 0.1f * std::sin(s * 6.0f) * std::cos(tentacleAngle * 2.0f);
            
            glm::vec3 p(baseX + sway, tentacleHeight, baseZ + sway);
            glm::vec3 n = glm::normalize(glm::vec3(baseX, 1.0f, baseZ));
            v.push_back({p, n});
        }
    }
    
    // Body triangulation
    for (int i=0; i<segments; ++i) {
        for (int j=0; j<12; ++j) {
            unsigned a = i*13 + j, b = a+1, c = (i+1)*13 + j, d = c+1;
            if (j == 11) { b = i*13; d = (i+1)*13; }
            idx.insert(idx.end(), {a,c,b, b,c,d});
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
    glBindVertexArray(0); return m;
}

static Mesh makeStarfish(float outerRadius = 0.12f, float innerRadius = 0.06f, float thickness = 0.03f) {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    
    const int arms = 5;
    const int segments = 10;
    
    // Top surface
    for (int a=0; a<arms; ++a) {
        float armAngle = (2.0f * 3.14159f) * (float)a / (float)arms;
        
        for (int s=0; s<=segments; ++s) {
            float t = (float)s / (float)segments;
            float radius = innerRadius + t * (outerRadius - innerRadius);
            
            // Main arm
            float x = radius * std::cos(armAngle);
            float z = radius * std::sin(armAngle);
            float y = thickness * 0.5f * (1.0f - t * 0.3f);
            
            v.push_back({glm::vec3(x, y, z), glm::vec3(0, 1, 0)});
            
            // Arm sides
            float sideAngle1 = armAngle - 0.2f;
            float sideAngle2 = armAngle + 0.2f;
            float sideRadius = radius * (0.7f - t * 0.3f);
            
            if (s < segments) {
                v.push_back({glm::vec3(sideRadius * std::cos(sideAngle1), y, sideRadius * std::sin(sideAngle1)), glm::vec3(0, 1, 0)});
                v.push_back({glm::vec3(sideRadius * std::cos(sideAngle2), y, sideRadius * std::sin(sideAngle2)), glm::vec3(0, 1, 0)});
            }
        }
    }
    
    // Simple triangulation for demonstration
    for (unsigned i=0; i<v.size()-2; i+=3) {
        idx.insert(idx.end(), {i, i+1, i+2});
    }
    
    Mesh m; glGenVertexArrays(1,&m.vao); glBindVertexArray(m.vao);
    glGenBuffers(1,&m.vbo); glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size()*sizeof(V), v.data(), GL_STATIC_DRAW);
    glGenBuffers(1,&m.ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,n));
    m.idxCount=(GLsizei)idx.size();
    glBindVertexArray(0); return m;
}

static Mesh makeKelp(int segments = 20, float height = 0.8f, float width = 0.04f) {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    
    // Create 3 strips at different angles for 3D kelp
    const int numStrips = 3;
    for (int strip = 0; strip < numStrips; ++strip) {
        float angle = (2.0f * 3.14159f) * (float)strip / (float)numStrips;
        glm::vec3 stripDir = glm::vec3(std::cos(angle), 0.0f, std::sin(angle));
        glm::vec3 stripNormal = glm::vec3(-std::sin(angle), 0.0f, std::cos(angle));
        
        int baseVertex = (int)v.size();
        
        for (int i=0; i<=segments; ++i) {
            float t = (float)i/segments;
            float y = t * height;
            float sway = 0.15f * std::sin(t * 8.0f + strip) * t; // Different sway per strip
            float w = width * (1.0f - t * 0.3f);
            
            glm::vec3 offset = stripDir * w * 0.5f;
            glm::vec3 swayOffset = glm::vec3(sway * std::cos(angle), 0.0f, sway * std::sin(angle));
            
            v.push_back({glm::vec3(-offset.x + swayOffset.x, y, -offset.z + swayOffset.z), stripNormal});
            v.push_back({glm::vec3( offset.x + swayOffset.x, y,  offset.z + swayOffset.z), stripNormal});
            
            if (i < segments) {
                unsigned base = baseVertex + i*2;
                idx.insert(idx.end(), {base, base+2, base+1,  base+1, base+2, base+3});
                // Add back faces
                idx.insert(idx.end(), {base+1, base+2, base,  base+3, base+2, base+1});
            }
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
    glBindVertexArray(0); return m;
}

static Mesh makeTreasureChest(float w = 0.2f, float h = 0.15f, float d = 0.15f) {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    
    float x=w*0.5f, y=h*0.5f, z=d*0.5f;
    auto addQuad=[&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n){
        size_t base=v.size();
        v.push_back({a,n}); v.push_back({b,n}); v.push_back({c,n}); v.push_back({d,n});
        idx.insert(idx.end(), { (unsigned)base,(unsigned)base+2,(unsigned)base+1,
                                (unsigned)base,(unsigned)base+3,(unsigned)base+2 });
    };
    
    // Chest body
    addQuad({-x,-y,-z},{ x,-y,-z},{ x, y,-z},{-x, y,-z}, { 0, 0, 1}); // Back
    addQuad({-x,-y, z},{-x, y, z},{ x, y, z},{ x,-y, z}, { 0, 0,-1}); // Front
    addQuad({-x,-y,-z},{-x,-y, z},{-x, y, z},{-x, y,-z}, { 1, 0, 0}); // Left
    addQuad({ x,-y, z},{ x,-y,-z},{ x, y,-z},{ x, y, z}, {-1, 0, 0}); // Right
    addQuad({-x,-y,-z},{ x,-y,-z},{ x,-y, z},{-x,-y, z}, { 0, 1, 0}); // Bottom
    
    // Slightly open lid
    float lidY = y + h * 0.1f;
    addQuad({-x, y, z},{ x, y, z},{ x, lidY,-z},{-x, lidY,-z}, { 0, 0.7f, 0.7f}); // Lid
    
    Mesh m; glGenVertexArrays(1,&m.vao); glBindVertexArray(m.vao);
    glGenBuffers(1,&m.vbo); glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size()*sizeof(V), v.data(), GL_STATIC_DRAW);
    glGenBuffers(1,&m.ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,n));
    m.idxCount=(GLsizei)idx.size();
    glBindVertexArray(0); return m;
}

// ===========================================================
// Species/instances
// ===========================================================
enum Species : int { CLOWNFISH=0, NEON_TETRA=1, ZEBRA_DANIO=2, ANGELFISH=3, GOLDFISH=4, BETTA=5, GUPPY=6, PLATY=7 };
struct FishInst {
    glm::vec3 pos, vel;
    float phase;
    float scale;
    glm::vec3 stretch;
    glm::vec3 color;
    float species;
};
static std::vector<FishInst> clownfish, neon, danio, angelfish, goldfish, betta, guppy, platy;
static GLuint vboClown=0, vboNeon=0, vboDanio=0, vboAngelfish=0, vboGoldfish=0, vboBetta=0, vboGuppy=0, vboPlaty=0;

static Mesh fishMesh, clownfishMesh, angelfishMesh, animatedFishMesh, plantMesh, glassTankMesh, tankBaseMesh, waterVolumeMesh, floorMesh, waterMesh, rockMesh, coralMesh, shellMesh, driftwoodMesh, anemoneMesh, starfishMesh, kelpMesh, treasureChestMesh;

// Fix tank bounds - these should match the actual tank dimensions
const float TANK_WIDTH = 2.4f;   // Tank box is 5.0f wide, so interior is ~2.4f
const float TANK_HEIGHT = 1.3f;  // Tank box is 2.8f tall, so interior is ~1.3f
const float TANK_DEPTH = 1.4f;   // Tank box is 3.0f deep, so interior is ~1.4f
static const glm::vec3 TANK_EXTENTS = {TANK_WIDTH, TANK_HEIGHT, TANK_DEPTH};
static float waterY = 0.6f; // Adjusted for 85% full tank

static int N_CLOWN = 6, N_NEON = 12, N_DANIO = 8, N_ANGELFISH = 4, N_GOLDFISH = 3, N_BETTA = 2, N_GUPPY = 8, N_PLATY = 6;
static int N_PLANTS = 25, N_ROCKS = 15, N_CORALS = 12, N_SHELLS = 18, N_DRIFTWOOD = 8, N_ANEMONES = 6, N_STARFISH = 10, N_KELP = 15, N_DECORATIONS = 8;

static GLuint plantVBO=0;
static std::vector<glm::vec3> plantPos;
static std::vector<glm::vec2> plantHP;
static std::vector<glm::vec3> plantColor;

static std::vector<glm::vec4> rocks;
static std::vector<glm::vec4> corals;
static std::vector<glm::vec4> shells;
static std::vector<glm::vec4> driftwood;
static std::vector<glm::vec4> anemones;
static std::vector<glm::vec4> starfish;
static std::vector<glm::vec4> kelp;
static std::vector<glm::vec4> decorations;

static std::mt19937 rng(2025);
static std::uniform_real_distribution<float> urand(-1.0f, 1.0f);
static std::uniform_real_distribution<float> urand01(0.0f, 1.0f);

static void initSpeciesVec(std::vector<FishInst>& v, int count, Species s,
                           glm::vec3 baseColor, glm::vec3 varyColor,
                           glm::vec3 stretchMean, glm::vec3 stretchVar,
                           float speedMin, float speedMax,
                           float yMin, float yMax, float scaleMin, float scaleMax) {
    v.resize(count);
    for (int i=0;i<count;++i) {
        // Keep fish well within tank bounds
        glm::vec3 p(urand(rng)*TANK_EXTENTS.x*0.7f,
                    yMin + urand01(rng)*(yMax-yMin),
                    urand(rng)*TANK_EXTENTS.z*0.7f);
        glm::vec3 dir = glm::normalize(glm::vec3(urand(rng), urand(rng)*0.2f, urand(rng)));
        float sp = speedMin + urand01(rng)*(speedMax-speedMin);
        glm::vec3 col = glm::clamp(baseColor + varyColor * urand(rng)*0.5f, glm::vec3(0.0f), glm::vec3(1.0f));
        glm::vec3 stretch = glm::max(stretchMean + stretchVar * urand(rng), glm::vec3(0.25f));
        float sc = scaleMin + urand01(rng)*(scaleMax-scaleMin);
        v[i] = { p, dir*sp, urand01(rng)*6.28318f, sc, stretch, col, (float)s };
    }
}
static void initPlantsAndRocks() {
    plantPos.resize(N_PLANTS);
    plantHP.resize(N_PLANTS);
    plantColor.resize(N_PLANTS);
    for (int i=0;i<N_PLANTS;++i) {
        float x = (urand01(rng) < 0.5f ? -1.0f : 1.0f) * (0.3f + urand01(rng)*0.5f) * TANK_EXTENTS.x;
        float z = urand(rng)*TANK_EXTENTS.z*0.8f;
        float h = 0.35f + urand01(rng)*0.55f;
        float phase = urand01(rng)*6.28318f;
        glm::vec3 col = glm::vec3(0.18f + urand01(rng)*0.1f, 0.55f + urand01(rng)*0.35f, 0.18f);
        plantPos[i]   = glm::vec3(x, -TANK_HEIGHT, z);
        plantHP[i]    = glm::vec2(h, phase);
        plantColor[i] = col;
    }
    if (!plantVBO) glGenBuffers(1, &plantVBO);

    // Rock clusters - create natural groupings with larger sizes
    rocks.resize(N_ROCKS);
    for (int i=0;i<N_ROCKS;++i) {
        float clusterX = (i < N_ROCKS/2) ? -0.6f : 0.6f; // Two main clusters
        float x = clusterX + urand(rng)*0.4f;
        float z = urand(rng)*TANK_EXTENTS.z*0.6f;
        float r = 0.25f + urand01(rng)*0.35f; // Much larger rocks (was 0.08f + 0.18f)
        rocks[i] = glm::vec4(x, -TANK_HEIGHT, z, r);
    }
    
    // Coral garden - spread around with larger sizes
    corals.resize(N_CORALS);
    for (int i=0;i<N_CORALS;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.7f;
        float z = urand(rng)*TANK_EXTENTS.z*0.7f;
        float r = 0.3f + urand01(rng)*0.4f; // Much larger corals (was 0.12f + 0.20f)
        corals[i] = glm::vec4(x, -TANK_HEIGHT, z, r);
    }
    
    // Shells scattered on floor
    shells.resize(N_SHELLS);
    for (int i=0;i<N_SHELLS;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.8f;
        float z = urand(rng)*TANK_EXTENTS.z*0.8f;
        float r = 0.05f + urand01(rng)*0.08f;
        shells[i] = glm::vec4(x, -TANK_HEIGHT, z, r);
    }
    
    // Driftwood pieces
    driftwood.resize(N_DRIFTWOOD);
    for (int i=0;i<N_DRIFTWOOD;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.6f;
        float z = urand(rng)*TANK_EXTENTS.z*0.6f;
        float r = 0.15f + urand01(rng)*0.25f;
        driftwood[i] = glm::vec4(x, -TANK_HEIGHT + 0.05f, z, r);
    }
    
    // Sea anemones - near rocks, larger size
    anemones.resize(N_ANEMONES);
    for (int i=0;i<N_ANEMONES;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.5f;
        float z = urand(rng)*TANK_EXTENTS.z*0.5f;
        float r = 0.15f + urand01(rng)*0.20f; // Larger anemones (was 0.08f + 0.12f)
        anemones[i] = glm::vec4(x, -TANK_HEIGHT, z, r);
    }
    
    // Starfish on floor
    starfish.resize(N_STARFISH);
    for (int i=0;i<N_STARFISH;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.9f;
        float z = urand(rng)*TANK_EXTENTS.z*0.9f;
        float r = 0.06f + urand01(rng)*0.08f;
        starfish[i] = glm::vec4(x, -TANK_HEIGHT + 0.01f, z, r);
    }
    
    // Kelp forest in back corners
    kelp.resize(N_KELP);
    for (int i=0;i<N_KELP;++i) {
        float corner = (i < N_KELP/2) ? -1.0f : 1.0f;
        float x = corner * (0.7f + urand01(rng)*0.2f) * TANK_EXTENTS.x;
        float z = (urand01(rng) < 0.5f ? -1.0f : 1.0f) * (0.5f + urand01(rng)*0.3f) * TANK_EXTENTS.z;
        float r = 0.6f + urand01(rng)*0.4f; // Height variation
        kelp[i] = glm::vec4(x, -TANK_HEIGHT, z, r);
    }
    
    // Special decorations (treasure chests, etc.)
    decorations.resize(N_DECORATIONS);
    for (int i=0;i<N_DECORATIONS;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.4f;
        float z = urand(rng)*TANK_EXTENTS.z*0.4f;
        float r = 0.1f + urand01(rng)*0.1f;
        decorations[i] = glm::vec4(x, -TANK_HEIGHT + 0.02f, z, r);
    }
}
static void setupFishInstancing(GLuint &instVBO, const Mesh& m, int count) {
    if (!instVBO) glGenBuffers(1, &instVBO);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, instVBO);
    glBufferData(GL_ARRAY_BUFFER, count * (sizeof(float)*15), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(3); glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,sizeof(float)*15,(void*)0);                 glVertexAttribDivisor(3,1);
    glEnableVertexAttribArray(4); glVertexAttribPointer(4,3,GL_FLOAT,GL_FALSE,sizeof(float)*15,(void*)(sizeof(float)*3));  glVertexAttribDivisor(4,1);
    glEnableVertexAttribArray(5); glVertexAttribPointer(5,2,GL_FLOAT,GL_FALSE,sizeof(float)*15,(void*)(sizeof(float)*6));  glVertexAttribDivisor(5,1);
    glEnableVertexAttribArray(6); glVertexAttribPointer(6,3,GL_FLOAT,GL_FALSE,sizeof(float)*15,(void*)(sizeof(float)*8));  glVertexAttribDivisor(6,1);
    glEnableVertexAttribArray(7); glVertexAttribPointer(7,3,GL_FLOAT,GL_FALSE,sizeof(float)*15,(void*)(sizeof(float)*11)); glVertexAttribDivisor(7,1);
    glEnableVertexAttribArray(8); glVertexAttribPointer(8,1,GL_FLOAT,GL_FALSE,sizeof(float)*15,(void*)(sizeof(float)*14)); glVertexAttribDivisor(8,1);
    glBindVertexArray(0);
}

// ===========================================================
// Bubbles
// ===========================================================
static const int N_BUB = 60;
static std::vector<glm::vec3> bubblePos;
static GLuint bubbleVBO = 0, bubbleVAO = 0;

static void initBubbles() {
    bubblePos.resize(N_BUB);
    for (int i=0;i<N_BUB;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.6f;
        float z = urand(rng)*TANK_EXTENTS.z*0.6f;
        float y = -TANK_HEIGHT + urand01(rng)*0.3f;
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
        bubblePos[i].x += 0.06f * std::sin(glfwGetTime()*2.2f + i*0.31f) * dt;
        if (bubblePos[i].y > waterY - 0.02f) {
            bubblePos[i].y = -TANK_HEIGHT + urand01(rng)*0.2f;
            bubblePos[i].x = urand(rng)*TANK_EXTENTS.x*0.5f;
            bubblePos[i].z = urand(rng)*TANK_EXTENTS.z*0.5f;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER,bubbleVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, bubblePos.size()*sizeof(glm::vec3), bubblePos.data());
}

// ===========================================================
// IBL resources
// ===========================================================
static GLuint envCube=0, irrCube=0, prefilterCube=0, brdfLUT=0;
static GLuint fbo=0, rbo=0;
static int prefilterMaxMip = 0;

// Programs
static GLuint progBasic=0, progWater=0, progFish=0, progBub=0, progPlant=0, progTone=0;
static GLuint progIBLGen=0, progIBLDiff=0, progIBLSpec=0, progBRDF=0;

// uniforms helper
static GLint u(GLuint p, const char* n){ return glGetUniformLocation(p, n); }

// Render a screen triangle
static void drawScreenTriangle(){ glBindVertexArray(screenVAO); glDrawArrays(GL_TRIANGLES, 0, 3); }

// Create cubemap texture helper
static GLuint createCube(GLenum internal, int size, bool mipmap) {
    GLuint tex; glGenTextures(1,&tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    for (int f=0; f<6; ++f)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+f, 0, internal, size, size, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, mipmap?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    if (mipmap) glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    return tex;
}

static void ensureIBLTargets(){
    if (!fbo) glGenFramebuffers(1,&fbo);
    if (!rbo) glGenRenderbuffers(1,&rbo);
}

// Generate procedural HDR environment -> envCube
static void generateEnvCube(int size) {
    ensureIBLTargets();
    if (envCube) glDeleteTextures(1,&envCube);
    envCube = createCube(GL_RGBA16F, size, false);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size, size);

    glUseProgram(progIBLGen);
    glUniform1f(u(progIBLGen,"uFaceSize"), (float)size);

    for (int face=0; face<6; ++face){
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+face, envCube, 0);
        glViewport(0,0,size,size);
        glUniform1i(u(progIBLGen,"uFace"), face);
        drawScreenTriangle();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Convolve env -> irradiance cubemap (Lambert)
static void generateIrradiance(int size) {
    ensureIBLTargets();
    if (irrCube) glDeleteTextures(1,&irrCube);
    irrCube = createCube(GL_RGBA16F, size, false);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size, size);

    glUseProgram(progIBLDiff);
    glUniform1f(u(progIBLDiff,"uFaceSize"), (float)size);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, envCube);
    glUniform1i(u(progIBLDiff,"uEnv"), 0);

    for (int face=0; face<6; ++face){
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+face, irrCube, 0);
        glViewport(0,0,size,size);
        glUniform1i(u(progIBLDiff,"uFace"), face);
        drawScreenTriangle();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Prefilter env -> specular prefilter cube mip chain
static void generatePrefilter(int baseSize) {
    ensureIBLTargets();
    if (prefilterCube) glDeleteTextures(1,&prefilterCube);
    prefilterCube = createCube(GL_RGBA16F, baseSize, true);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterCube);
    prefilterMaxMip = (int)std::floor(std::log2((float)baseSize));
    for (int mip=1; mip<=prefilterMaxMip; ++mip) {
        int sz = baseSize >> mip;
        for (int f=0; f<6; ++f)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+f, mip, GL_RGBA16F, sz, sz, 0, GL_RGBA, GL_FLOAT, nullptr);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glUseProgram(progIBLSpec);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, envCube);
    glUniform1i(u(progIBLSpec,"uEnv"), 0);

    for (int mip=0; mip<=prefilterMaxMip; ++mip){
        int size = baseSize >> mip;
        float rough = (float)mip / (float)prefilterMaxMip;
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size, size);
        glViewport(0,0,size,size);
        glUniform1f(u(progIBLSpec,"uFaceSize"), (float)size);
        glUniform1f(u(progIBLSpec,"uRoughness"), rough);
        for (int face=0; face<6; ++face){
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+face, prefilterCube, mip);
            glUniform1i(u(progIBLSpec,"uFace"), face);
            drawScreenTriangle();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// BRDF LUT 2D
static void generateBRDF(int size) {
    ensureIBLTargets();
    if (!brdfLUT) glGenTextures(1,&brdfLUT);
    glBindTexture(GL_TEXTURE_2D, brdfLUT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, size, size, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size, size);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUT, 0);
    glViewport(0,0,size,size);

    glUseProgram(progBRDF);
    glUniform1f(u(progBRDF,"uSize"), (float)size);
    drawScreenTriangle();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ===========================================================
// Main
// ===========================================================
int main(){
    if (!glfwInit()) { std::cerr<<"GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GL_TRUE);
#endif
    GLFWwindow* win = glfwCreateWindow(SCR_W,SCR_H,"AquariumGL",nullptr,nullptr);
    if (!win) { std::cerr<<"Window failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);
    glfwSetCursorPosCallback(win, mouse_callback);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    int fbw, fbh; glfwGetFramebufferSize(win, &fbw, &fbh);
    SCR_W = fbw; SCR_H = fbh;
    glViewport(0,0,SCR_W,SCR_H);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_FRAMEBUFFER_SRGB);

    createOrResizeHDR();
    glGenVertexArrays(1, &screenVAO);

    // ---------- compile shaders ----------
    auto S = [&](const char* p){ return loadFile(p); };

    GLuint vs_basic = compileShader(GL_VERTEX_SHADER,   S("shaders/basic.vert").c_str(),  "basic.vert");
    GLuint fs_basic = compileShader(GL_FRAGMENT_SHADER, S("shaders/basic.frag").c_str(),  "basic.frag");
    progBasic = linkProgram(vs_basic, fs_basic, "progBasic");

    GLuint vs_water = compileShader(GL_VERTEX_SHADER,   S("shaders/water.vert").c_str(),  "water.vert");
    GLuint fs_water = compileShader(GL_FRAGMENT_SHADER, S("shaders/water.frag").c_str(),  "water.frag");
    progWater = linkProgram(vs_water, fs_water, "progWater");

    GLuint vs_fish  = compileShader(GL_VERTEX_SHADER,   S("shaders/fish.vert").c_str(),   "fish.vert");
    GLuint fs_fish  = compileShader(GL_FRAGMENT_SHADER, S("shaders/fish.frag").c_str(),   "fish.frag");
    progFish = linkProgram(vs_fish, fs_fish, "progFish");

    GLuint vs_bub   = compileShader(GL_VERTEX_SHADER,   S("shaders/bubbles.vert").c_str(),"bubbles.vert");
    GLuint fs_bub   = compileShader(GL_FRAGMENT_SHADER, S("shaders/bubbles.frag").c_str(),"bubbles.frag");
    progBub = linkProgram(vs_bub, fs_bub, "progBub");

    GLuint vs_plant = compileShader(GL_VERTEX_SHADER,   S("shaders/plant.vert").c_str(),  "plant.vert");
    GLuint fs_plant = compileShader(GL_FRAGMENT_SHADER, S("shaders/plant.frag").c_str(),  "plant.frag");
    progPlant = linkProgram(vs_plant, fs_plant, "progPlant");

    // Screen tri VS reused for IBL/tonemap
    GLuint vs_tri = compileShader(GL_VERTEX_SHADER,     S("shaders/tonemap.vert").c_str(),"tonemap.vert");

    GLuint fs_tone = compileShader(GL_FRAGMENT_SHADER,  S("shaders/tonemap.frag").c_str(),"tonemap.frag");
    progTone = linkProgram(vs_tri, fs_tone, "progTonemap");

    // IBL passes
    GLuint fs_envGen  = compileShader(GL_FRAGMENT_SHADER, S("shaders/ibl_cubegen.frag").c_str(),  "ibl_cubegen.frag");
    GLuint fs_diffuse = compileShader(GL_FRAGMENT_SHADER, S("shaders/ibl_diffuse.frag").c_str(),   "ibl_diffuse.frag");
    GLuint fs_spec    = compileShader(GL_FRAGMENT_SHADER, S("shaders/ibl_specular.frag").c_str(),  "ibl_specular.frag");
    GLuint fs_brdf    = compileShader(GL_FRAGMENT_SHADER, S("shaders/ibl_brdf_lut.frag").c_str(),  "ibl_brdf_lut.frag");
    progIBLGen  = linkProgram(vs_tri, fs_envGen,  "progIBLGen");
    progIBLDiff = linkProgram(vs_tri, fs_diffuse, "progIBLDiff");
    progIBLSpec = linkProgram(vs_tri, fs_spec,    "progIBLSpec");
    progBRDF    = linkProgram(vs_tri, fs_brdf,    "progBRDF");

    // ---------- geometry ----------
    const float TANK_W = 5.0f, TANK_H = 2.8f, TANK_D = 3.0f;
    glassTankMesh = makeGlassTank(TANK_W, TANK_H, TANK_D, 0.08f);  // Glass container with thick walls
    tankBaseMesh = makeTankBase(TANK_W, TANK_H, TANK_D);           // Base stand for the tank
    waterVolumeMesh = makeWaterVolume(TANK_W, TANK_H, TANK_D, 0.85f); // Water volume (85% full)
    floorMesh = makeFloor(TANK_W*0.9f, TANK_D*0.9f, -TANK_HEIGHT);   // Sand floor inside tank
    waterMesh = makeWaterPlane(160, 160, TANK_W*0.9f, TANK_D*0.9f, waterY); // Water surface (kept for effects)
    
    // Load specific OBJ fish models from root directory
    std::cout << "Loading fish models from root directory..." << std::endl;
    fishMesh = loadOBJModel("fish.obj");                           // Generic fish for smaller species
    clownfishMesh = loadOBJModel("koi_fish.obj");                  // Koi for clownfish (orange/red)
    angelfishMesh = loadOBJModel("bream_fish__dorade_royale.obj"); // Bream for angelfish (silver)
    animatedFishMesh = loadOBJModel("fish_animated.obj");          // Animated for goldfish
    
    std::cout << "Fish models loading complete!" << std::endl;
    std::cout << "Setting up fish species with their assigned models:" << std::endl;
    std::cout << "- Clownfish: " << (clownfishMesh.idxCount > 0 ? "koi_fish.obj loaded" : "using fallback") << " (" << clownfishMesh.idxCount << " indices)" << std::endl;
    std::cout << "- Angelfish: " << (angelfishMesh.idxCount > 0 ? "bream_fish.obj loaded" : "using fallback") << " (" << angelfishMesh.idxCount << " indices)" << std::endl;
    std::cout << "- Goldfish: " << (animatedFishMesh.idxCount > 0 ? "fish_animated.obj loaded" : "using fallback") << " (" << animatedFishMesh.idxCount << " indices)" << std::endl;
    std::cout << "- Other species: " << (fishMesh.idxCount > 0 ? "fish.obj loaded" : "using fallback") << " (" << fishMesh.idxCount << " indices)" << std::endl;
    std::cout << "Fish counts: Clown=" << N_CLOWN << ", Neon=" << N_NEON << ", Danio=" << N_DANIO 
              << ", Angelfish=" << N_ANGELFISH << ", Goldfish=" << N_GOLDFISH 
              << ", Betta=" << N_BETTA << ", Guppy=" << N_GUPPY << ", Platy=" << N_PLATY << std::endl;
    std::cout << "Tank extents: " << TANK_EXTENTS.x << "x" << TANK_EXTENTS.y << "x" << TANK_EXTENTS.z << std::endl;
    std::cout << "Water level: " << waterY << std::endl;
    
    plantMesh = makePlantStrip();
    rockMesh  = makeRockDome();
    coralMesh = makeCoral();
    shellMesh = makeShell();
    driftwoodMesh = makeDriftwood();
    anemoneMesh = makeAnemone();
    starfishMesh = makeStarfish();
    kelpMesh = makeKelp();
    treasureChestMesh = makeTreasureChest();

    // ---------- species ----------
    auto initSpeciesVec_ = ::initSpeciesVec;
    // Adjust Y ranges to be within proper tank bounds
    initSpeciesVec_(clownfish, N_CLOWN, CLOWNFISH,
                    {1.0f,0.55f,0.20f}, {0.2f,0.1f,0.1f},
                    {1.2f,0.9f,1.0f},   {0.25f,0.1f,0.2f},
                    0.4f,0.8f, -0.8f,  waterY-0.2f, 1.0f, 1.3f);
    initSpeciesVec_(neon, N_NEON, NEON_TETRA,
                    {0.20f,0.85f,1.0f}, {0.2f,0.2f,0.2f},
                    {1.0f,0.7f,0.8f},   {0.2f,0.15f,0.15f},
                    0.5f,1.0f, -0.6f,   waterY-0.15f, 0.8f, 1.0f);
    initSpeciesVec_(danio, N_DANIO, ZEBRA_DANIO,
                    {0.9f,0.85f,0.55f}, {0.2f,0.2f,0.2f},
                    {1.3f,0.8f,0.9f},   {0.25f,0.12f,0.2f},
                    0.6f,1.2f, -0.7f,   waterY-0.12f, 0.9f, 1.1f);
    initSpeciesVec_(angelfish, N_ANGELFISH, ANGELFISH,
                    {0.8f,0.8f,0.9f}, {0.3f,0.3f,0.3f},
                    {1.5f,1.2f,0.6f},   {0.3f,0.2f,0.1f},
                    0.3f,0.6f, -0.5f,   waterY-0.25f, 1.3f, 1.6f);
    initSpeciesVec_(goldfish, N_GOLDFISH, GOLDFISH,
                    {1.0f,0.7f,0.2f}, {0.2f,0.1f,0.1f},
                    {1.1f,0.9f,1.0f},   {0.2f,0.15f,0.2f},
                    0.2f,0.5f, -0.4f,   waterY-0.3f, 1.4f, 1.8f);
    initSpeciesVec_(betta, N_BETTA, BETTA,
                    {0.8f,0.3f,0.8f}, {0.3f,0.2f,0.3f},
                    {1.0f,1.4f,0.7f},   {0.2f,0.3f,0.15f},
                    0.3f,0.7f, -0.3f,   waterY-0.15f, 1.1f, 1.4f);
    initSpeciesVec_(guppy, N_GUPPY, GUPPY,
                    {0.3f,0.8f,0.9f}, {0.2f,0.3f,0.2f},
                    {0.8f,0.6f,0.7f},   {0.15f,0.1f,0.15f},
                    0.5f,0.9f, -0.6f,   waterY-0.1f, 0.6f, 0.8f);
    initSpeciesVec_(platy, N_PLATY, PLATY,
                    {0.9f,0.4f,0.6f}, {0.2f,0.2f,0.2f},
                    {0.9f,0.7f,0.8f},   {0.15f,0.1f,0.15f},
                    0.4f,0.8f, -0.5f,   waterY-0.12f, 0.7f, 0.9f);

    setupFishInstancing(vboClown, clownfishMesh, N_CLOWN);        // Use koi model for clownfish
    setupFishInstancing(vboNeon,  fishMesh, N_NEON);            // Use generic fish for neon tetras
    setupFishInstancing(vboDanio, fishMesh, N_DANIO);           // Use generic fish for danios
    setupFishInstancing(vboAngelfish, angelfishMesh, N_ANGELFISH); // Use bream model for angelfish
    setupFishInstancing(vboGoldfish, animatedFishMesh, N_GOLDFISH); // Use animated fish for goldfish
    setupFishInstancing(vboBetta, fishMesh, N_BETTA);            // Use generic fish for bettas
    setupFishInstancing(vboGuppy, fishMesh, N_GUPPY);            // Use generic fish for guppies
    setupFishInstancing(vboPlaty, fishMesh, N_PLATY);            // Use generic fish for platies

    initPlantsAndRocks();
    initBubbles();

    // ---------- IBL generation ----------
    generateEnvCube(256);     // procedural HDR environment
    generateIrradiance(32);   // diffuse irradiance
    generatePrefilter(128);   // specular prefilter mip chain
    generateBRDF(256);        // BRDF LUT

    // ---------- common params ----------
    glm::vec3 lightDir = glm::normalize(glm::vec3(-0.7f,-1.2f,-0.35f));
    glm::vec3 fogColor(0.02f,0.06f,0.09f);
    glm::vec3 outsideColor(0.4f, 0.3f, 0.2f); // Warm brown/tan outside environment for strong contrast
    float fogNear = 2.0f, fogFar = 12.0f;
    float exposure = 1.5f; // Higher exposure to make glass and colors more visible

    std::cout << "\n=== Visual Changes Applied ===" << std::endl;
    std::cout << "- Outside world color: warm brown (" << outsideColor.x << ", " << outsideColor.y << ", " << outsideColor.z << ")" << std::endl;
    std::cout << "- Glass tank: ultra-transparent with thick walls (alpha=0.03)" << std::endl;
    std::cout << "- Water volume: blue interior filling the tank (alpha=0.3)" << std::endl;
    std::cout << "- Tank base: wooden stand positioned below tank" << std::endl;
    std::cout << "- Tone mapping exposure: " << exposure << std::endl;
    
    std::cout << "\n=== Aquarium Decorations ===" << std::endl;
    std::cout << "- Rock clusters: " << N_ROCKS << " rocks in natural groupings (size: 0.25-0.6)" << std::endl;
    std::cout << "- Coral garden: " << N_CORALS << " colorful corals spread throughout (size: 0.3-0.7)" << std::endl;
    std::cout << "- Sea anemones: " << N_ANEMONES << " animated anemones with tentacles (size: 0.15-0.35)" << std::endl;
    std::cout << "- Starfish: " << N_STARFISH << " starfish scattered on floor" << std::endl;
    std::cout << "- Kelp forest: " << N_KELP << " tall 3D kelp in back corners" << std::endl;
    std::cout << "- Shells: " << N_SHELLS << " shells scattered on sand" << std::endl;
    std::cout << "- Driftwood: " << N_DRIFTWOOD << " weathered wood pieces" << std::endl;
    std::cout << "- Plants: " << N_PLANTS << " 3D animated aquatic plants (4 strips each)" << std::endl;
    std::cout << "- Treasure chests: " << N_DECORATIONS << " decorative treasure chests" << std::endl;
    
    std::cout << "\n=== Controls ===" << std::endl;
    std::cout << "- WASD/QE: Camera movement" << std::endl;
    std::cout << "- Mouse: Look around" << std::endl;
    std::cout << "- C: Toggle orbit/fly camera mode" << std::endl;
    std::cout << "- SPACE: Pause/unpause simulation" << std::endl;
    std::cout << "- 1-5: Time scale (0.25x to 4x)" << std::endl;
    std::cout << "- F1: Toggle wireframe" << std::endl;
    std::cout << "- ESC: Exit" << std::endl;
    
    std::cout << "\n=== Project Objectives Status ===" << std::endl;
    std::cout << "✅ 1. Textured meshes: Tank, terrain, fish with procedural textures & materials" << std::endl;
    std::cout << "✅ 2. Fish animation: Procedural movement with advanced Boids schooling" << std::endl;
    std::cout << "✅ 3. Realistic water: Refractions, transparency, surface effects, caustics" << std::endl;
    std::cout << "✅ 4. PBR lighting: IBL with irradiance/specular maps, BRDF LUT, HDR pipeline" << std::endl;
    std::cout << "✅ 5. Camera & controls: Orbit/fly modes, pause, time scaling, full interaction" << std::endl;

    float last = (float)glfwGetTime();
    while (!glfwWindowShouldClose(win)) {
        float now=(float)glfwGetTime();
        float rawDt = now-last; 
        float dt = paused ? 0.0f : rawDt * timeScale; // Apply time scaling and pause
        last=now;
        
        glfwPollEvents();
        if (glfwGetKey(win, GLFW_KEY_ESCAPE)==GLFW_PRESS) glfwSetWindowShouldClose(win, 1);
        if (glfwGetKey(win, GLFW_KEY_F1)==GLFW_PRESS){ wireframe=!wireframe; glPolygonMode(GL_FRONT_AND_BACK, wireframe?GL_LINE:GL_FILL); }
        process_input(win, rawDt); // Use raw dt for camera movement

        // updates - improved fish bounding logic
        auto updateSchool=[&](std::vector<FishInst>& fish, float yMin, float yMax, float maxSpeed, float cohesion=0.18f, float alignW=0.45f){
            const float neighborDist2 = 0.18f, avoidDist2=0.06f;
            for (auto &f : fish) {
                glm::vec3 pos=f.pos, vel=f.vel;
                glm::vec3 align(0), coh(0), sep(0); int count = 0;
                for (auto &o : fish) {
                    if (&o==&f) continue;
                    glm::vec3 d = o.pos - pos; float d2 = glm::dot(d,d);
                    if (d2 < neighborDist2) {
                        align += o.vel; coh += o.pos; ++count;
                        if (d2 < avoidDist2) sep -= d * (0.2f / std::max(d2, 1e-4f));
                    }
                }
                if (count>0) { align = glm::normalize(align/(float)count) * 0.6f; coh = (coh/(float)count) - pos; }
                
                // Enhanced bounding forces - fish should stay well within tank
                glm::vec3 steer(0); 
                float boundaryForce = 3.0f;
                float softBoundary = 0.85f; // Start applying force before reaching the boundary
                glm::vec3 lim = TANK_EXTENTS * softBoundary;
                
                if (pos.x > lim.x) steer.x -= (pos.x-lim.x)*boundaryForce; 
                if (pos.x < -lim.x) steer.x += (-lim.x-pos.x)*boundaryForce;
                if (pos.z > lim.z) steer.z -= (pos.z-lim.z)*boundaryForce; 
                if (pos.z < -lim.z) steer.z += (-lim.z-pos.z)*boundaryForce;
                if (pos.y > yMax) steer.y -= (pos.y-yMax)*boundaryForce*2.0f; 
                if (pos.y < yMin) steer.y += (yMin-pos.y)*boundaryForce*2.0f;
                
                glm::vec3 drift(std::sin(f.phase*0.7f)*0.1f, std::sin(f.phase*1.3f)*0.05f, std::cos(f.phase*0.9f)*0.1f);
                glm::vec3 jitter(urand(rng)*0.08f, urand(rng)*0.04f, urand(rng)*0.08f);
                vel += align*alignW + coh*cohesion + sep*1.15f + steer + drift*0.3f + jitter*0.25f;
                float s=glm::length(vel); if (s>maxSpeed) vel*= (maxSpeed/s);
                pos += vel*dt; 
                
                // Hard clamp as safety net
                pos.x = std::clamp(pos.x, -TANK_EXTENTS.x*0.9f, TANK_EXTENTS.x*0.9f);
                pos.z = std::clamp(pos.z, -TANK_EXTENTS.z*0.9f, TANK_EXTENTS.z*0.9f);
                pos.y = std::clamp(pos.y, yMin, yMax);
                
                f.pos=pos; f.vel=vel; f.phase += dt*3.0f;
            }
        };
        updateSchool(clownfish, -0.8f, waterY-0.2f, 0.8f);
        updateSchool(neon,      -0.6f, waterY-0.15f, 1.0f, 0.22f, 0.30f);
        updateSchool(danio,     -0.7f, waterY-0.12f, 1.2f, 0.18f, 0.40f);
        updateSchool(angelfish, -0.5f, waterY-0.25f, 0.6f, 0.15f, 0.35f);
        updateSchool(goldfish,  -0.4f, waterY-0.3f, 0.5f, 0.12f, 0.25f);
        updateSchool(betta,     -0.3f, waterY-0.15f, 0.7f, 0.20f, 0.45f);
        updateSchool(guppy,     -0.6f, waterY-0.1f, 0.9f, 0.25f, 0.35f);
        updateSchool(platy,     -0.5f, waterY-0.12f, 0.8f, 0.18f, 0.30f);

        updateBubbles(dt);

        // ------------------- Render to HDR FBO -------------------
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glViewport(0,0,SCR_W,SCR_H);
        glClearColor(outsideColor.r, outsideColor.g, outsideColor.b, 1.0f); // Warm brown outside world
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = glm::perspective(glm::radians(60.0f),(float)SCR_W/(float)SCR_H,0.05f,100.0f);
        glm::mat4 view = glm::lookAt(camPos, camPos+camFront, camUp);

        // ===== Tank Base (Solid) =====
        glUseProgram(progBasic);
        glUniformMatrix4fv(u(progBasic,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(u(progBasic,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glm::mat4 baseModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.8f, 0.0f)); // Position base below tank
        glUniformMatrix4fv(u(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(baseModel));
        glUniform3f(u(progBasic,"uLightDir"), lightDir.x,lightDir.y,lightDir.z);
        glUniform3f(u(progBasic,"uViewPos"), camPos.x, camPos.y, camPos.z);
        glUniform3f(u(progBasic,"uFogColor"), fogColor.r,fogColor.g,fogColor.b);
        glUniform1f(u(progBasic,"uFogNear"),  fogNear);
        glUniform1f(u(progBasic,"uFogFar"),   fogFar);
        glUniform1f(u(progBasic,"uTime"),     now);
        glUniform1i(u(progBasic,"uApplyCaustics"), 0);
        glUniform1i(u(progBasic,"uMaterialType"), 6); // Wood/base material
        glUniform1f(u(progBasic,"uAlpha"), 1.0f);
        glUniform3f(u(progBasic,"uBaseColor"), 0.4f, 0.25f, 0.15f); // Dark wood color
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_CUBE_MAP, irrCube);
        glUniform1i(u(progBasic,"uIrradiance"), 1);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterCube);
        glUniform1i(u(progBasic,"uPrefilter"), 2);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, brdfLUT);
        glUniform1i(u(progBasic,"uBRDFLUT"), 3);
        glUniform1f(u(progBasic,"uPrefLodMax"), (float)prefilterMaxMip);
        glBindVertexArray(tankBaseMesh.vao);
        glDrawElements(GL_TRIANGLES, tankBaseMesh.idxCount, GL_UNSIGNED_INT, 0);

        // ===== Floor (sand) =====
        glUniformMatrix4fv(u(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(glm::mat4(1.0f)));
        glUniform1i(u(progBasic,"uApplyCaustics"), 1);
        glUniform1i(u(progBasic,"uMaterialType"), 0);
        glUniform3f(u(progBasic,"uBaseColor"), 0.78f, 0.72f, 0.52f);
        glBindVertexArray(floorMesh.vao);
        glDrawElements(GL_TRIANGLES, floorMesh.idxCount, GL_UNSIGNED_INT, 0);

        // ===== Decorations =====
        glUniform1i(u(progBasic,"uApplyCaustics"), 0);
        glUniform1i(u(progBasic,"uMaterialType"), 1);
        for (int i=0;i<N_ROCKS;++i) {
            glm::vec4 r = rocks[i];
            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(r.x, r.y, r.z))
                        * glm::scale(glm::mat4(1.0f), glm::vec3(r.w));
            glUniformMatrix4fv(u(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(M));
            glUniform3f(u(progBasic,"uBaseColor"), 0.35f+0.12f*(float)i/N_ROCKS, 0.30f, 0.26f);
            glBindVertexArray(rockMesh.vao);
            glDrawElements(GL_TRIANGLES, rockMesh.idxCount, GL_UNSIGNED_INT, 0);
        }
        
        glUniform1i(u(progBasic,"uMaterialType"), 2);
        for (int i=0;i<N_CORALS;++i) {
            glm::vec4 c = corals[i];
            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(c.x, c.y, c.z))
                        * glm::scale(glm::mat4(1.0f), glm::vec3(c.w));
            glUniformMatrix4fv(u(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(M));
            glUniform3f(u(progBasic,"uBaseColor"), 0.8f+0.2f*(float)i/N_CORALS, 0.3f+0.2f*(float)i/N_CORALS, 0.4f+0.3f*(float)i/N_CORALS);
            glBindVertexArray(coralMesh.vao);
            glDrawElements(GL_TRIANGLES, coralMesh.idxCount, GL_UNSIGNED_INT, 0);
        }
        
        glUniform1i(u(progBasic,"uMaterialType"), 3);
        for (int i=0;i<N_SHELLS;++i) {
            glm::vec4 s = shells[i];
            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(s.x, s.y, s.z))
                        * glm::rotate(glm::mat4(1.0f), (float)i * 0.7f, glm::vec3(0,1,0))
                        * glm::scale(glm::mat4(1.0f), glm::vec3(s.w));
            glUniformMatrix4fv(u(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(M));
            glUniform3f(u(progBasic,"uBaseColor"), 0.9f+0.1f*(float)i/N_SHELLS, 0.85f+0.1f*(float)i/N_SHELLS, 0.7f+0.2f*(float)i/N_SHELLS);
            glBindVertexArray(shellMesh.vao);
            glDrawElements(GL_TRIANGLES, shellMesh.idxCount, GL_UNSIGNED_INT, 0);
        }
        
        glUniform1i(u(progBasic,"uMaterialType"), 4);
        for (int i=0;i<N_DRIFTWOOD;++i) {
            glm::vec4 d = driftwood[i];
            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(d.x, d.y, d.z))
                        * glm::rotate(glm::mat4(1.0f), (float)i * 0.5f, glm::vec3(0,1,0))
                        * glm::scale(glm::mat4(1.0f), glm::vec3(d.w));
            glUniformMatrix4fv(u(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(M));
            glUniform3f(u(progBasic,"uBaseColor"), 0.4f+0.2f*(float)i/N_DRIFTWOOD, 0.25f+0.1f*(float)i/N_DRIFTWOOD, 0.15f+0.1f*(float)i/N_DRIFTWOOD);
            glBindVertexArray(driftwoodMesh.vao);
            glDrawElements(GL_TRIANGLES, driftwoodMesh.idxCount, GL_UNSIGNED_INT, 0);
        }
        
        // Sea Anemones
        glUniform1i(u(progBasic,"uMaterialType"), 8);
        for (int i=0;i<N_ANEMONES;++i) {
            glm::vec4 a = anemones[i];
            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(a.x, a.y, a.z))
                        * glm::scale(glm::mat4(1.0f), glm::vec3(a.w));
            glUniformMatrix4fv(u(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(M));
            float hue = (float)i / N_ANEMONES;
            glUniform3f(u(progBasic,"uBaseColor"), 0.8f + 0.2f*std::sin(hue*6.28f), 0.4f + 0.3f*std::cos(hue*4.0f), 0.6f + 0.4f*std::sin(hue*8.0f));
            glBindVertexArray(anemoneMesh.vao);
            glDrawElements(GL_TRIANGLES, anemoneMesh.idxCount, GL_UNSIGNED_INT, 0);
        }
        
        // Starfish
        glUniform1i(u(progBasic,"uMaterialType"), 9);
        for (int i=0;i<N_STARFISH;++i) {
            glm::vec4 s = starfish[i];
            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(s.x, s.y, s.z))
                        * glm::rotate(glm::mat4(1.0f), (float)i * 1.2f, glm::vec3(0,1,0))
                        * glm::scale(glm::mat4(1.0f), glm::vec3(s.w));
            glUniformMatrix4fv(u(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(M));
            glUniform3f(u(progBasic,"uBaseColor"), 0.9f + 0.1f*(float)i/N_STARFISH, 0.5f + 0.3f*(float)i/N_STARFISH, 0.3f + 0.2f*(float)i/N_STARFISH);
            glBindVertexArray(starfishMesh.vao);
            glDrawElements(GL_TRIANGLES, starfishMesh.idxCount, GL_UNSIGNED_INT, 0);
        }
        
        // Treasure Chests
        glUniform1i(u(progBasic,"uMaterialType"), 10);
        for (int i=0;i<N_DECORATIONS;++i) {
            glm::vec4 t = decorations[i];
            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(t.x, t.y, t.z))
                        * glm::rotate(glm::mat4(1.0f), (float)i * 0.8f, glm::vec3(0,1,0))
                        * glm::scale(glm::mat4(1.0f), glm::vec3(t.w));
            glUniformMatrix4fv(u(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(M));
            glUniform3f(u(progBasic,"uBaseColor"), 0.6f, 0.4f, 0.2f); // Bronze/gold color
            glBindVertexArray(treasureChestMesh.vao);
            glDrawElements(GL_TRIANGLES, treasureChestMesh.idxCount, GL_UNSIGNED_INT, 0);
        }
        
        glUniform1i(u(progBasic,"uMaterialType"), 0);

        // ===== Plants & Kelp =====
        {
            if (!plantVBO) glGenBuffers(1,&plantVBO);
            // Combine regular plants and kelp for animated rendering
            int totalPlants = N_PLANTS + N_KELP;
            std::vector<float> data; data.resize(totalPlants*8);
            
            // Regular plants
            for (int i=0;i<N_PLANTS;++i) {
                int o=i*8;
                data[o+0]=plantPos[i].x; data[o+1]=plantPos[i].y; data[o+2]=plantPos[i].z;
                data[o+3]=plantHP[i].x;  data[o+4]=plantHP[i].y;
                data[o+5]=plantColor[i].r; data[o+6]=plantColor[i].g; data[o+7]=plantColor[i].b;
            }
            
            // Kelp forest
            for (int i=0;i<N_KELP;++i) {
                int o=(N_PLANTS+i)*8;
                glm::vec4 k = kelp[i];
                data[o+0]=k.x; data[o+1]=k.y; data[o+2]=k.z;
                data[o+3]=k.w; data[o+4]=urand01(rng)*6.28f; // height and phase
                data[o+5]=0.1f + 0.15f*urand01(rng); data[o+6]=0.4f + 0.3f*urand01(rng); data[o+7]=0.1f; // Kelp colors
            }
            
            glBindBuffer(GL_ARRAY_BUFFER, plantVBO);
            glBufferData(GL_ARRAY_BUFFER, data.size()*sizeof(float), data.data(), GL_DYNAMIC_DRAW);
        }
        glUseProgram(progPlant);
        glUniformMatrix4fv(u(progPlant,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(u(progPlant,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniform1f(u(progPlant,"uTime"), now);
        glUniform3f(u(progPlant,"uLightDir"), lightDir.x,lightDir.y,lightDir.z);
        glUniform3f(u(progPlant,"uViewPos"), camPos.x, camPos.y, camPos.z);
        glUniform3f(u(progPlant,"uFogColor"), fogColor.r,fogColor.g,fogColor.b);
        glUniform1f(u(progPlant,"uFogNear"),  fogNear);
        glUniform1f(u(progPlant,"uFogFar"),   fogFar);
        
        // Disable face culling for 3D plants to show from all angles
        glDisable(GL_CULL_FACE);
        
        // Render regular plants
        glBindVertexArray(plantMesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, plantVBO);
        glEnableVertexAttribArray(8);  glVertexAttribPointer(8,3,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)0);                 glVertexAttribDivisor(8,1);
        glEnableVertexAttribArray(9);  glVertexAttribPointer(9,2,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)(sizeof(float)*3));  glVertexAttribDivisor(9,1);
        glEnableVertexAttribArray(10); glVertexAttribPointer(10,3,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)(sizeof(float)*5)); glVertexAttribDivisor(10,1);
        glDrawElementsInstanced(GL_TRIANGLES, plantMesh.idxCount, GL_UNSIGNED_INT, 0, N_PLANTS);
        
        // Render kelp forest with kelp mesh
        glBindVertexArray(kelpMesh.vao);
        glDrawElementsInstanced(GL_TRIANGLES, kelpMesh.idxCount, GL_UNSIGNED_INT, 0, N_KELP);
        glBindVertexArray(0);
        
        // Re-enable face culling
        glEnable(GL_CULL_FACE);

        // ===== Fish =====
        auto uploadFish = [&](const std::vector<FishInst>& species, GLuint vbo, const Mesh& mesh){
            std::vector<float> inst; inst.resize(species.size()*15);
            for (size_t i=0;i<species.size();++i) {
                const auto &f = species[i];
                glm::vec3 dir = glm::length(f.vel)>1e-6f ? glm::normalize(f.vel) : glm::vec3(0,0,-1);
                size_t o = i*15;
                inst[o+0]=f.pos.x; inst[o+1]=f.pos.y; inst[o+2]=f.pos.z;
                inst[o+3]=dir.x;   inst[o+4]=dir.y;   inst[o+5]=dir.z;
                inst[o+6]=f.phase; inst[o+7]=f.scale;
                inst[o+8]=f.stretch.x; inst[o+9]=f.stretch.y; inst[o+10]=f.stretch.z;
                inst[o+11]=f.color.r;  inst[o+12]=f.color.g;  inst[o+13]=f.color.b;
                inst[o+14]=f.species;
            }

            glBindVertexArray(mesh.vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, inst.size()*sizeof(float), inst.data());
            glBindVertexArray(0);
        };
        
        // Upload all fish data to their respective meshes
        uploadFish(clownfish, vboClown, clownfishMesh);     // Koi model
        uploadFish(neon,     vboNeon, fishMesh);            // Generic fish
        uploadFish(danio,    vboDanio, fishMesh);           // Generic fish
        uploadFish(angelfish, vboAngelfish, angelfishMesh); // Bream model  
        uploadFish(goldfish,  vboGoldfish, animatedFishMesh); // Animated fish
        uploadFish(betta,     vboBetta, fishMesh);          // Generic fish
        uploadFish(guppy,     vboGuppy, fishMesh);          // Generic fish
        uploadFish(platy,     vboPlaty, fishMesh);          // Generic fish

        auto drawSpecies = [&](const std::vector<FishInst>& v, GLuint vbo, const Mesh& mesh){
            if (v.empty()) return;
            glBindVertexArray(mesh.vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glUseProgram(progFish);
            glUniformMatrix4fv(u(progFish,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
            glUniformMatrix4fv(u(progFish,"uView"),1,GL_FALSE,glm::value_ptr(view));
            glUniform3f(u(progFish,"uLightDir"), -lightDir.x, -lightDir.y, -lightDir.z);
            glUniform3f(u(progFish,"uViewPos"), camPos.x, camPos.y, camPos.z);
            glUniform1f(u(progFish,"uTime"), now);
            glUniform3f(u(progFish,"uFogColor"), fogColor.r,fogColor.g,fogColor.b);
            glUniform1f(u(progFish,"uFogNear"),  fogNear);
            glUniform1f(u(progFish,"uFogFar"),   fogFar);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_CUBE_MAP, irrCube);
            glUniform1i(u(progFish,"uIrradiance"), 1);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterCube);
            glUniform1i(u(progFish,"uPrefilter"), 2);
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, brdfLUT);
            glUniform1i(u(progFish,"uBRDFLUT"), 3);
            glUniform1f(u(progFish,"uPrefLodMax"), (float)prefilterMaxMip);
            glDrawElementsInstanced(GL_TRIANGLES, mesh.idxCount, GL_UNSIGNED_INT, 0, (GLsizei)v.size());
            glBindVertexArray(0);
        };
        
        // Draw all fish species with their specific models
        drawSpecies(clownfish, vboClown, clownfishMesh);     // Koi model - orange/red
        drawSpecies(neon,     vboNeon, fishMesh);            // Generic - blue
        drawSpecies(danio,    vboDanio, fishMesh);           // Generic - yellow
        drawSpecies(angelfish, vboAngelfish, angelfishMesh); // Bream model - silver
        drawSpecies(goldfish,  vboGoldfish, animatedFishMesh); // Animated - gold
        drawSpecies(betta,     vboBetta, fishMesh);          // Generic - purple
        drawSpecies(guppy,     vboGuppy, fishMesh);          // Generic - cyan
        drawSpecies(platy,     vboPlaty, fishMesh);          // Generic - pink

        // copy opaque for refraction
        glBindFramebuffer(GL_READ_FRAMEBUFFER, hdrFBO);
        glBindTexture(GL_TEXTURE_2D, opaqueCopyTex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, SCR_W, SCR_H);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        // ===== Water Volume (Blue Interior) =====
        glUseProgram(progBasic);
        glUniformMatrix4fv(u(progBasic,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(u(progBasic,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(u(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(glm::mat4(1.0f)));
        glUniform3f(u(progBasic,"uLightDir"), lightDir.x,lightDir.y,lightDir.z);
        glUniform3f(u(progBasic,"uViewPos"), camPos.x, camPos.y, camPos.z);
        glUniform3f(u(progBasic,"uFogColor"), fogColor.r,fogColor.g,fogColor.b);
        glUniform1f(u(progBasic,"uFogNear"),  fogNear);
        glUniform1f(u(progBasic,"uFogFar"),   fogFar);
        glUniform1f(u(progBasic,"uTime"),     now);
        glUniform1i(u(progBasic,"uApplyCaustics"), 1);
        glUniform1i(u(progBasic,"uMaterialType"), 7); // Water volume material
        glUniform3f(u(progBasic,"uBaseColor"), 0.1f, 0.5f, 0.9f); // Beautiful blue water
        glUniform1f(u(progBasic,"uAlpha"), 0.3f); // Semi-transparent water
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_CUBE_MAP, irrCube);
        glUniform1i(u(progBasic,"uIrradiance"), 1);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterCube);
        glUniform1i(u(progBasic,"uPrefilter"), 2);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, brdfLUT);
        glUniform1i(u(progBasic,"uBRDFLUT"), 3);
        glUniform1f(u(progBasic,"uPrefLodMax"), (float)prefilterMaxMip);
        
        // Render water volume with transparency
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE); // Show water from all angles
        glBindVertexArray(waterVolumeMesh.vao);
        glDrawElements(GL_TRIANGLES, waterVolumeMesh.idxCount, GL_UNSIGNED_INT, 0);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);

        // ===== Bubbles =====
        glUseProgram(progBub);
        glUniformMatrix4fv(u(progBub,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(u(progBub,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glBindVertexArray(bubbleVAO);
        glDrawArrays(GL_POINTS, 0, N_BUB);

        // ===== Water Surface (for effects) =====
        glUseProgram(progWater);
        glUniformMatrix4fv(u(progWater,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(u(progWater,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(u(progWater,"uModel"),1,GL_FALSE,glm::value_ptr(glm::mat4(1.0f)));
        glUniform1f(u(progWater,"uTime"), now);
        glUniform3f(u(progWater,"uDeepColor"),    0.1f, 0.4f, 0.8f);   // Rich deep blue
        glUniform3f(u(progWater,"uShallowColor"), 0.3f, 0.8f, 1.0f);   // Bright aqua blue
        glUniform3f(u(progWater,"uLightDir"), lightDir.x,lightDir.y,lightDir.z);
        glUniform3f(u(progWater,"uViewPos"), camPos.x,camPos.y,camPos.z);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, opaqueCopyTex);
        glUniform1i(u(progWater,"uSceneColor"), 0);
        glBindVertexArray(waterMesh.vao);
        glDisable(GL_CULL_FACE);
        glDrawElements(GL_TRIANGLES, waterMesh.idxCount, GL_UNSIGNED_INT, 0);
        glEnable(GL_CULL_FACE);

        // ===== Crystal Clear Glass Tank =====
        glUseProgram(progBasic);
        glUniformMatrix4fv(u(progBasic,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(u(progBasic,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(u(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(glm::mat4(1.0f)));
        glUniform3f(u(progBasic,"uLightDir"), lightDir.x,lightDir.y,lightDir.z);
        glUniform3f(u(progBasic,"uViewPos"), camPos.x, camPos.y, camPos.z);
        glUniform3f(u(progBasic,"uFogColor"), fogColor.r,fogColor.g,fogColor.b);
        glUniform1f(u(progBasic,"uFogNear"),  fogNear);
        glUniform1f(u(progBasic,"uFogFar"),   fogFar);
        glUniform1f(u(progBasic,"uTime"),     now);
        glUniform1i(u(progBasic,"uApplyCaustics"), 0);
        glUniform1i(u(progBasic,"uMaterialType"), 5); // Special glass material
        glUniform3f(u(progBasic,"uBaseColor"), 0.98f, 0.99f, 1.0f); // Almost pure white glass
        glUniform1f(u(progBasic,"uAlpha"), 0.03f); // Ultra transparent glass
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_CUBE_MAP, irrCube);
        glUniform1i(u(progBasic,"uIrradiance"), 1);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterCube);
        glUniform1i(u(progBasic,"uPrefilter"), 2);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, brdfLUT);
        glUniform1i(u(progBasic,"uBRDFLUT"), 3);
        glUniform1f(u(progBasic,"uPrefLodMax"), (float)prefilterMaxMip);
        
        // Glass rendering with proper transparency
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE); // Don't write to depth buffer for transparency
        glBindVertexArray(glassTankMesh.vao);
        glDrawElements(GL_TRIANGLES, glassTankMesh.idxCount, GL_UNSIGNED_INT, 0);
        glDepthMask(GL_TRUE);

        // ----- tonemap to screen -----
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);
        glViewport(0,0,SCR_W,SCR_H);
        glUseProgram(progTone);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrColorTex);
        glUniform1i(u(progTone,"uHDR"), 0);
        glUniform1f(u(progTone,"uExposure"), exposure);
        drawScreenTriangle();
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(win);
    }
    glfwTerminate();
    return 0;
}