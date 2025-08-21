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
// Window/camera
// ===========================================================
static int SCR_W = 1280, SCR_H = 720;

static float camYaw = -90.0f, camPitch = -5.0f;
static glm::vec3 camPos(0.0f, 0.20f, 0.6f);
static glm::vec3 camFront(0.0f, 0.0f, -1.0f);
static glm::vec3 camUp(0.0f, 1.0f, 0.0f);
static bool firstMouse = true;
static double lastX = SCR_W * 0.5, lastY = SCR_H * 0.5;
static bool wireframe = false;

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

// Simple fallback mesh for OBJ loader
static Mesh createFallbackMesh() {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    auto push = [&](const glm::vec3& p, const glm::vec3& n){ v.push_back({p, glm::normalize(n)}); };

    const int segX=24, segR=20;
    const float rMax=0.065f, zFlatten=0.65f;
    for (int i=0; i<=segX; ++i) {
        float t = (float)i / (float)segX;
        float r = rMax * std::pow(std::sin(3.14159f * std::clamp(t*1.02f, 0.0f, 1.0f)), 0.75f);
        if (i == 0) r *= 0.6f;
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

// Simple OBJ loader for fish models
static Mesh loadOBJModel(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ file: " << filename << std::endl;
        return createFallbackMesh(); // Fallback to basic mesh
    }
    
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
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
        } else if (type == "vt") {
            float u, v;
            iss >> u >> v;
            // Skip texture coordinates - we're not using them
            // texcoords.push_back(glm::vec2(u, v));
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
        std::cerr << "No vertices found in OBJ file: " << filename << std::endl;
        return createFallbackMesh(); // Fallback
    }
    
    // Calculate bounding box to determine scale
    glm::vec3 minPos = positions[0], maxPos = positions[0];
    for (const auto& pos : positions) {
        minPos = glm::min(minPos, pos);
        maxPos = glm::max(maxPos, pos);
    }
    glm::vec3 size = maxPos - minPos;
    float maxSize = std::max({size.x, size.y, size.z});
    // Use a much larger scale to make fish clearly visible
    float scale = 0.2f; // Much larger scale for all fish models
    
    std::cout << "Loaded OBJ: " << filename << std::endl;
    std::cout << "  Vertices: " << positions.size() << std::endl;
    std::cout << "  Bounding box: " << minPos.x << "," << minPos.y << "," << minPos.z 
              << " to " << maxPos.x << "," << maxPos.y << "," << maxPos.z << std::endl;
    std::cout << "  Size: " << size.x << "x" << size.y << "x" << size.z << std::endl;
    std::cout << "  Scale factor: " << scale << std::endl;
    
    // Create mesh from loaded data with scaling
    struct V { glm::vec3 p, n; };
    std::vector<V> vertices;
    
    for (size_t i = 0; i < positions.size(); ++i) {
        V v;
        v.p = positions[i] * scale; // Apply scaling
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
    
    // Set up instancing attributes for fish
    glEnableVertexAttribArray(3); // iPos
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 15*sizeof(float), (void*)0);
    glVertexAttribDivisor(3, 1);
    
    glEnableVertexAttribArray(4); // iDir
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 15*sizeof(float), (void*)(3*sizeof(float)));
    glVertexAttribDivisor(4, 1);
    
    glEnableVertexAttribArray(5); // iPhaseScale
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, 15*sizeof(float), (void*)(6*sizeof(float)));
    glVertexAttribDivisor(5, 1);
    
    glEnableVertexAttribArray(6); // iStretch
    glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, 15*sizeof(float), (void*)(8*sizeof(float)));
    glVertexAttribDivisor(6, 1);
    
    glEnableVertexAttribArray(7); // iColor
    glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, 15*sizeof(float), (void*)(11*sizeof(float)));
    glVertexAttribDivisor(7, 1);
    
    glEnableVertexAttribArray(8); // iSpecies
    glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE, 15*sizeof(float), (void*)(14*sizeof(float)));
    glVertexAttribDivisor(8, 1);
    
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
static Mesh makeFishMesh() {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    auto push = [&](const glm::vec3& p, const glm::vec3& n){ v.push_back({p, glm::normalize(n)}); };

    const int segX=24, segR=20;
    const float rMax=0.065f, zFlatten=0.65f;
    for (int i=0; i<=segX; ++i) {
        float t = (float)i / (float)segX;
        float r = rMax * std::pow(std::sin(3.14159f * std::clamp(t*1.02f, 0.0f, 1.0f)), 0.75f);
        if (i == 0) r *= 0.6f;
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
    {   // nose
        glm::vec3 nose = {0.0f, 0.0f, 0.0f};
        unsigned baseCenter = (unsigned)v.size();
        v.push_back({nose, glm::vec3(-1,0,0)});
        for (int j=0; j<segR; ++j) { unsigned a=j, b=j+1; idx.insert(idx.end(), {baseCenter, b, a}); }
    }
    {   // tail fin (two-sided)
        float x = 1.05f;
        glm::vec3 tU = {x,  0.15f,  0.0f}, tD = {x, -0.15f,  0.0f};
        glm::vec3 baseL = {1.0f,  0.04f,  0.02f}, baseR = {1.0f, -0.04f,  0.02f};
        glm::vec3 baseL2= {1.0f,  0.04f, -0.02f}, baseR2= {1.0f, -0.04f, -0.02f};
        unsigned s = (unsigned)v.size();
        v.push_back({tU,{0,0, 1}}); v.push_back({tD,{0,0, 1}}); v.push_back({baseL,{0,0, 1}}); v.push_back({baseR,{0,0, 1}});
        v.push_back({tU,{0,0,-1}}); v.push_back({tD,{0,0,-1}}); v.push_back({baseL2,{0,0,-1}}); v.push_back({baseR2,{0,0,-1}});
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
    m.idxCount=(GLsizei)idx.size();
    glBindVertexArray(0); return m;
}

// Create realistic fish meshes for different species
static Mesh makeClownfishMesh() {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    auto push = [&](const glm::vec3& p, const glm::vec3& n){ v.push_back({p, glm::normalize(n)}); };

    // More detailed clownfish body
    const int segX=32, segR=24;
    const float rMax=0.07f, zFlatten=0.6f;
    for (int i=0; i<=segX; ++i) {
        float t = (float)i / (float)segX;
        float r = rMax * std::pow(std::sin(3.14159f * std::clamp(t*1.1f, 0.0f, 1.0f)), 0.8f);
        if (i == 0) r *= 0.5f; // Smaller head
        for (int j=0; j<=segR; ++j) {
            float a = (2.0f * 3.14159f) * (float)j / (float)segR;
            float cy = std::cos(a), sy = std::sin(a);
            glm::vec3 p = { t, r * cy, zFlatten * r * sy };
            glm::vec3 n = { 0.0f, cy, (1.0f / zFlatten) * sy };
            push(p, n);
        }
    }
    
    // Body triangles
    int ring = segR + 1;
    for (int i=0; i<segX; ++i) for (int j=0; j<segR; ++j) {
        unsigned a = i*ring + j, b=a+1, c=(i+1)*ring + j, d=c+1;
        idx.insert(idx.end(), {a,c,b, b,c,d});
    }
    
    // Dorsal fin
    for (int i=8; i<segX-4; i+=2) {
        float t = (float)i / (float)segX;
        float r = rMax * std::pow(std::sin(3.14159f * std::clamp(t*1.1f, 0.0f, 1.0f)), 0.8f);
        glm::vec3 base = {t, r, 0.0f};
        glm::vec3 tip = {t, r + 0.08f, 0.0f};
        unsigned baseIdx = i*ring + segR/2;
        unsigned tipIdx = (unsigned)v.size();
        v.push_back({tip, glm::vec3(0,1,0)});
        if (i > 8) {
            unsigned prevTip = tipIdx - 1;
            idx.insert(idx.end(), {baseIdx, tipIdx, prevTip});
        }
    }
    
    // Tail fin (more detailed)
    float x = 1.1f;
    glm::vec3 tU = {x,  0.18f,  0.0f}, tD = {x, -0.18f,  0.0f};
    glm::vec3 baseL = {1.0f,  0.05f,  0.03f}, baseR = {1.0f, -0.05f,  0.03f};
    glm::vec3 baseL2= {1.0f,  0.05f, -0.03f}, baseR2= {1.0f, -0.05f, -0.03f};
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
    m.idxCount=(GLsizei)idx.size();
    glBindVertexArray(0); return m;
}

static Mesh makeAngelfishMesh() {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    auto push = [&](const glm::vec3& p, const glm::vec3& n){ v.push_back({p, glm::normalize(n)}); };

    // Angelfish have a more triangular, compressed body
    const int segX=28, segR=20;
    const float rMax=0.08f, zFlatten=0.4f; // More compressed
    for (int i=0; i<=segX; ++i) {
        float t = (float)i / (float)segX;
        float r = rMax * std::pow(std::sin(3.14159f * std::clamp(t*1.2f, 0.0f, 1.0f)), 0.7f);
        if (i == 0) r *= 0.4f; // Very small head
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
    
    // Large dorsal fin
    for (int i=6; i<segX-2; i++) {
        float t = (float)i / (float)segX;
        float r = rMax * std::pow(std::sin(3.14159f * std::clamp(t*1.2f, 0.0f, 1.0f)), 0.7f);
        glm::vec3 base = {t, r, 0.0f};
        glm::vec3 tip = {t, r + 0.15f, 0.0f}; // Much larger fin
        unsigned baseIdx = i*ring + segR/2;
        unsigned tipIdx = (unsigned)v.size();
        v.push_back({tip, glm::vec3(0,1,0)});
        if (i > 6) {
            unsigned prevTip = tipIdx - 1;
            idx.insert(idx.end(), {baseIdx, tipIdx, prevTip});
        }
    }
    
    // Anal fin
    for (int i=segX/2; i<segX-2; i++) {
        float t = (float)i / (float)segX;
        float r = rMax * std::pow(std::sin(3.14159f * std::clamp(t*1.2f, 0.0f, 1.0f)), 0.7f);
        glm::vec3 base = {t, -r, 0.0f};
        glm::vec3 tip = {t, -r - 0.12f, 0.0f};
        unsigned baseIdx = i*ring + segR/2;
        unsigned tipIdx = (unsigned)v.size();
        v.push_back({tip, glm::vec3(0,-1,0)});
        if (i > segX/2) {
            unsigned prevTip = tipIdx - 1;
            idx.insert(idx.end(), {baseIdx, tipIdx, prevTip});
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
static Mesh makePlantStrip(int segments = 12, float height = 0.6f, float width = 0.027f) {
    struct V { glm::vec3 p,n; };
    std::vector<V> v;
    std::vector<unsigned> idx;
    for (int i=0;i<=segments;++i) {
        float t = (float)i/segments;
        float y = t * height;
        float w = width * (0.7f + 0.3f * (1.0f - t));
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

static Mesh makeCoral(int segments = 8, float height = 0.4f, float baseRadius = 0.08f) {
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> idx;
    
    for (int i=0; i<=segments; ++i) {
        float t = (float)i/segments;
        float y = t * height;
        float radius = baseRadius * (1.0f - t * 0.3f) + 0.02f * std::sin(t * 8.0f);
        
        for (int j=0; j<=8; ++j) {
            float angle = (2.0f * 3.14159f) * (float)j / 8.0f;
            float x = radius * std::cos(angle);
            float z = radius * std::sin(angle);
            
            glm::vec3 p(x, y, z);
            glm::vec3 n = glm::normalize(glm::vec3(x, 0.1f, z));
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

static Mesh fishMesh, clownfishMesh, angelfishMesh, plantMesh, tankMesh, floorMesh, waterMesh, rockMesh, coralMesh, shellMesh, driftwoodMesh;
static const glm::vec3 TANK_EXTENTS = {5.0f*0.5f - 0.05f, 1.4f, 3.0f*0.5f - 0.05f};
static float waterY = 0.85f;

static int N_CLOWN = 8, N_NEON = 15, N_DANIO = 12, N_ANGELFISH = 6, N_GOLDFISH = 4, N_BETTA = 3, N_GUPPY = 10, N_PLATY = 8;
static int N_PLANTS = 80, N_ROCKS = 12, N_CORALS = 8, N_SHELLS = 15, N_DRIFTWOOD = 6;

static GLuint plantVBO=0;
static std::vector<glm::vec3> plantPos;
static std::vector<glm::vec2> plantHP;
static std::vector<glm::vec3> plantColor;

static std::vector<glm::vec4> rocks;
static std::vector<glm::vec4> corals;
static std::vector<glm::vec4> shells;
static std::vector<glm::vec4> driftwood;

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
        glm::vec3 p(urand(rng)*TANK_EXTENTS.x*0.9f,
                    yMin + urand01(rng)*(yMax-yMin),
                    urand(rng)*TANK_EXTENTS.z*0.9f);
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
        float x = (urand01(rng) < 0.5f ? -1.0f : 1.0f) * (0.2f + urand01(rng)*0.6f) * TANK_EXTENTS.x;
        float z = urand(rng)*TANK_EXTENTS.z*0.9f;
        float h = 0.35f + urand01(rng)*0.55f;
        float phase = urand01(rng)*6.28318f;
        glm::vec3 col = glm::vec3(0.18f + urand01(rng)*0.1f, 0.55f + urand01(rng)*0.35f, 0.18f);
        plantPos[i]   = glm::vec3(x, -1.4f, z);
        plantHP[i]    = glm::vec2(h, phase);
        plantColor[i] = col;
    }
    if (!plantVBO) glGenBuffers(1, &plantVBO);

    rocks.resize(N_ROCKS);
    for (int i=0;i<N_ROCKS;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.75f;
        float z = urand(rng)*TANK_EXTENTS.z*0.75f;
        float r = 0.12f + urand01(rng)*0.22f;
        rocks[i] = glm::vec4(x, -1.4f, z, r);
    }
    
    corals.resize(N_CORALS);
    for (int i=0;i<N_CORALS;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.7f;
        float z = urand(rng)*TANK_EXTENTS.z*0.7f;
        float r = 0.15f + urand01(rng)*0.25f;
        corals[i] = glm::vec4(x, -1.4f, z, r);
    }
    
    shells.resize(N_SHELLS);
    for (int i=0;i<N_SHELLS;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.8f;
        float z = urand(rng)*TANK_EXTENTS.z*0.8f;
        float r = 0.08f + urand01(rng)*0.12f;
        shells[i] = glm::vec4(x, -1.4f, z, r);
    }
    
    driftwood.resize(N_DRIFTWOOD);
    for (int i=0;i<N_DRIFTWOOD;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.6f;
        float z = urand(rng)*TANK_EXTENTS.z*0.6f;
        float r = 0.2f + urand01(rng)*0.3f;
        driftwood[i] = glm::vec4(x, -1.4f, z, r);
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
static const int N_BUB = 120;
static std::vector<glm::vec3> bubblePos;
static GLuint bubbleVBO = 0, bubbleVAO = 0;

static void initBubbles() {
    bubblePos.resize(N_BUB);
    for (int i=0;i<N_BUB;++i) {
        float x = urand(rng)*TANK_EXTENTS.x*0.7f;
        float z = urand(rng)*TANK_EXTENTS.z*0.7f;
        float y = -1.4f + urand01(rng)*0.3f;
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
            bubblePos[i].y = -1.4f + urand01(rng)*0.2f;
            bubblePos[i].x = urand(rng)*TANK_EXTENTS.x*0.6f;
            bubblePos[i].z = urand(rng)*TANK_EXTENTS.z*0.6f;
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
    tankMesh  = makeBox(5.0f, 2.8f, 3.0f);
    floorMesh = makeFloor(5.0f, 3.0f, -1.4f);
    waterMesh = makeWaterPlane(160, 160, 5.0f, 3.0f, waterY);
    
    // Load realistic OBJ fish models
    std::cout << "Loading fish models..." << std::endl;
    fishMesh  = loadOBJModel("../models/fish.obj");           // Generic fish for most species
    clownfishMesh = loadOBJModel("../models/koi_fish.obj");   // Koi for clownfish (orange/reddish)
    angelfishMesh = loadOBJModel("../models/bream_fish__dorade_royale.obj"); // Bream for angelfish (silver/blue)
    
    // Create additional mesh for animated fish model
    Mesh animatedFishMesh = loadOBJModel("../models/fish_animated.obj"); // For goldfish (more detailed)
    std::cout << "Fish models loaded!" << std::endl;
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

    // ---------- species ----------
    auto initSpeciesVec_ = ::initSpeciesVec;
    initSpeciesVec_(clownfish, N_CLOWN, CLOWNFISH,
                    {1.0f,0.55f,0.20f}, {0.2f,0.1f,0.1f},
                    {1.2f,0.9f,1.0f},   {0.25f,0.1f,0.2f},
                    0.6f,1.2f, -1.0f,  waterY-0.09f, 0.85f, 1.10f);
    initSpeciesVec_(neon, N_NEON, NEON_TETRA,
                    {0.20f,0.85f,1.0f}, {0.2f,0.2f,0.2f},
                    {1.0f,0.7f,0.8f},   {0.2f,0.15f,0.15f},
                    0.7f,1.6f, -0.9f,   waterY-0.07f, 0.55f, 0.75f);
    initSpeciesVec_(danio, N_DANIO, ZEBRA_DANIO,
                    {0.9f,0.85f,0.55f}, {0.2f,0.2f,0.2f},
                    {1.3f,0.8f,0.9f},   {0.25f,0.12f,0.2f},
                    0.8f,1.8f, -0.9f,   waterY-0.07f, 0.65f, 0.9f);
    initSpeciesVec_(angelfish, N_ANGELFISH, ANGELFISH,
                    {0.8f,0.8f,0.9f}, {0.3f,0.3f,0.3f},
                    {1.5f,1.2f,0.6f},   {0.3f,0.2f,0.1f},
                    0.5f,1.0f, -0.8f,   waterY-0.12f, 1.1f, 1.3f);
    initSpeciesVec_(goldfish, N_GOLDFISH, GOLDFISH,
                    {1.0f,0.7f,0.2f}, {0.2f,0.1f,0.1f},
                    {1.1f,0.9f,1.0f},   {0.2f,0.15f,0.2f},
                    0.4f,0.8f, -0.7f,   waterY-0.15f, 1.2f, 1.4f);
    initSpeciesVec_(betta, N_BETTA, BETTA,
                    {0.8f,0.3f,0.8f}, {0.3f,0.2f,0.3f},
                    {1.0f,1.4f,0.7f},   {0.2f,0.3f,0.15f},
                    0.6f,1.1f, -0.6f,   waterY-0.08f, 0.9f, 1.1f);
    initSpeciesVec_(guppy, N_GUPPY, GUPPY,
                    {0.3f,0.8f,0.9f}, {0.2f,0.3f,0.2f},
                    {0.8f,0.6f,0.7f},   {0.15f,0.1f,0.15f},
                    0.8f,1.4f, -0.8f,   waterY-0.05f, 0.4f, 0.6f);
    initSpeciesVec_(platy, N_PLATY, PLATY,
                    {0.9f,0.4f,0.6f}, {0.2f,0.2f,0.2f},
                    {0.9f,0.7f,0.8f},   {0.15f,0.1f,0.15f},
                    0.7f,1.3f, -0.7f,   waterY-0.06f, 0.5f, 0.7f);

    setupFishInstancing(vboClown, fishMesh, N_CLOWN);
    setupFishInstancing(vboNeon,  fishMesh, N_NEON);
    setupFishInstancing(vboDanio, fishMesh, N_DANIO);
    setupFishInstancing(vboAngelfish, fishMesh, N_ANGELFISH);
    setupFishInstancing(vboGoldfish, fishMesh, N_GOLDFISH);
    setupFishInstancing(vboBetta, fishMesh, N_BETTA);
    setupFishInstancing(vboGuppy, fishMesh, N_GUPPY);
    setupFishInstancing(vboPlaty, fishMesh, N_PLATY);

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
    float fogNear = 2.0f, fogFar = 12.0f;
    float exposure = 1.25f;

    float last = (float)glfwGetTime();
    while (!glfwWindowShouldClose(win)) {
        float now=(float)glfwGetTime(), dt=now-last; last=now;
        glfwPollEvents();
        if (glfwGetKey(win, GLFW_KEY_ESCAPE)==GLFW_PRESS) glfwSetWindowShouldClose(win, 1);
        if (glfwGetKey(win, GLFW_KEY_F1)==GLFW_PRESS){ wireframe=!wireframe; glPolygonMode(GL_FRONT_AND_BACK, wireframe?GL_LINE:GL_FILL); }
        process_input(win, dt);

        // updates
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
                glm::vec3 steer(0); glm::vec3 lim=TANK_EXTENTS;
                if (pos.x> lim.x) steer.x -= (pos.x-lim.x)*2.2f; if (pos.x<-lim.x) steer.x += (-lim.x-pos.x)*2.2f;
                if (pos.z> lim.z) steer.z -= (pos.z-lim.z)*2.2f; if (pos.z<-lim.z) steer.z += (-lim.z-pos.z)*2.2f;
                if (pos.y> yMax)  steer.y -= (pos.y-yMax)*3.2f; if (pos.y< yMin)  steer.y += (yMin-pos.y)*3.2f;
                glm::vec3 drift(std::sin(f.phase*0.7f)*0.1f, std::sin(f.phase*1.3f)*0.05f, std::cos(f.phase*0.9f)*0.1f);
                glm::vec3 jitter(urand(rng)*0.12f, urand(rng)*0.06f, urand(rng)*0.12f);
                vel += align*alignW + coh*cohesion + sep*1.15f + steer*1.2f + drift*0.3f + jitter*0.25f;
                float s=glm::length(vel); if (s>maxSpeed) vel*= (maxSpeed/s);
                pos += vel*dt; f.pos=pos; f.vel=vel; f.phase += dt*3.0f;
            }
        };
        updateSchool(clownfish, -1.0f, waterY-0.09f, 1.2f);
        updateSchool(neon,      -0.9f, waterY-0.07f, 1.6f, 0.22f, 0.30f);
        updateSchool(danio,     -0.9f, waterY-0.07f, 1.8f, 0.18f, 0.40f);
        updateSchool(angelfish, -0.8f, waterY-0.12f, 1.0f, 0.15f, 0.35f);
        updateSchool(goldfish,  -0.7f, waterY-0.15f, 0.8f, 0.12f, 0.25f);
        updateSchool(betta,     -0.6f, waterY-0.08f, 1.1f, 0.20f, 0.45f);
        updateSchool(guppy,     -0.8f, waterY-0.05f, 1.4f, 0.25f, 0.35f);
        updateSchool(platy,     -0.7f, waterY-0.06f, 1.3f, 0.18f, 0.30f);
        

        updateBubbles(dt);

        // ------------------- Render to HDR FBO -------------------
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glViewport(0,0,SCR_W,SCR_H);
        glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = glm::perspective(glm::radians(60.0f),(float)SCR_W/(float)SCR_H,0.05f,100.0f);
        glm::mat4 view = glm::lookAt(camPos, camPos+camFront, camUp);

        // ===== Floor (sand) =====
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
        glUniform1i(u(progBasic,"uMaterialType"), 0);
        glUniform1f(u(progBasic,"uAlpha"), 1.0f);
        glUniform3f(u(progBasic,"uBaseColor"), 0.78f, 0.72f, 0.52f);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_CUBE_MAP, irrCube);
        glUniform1i(u(progBasic,"uIrradiance"), 1);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterCube);
        glUniform1i(u(progBasic,"uPrefilter"), 2);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, brdfLUT);
        glUniform1i(u(progBasic,"uBRDFLUT"), 3);
        glUniform1f(u(progBasic,"uPrefLodMax"), (float)prefilterMaxMip);
        glBindVertexArray(floorMesh.vao);
        glDrawElements(GL_TRIANGLES, floorMesh.idxCount, GL_UNSIGNED_INT, 0);

        // ===== Rocks =====
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
        
        // ===== Corals =====
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
        
        // ===== Shells =====
        glUniform1i(u(progBasic,"uMaterialType"), 3);
        for (int i=0;i<N_SHELLS;++i) {
            glm::vec4 s = shells[i];
            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(s.x, s.y, s.z))
                        * glm::scale(glm::mat4(1.0f), glm::vec3(s.w));
            glUniformMatrix4fv(u(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(M));
            glUniform3f(u(progBasic,"uBaseColor"), 0.9f+0.1f*(float)i/N_SHELLS, 0.85f+0.1f*(float)i/N_SHELLS, 0.7f+0.2f*(float)i/N_SHELLS);
            glBindVertexArray(shellMesh.vao);
            glDrawElements(GL_TRIANGLES, shellMesh.idxCount, GL_UNSIGNED_INT, 0);
        }
        
        // ===== Driftwood =====
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
        glUniform1i(u(progBasic,"uMaterialType"), 0);

        // ===== Plants =====
        {
            if (!plantVBO) glGenBuffers(1,&plantVBO);
            std::vector<float> data; data.resize(N_PLANTS*8);
            for (int i=0;i<N_PLANTS;++i) {
                int o=i*8;
                data[o+0]=plantPos[i].x; data[o+1]=plantPos[i].y; data[o+2]=plantPos[i].z;
                data[o+3]=plantHP[i].x;  data[o+4]=plantHP[i].y;
                data[o+5]=plantColor[i].r; data[o+6]=plantColor[i].g; data[o+7]=plantColor[i].b;
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
        glBindVertexArray(plantMesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, plantVBO);
        glEnableVertexAttribArray(8);  glVertexAttribPointer(8,3,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)0);                 glVertexAttribDivisor(8,1);
        glEnableVertexAttribArray(9);  glVertexAttribPointer(9,2,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)(sizeof(float)*3));  glVertexAttribDivisor(9,1);
        glEnableVertexAttribArray(10); glVertexAttribPointer(10,3,GL_FLOAT,GL_FALSE,sizeof(float)*8,(void*)(sizeof(float)*5)); glVertexAttribDivisor(10,1);
        glDrawElementsInstanced(GL_TRIANGLES, plantMesh.idxCount, GL_UNSIGNED_INT, 0, N_PLANTS);
        glBindVertexArray(0);

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
        uploadFish(clownfish, vboClown, clownfishMesh);
        uploadFish(neon,     vboNeon, fishMesh);
        uploadFish(danio,    vboDanio, fishMesh);
        uploadFish(angelfish, vboAngelfish, angelfishMesh);
        uploadFish(goldfish,  vboGoldfish, animatedFishMesh);
        uploadFish(betta,     vboBetta, fishMesh);
        uploadFish(guppy,     vboGuppy, fishMesh);
        uploadFish(platy,     vboPlaty, fishMesh);

        auto drawSpecies = [&](const std::vector<FishInst>& v, GLuint vbo, const Mesh& mesh){
            if (v.empty()) {
                return;
            }
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
        // Temporary: Use procedural fish to test positioning
        auto fallbackMesh = createFallbackMesh();
        drawSpecies(clownfish, vboClown, fallbackMesh);
        drawSpecies(neon,     vboNeon, fallbackMesh);
        drawSpecies(danio,    vboDanio, fallbackMesh);
        drawSpecies(angelfish, vboAngelfish, fallbackMesh);
        drawSpecies(goldfish,  vboGoldfish, fallbackMesh);
        drawSpecies(betta,     vboBetta, fallbackMesh);
        drawSpecies(guppy,     vboGuppy, fallbackMesh);
        drawSpecies(platy,     vboPlaty, fallbackMesh);

        // copy opaque for refraction
        glBindFramebuffer(GL_READ_FRAMEBUFFER, hdrFBO);
        glBindTexture(GL_TEXTURE_2D, opaqueCopyTex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, SCR_W, SCR_H);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        // ===== Bubbles =====
        glUseProgram(progBub);
        glUniformMatrix4fv(u(progBub,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(u(progBub,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glBindVertexArray(bubbleVAO);
        glDrawArrays(GL_POINTS, 0, N_BUB);

        // ===== Water =====
        glUseProgram(progWater);
        glUniformMatrix4fv(u(progWater,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(u(progWater,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(u(progWater,"uModel"),1,GL_FALSE,glm::value_ptr(glm::mat4(1.0f)));
        glUniform1f(u(progWater,"uTime"), now);
        glUniform3f(u(progWater,"uDeepColor"),    0.0f, 0.25f, 0.45f);
        glUniform3f(u(progWater,"uShallowColor"), 0.1f, 0.6f,  0.8f);
        glUniform3f(u(progWater,"uLightDir"), lightDir.x,lightDir.y,lightDir.z);
        glUniform3f(u(progWater,"uViewPos"), camPos.x,camPos.y,camPos.z);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, opaqueCopyTex);
        glUniform1i(u(progWater,"uSceneColor"), 0);
        glBindVertexArray(waterMesh.vao);
        glDisable(GL_CULL_FACE);
        glDrawElements(GL_TRIANGLES, waterMesh.idxCount, GL_UNSIGNED_INT, 0);
        glEnable(GL_CULL_FACE);

        // ===== Glass =====
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
        glUniform1i(u(progBasic,"uMaterialType"), 0);
        glUniform3f(u(progBasic,"uBaseColor"), 0.12f, 0.28f, 0.45f);
        glUniform1f(u(progBasic,"uAlpha"), 0.18f);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_CUBE_MAP, irrCube);
        glUniform1i(u(progBasic,"uIrradiance"), 1);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterCube);
        glUniform1i(u(progBasic,"uPrefilter"), 2);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, brdfLUT);
        glUniform1i(u(progBasic,"uBRDFLUT"), 3);
        glUniform1f(u(progBasic,"uPrefLodMax"), (float)prefilterMaxMip);
        glDepthMask(GL_FALSE);
        glBindVertexArray(tankMesh.vao);
        glDrawElements(GL_TRIANGLES, tankMesh.idxCount, GL_UNSIGNED_INT, 0);
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
