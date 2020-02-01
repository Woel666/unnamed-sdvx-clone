#include "stdafx.h"
#include "Camera.hpp"
#include "Application.hpp"
#include "Track.hpp"

const float ZOOM_POW = 1.65f;

Camera::Camera()
{
	m_spinType = SpinStruct::SpinType::None;
}
Camera::~Camera()
{

}

static float DampedSin(float t, float amplitude, float frequency, float decay)
{
	return amplitude * (float)pow(Math::e, -decay * t) * sin(frequency * 2 * t * Math::pi);
}

static float Swing(float time) { return DampedSin(time, 120.0f / 360, 1, 3.5f); }

static void Spin(float time, float &roll, float &bgAngle, float dir)
{
	const float TSPIN = 0.75f / 2.0f;
	const float TRECOV = 0.75f / 2.0f;

	bgAngle = Math::Clamp(time * 4.0f, 0.0f, 2.0f) * dir;
	if (time <= TSPIN)
		roll = -dir * (TSPIN - time) / TSPIN;
	else
	{
		if (time < TSPIN + TRECOV)
			roll = Swing((time - TSPIN) / TRECOV) * 0.25f * dir;
		else roll = 0.0f;
	}
}

static float PitchScaleFunc(float input)
{
	float kLower = -4, uLower;
	float kUpper = 5.59, uUpper;

	if (g_aspectRatio < 1.0f)
	{
		uLower = -2.8195f;
		uUpper = 4.675f;
	}
	else
	{
		uLower = -3.05f;
		uUpper = 4.75f;
	}

	int rot = 0, dir = input < 0 ? -1 : 1;
	if (dir == -1)
	{
		while (input < -12.00)
		{
			input += 24.00;
			rot++;
		}
	}
	else
	{
		while (input > 12.00)
		{
			input -= 24.00;
			rot++;
		}
	}

	double scaled = input;
	if (input < kLower)
		scaled = -(-(input - kLower) / (12 + kLower)) * (12 + uLower) + uLower;
	else if (input < 0.00)
		scaled = (input / kLower) * uLower;
	else if (input < kUpper)
		scaled = (input / kUpper) * uUpper;
	else scaled = ((input - kUpper) / (12 - kUpper)) * (12 - uUpper) + uUpper;

	return rot * dir * 24.00 + scaled;
};


static Transform GetOriginTransform(float pitch, float offs, float roll)
{
	if (g_aspectRatio < 1.0f)
	{
		auto origin = Transform::Rotation({ 0, 0, roll });
		auto anchor = Transform::Translation({ offs, -0.775f, 0 })
			* Transform::Rotation({ 1.5f, 0, 0 });
		auto contnr = Transform::Translation({ 0, 0, -0.9f })
			* Transform::Rotation({ -90 + pitch, 0, 0, });
		return origin * anchor * contnr;
	}
	else
	{
		auto origin = Transform::Rotation({ 0, 0, roll });
		auto anchor = Transform::Translation({ offs, -0.9f, 0 })
			* Transform::Rotation({ 1.5f, 0, 0 });
		auto contnr = Transform::Translation({ 0, 0, -0.9f })
			* Transform::Rotation({ -90 + pitch, 0, 0, });
		return origin * anchor * contnr;
	}

};

void Camera::Tick(float deltaTime, class BeatmapPlayback& playback)
{
	auto LerpTo = [&](float &value, float target, float speed = 0.5f)
	{
		float diff = abs(target - value);
		float change = deltaTime * speed;

		if (target < value)
			value = Math::Max(value - change, target);
		else value = Math::Min(value + change, target);
	};

	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();
	float speedLimit = Math::Max(m_rollIntensity * ROLL_SPEED, MAX_ROLL_ANGLE);

	if (pManualTiltEnabled)
	{
		m_actualTargetLaserRoll = pLaneTilt;
		speedLimit = MAX_ROLL_ANGLE * ROLL_SPEED * 2.5f; // BIGGEST roll speed
	}
	else if (!m_rollKeep)
	{
		if (m_rollIntensityChanged)
		{
			// Get new roll value based off of the new tilt value
			float target = (m_laserRoll / MAX_ROLL_ANGLE) * m_rollIntensity;

			// Get the roll speed based on the larger tilt value
			// i.e. rollSpeedFactor = 1 if NORMAL, 1.75 if BIGGER, 2.5 if BIGGEST
			float rollSpeedFactor = Math::Max(m_oldRollIntensity, m_rollIntensity) / MAX_ROLL_ANGLE;
			speedLimit = MAX_ROLL_ANGLE * ROLL_SPEED * rollSpeedFactor;
			m_actualTargetLaserRoll = target;

			// Check if roll has met target
			m_rollIntensityChanged = m_actualRoll != target;
		}
		else if (m_actualRoll != m_laserRoll && m_rollIntensity == MAX_ROLL_ANGLE)
		{
			// Catch up to regular roll position if for some reason they're not the same
			m_actualTargetLaserRoll = m_laserRoll;
		}
		else if (m_slowTilt)
		{
			// Roll even slower when roll is less than 1 / 10 of tilt
			speedLimit /= fabsf(m_actualRoll) > m_rollIntensity * SLOWEST_TILT_THRESHOLD ? 4.f : 8.f;
		}
	}

	// Lerp highway tilt
	LerpTo(m_actualRoll, m_actualTargetLaserRoll, speedLimit);

	// Lerp crit line position
	speedLimit = MAX_ROLL_ANGLE * ROLL_SPEED; // Reset roll speed to normal
	if (m_slowTilt)
		// Roll even slower when roll is less than 1 / 10 of tilt
		speedLimit /= fabsf(m_laserRoll) > MAX_ROLL_ANGLE * SLOWEST_TILT_THRESHOLD ? 4.f : 8.f;
	LerpTo(m_laserRoll, m_targetLaserRoll, speedLimit);
	
	for (int index = 0; index < 2; ++index)
	{
		m_rollIgnoreTimer[index] = Math::Max(m_rollIgnoreTimer[index] - deltaTime, 0.f);

		// Apply slam roll for 0 to 100ms (depending on user config)
		if (m_rollIgnoreTimer[index] <= m_rollIgnoreDuration)
			m_slamRoll[index] = 0;
	}

	m_spinProgress = (float)(playback.GetLastTime() - m_spinStart) / m_spinDuration;
	// Calculate camera spin
	// TODO(local): spins need a progress of 1
	if (m_spinProgress < 2.0f)
	{
		if (m_spinType == SpinStruct::SpinType::Full)
		{
			Spin(m_spinProgress / 2.0f, m_spinRoll, m_bgSpin, m_spinDirection);
		}
		else if (m_spinType == SpinStruct::SpinType::Quarter)
		{
			const float BG_SPIN_SPEED = 4.0f / 3.0f;
			m_bgSpin = Math::Clamp(m_spinProgress * BG_SPIN_SPEED / 2, 0.0f, 1.0f) * m_spinDirection;
			m_spinRoll = Swing(m_spinProgress / 2) * m_spinDirection;
		}
		else if (m_spinType == SpinStruct::SpinType::Bounce)
		{
			m_bgSpin = 0.0f;
			m_spinBounceOffset = DampedSin(m_spinProgress / 2, m_spinBounceAmplitude,
				m_spinBounceFrequency / 2, m_spinBounceDecay) * m_spinDirection;
		}

		m_spinProgress = Math::Clamp(m_spinProgress, 0.0f, 2.0f);
	}
	else
	{
		m_bgSpin = 0.0f;
		m_spinRoll = 0.0f;
		m_spinProgress = 0.0f;
	}

	m_totalRoll = m_spinRoll + m_actualRoll;
	m_totalOffset = (pLaneOffset * (5 * 100) / (6 * 116)) / 2.0f + m_spinBounceOffset;

	if (!m_rollKeep)
	{
		m_targetRollSet = false;
		m_actualTargetLaserRoll = 0.0f;
	}

	// Update camera shake effects
	m_shakeOffset = Vector3(0.0f);

	m_shakeEffect.time -= deltaTime;
	if (m_shakeEffect.time >= 0.f)
	{
		float shakeProgress = m_shakeEffect.time / m_shakeEffect.duration;
		float shakeIntensity = sinf(powf(shakeProgress, 1.6) * Math::pi);

		Vector3 shakeVec = Vector3(m_shakeEffect.amplitude * shakeIntensity) * Vector3(cameraShakeX, cameraShakeY, cameraShakeZ);

		m_shakeOffset += shakeVec;
	}

	bool portrait = g_aspectRatio < 1.0f;
	float lanePitch = (portrait ? pLanePitch : PitchScaleFunc(pLanePitch)) * pitchUnit;

	worldNormal = GetOriginTransform(lanePitch, m_totalOffset, m_totalRoll * 360.0f);
	worldNoRoll = GetOriginTransform(lanePitch, 0, 0);

	auto GetZoomedTransform = [&](Transform t)
	{
		auto zoomDir = t.GetPosition();
		float highwayDist = zoomDir.Length();
		zoomDir = zoomDir.Normalized();

		float zoomAmt;
		if (portrait)
		{
			// This doesn't scale correctly with some charts (e.g. INDEPENDENT SKY GRV)
			zoomAmt = -pLaneZoom / (pLaneZoom > 0 ? 2.f : 1.1f);
		}
		else
		{
			if (pLaneZoom <= 0) zoomAmt = pow(ZOOM_POW, -pLaneZoom) - 1;
			else zoomAmt = highwayDist * (pow(ZOOM_POW, -pow(pLaneZoom, 1.35f)) - 1);
		}
		
		return Transform::Translation(zoomDir * zoomAmt) * t;
	};

	track->trackOrigin = GetZoomedTransform(worldNormal);

	critOrigin = GetZoomedTransform(GetOriginTransform(lanePitch, m_totalOffset, m_actualRoll * 360.0f + sin(m_spinRoll * Math::pi * 2) * 20));
}
void Camera::AddCameraShake(CameraShake cameraShake)
{
	m_shakeEffect = cameraShake;
}
void Camera::AddRollImpulse(float dir, float strength)
{
	m_rollVelocity += dir * strength;
}

void Camera::SetRollIntensity(float val)
{
	if (m_rollIntensity != val)
		m_rollIntensityChanged = true;

	m_oldRollIntensity = m_rollIntensity;
	m_rollIntensity = val;
}

bool Camera::GetRollKeep()
{
	return m_rollKeep;
}

void Camera::SetRollKeep(bool rollKeep)
{
	m_rollKeep = rollKeep;
}

void Camera::SetSlowTilt(bool tilt)
{
	m_slowTilt = tilt;
}

void Camera::SetSlamAmount(uint32 index, float amount)
{
	assert(index >= 0 && index <= 1);
	m_slamRoll[index] = amount;
	SetRollIgnore(index, true);
}

void Camera::SetRollIgnore(uint32 index, bool slam)
{
	m_rollIgnoreTimer[index] = m_rollIgnoreDuration + (slam ? m_slamLength : 0);
}

float Camera::GetRollIgnoreTimer(uint32 index)
{
	assert(index >= 0 && index <= 1);
	return m_rollIgnoreTimer[index];
}

float Camera::GetSlamAmount(uint32 index)
{
	assert(index >= 0 && index <= 1);
	return m_slamRoll[index];
}

void Camera::SetRollIgnoreDuration(float duration)
{
	m_rollIgnoreDuration = duration;
}

void Camera::SetSlamLength(float length)
{
	m_slamLength = length;
}

Vector2 Camera::Project(const Vector3& pos)
{
	Vector3 cameraSpace = m_rsLast.cameraTransform.TransformPoint(pos);
	Vector3 screenSpace = m_rsLast.projectionTransform.TransformPoint(cameraSpace);
	screenSpace.y = -screenSpace.y;
	screenSpace *= 0.5f;
	screenSpace += Vector2(0.5f, 0.5f);
	screenSpace *= m_rsLast.viewportSize;
	return screenSpace.xy();
}

static float Lerp(float a, float b, float alpha)
{
	return a + (b - a) * alpha;
}

RenderState Camera::CreateRenderState(bool clipped)
{
	int portrait = g_aspectRatio > 1 ? 0 : 1;

	// Extension of clipping planes in outward direction
	float viewRangeExtension = clipped ? 0.0f : 5.0f;

	RenderState rs = g_application->GetRenderStateBase();

	auto critDir = worldNoRoll.GetPosition().Normalized();
	float rotToCrit = -atan2(critDir.y, -critDir.z) * Math::radToDeg;

	float fov = fovs[portrait];
	float cameraRot = fov / 2 - fov * pitchOffsets[portrait];

	m_actualCameraPitch = rotToCrit - cameraRot + basePitch[portrait];
	auto cameraTransform = Transform::Rotation(Vector3(m_actualCameraPitch, 0, 0) + m_shakeOffset);

	// Calculate clipping distances
	Vector3 toTrackEnd = (track->trackOrigin).TransformPoint(Vector3(0.0f, track->trackLength, 0));
	Vector3 toTrackBegin = (track->trackOrigin).TransformPoint(Vector3(0.0f, -1.f, 0.f));
	
	float radPitch = Math::degToRad * m_actualCameraPitch;
	float endDist = -VectorMath::Dot(toTrackEnd, { 0, sinf(radPitch) ,cosf(radPitch) });
	float beginDist = -VectorMath::Dot(toTrackBegin, { 0, sinf(radPitch) ,cosf(radPitch) });
	float clipFar = Math::Max(endDist, beginDist);
	float clipNear = Math::Min(endDist, beginDist);

	rs.cameraTransform = cameraTransform;
	rs.projectionTransform = ProjectionMatrix::CreatePerspective(fov, g_aspectRatio, Math::Max(clipNear, 0.1f), clipFar + viewRangeExtension);

	m_rsLast = rs;
	return rs;
}

void Camera::SetTargetRoll(float target)
{
	auto ShouldRollDuringKeep = [](float target, float roll)
	{
		if (roll == 0.0f || Math::Sign(roll) == Math::Sign(target))
		{
			return roll == 0 || (target < roll && roll < 0) || (target > roll && roll > 0);
		}
		return false;
	};

	// Work around for slams being applied for at least 1 frame
	float slamRollTotal = m_slamLength == 0.f ? 0 : m_slamRoll[0] + m_slamRoll[1];
	m_targetLaserRoll = Math::Clamp(target + slamRollTotal, -1.f, 1.f) * MAX_ROLL_ANGLE;

	if (!m_rollKeep)
	{
		m_actualTargetLaserRoll = Math::Clamp(target + slamRollTotal, -1.f, 1.f) * m_rollIntensity;
		m_targetRollSet = true;
	}
	else
	{
		float actualTarget = Math::Clamp(target, -1.f, 1.f) * m_rollIntensity;
		if (ShouldRollDuringKeep(actualTarget, m_actualTargetLaserRoll))
		{
			m_actualTargetLaserRoll = actualTarget;
			m_targetRollSet = true;
		}
	}
}

void Camera::SetSpin(float direction, uint32 duration, uint8 type, class BeatmapPlayback& playback)
{
	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();

	m_spinDirection = direction;
	m_spinDuration = (duration / 192.0f) * (currentTimingPoint.beatDuration) * 4;
	m_spinStart = playback.GetLastTime();
	m_spinType = type;
}

void Camera::SetXOffsetBounce(float direction, uint32 duration, uint32 amplitude, uint32 frequency, float decay, class BeatmapPlayback &playback)
{
	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();

	m_spinDirection = direction;
	// since * 2 and stuff
	m_spinDuration = 0.5f * (duration / 192.0f) * (currentTimingPoint.beatDuration) * 4;
	m_spinStart = playback.GetLastTime();
	m_spinType = SpinStruct::SpinType::Bounce;

	m_spinBounceAmplitude = amplitude / 250.0f;
	m_spinBounceFrequency = frequency;
	m_spinBounceDecay = decay == 0 ? 0 : (decay == 1 ? 1.5f : 3.0f);
}

void Camera::SetLasersActive(bool lasersActive)
{
	m_lasersActive = lasersActive;
}

float Camera::GetRoll() const
{
	return m_totalRoll;
}

float Camera::GetLaserRoll() const
{
	return m_laserRoll;
}

float Camera::GetActualRoll() const
{
	return m_actualRoll;
}

float Camera::GetHorizonHeight()
{
	float angle = fmodf(m_actualCameraPitch + PitchScaleFunc(pLanePitch) * pitchUnit, 360.0f);
	return (0.5 + (-angle / fovs[g_aspectRatio > 1.0f ? 0 : 1])) * m_rsLast.viewportSize.y;
}

Vector2i Camera::GetScreenCenter()
{
	Vector2i ret = Vector2i(0, GetHorizonHeight());

	uint8 portrait = g_aspectRatio > 1.0f ? 0 : 1;
	float fov = fovs[portrait];

	ret.x = m_rsLast.viewportSize.x / 2;
	ret.x -= (m_shakeOffset.y / (fov * g_aspectRatio)) * m_rsLast.viewportSize.x;

	return ret;
}

Vector3 Camera::GetShakeOffset()
{
	return m_shakeOffset;
}

float Camera::m_ClampRoll(float in) const
{
	float ain = fabs(in);
	if(ain < 1.0f)
		return in;
	bool odd = ((uint32)fabs(in) % 2) == 1;
	float sign = Math::Sign(in);
	if(odd)
	{
		// Swap sign and modulo
		return -sign * (1.0f-fmodf(ain, 1.0f));
	}
	else
	{
		// Keep sign and modulo
		return sign * fmodf(ain, 1.0f);
	}
}

CameraShake::CameraShake(float duration) : duration(duration)
{
	time = duration;
}
CameraShake::CameraShake(float duration, float amplitude) : duration(duration), amplitude(amplitude)
{
	time = duration;
}

