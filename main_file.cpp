#define GLM_FORCE_RADIANS
#define GLM_FORCE_SWIZZLE

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <array>
#include <string>
#include "constants.h"
#include "lodepng.h"
#include "shaderprogram.h"
#include "model.h"

// Shadow‐map size and globals
const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
unsigned int depthCubemap[2], depthMapFBO[2];
ShaderProgram* depthShader;

// near/far for point‐light projection
const float near_plane = 1.0f, far_plane = 25.0f;

// Camera & timing
float aspectRatio = 1.0f;
glm::vec3 cameraPos = { 1.0f, 0.5f, -0.4f };
glm::vec3 cameraFront = { 0,0,-1 };
glm::vec3 cameraUp = { 0,1,0 };
glm::vec2 cameraShakeOffset = { 0,0 };

float totalTime = 0.0f;
float deltaTime = 0.0f, lastFrame = 0.0f;

// mouse look
float yaw = -90.0f, pitch = 0.0f;
float lastX = 250, lastY = 250;
bool  firstMouse = true;

// Drinkable state
struct Drinkable {
    Model* model;
    glm::vec3 position;
    glm::vec3 scale;
};
std::vector<Drinkable> drinkables;
int   heldDrinkableIndex = -1;
bool  isDrinking = false;
float drinkingTimer = 0.0f;
const float DRINK_DURATION = 3.0f;
int   intoxicationLevel = 0;
float intoxicationTimer = 0.0f;
const float INTOX_DURATION = 10.0f;

// collision boxes
struct AABB {
    glm::vec3 min, max;
    bool contains(const glm::vec3& p) const {
        return p.x >= std::min(min.x, max.x) && p.x <= std::max(min.x, max.x)
            && p.y >= std::min(min.y, max.y) && p.y <= std::max(min.y, max.y)
            && p.z >= std::min(min.z, max.z) && p.z <= std::max(min.z, max.z);
    }
};
std::vector<AABB> sceneColliders;

// Shaders & models
ShaderProgram* spModel;
Model* bottlesModel, * modelDesk, * modelDoor,
* modelFloor, * modelShelfs, * modelWalls,
* modelCeiling, * modelLamp;

// forward declarations
void renderSceneDepth(ShaderProgram*);
void RenderDepthCubemaps(GLFWwindow*);

// Error callback
void error_callback(int e, const char* desc) {
    fputs(desc, stderr);
}

// Input helpers
glm::vec3 getRayDirection() {
    return glm::normalize(cameraFront);
}
int getDrinkableInFocus(float maxD = 0.5f, float r = 0.15f) {
    glm::vec3 ro = cameraPos, rd = getRayDirection();
    for (size_t i = 0; i < drinkables.size(); ++i) {
        glm::vec3 toC = drinkables[i].position - ro;
        float proj = glm::dot(toC, rd);
        if (proj<0 || proj>maxD) continue;
        glm::vec3 cp = ro + rd * proj;
        float dsq = glm::dot(drinkables[i].position - cp,
            drinkables[i].position - cp);
        if (dsq < r * r) return (int)i;
    }
    return -1;
}
void updateCameraDirection() {
    glm::vec3 dir;
    dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.y = sin(glm::radians(pitch));
    dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(dir);
}
void processInput(GLFWwindow* w) {
    float speed = deltaTime;
    glm::vec3 flat = glm::normalize(glm::vec3(cameraFront.x, 0, cameraFront.z));
    glm::vec3 right = glm::normalize(glm::cross(flat, cameraUp));
    glm::vec3 prop = cameraPos;
    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) prop += flat * speed;
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) prop -= flat * speed;
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) prop -= right * speed;
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) prop += right * speed;
    glm::vec3 feet = prop - glm::vec3(0, 0.4f, 0);
    bool collide = false;
    for (auto& b : sceneColliders)
        if (b.contains(feet)) { collide = true; break; }
    if (!collide) cameraPos = prop;
    // E pick/drop
    static bool eL = false; bool eN = (glfwGetKey(w, GLFW_KEY_E) == GLFW_PRESS);
    if (eN && !eL && !isDrinking) {
        if (heldDrinkableIndex < 0) {
            int idx = getDrinkableInFocus();
            if (idx >= 0) heldDrinkableIndex = idx;
        }
        else heldDrinkableIndex = -1;
    }
    eL = eN;
    // F drink
    static bool fL = false; bool fN = (glfwGetKey(w, GLFW_KEY_F) == GLFW_PRESS);
    if (fN && !fL && heldDrinkableIndex >= 0 && !isDrinking) {
        isDrinking = true;
        drinkingTimer = DRINK_DURATION;
    }
    fL = fN;
}
void mouse_callback(GLFWwindow*, double xpos, double ypos) {
    if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }
    float xoff = (float)xpos - lastX, yoff = lastY - (float)ypos;
    lastX = (float)xpos; lastY = (float)ypos;
    const float S = 0.05f; xoff *= S; yoff *= S;
    yaw += xoff; pitch += yoff;
    if (pitch > 89.0f)pitch = 89.0f; if (pitch < -89.0f)pitch = -89.0f;
    updateCameraDirection();
}
void windowResizeCallback(GLFWwindow*, int w, int h) {
    if (h == 0) return;
    aspectRatio = (float)w / (float)h;
    glViewport(0, 0, w, h);
}

// Build point‐light transforms
std::vector<glm::mat4> buildPointLightTransforms(const glm::vec3& lp) {
    float aspect = float(SHADOW_WIDTH) / float(SHADOW_HEIGHT);
    glm::mat4 P = glm::perspective(glm::radians(90.0f),
        aspect, near_plane, far_plane);
    std::vector<glm::mat4> M(6);
    M[0] = P * glm::lookAt(lp, lp + glm::vec3(1, 0, 0), glm::vec3(0, -1, 0));
    M[1] = P * glm::lookAt(lp, lp + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0));
    M[2] = P * glm::lookAt(lp, lp + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1));
    M[3] = P * glm::lookAt(lp, lp + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1));
    M[4] = P * glm::lookAt(lp, lp + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0));
    M[5] = P * glm::lookAt(lp, lp + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0));
    return M;
}

// Render geometry for depth‐pass
void renderSceneDepth(ShaderProgram* sh) {
    auto setM = [&](const glm::mat4& M) {
        glUniformMatrix4fv(sh->u("model"), 1, GL_FALSE, glm::value_ptr(M));
        };
    // desk
    glm::mat4 Mdesk = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0))
        * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 0.64f, 1.0f));
    setM(Mdesk); modelDesk->Draw(sh);
    // door/floor/shelfs/walls
    setM(glm::mat4(1.0f)); modelDoor->Draw(sh);
    setM(glm::mat4(1.0f)); modelFloor->Draw(sh);
    setM(glm::mat4(1.0f)); modelShelfs->Draw(sh);
    setM(glm::mat4(1.0f)); modelWalls->Draw(sh);
    // ceiling
    setM(glm::translate(glm::mat4(1.0f), glm::vec3(0, 1.1f, 0)));
    modelCeiling->Draw(sh);
    // lamps
    glm::mat4 L1 = glm::translate(glm::mat4(1.0f), glm::vec3(2.47f, 0.6f, -1.5f))
        * glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0, 1, 0))
        * glm::scale(glm::mat4(1.0f), glm::vec3(1.25f));
    setM(L1); modelLamp->Draw(sh);
    glm::mat4 L2 = glm::translate(glm::mat4(1.0f), glm::vec3(0.04f, 0.6f, -1.5f))
        * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 1, 0))
        * glm::scale(glm::mat4(1.0f), glm::vec3(1.25f));
    setM(L2); modelLamp->Draw(sh);
    // shelf bottles
    int rows = 3, cols = 5;
    for (int r = 0; r < rows; ++r)for (int c = 0; c < cols; ++c) {
        glm::vec3 pos = { 0.2f + c * 0.4f,0.24f + r * 0.235f,-2.25f };
        glm::mat4 Mb = glm::translate(glm::mat4(1.0f), pos)
            * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
        setM(Mb); bottlesModel->Draw(sh);
    }
    // drinkables
    for (size_t i = 0; i < drinkables.size(); ++i) {
        auto& d = drinkables[i];
        float hover = sin((totalTime + i * 5.0f) * 2.0f) * 0.02f;
        float ang = totalTime * 60.0f;
        glm::mat4 M(1.0f);
        if ((int)i == heldDrinkableIndex) {
            glm::vec3 hp = cameraPos
                + cameraFront * (isDrinking ? 0.25f : 0.3f)
                + cameraUp * (isDrinking ? 0.05f : -0.15f);
            M = glm::translate(glm::mat4(1.0f), hp);
            float a = atan2(cameraFront.x, cameraFront.z);
            M = glm::rotate(M, a + glm::radians(180.0f), glm::vec3(0, 1, 0));
            if (isDrinking)
                M = glm::rotate(M, glm::radians(120.0f), glm::vec3(1, 0, 0));
            M = glm::scale(M, d.scale);
        }
        else {
            M = glm::translate(glm::mat4(1.0f), d.position + glm::vec3(0, hover, 0))
                * glm::rotate(glm::mat4(1.0f), glm::radians(ang), glm::vec3(0, 1, 0))
                * glm::scale(glm::mat4(1.0f), d.scale);
        }
        setM(M); d.model->Draw(sh);
    }
}



// Render both point‐light depth cubemaps
void RenderDepthCubemaps(GLFWwindow* window) {
    std::array<glm::vec3, 2> LP = {
        glm::vec3(2.3f,0.75f,-1.52f),
        glm::vec3(0.21f,0.75f,-1.52f)
    };
    for (int i = 0; i < 2; ++i) {
        auto mats = buildPointLightTransforms(LP[i]);
        depthShader->use();
        for (int f = 0; f < 6; ++f) {
            std::string name = "shadowMatrices[" + std::to_string(f) + "]";
            glUniformMatrix4fv(depthShader->u(name.c_str()),
                1, GL_FALSE, glm::value_ptr(mats[f]));
        }
        glUniform3fv(depthShader->u("lightPos"), 1, glm::value_ptr(LP[i]));
        glUniform1f(depthShader->u("far_plane"), far_plane);

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO[i]);
        glClear(GL_DEPTH_BUFFER_BIT);
        renderSceneDepth(depthShader);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    int w, h; glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
}

// Initialize everything
void initOpenGLProgram(GLFWwindow* window) {
    glClearColor(0, 0, 0, 1);
    glEnable(GL_DEPTH_TEST);
    glfwSetWindowSizeCallback(window, windowResizeCallback);

    // load your main textured shader
    spModel = new ShaderProgram("v_textures.glsl", nullptr, "f_textures.glsl");

    // load the depth‐only shader (VS, GS, FS) for point‐light shadows
    depthShader = new ShaderProgram("depth_shader.vs",
        "depth_shader.gs",
        "depth_shader.fs");

    // create two FBOs + 2 cubemap textures for your two point lights
    glGenFramebuffers(2, depthMapFBO);
    glGenTextures(2, depthCubemap);
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap[i]);
        for (unsigned f = 0; f < 6; ++f) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f,
                0, GL_DEPTH_COMPONENT,
                SHADOW_WIDTH, SHADOW_HEIGHT,
                0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO[i]);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            depthCubemap[i], 0);
        // no color
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // load all your scene models
    bottlesModel = new Model("models/bottles_for_shelf/bottles_for_shelf.obj");
    modelDesk = new Model("models/Desk/Desk.obj");
    modelDoor = new Model("models/Door/Door.obj");
    modelFloor = new Model("models/Floor/Floor.obj");
    modelShelfs = new Model("models/Shelfs/Shelfs.obj");
    modelWalls = new Model("models/Walls/Walls.obj");
    modelCeiling = new Model("models/Ceiling/Ceiling.obj");
    modelLamp = new Model("models/Lamp/Lamp.obj");

    // push your drinkables
    drinkables.clear();
    drinkables.push_back({ new Model("models/Drinkable1/drinkable1.obj"), glm::vec3(0.4f,0.35f,-1.3f), glm::vec3(1.0f) });
    drinkables.push_back({ new Model("models/Drinkable2/drinkable2.obj"), glm::vec3(0.8f,0.35f,-1.3f), glm::vec3(0.8f) });
    drinkables.push_back({ new Model("models/Drinkable3/drinkable3.obj"), glm::vec3(1.2f,0.35f,-1.3f), glm::vec3(0.035f) });
    drinkables.push_back({ new Model("models/Drinkable4/drinkable4.obj"), glm::vec3(1.6f,0.35f,-1.3f), glm::vec3(0.85f) });

    // your scene colliders
    sceneColliders.clear();
    sceneColliders.push_back({ {0.03f,0.01f,-1.79f},{ 2.43f,0.31f,-1.13f} });
    sceneColliders.push_back({ {0.07f,0.01f,-1.25f},{-0.03f,0.74f,-0.05f} });
    sceneColliders.push_back({ {2.54f,0.04f,-1.72f},{ 2.38f,0.87f, 0.02f} });
    sceneColliders.push_back({ {2.56f,-0.06f,-0.15f},{-0.07f,1.03f, 0.14f} });

    // capture the mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
}

// Cleanup
void freeOpenGLProgram(GLFWwindow*) {
    glDeleteFramebuffers(2, depthMapFBO);
    glDeleteTextures(2, depthCubemap);
    delete depthShader;
    delete spModel;
    delete bottlesModel;
    delete modelDesk;
    delete modelDoor;
    delete modelFloor;
    delete modelShelfs;
    delete modelWalls;
    delete modelCeiling;
    delete modelLamp;
    for (auto& d : drinkables) delete d.model;
}

// Draw the scene after you've already called RenderDepthCubemaps
void drawScene(GLFWwindow* window, float angle_x, float angle_y) {
    // Clear
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Build camera matrices
    glm::mat4 V = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    glm::mat4 P = glm::perspective(glm::radians(50.0f),
                                   aspectRatio, 0.01f, 50.0f);
    // Light positions + colors
    glm::vec3 lightPos1   = glm::vec3(2.3f,  0.75f, -1.52f);
    glm::vec3 lightColor1 = glm::vec3(1.0f,  1.0f,   0.9f);
    glm::vec3 lightPos2   = glm::vec3(0.21f, 0.75f, -1.52f);
    glm::vec3 lightColor2 = glm::vec3(1.0f,  1.0f,   0.9f);

    // Bind your main shader & pass all uniforms
    spModel->use();
    spModel->setMat4 ("P",          P);
    spModel->setMat4 ("V",          V);
    spModel->setVec3 ("cameraPos",  cameraPos);

    spModel->setInt("isEmissive", 0);
  
    spModel->setVec3("lightPos[0]", lightPos1);
    spModel->setVec3("lightColor[0]", lightColor1);
    spModel->setVec3("lightPos[1]", lightPos2);
    spModel->setVec3("lightColor[1]", lightColor2);

    spModel->setInt("depthMap[0]", 3);
    spModel->setInt("depthMap[1]", 4);
    spModel->setFloat("far_plane", far_plane);

    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap[0]);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap[1]);
    glActiveTexture(GL_TEXTURE0);

    // drawAt helper
    auto drawAt = [&](Model* Mdl, glm::vec3 pos, glm::vec3 s){
        glm::mat4 M = glm::translate(glm::mat4(1.0f), pos)
                    * glm::scale(glm::mat4(1.0f), s);
        spModel->setMat4("M", M);
        glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(M)));
        spModel->setMat3("normalMatrix", nm);
        Mdl->Draw(spModel);
        glBindVertexArray(0);
    };

    // draw static scene geometry
    drawAt(modelDesk, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.64f, 1.0f));
    drawAt(modelDoor, glm::vec3(0.0f), glm::vec3(1.0f));
    drawAt(modelFloor, glm::vec3(0.0f), glm::vec3(1.0f));
    drawAt(modelShelfs, glm::vec3(0.0f), glm::vec3(1.0f));
    drawAt(modelWalls, glm::vec3(0.0f), glm::vec3(1.0f));
    drawAt(modelCeiling, glm::vec3(0.0f, 1.1f, 0.0f), glm::vec3(1.0f));


    // lamp helper
    auto drawLamp = [&](glm::vec3 p, float ry){
        glm::mat4 M = glm::translate(glm::mat4(1.0f), p) * glm::rotate (glm::mat4(1.0f), glm::radians(ry), glm::vec3(0,1,0)) * glm::scale (glm::mat4(1.0f), glm::vec3(1.25f));
        spModel->setMat4("M", M);
        glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(M)));
        spModel->setMat3("normalMatrix", nm);
        modelLamp->Draw(spModel);
        glBindVertexArray(0);
    };
    drawLamp({2.47f,0.6f,-1.5f}, -90.0f);
    drawLamp({0.04f,0.6f,-1.5f},  90.0f);

    // levitating & held drinkables:
    for(size_t i=0;i<drinkables.size();++i){
      auto& d = drinkables[i];
      float hover = sin((totalTime + i*5.0f)*2.0f)*0.02f;
      float angle = totalTime * 60.0f;
      glm::mat4 M(1.0f);
      if ((int)i == heldDrinkableIndex) {
        glm::vec3 hp = cameraPos + cameraFront * (isDrinking ? 0.25f : 0.3f) + cameraUp    * (isDrinking ? 0.05f : -0.15f);
        M = glm::translate(glm::mat4(1.0f), hp);
        float a = atan2(cameraFront.x, cameraFront.z);
        M = glm::rotate(M, a + glm::radians(180.0f), glm::vec3(0,1,0));
        if (isDrinking)
          M = glm::rotate(M, glm::radians(120.0f), glm::vec3(1,0,0));
        M = glm::scale(M, d.scale);
      }
      else {
        M = glm::translate(glm::mat4(1.0f), d.position + glm::vec3(0,hover,0)) * glm::rotate (glm::mat4(1.0f), glm::radians(angle), glm::vec3(0,1,0)) * glm::scale  (glm::mat4(1.0f), d.scale);
      }
      spModel->setMat4("M", M);
      glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(M)));
      spModel->setMat3("normalMatrix", nm);
      d.model->Draw(spModel);
      glBindVertexArray(0);
    }

    // shelf bottles (3×5)
    {
      int rows=3, cols=5;
      float bx=0.2f, by=0.24f, bz=-2.25f;
      for(int r=0;r<rows;++r) for(int c=0;c<cols;++c){
        glm::vec3 p = {bx + c*0.4f, by + r*0.235f, bz};
        glm::mat4 M = glm::translate(glm::mat4(1.0f), p)
                    * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
        spModel->setMat4("M", M);
        glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(M)));
        spModel->setMat3("normalMatrix", nm);
        bottlesModel->Draw(spModel);
        glBindVertexArray(0);
      }
    }

    glfwSwapBuffers(window);
}

int main() {
    // GLFW error callback
    glfwSetErrorCallback(error_callback);

    // init window + OpenGL context
    if (!glfwInit()) {
        fprintf(stderr, "GLFW init failed\n");
        return -1;
    }
    GLFWwindow* win = glfwCreateWindow(800, 600, "Galeria Alkoholi", nullptr, nullptr);
    if (!win) {
        fprintf(stderr, "Window creation failed\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(win);
    glewInit();

    // build all shaders, FBOs, load models, etc.
    initOpenGLProgram(win);

    // main loop
    while (!glfwWindowShouldClose(win)) {
        // time management
        float now = (float)glfwGetTime();
        deltaTime = now - lastFrame;
        lastFrame = now;
        totalTime += deltaTime;

        // intoxication timer & camera shake
        if (intoxicationTimer > 0.0f) {
            intoxicationTimer -= deltaTime;
            if (intoxicationTimer <= 0.0f)
                intoxicationLevel = 0;
        }
        if (intoxicationLevel >= 1) {
            float freq = 1.8f + intoxicationLevel * 0.3f;
            float amp = 0.15f + 0.05f * intoxicationLevel;
            cameraShakeOffset.x = sin(totalTime * freq * 2 * PI) * amp;
            cameraShakeOffset.y = cos(totalTime * freq * 2 * PI) * amp;
            yaw += cameraShakeOffset.x;
            pitch += cameraShakeOffset.y;
            pitch = glm::clamp(pitch, -89.0f, 89.0f);
            updateCameraDirection();
        }

        // drinking logic 
        if (isDrinking) {
            drinkingTimer -= deltaTime;
            if (drinkingTimer <= 0.0f) {
                isDrinking = false;
                heldDrinkableIndex = -1;
                intoxicationLevel = std::min(4, intoxicationLevel + 1);
                intoxicationTimer = INTOX_DURATION;
            }
        }

        // input & movement 
        processInput(win);

        // shadow pass
        RenderDepthCubemaps(win);

        // ligtning pass
        drawScene(win, 0.0f, 0.0f);

        // Poll & swap
        glfwPollEvents();
    }

    // cleanup
    freeOpenGLProgram(win);
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
