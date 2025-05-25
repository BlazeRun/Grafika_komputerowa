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
#include "constants.h"
#include "lodepng.h"
#include "shaderprogram.h"
#include "myCube.h"
#include "myTeapot.h"

float speed_x = 0;
float speed_y = 0;
float aspectRatio = 1;

glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 6.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

float yaw = -90.0f;
float pitch = 0.0f;
float lastX = 250.0f;
float lastY = 250.0f;
bool firstMouse = true;

ShaderProgram* sp;

void error_callback(int error, const char* description) {
	fputs(description, stderr);
}

void processInput(GLFWwindow* window) {
	float cameraSpeed = 2.5f * deltaTime;
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		cameraPos += cameraSpeed * cameraFront;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		cameraPos -= cameraSpeed * cameraFront;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	if (firstMouse) {
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos;

	lastX = xpos;
	lastY = ypos;

	float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	if (pitch > 89.0f) pitch = 89.0f;
	if (pitch < -89.0f) pitch = -89.0f;

	glm::vec3 direction;
	direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	direction.y = sin(glm::radians(pitch));
	direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraFront = glm::normalize(direction);
}

void windowResizeCallback(GLFWwindow* window, int width, int height) {
	if (height == 0) return;
	aspectRatio = (float)width / (float)height;
	glViewport(0, 0, width, height);
}

void initOpenGLProgram(GLFWwindow* window) {
	glClearColor(0, 0, 0, 1);
	glEnable(GL_DEPTH_TEST);
	glfwSetWindowSizeCallback(window, windowResizeCallback);

	sp = new ShaderProgram("v_simplest.glsl", NULL, "f_simplest.glsl");
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window, mouse_callback);
}

void freeOpenGLProgram(GLFWwindow* window) {
	delete sp;
}

void drawCube(glm::vec3 position, glm::vec3 scale, glm::vec3 color) {
	glm::mat4 M = glm::mat4(1.0f);
	M = glm::translate(M, position);
	M = glm::scale(M, scale);

	glUniformMatrix4fv(sp->u("M"), 1, false, glm::value_ptr(M));
	glVertexAttribPointer(sp->a("vertex"), 4, GL_FLOAT, false, 0, myCubeVertices);
	glEnableVertexAttribArray(sp->a("vertex"));

	std::vector<float> tempColors(4 * myCubeVertexCount, 0.0f);
	for (int i = 0; i < myCubeVertexCount; ++i) {
		tempColors[i * 4 + 0] = color.r;
		tempColors[i * 4 + 1] = color.g;
		tempColors[i * 4 + 2] = color.b;
		tempColors[i * 4 + 3] = 1.0f;
	}
	glVertexAttribPointer(sp->a("color"), 4, GL_FLOAT, false, 0, tempColors.data());
	glEnableVertexAttribArray(sp->a("color"));

	glVertexAttribPointer(sp->a("normal"), 3, GL_FLOAT, false, 0, myCubeNormals);
	glEnableVertexAttribArray(sp->a("normal"));

	glDrawArrays(GL_TRIANGLES, 0, myCubeVertexCount);

	glDisableVertexAttribArray(sp->a("vertex"));
	glDisableVertexAttribArray(sp->a("color"));
	glDisableVertexAttribArray(sp->a("normal"));
}

void drawScene(GLFWwindow* window, float angle_x, float angle_y) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 V = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
	glm::mat4 P = glm::perspective(50.0f * PI / 180.0f, aspectRatio, 0.01f, 50.0f);

	sp->use();
	glUniform1i(sp->u("isEmissive"), false); // Domyślnie: brak emisji światła
	glUniformMatrix4fv(sp->u("P"), 1, false, glm::value_ptr(P));
	glUniformMatrix4fv(sp->u("V"), 1, false, glm::value_ptr(V));

	glm::vec3 lightPos1 = glm::vec3(-2.0f, 2.85f, 0.0f);
	glm::vec3 lightColor1 = glm::vec3(1.2f, 1.05f, 0.85f);
	glm::vec3 lightPos2 = glm::vec3(2.0f, 2.85f, 0.0f);
	glm::vec3 lightColor2 = glm::vec3(1.2f, 1.05f, 0.85f);

	glUniform3fv(sp->u("lightPos1"), 1, glm::value_ptr(lightPos1));
	glUniform3fv(sp->u("lightColor1"), 1, glm::value_ptr(lightColor1));
	glUniform3fv(sp->u("lightPos2"), 1, glm::value_ptr(lightPos2));
	glUniform3fv(sp->u("lightColor2"), 1, glm::value_ptr(lightColor2));
	glUniform3fv(sp->u("cameraPos"), 1, glm::value_ptr(cameraPos));

	// Pomieszczenie
	drawCube(glm::vec3(0, -1, 0), glm::vec3(10, 0.1, 10), glm::vec3(0.3f));                  // podłoga
	drawCube(glm::vec3(0, 1.5, -5), glm::vec3(10, 3, 0.1), glm::vec3(0.2f));                 // tylna ściana
	drawCube(glm::vec3(-5, 1.5, 0), glm::vec3(0.1, 3, 10), glm::vec3(0.2f));                 // lewa ściana
	drawCube(glm::vec3(5, 1.5, 0), glm::vec3(0.1, 3, 10), glm::vec3(0.2f));                  // prawa ściana
	drawCube(glm::vec3(0, 3.0f, 0), glm::vec3(10, 0.1f, 10), glm::vec3(0.25f));              // sufit



	// Lampy (emitujące światło)
	glUniform1i(sp->u("isEmissive"), true);
	drawCube(glm::vec3(-2.0f, 2.85f, 0.0f), glm::vec3(1.0f, 0.05f, 2.0f), glm::vec3(1.0f, 1.0f, 0.85f));
	drawCube(glm::vec3(2.0f, 2.85f, 0.0f), glm::vec3(1.0f, 0.05f, 2.0f), glm::vec3(1.0f, 1.0f, 0.85f));
	glUniform1i(sp->u("isEmissive"), false); // reset


	// Kontuar
	drawCube(glm::vec3(0, -0.5, -2), glm::vec3(3, 1, 1), glm::vec3(0.4f, 0.2f, 0.1f));

	// Czajniki (butelki)
	for (int i = 0; i < 4; i++) {
		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(-1.2f + i * 0.8f, 0.62f, -2.0f));
		model = glm::scale(model, glm::vec3(0.3f));
		glUniformMatrix4fv(sp->u("M"), 1, false, glm::value_ptr(model));

		glVertexAttribPointer(sp->a("vertex"), 4, GL_FLOAT, false, 0, myTeapotVertices);
		glEnableVertexAttribArray(sp->a("vertex"));
		glVertexAttribPointer(sp->a("color"), 4, GL_FLOAT, false, 0, myTeapotColors);
		glEnableVertexAttribArray(sp->a("color"));
		glVertexAttribPointer(sp->a("normal"), 3, GL_FLOAT, false, 0, myTeapotNormals);
		glEnableVertexAttribArray(sp->a("normal"));
		glDrawArrays(GL_TRIANGLES, 0, myTeapotVertexCount);
		glDisableVertexAttribArray(sp->a("vertex"));
		glDisableVertexAttribArray(sp->a("color"));
		glDisableVertexAttribArray(sp->a("normal"));
	}

	glfwSwapBuffers(window);
}

int main(void) {
	GLFWwindow* window;
	glfwSetErrorCallback(error_callback);

	if (!glfwInit()) {
		fprintf(stderr, "Nie można zainicjować GLFW.\n");
		exit(EXIT_FAILURE);
	}

	window = glfwCreateWindow(800, 600, "Galeria Alkoholi", NULL, NULL);
	if (!window) {
		fprintf(stderr, "Nie można utworzyć okna.\n");
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Nie można zainicjować GLEW.\n");
		exit(EXIT_FAILURE);
	}

	initOpenGLProgram(window);

	while (!glfwWindowShouldClose(window)) {
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		processInput(window);
		drawScene(window, speed_x, speed_y);
		glfwPollEvents();
	}

	freeOpenGLProgram(window);
	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}
