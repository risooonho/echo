#pragma once

#include "engine/core/math/Quaternion.h"
#include "engine/core/geom/Frustum.h"
#include "engine/core/util/Array.hpp"

namespace Echo
{
	class Ray;
	class Camera
	{
	public:
		enum ProjMode
		{
			PM_PERSPECTIVE,
			PM_ORTHO,
			PM_UI,
		};

	public:
		Camera(ProjMode mode = PM_PERSPECTIVE);
		virtual ~Camera();

		// proj mode
		ProjMode getProjectionMode() const;
		void setProjectionMode(ProjMode mode);

		// position
		const Vector3& getPosition() const { return m_position; }
		void setPosition(const Vector3& pos);

		// direction
		const Vector3& getDirection() const { return m_dir; }
		void setDirection(const Vector3& dir);

		// up 
		const Vector3& getUp() const { return m_up; }
		void setUp(const Vector3& vUp);

		// right
		const Vector3& getRight() const { return m_right; }

		// fov
		Real getFov() const;
		void setFov(Real fov);

		// width
		ui32 getWidth() const;
		void setWidth(ui32 width);

		// height
		ui32 getHeight() const;
		void setHeight(ui32 height);

		// scale
		void setScale(Real scale);
		float getScale() const { return m_scale; }

		// near clip
		const Real&	getNear() const;
		void setNearClip(Real nearClip);

		// far clip
		const Real&	getFar() const;
		void setFarClip(Real farClip);

		// update
		void update();

		// calculate
		void getCameraRay(Ray& ray, const Vector2& screenPos);
		void unProjectionMousePos(Vector3& from, Vector3& to, const Vector2& screenPos);

		// matrix
		const Matrix4& getViewMatrix() const { return m_matView; }
		const Matrix4& getProjMatrix() const { return m_matProj; }
		const Matrix4& getViewProjMatrix() const { return m_matVP; }

	protected:
		ProjMode		m_projMode;
		Vector3			m_position;
		Vector3			m_dir;
		Vector3			m_up;
		Vector3			m_right;
		Matrix4			m_matView;
		bool			m_isViewDirty;
		Real			m_fov;
		ui32			m_width;
		ui32			m_height;
		Real			m_scale;
		Real			m_aspect;
		Real			m_nearClip;
		Real			m_farClip;
		Matrix4			m_matProj;
		bool			m_isProjDirty;
		Matrix4			m_matVP;
	};
}
