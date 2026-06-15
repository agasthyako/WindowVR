#include <iostream>
#include <vector>
#include <cmath>

#include <glad/glad.h> 
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "Camera.h"

// Networking includes 
#include <algorithm>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>

struct Vertex
{
    float x,y,z;
    float r,g,b;
};

static bool calibrated = false;
static float x_calib = 0.0f;
static float y_calib = 0.0f;

const char* vertexShaderSource = R"(

#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 MVP;

out vec3 Color;

void main()
{
    Color = aColor;
    gl_Position = MVP * vec4(aPos,1.0);
}

)";

const char* fragmentShaderSource = R"(

#version 330 core

in vec3 Color;

out vec4 FragColor;

void main()
{
    FragColor = vec4(Color,1.0);
}

)";

GLuint compileShader(GLenum type,const char* src)
{
    GLuint shader = glCreateShader(type);

    glShaderSource(shader,1,&src,nullptr);
    glCompileShader(shader);

    return shader;
}

GLuint createProgram()
{
    GLuint vs =
        compileShader(
            GL_VERTEX_SHADER,
            vertexShaderSource);

    GLuint fs =
        compileShader(
            GL_FRAGMENT_SHADER,
            fragmentShaderSource);

    GLuint program =
        glCreateProgram();

    glAttachShader(program,vs);
    glAttachShader(program,fs);

    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

int main()
{
    glfwInit();

    int udpSocket =
    socket(
        AF_INET,
        SOCK_DGRAM,
        0
    );

    sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(5005);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(
        udpSocket,
        (sockaddr*)&addr,
        sizeof(addr)
    );

    fcntl(
        udpSocket,
        F_SETFL,
        O_NONBLOCK
    );

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,
                   GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,
                   GL_TRUE);
#endif

    GLFWwindow* window =
        glfwCreateWindow(
            1280,
            720,
            "Window VR",
            nullptr,
            nullptr);

    glfwMakeContextCurrent(window);

    gladLoadGLLoader(
        (GLADloadproc)
        glfwGetProcAddress);

    GLuint shader = createProgram();

    Camera camera;

    std::vector<Vertex> vertices;

    const float HALF_W = 0.156f;
    const float HALF_H = 0.098f;

    const float ROOM_DEPTH = -5.0f; // MUST BE NEGATIVE

    const float BACK_HALF_W = HALF_W;
    const float BACK_HALF_H = HALF_H;

    const float START_DENSITY = 0.9f;
    const float END_DENSITY = 0.0f;

    int numSideSegments = 5;
    int numTopDownSegments = 7;
    int linesPerDepth = 2;


    // Side Walls Down The Line Segments
    for (int i = -numSideSegments/2; i <= numSideSegments/2; i++) {
        vertices.insert(vertices.end(), {
            // Left Wall Down The Line Segments
            {-HALF_W, HALF_H * (i) / (numSideSegments/2), 0, START_DENSITY, START_DENSITY, START_DENSITY},
            {-BACK_HALF_W, BACK_HALF_H * (i) / (numSideSegments/2), ROOM_DEPTH, END_DENSITY, END_DENSITY, END_DENSITY},
            
            // Right Wall Down The Line Segments
            {HALF_W, HALF_H * (i) / (numSideSegments/2), 0, START_DENSITY, START_DENSITY, START_DENSITY},
            {BACK_HALF_W, BACK_HALF_H * (i) / (numSideSegments/2), ROOM_DEPTH, END_DENSITY, END_DENSITY, END_DENSITY}
        });
    }


    // Ceil & Floor Down The Line Segments
    for (int i = -numTopDownSegments/2; i <= numTopDownSegments/2; i++) {
        vertices.insert(vertices.end(), {
            // Ceiling segments
            {-HALF_W * (i) / (numTopDownSegments/2), HALF_H, 0, START_DENSITY, START_DENSITY, START_DENSITY},
            {-BACK_HALF_W * (i) / (numTopDownSegments/2), BACK_HALF_H, ROOM_DEPTH, END_DENSITY, END_DENSITY, END_DENSITY},
            // Floor segments
            {-HALF_W * (i) / (numTopDownSegments/2), -HALF_H, 0, START_DENSITY, START_DENSITY, START_DENSITY},
            {-BACK_HALF_W * (i) / (numTopDownSegments/2), -BACK_HALF_H, ROOM_DEPTH, END_DENSITY, END_DENSITY, END_DENSITY}
        });
    }

    // Across Segments 
    int numDepthSegments = static_cast<int>(-ROOM_DEPTH * linesPerDepth);
    for (int i = 0; i <= numDepthSegments; i++) {

        float z = -i * (1.0f / linesPerDepth); // z goes from 0 to ROOM_DEPTH
        // Make less dense using a gradient near the back wall to avoid z-fighting
        float t = START_DENSITY - (START_DENSITY - END_DENSITY) * ((i * 1.0f) / numDepthSegments);

        vertices.insert(vertices.end(), {
            // Left
            {-BACK_HALF_W, BACK_HALF_H, z, t,t,t},
            {-BACK_HALF_W, -BACK_HALF_H, z, t,t,t},
            // Right
            {BACK_HALF_W, BACK_HALF_H, z, t,t,t},
            {BACK_HALF_W, -BACK_HALF_H, z, t,t,t},
            // Floor
            {-BACK_HALF_W, -BACK_HALF_H, z, t,t,t},
            { BACK_HALF_W, -BACK_HALF_H, z, t,t,t},
            // Ceiling
            {-BACK_HALF_W, BACK_HALF_H, z, t,t,t},
            { BACK_HALF_W, BACK_HALF_H, z, t,t,t}
        });
    }


    float s = 0.01f;
    // Cube edges
    std::vector<Vertex> cube = {

        // bottom square
        {-s,-s,-2, 1,0,0}, { s,-s,-2, 1,0,0},
        { s,-s,-2, 1,0,0}, { s,-s,-3, 1,0,0},
        { s,-s,-3, 1,0,0}, {-s,-s,-3, 1,0,0},
        {-s,-s,-3, 1,0,0}, {-s,-s,-2, 1,0,0},

        // top square
        {-s, s,-2, 1,0,0}, { s, s,-2, 1,0,0},
        { s, s,-2, 1,0,0}, { s, s,-3, 1,0,0},
        { s, s,-3, 1,0,0}, {-s, s,-3, 1,0,0},
        {-s, s,-3, 1,0,0}, {-s, s,-2, 1,0,0},

        // vertical lines
        {-s,-s,-2, 1,0,0}, {-s, s,-2, 1,0,0},
        { s,-s,-2, 1,0,0}, { s, s,-2, 1,0,0},
        { s,-s,-3, 1,0,0}, { s, s,-3, 1,0,0},
        {-s,-s,-3, 1,0,0}, {-s, s,-3, 1,0,0},
    };

    vertices.insert(vertices.end(), cube.begin(), cube.end());

    GLuint vao,vbo;

    glGenVertexArrays(1,&vao);
    glGenBuffers(1,&vbo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER,vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size()*sizeof(Vertex),
        vertices.data(),
        GL_STATIC_DRAW);

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)0);

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)(3*sizeof(float)));

    glEnableVertexAttribArray(1);

    glEnable(GL_DEPTH_TEST);

    while(!glfwWindowShouldClose(window))
    {
        float t = glfwGetTime();
        
        
        char buffer[128];
        int bytes =
            recv(
                udpSocket,
                buffer,
                sizeof(buffer)-1,
                0
            );

        if(bytes > 0)
        {
            buffer[bytes] = '\0';

            std::stringstream ss(buffer);
            
            float camera_HFOV = 122.0f; // in degrees, adjust as needed
            float camera_VFOV = 40.0f; // in degrees, adjust as

            float x;
            float y;
            float z;
            /* 
                0,0     1,0

                0,1     1,1
            */ 

            char comma;

            ss >>
                x >>
                comma >>
                y >>
                comma >>
                z;

            float screenW = 0.312f;  // MacBook 14"
            float screenH = 0.196f;

            /*float eyeX = (x - 0.5f) * screenW;
            float eyeY = -(y - 0.5f) * screenH;
            

            if (!calibrated)
            {
                baselineEyeDist = eyeDist;
                calibrated = true;
            }
            float eyeZ = 0.6f * (baselineEyeDist / eyeDist);
            eyeZ = std::clamp(eyeZ, 0.3f, 1.5f);*/

            /*if (!calibrated)
            {
                x_calib = x;
                y_calib = y;
                calibrated = true;
            }
            y = y - y_calib; // center y around 0
            x = x - x_calib; // center x around 0 */
            x = std::clamp(x, -screenW/2, screenW/2);
            y = std::clamp(y, -screenH/2, screenH/2);

            camera.setHeadPosition(
            {
                x,
                y,
                z
            });
        }

        glClearColor(
            0.05f,
            0.05f,
            0.08f,
            1.0f);

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT);

        glm::mat4 mvp =
            camera.getProjectionMatrix() *
            camera.getViewMatrix();

        glUseProgram(shader);

        GLuint mvpLoc =
            glGetUniformLocation(
                shader,
                "MVP");

        glUniformMatrix4fv(
            mvpLoc,
            1,
            GL_FALSE,
            &mvp[0][0]);

        glBindVertexArray(vao);

        glDrawArrays(
            GL_LINES,
            0,
            vertices.size());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();

    return 0;
}