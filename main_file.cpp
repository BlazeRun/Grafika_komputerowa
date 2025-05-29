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
#include "model.h"

float speed_x = 0;
float speed_y = 0;
float aspectRatio = 1;

glm::vec3 cameraPos = glm::vec3(1.0f, 0.50f, -0.4f);  // [x, y, z]
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec2 cameraShakeOffset = glm::vec2(0.0f);


// Struktura do kolizji
struct AABB {
	glm::vec3 min;
	glm::vec3 max;

	bool contains(const glm::vec3& point) const {
		return point.x >= std::min(min.x, max.x) && point.x <= std::max(min.x, max.x) &&
			point.y >= std::min(min.y, max.y) && point.y <= std::max(min.y, max.y) &&
			point.z >= std::min(min.z, max.z) && point.z <= std::max(min.z, max.z);
	}
};

// Globalny wektor hitboxów sceny
std::vector<AABB> sceneColliders;

struct Drinkable {
	Model* model;
	glm::vec3 position;
	glm::vec3 scale;
};

//globany wektor butelek do picia
std::vector<Drinkable> drinkables;
int heldDrinkableIndex = -1; // -1 oznacza brak podniesionej butelki
bool isDrinking = false;
float drinkingTimer = 0.0f;
const float DRINK_DURATION = 3.0f; // sekundy
int intoxicationLevel = 0; // 0–4
float intoxicationTimer = 0.0f;
const float INTOXICATION_DURATION = 10.0f;


float totalTime = 0.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

float yaw = -90.0f;
float pitch = 0.0f;
float lastX = 250.0f;
float lastY = 250.0f;
bool firstMouse = true;

ShaderProgram* spModel;
ShaderProgram* spScene;

Model* bottlesModel, * modelDesk, * modelDoor, * modelFloor, * modelShelfs, * modelWalls, * modelCeiling, * modelLamp, * modelDrinkable1, * modelDrinkable2, * modelDrinkable3, * modelDrinkable4;

void error_callback(int error, const char* description) {
	fputs(description, stderr);
}

glm::vec3 getRayDirection() {
	return glm::normalize(cameraFront);
}

int getDrinkableInFocus(float maxDistance = 0.5f, float radius = 0.15f) {
	glm::vec3 rayOrigin = cameraPos;
	glm::vec3 rayDir = getRayDirection();

	for (size_t i = 0; i < drinkables.size(); ++i) {
		glm::vec3 toCenter = drinkables[i].position - rayOrigin;
		float projection = glm::dot(toCenter, rayDir);

		if (projection < 0 || projection > maxDistance)
			continue;

		glm::vec3 closestPoint = rayOrigin + rayDir * projection;
		float distSq = glm::dot(drinkables[i].position - closestPoint, drinkables[i].position - closestPoint);

		if (distSq < radius * radius)
			return static_cast<int>(i);
	}
	return -1;
}

void updateCameraDirection() {
	glm::vec3 direction;
	direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	direction.y = sin(glm::radians(pitch));
	direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraFront = glm::normalize(direction);
}

void processInput(GLFWwindow* window) {
	float cameraSpeed = 1.0f * deltaTime;
	glm::vec3 frontNoY = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
	glm::vec3 right = glm::normalize(glm::cross(frontNoY, cameraUp));
	glm::vec3 proposedPos = cameraPos;

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		proposedPos += cameraSpeed * frontNoY;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		proposedPos -= cameraSpeed * frontNoY;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		proposedPos -= right * cameraSpeed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		proposedPos += right * cameraSpeed;

	glm::vec3 collisionCheckPos = proposedPos - glm::vec3(0.0f, 0.4f, 0.0f); // obniżenie Y o 0.4f

	bool isColliding = false;
	for (const auto& box : sceneColliders) {
		if (box.contains(collisionCheckPos)) {
			isColliding = true;
			break;
		}
	}

	if (!isColliding) {
		cameraPos = proposedPos;
	}

	static bool ePressedLastFrame = false;
	bool ePressed = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;

	if (ePressed && !ePressedLastFrame && !isDrinking) {
		if (heldDrinkableIndex == -1) {
			int index = getDrinkableInFocus();
			if (index != -1) {
				heldDrinkableIndex = index;
			}
		}
		else {
			heldDrinkableIndex = -1; // upuszczenie
		}
	}

	ePressedLastFrame = ePressed;

	static bool fPressedLastFrame = false;
	bool fPressed = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;

	if (fPressed && !fPressedLastFrame) {
		if (heldDrinkableIndex != -1 && !isDrinking) {
			isDrinking = true;
			drinkingTimer = DRINK_DURATION;
		}
	}
	fPressedLastFrame = fPressed;


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

	float sensitivity = 0.05f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;
	updateCameraDirection();


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

	spModel = new ShaderProgram("v_textures.glsl", NULL, "f_textures.glsl");

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window, mouse_callback);

	bottlesModel = new Model("models/bottles_for_shelf/bottles_for_shelf.obj");
	modelDesk = new Model("models/Desk/Desk.obj");
	modelDoor = new Model("models/Door/Door.obj");
	modelFloor = new Model("models/Floor/Floor.obj");
	modelShelfs = new Model("models/Shelfs/Shelfs.obj");
	modelWalls = new Model("models/Walls/Walls.obj");
	modelCeiling = new Model("models/Ceiling/Ceiling.obj");
	modelLamp = new Model("models/Lamp/Lamp.obj");

	drinkables.push_back({ new Model("models/Drinkable1/drinkable1.obj"), glm::vec3(0.4f, 0.35f, -1.3f), glm::vec3(1.0f) });
	drinkables.push_back({ new Model("models/Drinkable2/drinkable2.obj"), glm::vec3(0.8f, 0.35f, -1.3f), glm::vec3(0.80f) });
	drinkables.push_back({ new Model("models/Drinkable3/drinkable3.obj"), glm::vec3(1.2f, 0.35f, -1.3f), glm::vec3(0.035f) });
	drinkables.push_back({ new Model("models/Drinkable4/drinkable4.obj"), glm::vec3(1.6f, 0.35f, -1.3f), glm::vec3(0.85f) });




	sceneColliders.push_back({ glm::vec3(0.03f, 0.01f, -1.79f), glm::vec3(2.43f, 0.31f, -1.13f) }); // Desk
	sceneColliders.push_back({ glm::vec3(0.07f, 0.01f, -1.25f), glm::vec3(-0.03f, 0.74f, -0.05f) }); // Left wall
	sceneColliders.push_back({ glm::vec3(2.54f, 0.04f, -1.72f), glm::vec3(2.38f, 0.87f, 0.02f) }); // Right wall
	sceneColliders.push_back({ glm::vec3(2.56f, -0.06f, -0.15f), glm::vec3(-0.07f, 1.03f, 0.14f) }); // Back wall with door
}

void freeOpenGLProgram(GLFWwindow* window) {
	delete spScene;
	delete spModel;

	delete bottlesModel;
	delete modelDesk;
	delete modelDoor;
	delete modelFloor;
	delete modelShelfs;
	delete modelWalls;
	delete modelLamp;

	for (auto& d : drinkables)
		delete d.model;

}

void drawScene(GLFWwindow* window, float angle_x, float angle_y) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 V = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
	glm::mat4 P = glm::perspective(50.0f * PI / 180.0f, aspectRatio, 0.01f, 50.0f);

	glm::vec3 lightPos1 = glm::vec3(2.3f, 0.75f, -1.52f);
	glm::vec3 lightColor1 = glm::vec3(1.0f, 1.0f, 0.9f);
	glm::vec3 lightPos2 = glm::vec3(0.21f, 0.75f, -1.52f);
	glm::vec3 lightColor2 = glm::vec3(1.0f, 1.0f, 0.9f);

	// Aktywacja shaderów modeli
	spModel->use();
	glUniformMatrix4fv(spModel->u("P"), 1, GL_FALSE, glm::value_ptr(P));
	glUniformMatrix4fv(spModel->u("V"), 1, GL_FALSE, glm::value_ptr(V));
	glUniform3fv(spModel->u("cameraPos"), 1, glm::value_ptr(cameraPos));
	glUniform3fv(spModel->u("lightPos1"), 1, glm::value_ptr(lightPos1));
	glUniform3fv(spModel->u("lightColor1"), 1, glm::value_ptr(lightColor1));
	glUniform3fv(spModel->u("lightPos2"), 1, glm::value_ptr(lightPos2));
	glUniform3fv(spModel->u("lightColor2"), 1, glm::value_ptr(lightColor2));

	auto drawAt = [&](Model* model, glm::vec3 pos, glm::vec3 scale) {
		glm::mat4 M = glm::translate(glm::mat4(1.0f), pos);
		M = glm::scale(M, scale);
		glUniformMatrix4fv(spModel->u("M"), 1, GL_FALSE, glm::value_ptr(M));
		glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(M)));
		glUniformMatrix3fv(spModel->u("normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
		model->Draw(spModel);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		};

	// Rysowanie obiektów sceny
	drawAt(modelDesk, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.64f, 1.0f));
	drawAt(modelDoor, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
	drawAt(modelFloor, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
	drawAt(modelShelfs, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
	drawAt(modelWalls, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
	drawAt(modelCeiling, glm::vec3(0.0f, 1.1f, 0.0f), glm::vec3(1.0f));
	auto drawLamp = [&](glm::vec3 pos, float rotationY_deg) {
		glm::mat4 M = glm::translate(glm::mat4(1.0f), pos);
		M = glm::rotate(M, glm::radians(rotationY_deg), glm::vec3(0.0f, 1.0f, 0.0f)); // obrót Y
		M = glm::scale(M, glm::vec3(1.25f)); // powiększenie o 50%

		glUniformMatrix4fv(spModel->u("M"), 1, GL_FALSE, glm::value_ptr(M));
		glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(M)));
		glUniformMatrix3fv(spModel->u("normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
		modelLamp->Draw(spModel);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		};


	// Wywołanie
	drawLamp(glm::vec3(2.47f, 0.6f, -1.5f), -90.0f);   // pierwsza: obrót w prawo
	drawLamp(glm::vec3(0.04f, 0.6f, -1.5f), 90.0f);  // druga: obrót w lewo


	//pętla rysyjąca butelki do picia
	for (size_t i = 0; i < drinkables.size(); ++i) {
		const auto& d = drinkables[i];

		// Lewitacja: sinusoida na Y
		float hoverOffset = sin((totalTime + i * 5.0f) * 2.0f) * 0.02f;

		// Obrót wokół Y
		float angle = totalTime * 60.0f; // stopni na sekundę

		glm::mat4 M;
		if ((int)i == heldDrinkableIndex) {
			glm::vec3 heldPos;
			if (isDrinking) {
				// Wyżej i bliżej twarzy
				heldPos = cameraPos + cameraFront * 0.25f + cameraUp * 0.05f;
			}
			else {
				heldPos = cameraPos + cameraFront * 0.3f + cameraUp * -0.15f;
			}

			M = glm::translate(glm::mat4(1.0f), heldPos);

			// Obrót względem kamery + 180 stopni
			float angle = atan2(cameraFront.x, cameraFront.z);
			M = glm::rotate(M, angle + glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

			// Jeśli pijemy – pochyl butelkę do góry (ruch w osi X)
			if (isDrinking) {
				M = glm::rotate(M, glm::radians(120.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			}

			M = glm::scale(M, d.scale);

		}
		else {
			M = glm::translate(glm::mat4(1.0f), d.position + glm::vec3(0.0f, hoverOffset, 0.0f));
			M = glm::rotate(M, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
			M = glm::scale(M, d.scale);
		}



		glUniformMatrix4fv(spModel->u("M"), 1, GL_FALSE, glm::value_ptr(M));
		glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(M)));
		glUniformMatrix3fv(spModel->u("normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
		d.model->Draw(spModel);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}


	// Butelki – 3 poziomy na regałach
	int rows = 3;
	int cols = 5;
	float baseX = 0.2f;
	float baseY = 0.24f;
	float baseZ = -2.25f;

	for (int row = 0; row < rows; row++) {
		for (int col = 0; col < cols; col++) {
			glm::vec3 pos = glm::vec3(
				baseX + col * 0.4f,
				baseY + row * 0.235f,
				baseZ
			);
			glm::mat4 M = glm::translate(glm::mat4(1.0f), pos);
			M = glm::scale(M, glm::vec3(0.5f));
			glUniformMatrix4fv(spModel->u("M"), 1, GL_FALSE, glm::value_ptr(M));
			glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(M)));
			glUniformMatrix3fv(spModel->u("normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
			bottlesModel->Draw(spModel);
			glBindVertexArray(0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}

	int focused = getDrinkableInFocus();
	float crosshairScale = (focused != -1) ? 1.5f : 1.0f;
	glColor3f((focused != -1) ? 1.0f : 0.0f, (focused != -1) ? 1.0f : 1.0f, 0.0f); // żółty lub zielony


	// Rysowanie celownika
	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, 800, 0, 600, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glBegin(GL_LINES);
	glColor3f(0.0f, 1.0f, 0.0f); // zielony limonkowy
	glVertex2f(400.0f - 5 * crosshairScale, 300.0f); glVertex2f(400.0f + 5 * crosshairScale, 300.0f);
	glVertex2f(400.0f, 300.0f - 5 * crosshairScale); glVertex2f(400.0f, 300.0f + 5 * crosshairScale);
	glEnd();
	glEnable(GL_DEPTH_TEST);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);


	// ==== DEBUG: Czerwony kontur źródła światła ====
	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(P));

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(V));

	glColor3f(1.0f, 0.0f, 0.0f); // czerwony kolor

	auto drawLightBox = [](glm::vec3 pos) {
		float s = 0.1f;
		glBegin(GL_LINE_LOOP);
		glVertex3f(pos.x - s, pos.y - s, pos.z);
		glVertex3f(pos.x + s, pos.y - s, pos.z);
		glVertex3f(pos.x + s, pos.y + s, pos.z);
		glVertex3f(pos.x - s, pos.y + s, pos.z);
		glEnd();

		glBegin(GL_LINE_LOOP);
		glVertex3f(pos.x - s, pos.y - s, pos.z + s);
		glVertex3f(pos.x + s, pos.y - s, pos.z + s);
		glVertex3f(pos.x + s, pos.y + s, pos.z + s);
		glVertex3f(pos.x - s, pos.y + s, pos.z + s);
		glEnd();

		glBegin(GL_LINES);
		glVertex3f(pos.x - s, pos.y - s, pos.z); glVertex3f(pos.x - s, pos.y - s, pos.z + s);
		glVertex3f(pos.x + s, pos.y - s, pos.z); glVertex3f(pos.x + s, pos.y - s, pos.z + s);
		glVertex3f(pos.x + s, pos.y + s, pos.z); glVertex3f(pos.x + s, pos.y + s, pos.z + s);
		glVertex3f(pos.x - s, pos.y + s, pos.z); glVertex3f(pos.x - s, pos.y + s, pos.z + s);
		glEnd();
		};

	glDisable(GL_DEPTH_TEST); // opcjonalnie: by nie schowało się za obiektami
	drawLightBox(lightPos1);
	drawLightBox(lightPos2);
	glEnable(GL_DEPTH_TEST);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);


	if (intoxicationLevel >= 4) {
		glUseProgram(0);
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0, 800, 0, 600, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glColor4f(0.0f, 0.0f, 0.0f, 0.7f); // lekka ciemność

		glBegin(GL_QUADS);
		glVertex2f(0, 0); glVertex2f(800, 0);
		glVertex2f(800, 600); glVertex2f(0, 600);
		glEnd();

		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);

		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
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
		totalTime += deltaTime;
		if (intoxicationTimer > 0.0f) {
			intoxicationTimer -= deltaTime;
			if (intoxicationTimer <= 0.0f) {
				intoxicationLevel = 0; // Reset po 10 sekundach
			}
		}

		if (intoxicationLevel >= 1) {
			float freq = 1.8f + intoxicationLevel * 0.3f; // częstotliwość wzrasta z poziomem
			float amp = 0.15f + 0.05f * intoxicationLevel; // amplituda również

			cameraShakeOffset.x = sin(totalTime * freq * 2.0f * PI) * amp;
			cameraShakeOffset.y = cos(totalTime * freq * 2.0f * PI) * amp;

			// Drgania kamery bez ruchu myszką
			yaw += cameraShakeOffset.x;
			pitch += cameraShakeOffset.y;

			if (pitch > 89.0f) pitch = 89.0f;
			if (pitch < -89.0f) pitch = -89.0f;

			updateCameraDirection();
		}



		if (isDrinking) {
			drinkingTimer -= deltaTime;
			if (drinkingTimer <= 0.0f) {
				isDrinking = false;
				heldDrinkableIndex = -1; // Odkładamy butelkę po wypiciu

				// Resetuj licznik czasu i zwiększ poziom
				intoxicationLevel = std::min(4, intoxicationLevel + 1);
				intoxicationTimer = INTOXICATION_DURATION;
			}
		}
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