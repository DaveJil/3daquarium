#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>  // for std::clamp

#ifdef __APPLE__
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl3.h>   // macOS OpenGL Core profile (no GLAD)
#else
  #error "This setup is macOS-only. For Windows/Linux, use the GLEW or GLAD path."
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

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log);
        std::cerr << "Shader error: " << log << "\n";
    }
    return s;
}
static GLuint linkProgram(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetProgramInfoLog(p, 1024, nullptr, log);
        std::cerr << "Link error: " << log << "\n";
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

static Mesh makeBox(float w, float h, float d) {
    // inward-facing normals so we view from inside the tank
    float x=w*0.5f, y=h*0.5f, z=d*0.5f;
    struct V { glm::vec3 p,n; };
    std::vector<V> v; std::vector<unsigned> i;
    auto addQuad=[&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n){
        size_t base=v.size();
        v.push_back({a,n}); v.push_back({b,n}); v.push_back({c,n}); v.push_back({d,n});
        // wind so normal faces inward
        i.insert(i.end(), { (unsigned)base,(unsigned)base+2,(unsigned)base+1,
                            (unsigned)base,(unsigned)base+3,(unsigned)base+2 });
    };
    // faces (normals inward)
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
static Mesh makePlane(int nx=100, int nz=100, float sx=3.0f, float sz=3.0f, float y=0.0f) {
    struct V { glm::vec3 p; glm::vec2 uv; };
    std::vector<V> v; v.reserve((nx+1)*(nz+1));
    for (int z=0; z<=nz; ++z) {
        for (int x=0; x<=nx; ++x) {
            float u = (float)x/nx, w=(float)z/nz;
            v.push_back({ glm::vec3((u-0.5f)*sx, y, (w-0.5f)*sz), glm::vec2(u,w) });
        }
    }
    std::vector<unsigned> idx; idx.reserve(nx*nz*6);
    for (int z=0; z<nz; ++z) {
        for (int x=0; x<nx; ++x) {
            unsigned a = z*(nx+1)+x;
            unsigned b = a+1;
            unsigned c = a+(nx+1);
            unsigned d = c+1;
            idx.insert(idx.end(), {a,c,b, b,c,d});
        }
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

int main() {
    if (!glfwInit()) { std::cerr<<"GLFW init failed\n"; return -1; }

    // Create a modern core profile. macOS supports up to 4.1.
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

    // shaders
    std::string vs_basic = loadFile("shaders/basic.vert");
    std::string fs_basic = loadFile("shaders/basic.frag");
    std::string vs_water = loadFile("shaders/water.vert");
    std::string fs_water = loadFile("shaders/water.frag");
    GLuint progBasic = linkProgram(compileShader(GL_VERTEX_SHADER, vs_basic.c_str()),
                                   compileShader(GL_FRAGMENT_SHADER, fs_basic.c_str()));
    GLuint progWater = linkProgram(compileShader(GL_VERTEX_SHADER, vs_water.c_str()),
                                   compileShader(GL_FRAGMENT_SHADER, fs_water.c_str()));

    Mesh tank = makeBox(3.5f, 2.0f, 2.0f);
    Mesh water = makePlane(120, 120, 3.0f, 1.6f, 0.4f);

    auto uLoc = [&](GLuint p, const char* n){ return glGetUniformLocation(p, n); };

    float last = (float)glfwGetTime();
    while (!glfwWindowShouldClose(win)) {
        float now=(float)glfwGetTime(), dt=now-last; last=now;
        glfwPollEvents();
        if (glfwGetKey(win, GLFW_KEY_ESCAPE)==GLFW_PRESS) glfwSetWindowShouldClose(win, 1);
        if (glfwGetKey(win, GLFW_KEY_F1)==GLFW_PRESS){ wireframe=!wireframe; glPolygonMode(GL_FRONT_AND_BACK, wireframe?GL_LINE:GL_FILL); }
        process_input(win, dt);

        glClearColor(0.02f,0.05f,0.09f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = glm::perspective(glm::radians(60.0f),(float)SCR_W/(float)SCR_H,0.05f,100.0f);
        glm::mat4 view = glm::lookAt(camPos, camPos+camFront, camUp);

        // tank
        glUseProgram(progBasic);
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(uLoc(progBasic,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(uLoc(progBasic,"uView"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(uLoc(progBasic,"uModel"),1,GL_FALSE,glm::value_ptr(model));
        glUniform3f(uLoc(progBasic,"uColor"), 0.15f,0.35f,0.6f);
        glm::vec3 ldir = glm::normalize(glm::vec3(-1.0f,-1.5f,-0.4f));
        glUniform3f(uLoc(progBasic,"uLightDir"), ldir.x,ldir.y,ldir.z);
        glUniform3f(uLoc(progBasic,"uViewPos"), camPos.x, camPos.y, camPos.z);
        glBindVertexArray(tank.vao);
        glDrawElements(GL_TRIANGLES, tank.idxCount, GL_UNSIGNED_INT, 0);

        // water
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

        glfwSwapBuffers(win);
    }
    glfwTerminate();
    return 0;
}
