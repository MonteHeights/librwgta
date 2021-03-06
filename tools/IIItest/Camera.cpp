#include "III.h"

#define PI 3.14159265359f

CCamera TheCamera;

using rw::Quat;
using rw::V3d;

void
CCamera::Process(void)
{
	// Keyboard
	float sensitivity = 1.0;
	float scale = CTimer::ms_fTimeStep;

	if(CPad::IsKeyDown(KEY_LSHIFT) || CPad::IsKeyDown(KEY_RSHIFT))
		sensitivity *= 2.0f;
	if(CPad::IsKeyDown('W')) TheCamera.orbit(0.0f, 0.05f*scale);
	if(CPad::IsKeyDown('S')) TheCamera.orbit(0.0f, -0.05f*scale);
	if(CPad::IsKeyDown('A')) TheCamera.orbit(-0.05f*scale, 0.0f*scale);
	if(CPad::IsKeyDown('D')) TheCamera.orbit(0.05f*scale, 0.0f*scale);
	if(CPad::IsKeyDown(KEY_UP)) TheCamera.turn(0.0f, 0.05f*scale);
	if(CPad::IsKeyDown(KEY_DOWN)) TheCamera.turn(0.0f, -0.05f*scale);
	if(CPad::IsKeyDown(KEY_LEFT)) TheCamera.turn(0.05f*scale, 0.0f);
	if(CPad::IsKeyDown(KEY_RIGHT)) TheCamera.turn(-0.05f*scale, 0.0f);
	if(CPad::IsKeyDown(KEY_LALT) || CPad::IsKeyDown(KEY_RALT)){
		if(CPad::IsKeyDown('R')) TheCamera.dolly(5.0f*sensitivity*scale);
		if(CPad::IsKeyDown('F')) TheCamera.dolly(-5.0f*sensitivity*scale);
	}else{
		if(CPad::IsKeyDown('R')) TheCamera.zoom(5.0f*sensitivity*scale);
		if(CPad::IsKeyDown('F')) TheCamera.zoom(-5.0f*sensitivity*scale);
	}

	// Pad
	CPad *pad = CPad::GetPad(0);
	sensitivity = 1.0f;
	if(pad->NewState.r2){
		sensitivity = 2.0f;
		if(pad->NewState.l2)
			sensitivity = 4.0f;
	}else if(pad->NewState.l2)
		sensitivity = 0.5f;
	if(pad->NewState.square) TheCamera.zoom(0.4f*sensitivity*scale);
	if(pad->NewState.cross) TheCamera.zoom(-0.4f*sensitivity*scale);
	TheCamera.orbit(pad->NewState.getLeftX()/30.0f*sensitivity*scale,
	                -pad->NewState.getLeftY()/30.0f*sensitivity*scale);
	TheCamera.turn(-pad->NewState.getRightX()/30.0f*sensitivity*scale,
	               pad->NewState.getRightY()/30.0f*sensitivity*scale);
	if(pad->NewState.up)
		TheCamera.dolly(2.0f*sensitivity*scale);
	if(pad->NewState.down)
		TheCamera.dolly(-2.0f*sensitivity*scale);

	if(IsButtonJustDown(pad, start)){
		printf("cam.position: %f, %f, %f\n", m_position.x, m_position.y, m_position.z);
		printf("cam.target: %f, %f, %f\n", m_target.x, m_target.y, m_target.z);
	}
}

void
CCamera::update(void)
{
	if(m_rwcam){
		m_rwcam->setFOV(m_fov, m_aspectRatio);

		rw::Frame *f = m_rwcam->getFrame();
		if(f){
			f->matrix.lookAt(sub(m_target, m_position),
			                 m_up);
			f->matrix.pos = m_position;
			f->updateObjects();
		}
	}
}

void
CCamera::setTarget(V3d target)
{
	m_position = sub(m_position, sub(m_target, target));
	m_target = target;
}

float
CCamera::getHeading(void)
{
	V3d dir = sub(m_target, m_position);
	float a = atan2(dir.y, dir.x)-PI/2.0f;
	return m_localup.z < 0.0f ? a-PI : a;
}

void
CCamera::turn(float yaw, float pitch)
{
	V3d dir = sub(m_target, m_position);
	V3d zaxis = { 0.0f, 0.0f, 1.0f };
	Quat r = Quat::rotation(yaw, zaxis);
	dir = rotate(dir, r);
	m_localup = rotate(m_localup, r);

	V3d right = normalize(cross(dir, m_localup));
	r = Quat::rotation(pitch, right);
	dir = rotate(dir, r);
	m_localup = normalize(cross(right, dir));
	if(m_localup.z >= 0.0) m_up.z = 1.0;
	else m_up.z = -1.0f;

	m_target = add(m_position, dir);
}

void
CCamera::orbit(float yaw, float pitch)
{
	V3d dir = sub(m_target, m_position);
	V3d zaxis = { 0.0f, 0.0f, 1.0f };
	Quat r = Quat::rotation(yaw, zaxis);
	dir = rotate(dir, r);
	m_localup = rotate(m_localup, r);

	V3d right = normalize(cross(dir, m_localup));
	r = Quat::rotation(-pitch, right);
	dir = rotate(dir, r);
	m_localup = normalize(cross(right, dir));
	if(m_localup.z >= 0.0) m_up.z = 1.0;
	else m_up.z = -1.0f;

	m_position = sub(m_target, dir);
}

void
CCamera::dolly(float dist)
{
	V3d dir = setlength(sub(m_target, m_position), dist);
	m_position = add(m_position, dir);
	m_target = add(m_target, dir);
}

void
CCamera::zoom(float dist)
{
	V3d dir = sub(m_target, m_position);
	float curdist = length(dir);
	if(dist >= curdist)
		dist = curdist-0.01f;
	dir = setlength(dir, dist);
	m_position = add(m_position, dir);
}

void
CCamera::pan(float x, float y)
{
	V3d dir = normalize(sub(m_target, m_position));
	V3d right = normalize(cross(dir, m_up));
	V3d localup = normalize(cross(right, dir));
	dir = add(scale(right, x), scale(localup, y));
	m_position = add(m_position, dir);
	m_target = add(m_target, dir);
}

float
CCamera::distanceTo(V3d v)
{
	return length(sub(m_position, v));
}

bool
CCamera::isSphereVisible(CVector &center, float radius)
{
	rw::Sphere sph;
	sph.center = *(rw::V3d*)&center;
	sph.radius = radius;
	return m_rwcam->frustumTestSphere(&sph) != rw::Camera::SPHEREOUTSIDE;
}

CCamera::CCamera()
{
	m_position.set(0.0f, 6.0f, 0.0f);
	m_target.set(0.0f, 0.0f, 0.0f);

	m_up.set(0.0f, 0.0f, 1.0f);
	m_localup = m_up;
	m_fov = 70.0f;
	m_aspectRatio = 1.0f;
	m_rwcam = nil;
	m_LODmult = 1.0f;
}

