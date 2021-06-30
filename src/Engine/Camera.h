#pragma once

enum CameraType
{
	LookAt,
	FirstPerson
};

class Camera
{
public:
	void Update(float deltaTime)
	{
		updated = false;
		if (type == CameraType::FirstPerson)
		{
			if (IsMoving())
			{
				glm::vec3 front;
				front.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
				front.y = sin(glm::radians(rotation.x));
				front.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
				front = glm::normalize(front);

				float move_speed = deltaTime * translation_speed;

				if (keys.up)
				{
					position += front * move_speed;
				}
				if (keys.down)
				{
					position -= front * move_speed;
				}
				if (keys.left)
				{
					position -= glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * move_speed;
				}
				if (keys.right)
				{
					position += glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * move_speed;
				}

				UpdateViewMatrix();
			}
		}
	}

	// Update camera passing separate axis data (gamepad)
	// Returns true if view or position has been changed
	bool UpdateGamepad(glm::vec2 axis_left, glm::vec2 axis_right, float delta_time)
	{
		bool changed = false;

		if (type == CameraType::FirstPerson)
		{
			// Use the common console thumbstick layout
			// Left = view, right = move

			const float dead_zone = 0.0015f;
			const float range = 1.0f - dead_zone;

			glm::vec3 front;
			front.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
			front.y = sin(glm::radians(rotation.x));
			front.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
			front = glm::normalize(front);

			float move_speed = delta_time * translation_speed * 2.0f;
			float new_rotation_speed = delta_time * rotation_speed * 50.0f;

			// Move
			if (fabsf(axis_left.y) > dead_zone)
			{
				float pos = (fabsf(axis_left.y) - dead_zone) / range;
				position -= front * pos * ((axis_left.y < 0.0f) ? -1.0f : 1.0f) * move_speed;
				changed = true;
			}
			if (fabsf(axis_left.x) > dead_zone)
			{
				float pos = (fabsf(axis_left.x) - dead_zone) / range;
				position += glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * pos * ((axis_left.x < 0.0f) ? -1.0f : 1.0f) * move_speed;
				changed = true;
			}

			// Rotate
			if (fabsf(axis_right.x) > dead_zone)
			{
				float pos = (fabsf(axis_right.x) - dead_zone) / range;
				rotation.y += pos * ((axis_right.x < 0.0f) ? -1.0f : 1.0f) * new_rotation_speed;
				changed = true;
			}
			if (fabsf(axis_right.y) > dead_zone)
			{
				float pos = (fabsf(axis_right.y) - dead_zone) / range;
				rotation.x -= pos * ((axis_right.y < 0.0f) ? -1.0f : 1.0f) * new_rotation_speed;
				changed = true;
			}
		}
		else
		{
			// todo: move code from example base class for look-at
		}

		if (changed)
		{
			UpdateViewMatrix();
		}

		return changed;
	}

	CameraType type = CameraType::LookAt;

	glm::vec3 rotation = glm::vec3();
	glm::vec3 position = glm::vec3();

	float rotation_speed = 1.0f;
	float translation_speed = 1.0f;

	bool updated = false;

	struct
	{
		glm::mat4 perspective;
		glm::mat4 view;
	} matrices;

	struct
	{
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
	} keys;

	bool IsMoving()
	{
		return keys.left || keys.right || keys.up || keys.down;
	}

	float GetNearClip()
	{
		return znear;
	}

	float GetFarClip()
	{
		return zfar;
	}

	void SetPerspective(float fov, float aspect, float znear, float zfar)
	{
		this->fov = fov;
		this->znear = znear;
		this->zfar = zfar;
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
	}

	void UpdateAspectRatio(float aspect)
	{
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
	}

	void SetPosition(const glm::vec3& position)
	{
		this->position = position;
		UpdateViewMatrix();
	}

	void SetRotation(const glm::vec3& rotation)
	{
		this->rotation = rotation;
		UpdateViewMatrix();
	}

	void Rotate(const glm::vec3& delta)
	{
		this->rotation += delta;
		UpdateViewMatrix();
	}

	void SetTranslation(const glm::vec3& translation)
	{
		this->position = translation;
		UpdateViewMatrix();
	}

	void Translate(const glm::vec3& delta)
	{
		this->position += delta;
		UpdateViewMatrix();
	}

private:
	float fov;
	float znear, zfar;

	void UpdateViewMatrix()
	{
		glm::mat4 rotation_matrix = glm::mat4(1.0f);
		glm::mat4 transformation_matrix;

		rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		transformation_matrix = glm::translate(glm::mat4(1.0f), position);

		if (type == CameraType::FirstPerson)
		{
			matrices.view = rotation_matrix * transformation_matrix;
		}
		else
		{
			matrices.view = transformation_matrix * rotation_matrix;
		}

		updated = true;
	}
};