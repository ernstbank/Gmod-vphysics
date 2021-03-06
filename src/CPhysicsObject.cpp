#include "StdAfx.h"

#include "math.h"
#include "CPhysicsObject.h"
#include "CPhysicsEnvironment.h"
#include "CPhysicsCollision.h"
#include "CPhysicsFrictionSnapshot.h"
#include "CShadowController.h"
#include "convert.h"
#include "CPhysicsDragController.h"
#include "CPhysicsSurfaceProps.h"

// memdbgon must be the last include file in a .cpp file!!!
////#include "tier0/memdbgon.h"

extern CPhysicsSurfaceProps g_SurfaceDatabase;

/***************************
* CLASS CPhysicsObject
***************************/

CPhysicsObject::CPhysicsObject() {
	m_contents = 0;
	m_iGameIndex = 0;
	m_pShadow = NULL;
	m_pFluidController = NULL;
	m_pEnv = NULL;
	m_pGameData = NULL;
	m_pObject = NULL;
	m_pName = NULL;
}

CPhysicsObject::~CPhysicsObject() {
	if (m_pEnv) {
		RemoveShadowController();
		m_pEnv->GetDragController()->RemovePhysicsObject(this);
	}
	
	if (m_pEnv && m_pObject) {
		m_pEnv->GetBulletEnvironment()->removeRigidBody(m_pObject);
		delete m_pObject;
	}
}

bool CPhysicsObject::IsStatic() const {
	return (m_pObject->getCollisionFlags() & btCollisionObject::CF_STATIC_OBJECT);
}

// Possible that the lag shitfest is caused by the engine processing collision?
// Returning true will cause the object to completely stop colliding with anything
// Including traces such as with physics_debug_entity
// Also note the lag doesn't stop when the game is paused, indicating that it isn't caused
// by physics simulations.
bool CPhysicsObject::IsAsleep() const {
	return m_pObject->getActivationState() == ISLAND_SLEEPING;
	// FIXME: Returning true ensues an extreme lag storm, figure out why since this fix is counter-effective
	//return false;
}

bool CPhysicsObject::IsTrigger() const {
	NOT_IMPLEMENTED;
	return false;
}

bool CPhysicsObject::IsFluid() const {
	return m_pFluidController != NULL;
}

bool CPhysicsObject::IsHinged() const {
	NOT_IMPLEMENTED;
	return false;
}

bool CPhysicsObject::IsCollisionEnabled() const {
	return !(m_pObject->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE);
}

bool CPhysicsObject::IsGravityEnabled() const {
	if (!IsStatic()) {
		return !(m_pObject->getFlags() & BT_DISABLE_WORLD_GRAVITY);
	}

	return false;
}

bool CPhysicsObject::IsDragEnabled() const {
	if ( !IsStatic() )
	{
		return m_pEnv->GetDragController()->IsControlling(this); // Expensive function
	}

	return false;
}

bool CPhysicsObject::IsMotionEnabled() const {
	return m_bMotionEnabled;
}

bool CPhysicsObject::IsMoveable() const {
	if (IsStatic() || !IsMotionEnabled()) return false;
	return true;
}

bool CPhysicsObject::IsAttachedToConstraint(bool bExternalOnly) const {
	NOT_IMPLEMENTED;
	return false;
}

void CPhysicsObject::EnableCollisions(bool enable) {
	if (IsCollisionEnabled() == enable) return;

	if (enable) {
		m_pObject->setCollisionFlags(m_pObject->getCollisionFlags() & ~btCollisionObject::CF_NO_CONTACT_RESPONSE);
	} else {
		m_pObject->setCollisionFlags(m_pObject->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
	}
}

void CPhysicsObject::EnableGravity(bool enable) {
	if (IsGravityEnabled() == enable || IsStatic()) return;

	if (enable) {
		m_pObject->setGravity(m_pEnv->GetBulletEnvironment()->getGravity());
		m_pObject->setFlags(m_pObject->getFlags() & ~BT_DISABLE_WORLD_GRAVITY);
	} else {
		m_pObject->setGravity(btVector3(0,0,0));
		m_pObject->setFlags(m_pObject->getFlags() | BT_DISABLE_WORLD_GRAVITY);
	}
}

void CPhysicsObject::EnableDrag(bool enable)  {
	if (IsStatic())
		return;

	if (enable != IsDragEnabled())
	{
		if (enable) {
			m_pEnv->GetDragController()->AddPhysicsObject(this);
		} else {
			m_pEnv->GetDragController()->RemovePhysicsObject(this);
		}
	}
}

void CPhysicsObject::EnableMotion(bool enable) {
	if (IsMotionEnabled() == enable || IsStatic()) return;
	m_bMotionEnabled = enable;

	// TODO: Does this cause any issues with player controllers?
	if (enable) {
		m_pObject->setLinearFactor(btVector3(1, 1, 1));
		m_pObject->setAngularFactor(1);
	} else {
		m_pObject->setLinearVelocity(btVector3(0, 0, 0));
		m_pObject->setAngularVelocity(btVector3(0, 0, 0));

		m_pObject->setLinearFactor(btVector3(0, 0, 0));
		m_pObject->setAngularFactor(0);
	}
}

void CPhysicsObject::SetGameData(void *pGameData) {
	m_pGameData = pGameData;
}

void *CPhysicsObject::GetGameData() const {
	return m_pGameData;
}

void CPhysicsObject::SetGameFlags(unsigned short userFlags) {
	m_gameFlags = userFlags;
}

unsigned short CPhysicsObject::GetGameFlags() const {
	return m_gameFlags;
}

void CPhysicsObject::SetGameIndex(unsigned short gameIndex) {
	m_iGameIndex = gameIndex;
}

unsigned short CPhysicsObject::GetGameIndex() const {
	return m_iGameIndex;
}

void CPhysicsObject::SetCallbackFlags(unsigned short callbackflags) {
	m_callbacks = callbackflags;
}

unsigned short CPhysicsObject::GetCallbackFlags() const {
	return m_callbacks;
}

// UNEXPOSED
void CPhysicsObject::AddCallbackFlags(unsigned short flags) {
	m_callbacks |= flags;
}

// UNEXPOSED
void CPhysicsObject::RemoveCallbackFlags(unsigned short flags) {
	m_callbacks &= ~(flags);
}

void CPhysicsObject::Wake() {
	m_pObject->setActivationState(ACTIVE_TAG);
}

void CPhysicsObject::Sleep() {
	m_pObject->setActivationState(ISLAND_SLEEPING);
}

void CPhysicsObject::RecheckCollisionFilter() {
	// Bullet caches nothing about what should collide with what
}

void CPhysicsObject::RecheckContactPoints() {
	NOT_IMPLEMENTED;
}

void CPhysicsObject::SetMass(float mass) {
	if (IsStatic()) return;

	btVector3 btvec = m_pObject->getInvInertiaDiagLocal();

	// Invert the inverse intertia to get inertia
	btvec.setX(SAFE_DIVIDE(1.0, btvec.x()));
	btvec.setY(SAFE_DIVIDE(1.0, btvec.y()));
	btvec.setZ(SAFE_DIVIDE(1.0, btvec.z()));

	m_pObject->setMassProps(mass, btvec);
}

float CPhysicsObject::GetMass() const {
	btScalar invmass = m_pObject->getInvMass();
	return SAFE_DIVIDE(1.0, invmass);
}

float CPhysicsObject::GetInvMass() const {
	return m_pObject->getInvMass();
}

Vector CPhysicsObject::GetInertia() const {
	btVector3 btvec = m_pObject->getInvInertiaDiagLocal();

	// Invert the inverse intertia to get inertia
	btvec.setX(SAFE_DIVIDE(1.0, btvec.x()));
	btvec.setY(SAFE_DIVIDE(1.0, btvec.y()));
	btvec.setZ(SAFE_DIVIDE(1.0, btvec.z()));

	Vector hlvec;
	ConvertDirectionToHL(btvec, hlvec);
	VectorAbs(hlvec, hlvec);
	return hlvec;
}

Vector CPhysicsObject::GetInvInertia() const {
	btVector3 btvec = m_pObject->getInvInertiaDiagLocal();
	Vector hlvec;
	ConvertDirectionToHL(btvec, hlvec);
	VectorAbs(hlvec, hlvec);
	return hlvec;
}

void CPhysicsObject::SetInertia(const Vector &inertia) {
	btVector3 btvec;
	ConvertDirectionToBull(inertia, btvec);
	btvec = btvec.absolute();

	btvec.setX(SAFE_DIVIDE(1.0, btvec.x()));
	btvec.setY(SAFE_DIVIDE(1.0, btvec.y()));
	btvec.setZ(SAFE_DIVIDE(1.0, btvec.z()));

	m_pObject->setInvInertiaDiagLocal(btvec);
	m_pObject->updateInertiaTensor();
}

void CPhysicsObject::SetDamping(const float *speed, const float *rot) {
	if (!speed && !rot) return;

	if (speed && rot) {
		m_pObject->setDamping(*speed, *rot);
		return;
	}
	if (speed) m_pObject->setDamping(*speed, m_pObject->getAngularDamping());
	if (rot) m_pObject->setDamping(m_pObject->getLinearDamping(), *rot);
}

void CPhysicsObject::GetDamping(float *speed, float *rot) const {
	if (speed) *speed = m_pObject->getLinearDamping();
	if (rot) *rot = m_pObject->getAngularDamping();
}

void CPhysicsObject::SetDragCoefficient( float *pDrag, float *pAngularDrag )
{
	if ( pDrag )
	{
		m_dragCoefficient = *pDrag;
	}
	if ( pAngularDrag )
	{
		m_angDragCoefficient = *pAngularDrag;
	}
}

void CPhysicsObject::SetBuoyancyRatio(float ratio) {
	m_fBuoyancyRatio = ratio;
}

int CPhysicsObject::GetMaterialIndex() const {
	return m_materialIndex;
}

void CPhysicsObject::SetMaterialIndex(int materialIndex) {
	m_materialIndex = materialIndex;
}

unsigned int CPhysicsObject::GetContents() const {
	return m_contents;
}

void CPhysicsObject::SetContents(unsigned int contents) {
	m_contents = contents;
}

float CPhysicsObject::GetSphereRadius() const {
	btCollisionShape  *shape = m_pObject->getCollisionShape();
	if (shape->getShapeType() != SPHERE_SHAPE_PROXYTYPE) return 0;
	btSphereShape *sphere = (btSphereShape*)shape;
	return ConvertDistanceToHL(sphere->getRadius());
}

float CPhysicsObject::GetEnergy() const {
	float e = 0.5 * GetMass() * m_pObject->getLinearVelocity().dot(m_pObject->getLinearVelocity());
	e =+ 0.5 * GetMass() * m_pObject->getAngularVelocity().dot(m_pObject->getAngularVelocity());
	return ConvertEnergyToHL(e);
}

// Local space means from the origin of the model
Vector CPhysicsObject::GetMassCenterLocalSpace() const {
	btTransform bullTransform = ((btMassCenterMotionState *)m_pObject->getMotionState())->m_centerOfMassOffset;
	Vector HLMassCenter;
	ConvertPosToHL(bullTransform.getOrigin(), HLMassCenter);

	return HLMassCenter;
}

void CPhysicsObject::SetPosition(const Vector &worldPosition, const QAngle &angles, bool isTeleport) {
	btVector3 bullPos;
	btMatrix3x3 bullAngles;
	ConvertPosToBull(worldPosition, bullPos);
	ConvertRotationToBull(angles, bullAngles);
	((btMassCenterMotionState *)m_pObject->getMotionState())->setGraphicTransform(btTransform(bullAngles, bullPos));

	// Mass center compensation.
	btTransform finaltrans;
	((btMassCenterMotionState *)m_pObject->getMotionState())->getWorldTransform(finaltrans);
	m_pObject->setWorldTransform(finaltrans);

	if (isTeleport)
		m_pObject->setActivationState(ACTIVE_TAG);
}

void CPhysicsObject::SetPositionMatrix(const matrix3x4_t &matrix, bool isTeleport) {
	btTransform trans;
	ConvertMatrixToBull(matrix, trans);
	((btMassCenterMotionState *)m_pObject->getMotionState())->setGraphicTransform(trans);

	// Mass center compensation.
	btTransform finaltrans;
	((btMassCenterMotionState *)m_pObject->getMotionState())->getWorldTransform(finaltrans);
	m_pObject->setWorldTransform(finaltrans);

	if (isTeleport)
		m_pObject->setActivationState(ACTIVE_TAG);
}

void CPhysicsObject::GetPosition(Vector *worldPosition, QAngle *angles) const {
	if (!worldPosition && !angles) return;

	btTransform transform;
	((btMassCenterMotionState *)m_pObject->getMotionState())->getGraphicTransform(transform);
	if (worldPosition) ConvertPosToHL(transform.getOrigin(), *worldPosition);
	if (angles) ConvertRotationToHL(transform.getBasis(), *angles);
}

void CPhysicsObject::GetPositionMatrix(matrix3x4_t *positionMatrix) const {
	if (!positionMatrix) return;

	btTransform transform;
	((btMassCenterMotionState*)m_pObject->getMotionState())->getGraphicTransform(transform);
	ConvertMatrixToHL(transform, *positionMatrix);
}

void CPhysicsObject::SetVelocity(const Vector *velocity, const AngularImpulse *angularVelocity) {
	if (!velocity && !angularVelocity) return;

	btVector3 vel, angvel;
	if (velocity) {
		ConvertPosToBull(*velocity, vel);
		m_pObject->setLinearVelocity(vel);
	}
	if (angularVelocity) {
		ConvertAngularImpulseToBull(*angularVelocity, angvel);
		m_pObject->setAngularVelocity(angvel);
	}
}

void CPhysicsObject::SetVelocityInstantaneous(const Vector *velocity, const AngularImpulse *angularVelocity) {
	// FIXME: what is different from SetVelocity?
	SetVelocity(velocity, angularVelocity);
}

void CPhysicsObject::GetVelocity(Vector *velocity, AngularImpulse *angularVelocity) const {
	if (velocity) ConvertPosToHL(m_pObject->getLinearVelocity(), *velocity);
	if (angularVelocity) ConvertAngularImpulseToHL(m_pObject->getAngularVelocity(), *angularVelocity);
}

void CPhysicsObject::AddVelocity(const Vector *velocity, const AngularImpulse *angularVelocity) {
	if (!velocity && !angularVelocity) return;

	btVector3 btvelocity, btangular;
	if (velocity) {
		ConvertPosToBull(*velocity, btvelocity);
		m_pObject->setLinearVelocity(m_pObject->getLinearVelocity() + btvelocity);
	}
	if (angularVelocity) {
		ConvertAngularImpulseToBull(*angularVelocity, btangular);
		m_pObject->setAngularVelocity(m_pObject->getAngularVelocity() + btangular);
	}
}

void CPhysicsObject::GetVelocityAtPoint(const Vector &worldPosition, Vector *pVelocity) const {
	if (!pVelocity) return;

	btVector3 vec;
	ConvertPosToBull(worldPosition, vec);
	ConvertPosToHL(m_pObject->getVelocityInLocalPoint(vec), *pVelocity);
}

void CPhysicsObject::GetImplicitVelocity(Vector *velocity, AngularImpulse *angularVelocity) const {
	if (!velocity && !angularVelocity) return;

	NOT_IMPLEMENTED;
}

void CPhysicsObject::LocalToWorld(Vector *worldPosition, const Vector &localPosition) const {
	if (!worldPosition) return;

	matrix3x4_t matrix;
	GetPositionMatrix(&matrix);
	VectorTransform(Vector(localPosition), matrix, *worldPosition);
}

void CPhysicsObject::WorldToLocal(Vector *localPosition, const Vector &worldPosition) const {
	if (!localPosition) return;

	matrix3x4_t matrix;
	GetPositionMatrix(&matrix);
	VectorITransform(Vector(worldPosition), matrix, *localPosition);
}

void CPhysicsObject::LocalToWorldVector(Vector *worldVector, const Vector &localVector) const {
	if (!worldVector) return;

	matrix3x4_t matrix;
	GetPositionMatrix(&matrix);
	VectorRotate(Vector(localVector), matrix, *worldVector);
}

void CPhysicsObject::WorldToLocalVector(Vector *localVector, const Vector &worldVector) const {
	if (!localVector) return;

	matrix3x4_t matrix;
	GetPositionMatrix(&matrix);
	VectorIRotate(Vector(worldVector), matrix, *localVector);
}

// These two functions apply an insanely low force!
void CPhysicsObject::ApplyForceCenter(const Vector &forceVector) {
	btVector3 force;
	ConvertForceImpulseToBull(forceVector, force);
	m_pObject->applyCentralForce(force);
}

void CPhysicsObject::ApplyForceOffset(const Vector &forceVector, const Vector &worldPosition) {
	Vector local;
	WorldToLocal(&local, worldPosition);
	btVector3 force, offset;
	ConvertForceImpulseToBull(forceVector, force);
	ConvertPosToBull(local, offset);
	m_pObject->applyForce(force, offset);
}

void CPhysicsObject::ApplyTorqueCenter(const AngularImpulse &torque) {
	btVector3 bullTorque;
	ConvertAngularImpulseToBull(torque, bullTorque);
	m_pObject->applyTorque(bullTorque);
}

void CPhysicsObject::CalculateForceOffset(const Vector &forceVector, const Vector &worldPosition, Vector *centerForce, AngularImpulse *centerTorque) const {
	if (!centerForce && !centerTorque) return;
	NOT_IMPLEMENTED;
}

// TODO: Thrusters call this
void CPhysicsObject::CalculateVelocityOffset(const Vector &forceVector, const Vector &worldPosition, Vector *centerVelocity, AngularImpulse *centerAngularVelocity) const {
	if (!centerVelocity && !centerAngularVelocity) return;

	btVector3 bullForceVector, bullWorldPosition;
	ConvertForceImpulseToBull(forceVector, bullForceVector);
	ConvertPosToBull(worldPosition, bullWorldPosition);
	NOT_IMPLEMENTED;
}

float CPhysicsObject::CalculateLinearDrag(const Vector &unitDirection) const {
	btVector3 bull_unitDirection;
	ConvertDirectionToBull(unitDirection,bull_unitDirection);
	return GetDragInDirection( &bull_unitDirection );
}

float CPhysicsObject::CalculateAngularDrag(const Vector &objectSpaceRotationAxis) const {
	btVector3 bull_unitDirection;
	ConvertDirectionToBull(objectSpaceRotationAxis, bull_unitDirection);
	return DEG2RAD(GetAngularDragInDirection(&bull_unitDirection));
}

bool CPhysicsObject::GetContactPoint(Vector *contactPoint, IPhysicsObject **contactObject) const {
	if (!contactPoint && !contactObject) return true;

	int numManifolds = m_pEnv->GetBulletEnvironment()->getDispatcher()->getNumManifolds();
	for (int i = 0; i < numManifolds; i++) {
		btPersistentManifold *contactManifold = m_pEnv->GetBulletEnvironment()->getDispatcher()->getManifoldByIndexInternal(i);
		const btCollisionObject *obA = contactManifold->getBody0();
		const btCollisionObject *obB = contactManifold->getBody1();

		if (contactManifold->getNumContacts() <= 0)
			continue;

		// Does it matter what the index is, or should we just return the first point of contact?
		btManifoldPoint bullContactPoint = contactManifold->getContactPoint(0);

		if (obA == m_pObject) {
			btVector3 bullContactVec = bullContactPoint.getPositionWorldOnA();

			if (contactPoint) {
				ConvertPosToHL(bullContactVec, *contactPoint);
			}

			if (contactObject) {
				*contactObject = (IPhysicsObject *)obA->getUserPointer();
			}

			return true;
		} else if (obB == m_pObject) {
			btVector3 bullContactVec = bullContactPoint.getPositionWorldOnB();

			if (contactPoint) {
				ConvertPosToHL(bullContactVec, *contactPoint);
			}

			if (contactObject) {
				*contactObject = (IPhysicsObject *)obB->getUserPointer();
			}

			return true;
		}
	}

	return false; // Bool success
}

void CPhysicsObject::SetShadow(float maxSpeed, float maxAngularSpeed, bool allowPhysicsMovement, bool allowPhysicsRotation) {
	if (m_pShadow) {
		m_pShadow->MaxSpeed(maxSpeed, maxAngularSpeed);
	} else {
		unsigned int flags = GetCallbackFlags() | CALLBACK_SHADOW_COLLISION;
		flags &= ~CALLBACK_GLOBAL_FRICTION;
		flags &= ~CALLBACK_GLOBAL_COLLIDE_STATIC;
		SetCallbackFlags(flags);

		m_pShadow = (CShadowController *)m_pEnv->CreateShadowController(this, allowPhysicsMovement, allowPhysicsRotation);
		m_pShadow->MaxSpeed(maxSpeed, maxAngularSpeed);
	}
}

void CPhysicsObject::UpdateShadow(const Vector &targetPosition, const QAngle &targetAngles, bool tempDisableGravity, float timeOffset) {
	if (m_pShadow) {
		m_pShadow->Update(targetPosition, targetAngles, timeOffset);
	}
}

int CPhysicsObject::GetShadowPosition(Vector *position, QAngle *angles) const {
	if (!position && !angles) return 1;

	btTransform transform;
	((btMassCenterMotionState*)m_pObject->getMotionState())->getGraphicTransform(transform);
	if (position) {
		ConvertPosToHL(transform.getOrigin(), *position);
	}
	if (angles) {
		ConvertRotationToHL(transform.getBasis(), *angles);
	}

	return 0; // return pVEnv->GetSimulatedPSIs();
}

IPhysicsShadowController *CPhysicsObject::GetShadowController() const {
	return m_pShadow;
}

void CPhysicsObject::RemoveShadowController() {
	if (m_pShadow)
		m_pEnv->DestroyShadowController(m_pShadow);

	m_pShadow = NULL;
}

float CPhysicsObject::ComputeShadowControl(const hlshadowcontrol_params_t &params, float secondsToArrival, float dt) {
	return ComputeShadowControllerHL(this, params, secondsToArrival, dt);
}

const CPhysCollide *CPhysicsObject::GetCollide() const {
	return (CPhysCollide *)m_pObject->getCollisionShape();
}

const char *CPhysicsObject::GetName() const {
	return m_pName;
}

void CPhysicsObject::BecomeTrigger() {
	NOT_IMPLEMENTED;
}

void CPhysicsObject::RemoveTrigger() {
	NOT_IMPLEMENTED;
}

void CPhysicsObject::BecomeHinged(int localAxis) {
	NOT_IMPLEMENTED;
}

void CPhysicsObject::RemoveHinged() {
	NOT_IMPLEMENTED;
}

IPhysicsFrictionSnapshot *CPhysicsObject::CreateFrictionSnapshot() {
	return new CPhysicsFrictionSnapshot(this);
}

void CPhysicsObject::DestroyFrictionSnapshot(IPhysicsFrictionSnapshot *pSnapshot) {
	delete pSnapshot;
}

void CPhysicsObject::OutputDebugInfo() const {
	Msg("-----------------\n");

	if (m_pName)
		Msg("Object: %s\n", m_pName);

	Msg("Mass: %f (inv %f)\n", GetMass(), GetInvMass());

	Vector pos;
	QAngle ang;
	GetPosition(&pos, &ang);
	Msg("Position: %f %f %f\nAngle: %f %f %f\n", pos.x, pos.y, pos.z, ang.x, ang.y, ang.z);

	Vector inertia = GetInertia();
	Vector invinertia = GetInvInertia();
	Msg("Inertia: %f %f %f (inv %f %f %f)\n", inertia.x, inertia.y, inertia.z, invinertia.x, invinertia.y, invinertia.z);

	Vector vel;
	AngularImpulse angvel;
	GetVelocity(&vel, &angvel);
	Msg("Velocity: %f, %f, %f\nAng Velocity: %f, %f, %f\n", vel.x, vel.y, vel.z, angvel.x, angvel.y, angvel.z);

	float dampspeed, damprot;
	GetDamping(&dampspeed, &damprot);
	Msg("Damping %f linear, %f angular\n", dampspeed, damprot);

	Vector dragBasis;
	Vector angDragBasis;
	ConvertPosToHL(m_dragBasis, dragBasis);
	ConvertDirectionToHL(m_angDragBasis, angDragBasis);
	Msg("Linear Drag: %f, %f, %f (factor %f)\n", dragBasis.x, dragBasis.y, dragBasis.z, m_dragCoefficient);
	Msg("Angular Drag: %f, %f, %f (factor %f)\n", angDragBasis.x, angDragBasis.y, angDragBasis.z, m_angDragCoefficient);

	// Attached to x controllers



	Msg("State: %s, Collision %s, Motion %s, Drag %s, Flags %04X (game %04x, index %d)\n", 
		IsAsleep() ? "Asleep" : "Awake",
		IsCollisionEnabled() ? "Enabled" : "Disabled",
		IsStatic() ? "Static" : IsMotionEnabled() ? "Enabled" : "Disabled",
		IsDragEnabled() ? "Enabled" : "Disabled",
		m_pObject->getFlags(),
		GetGameFlags(),
		GetGameIndex()
	);

	
	const char *pMaterialStr = g_SurfaceDatabase.GetPropName(m_materialIndex);
	surfacedata_t *surfaceData = g_SurfaceDatabase.GetSurfaceData(m_materialIndex);
	if (surfaceData) {
		Msg("Material: %s : density(%f), thickness(%f), friction(%f), elasticity(%f)\n", 
			pMaterialStr, surfaceData->physics.density, surfaceData->physics.thickness, surfaceData->physics.friction, surfaceData->physics.elasticity);
	}

	// FIXME: complete this function via format noted on
	// http://facepunch.com/threads/1178143?p=35663773&viewfull=1#post35663773
}

// UNEXPOSED
void CPhysicsObject::Init(CPhysicsEnvironment *pEnv, btRigidBody *pObject, int materialIndex, objectparams_t *pParams) {
	m_pEnv = pEnv;
	m_materialIndex = materialIndex;
	m_pObject = pObject;
	pObject->setUserPointer(this);
	m_gameFlags = 0;
	m_iLastActivationState = pObject->getActivationState();
	m_callbacks = CALLBACK_GLOBAL_COLLISION|CALLBACK_GLOBAL_FRICTION|CALLBACK_FLUID_TOUCH|CALLBACK_GLOBAL_TOUCH|CALLBACK_GLOBAL_COLLIDE_STATIC|CALLBACK_DO_FLUID_SIMULATION;
	float matdensity;
	g_SurfaceDatabase.GetPhysicsProperties(materialIndex, &matdensity, NULL, NULL, NULL);
	m_fBuoyancyRatio = (GetMass()/(GetVolume()*METERS_PER_INCH*METERS_PER_INCH*METERS_PER_INCH))/matdensity;
	m_bMotionEnabled = !IsStatic();
	m_fMass = GetMass();

	if (pParams) {
		m_pGameData = pParams->pGameData;
		m_pName = pParams->pName;
		m_fVolume = pParams->volume;
		EnableCollisions(pParams->enableCollisions);
	}

	surfacedata_t *surface = g_SurfaceDatabase.GetSurfaceData(materialIndex);
	if (surface) {
		m_pObject->setFriction(surface->physics.friction);
		// Note to self: using these dampening values = breakdancing fridges http://dl.dropbox.com/u/4838268/gm_construct%202012-4-24%2004-50-26.webm
		//m_pObject->setDamping(surface->physics.dampening, surface->physics.dampening);
	}

	// Drag calculations converted from  2003 source code
	float drag = 0;
	float angDrag = 0;
	if (pParams) {
		drag = pParams->dragCoefficient;
		angDrag = pParams->dragCoefficient;
	}

	m_dragBasis.setValue(0, 0, 0);
	m_angDragBasis.setValue(0, 0, 0);

	// THIS IS COMPLETELY BROKEN!
	if (!IsStatic() && GetCollide()) {
		btCollisionShape  *shape = m_pObject->getCollisionShape();

		btVector3 min, max, delta;
		btTransform t;

		shape->getAabb( t, min, max);

		delta = min, max;
		delta = delta.absolute();

		m_dragBasis.setX(delta.y() * delta.z());
		m_dragBasis.setY(delta.x() * delta.z());
		m_dragBasis.setZ(delta.x() * delta.y());

		btVector3 ang = m_pObject->getInvInertiaDiagLocal();
		delta *= 0.5;

		m_angDragBasis.setX(AngDragIntegral(ang[0], delta.x(), delta.y(), delta.z()) + AngDragIntegral(ang[0], delta.x(), delta.z(), delta.y()));
		m_angDragBasis.setY(AngDragIntegral(ang[1], delta.y(), delta.x(), delta.z()) + AngDragIntegral(ang[1], delta.y(), delta.z(), delta.x()));
		m_angDragBasis.setZ(AngDragIntegral(ang[2], delta.z(), delta.x(), delta.y()) + AngDragIntegral(ang[2], delta.z(), delta.y(), delta.x()));
	} else {
		drag = 0;
		angDrag = 0;
	}

	m_dragCoefficient = drag;
	m_angDragCoefficient = angDrag;
}

// UNEXPOSED
CPhysicsEnvironment *CPhysicsObject::GetVPhysicsEnvironment() {
	return m_pEnv;
}

// UNEXPOSED
btRigidBody *CPhysicsObject::GetObject() {
	return m_pObject;
}

// UNEXPOSED
float CPhysicsObject::GetDragInDirection(btVector3 *dir) const
{
	if (!dir) return 0.0f;

	btVector3 out;
	btMatrix3x3 mat = m_pObject->getCenterOfMassTransform().getBasis(); // const IVP_U_Matrix *m_world_f_core = m_pObject->get_core()->get_m_world_f_core_PSI();
	BtMatrix_vimult(&mat, dir, &out); // m_world_f_core->vimult3( &velocity, &local );

	return m_dragCoefficient * fabs(out.getX() * m_dragBasis.getX()) +	// Maybe the fabs need to be calculated first AND THEN be multiplied with the m_dragCoefficient
		fabs(out.getY() * m_dragBasis.getY()) +							// However this is the way its done in the 2003 code.
		fabs(out.getZ() * m_dragBasis.getZ());
	
}

// UNEXPOSED
float CPhysicsObject::GetAngularDragInDirection(btVector3 *dir) const
{
	if (!dir) return 0.0f;

	return m_angDragCoefficient * fabs(dir->getX() * m_angDragBasis.getX()) +
		fabs(dir->getY() * m_angDragBasis.getY()) +
		fabs(dir->getZ() * m_angDragBasis.getZ());
	/*
	return m_angDragCoefficient * IVP_Inline_Math::fabsd( angVelocity.k[0] * m_angDragBasis.x ) + 
		IVP_Inline_Math::fabsd( angVelocity.k[1] * m_angDragBasis.y ) + 
		IVP_Inline_Math::fabsd( angVelocity.k[2] * m_angDragBasis.z );
	*/
}

/************************
* CREATION FUNCTIONS
************************/

CPhysicsObject *CreatePhysicsObject(CPhysicsEnvironment *pEnvironment, const CPhysCollide *pCollisionModel, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic) {
	btCollisionShape *shape = (btCollisionShape *)pCollisionModel;
	
	btVector3 vector;
	btMatrix3x3 matrix;
	ConvertPosToBull(position, vector);
	ConvertRotationToBull(angles, matrix);
	btTransform transform(matrix, vector);

	PhysicsShapeInfo *shapeInfo = (PhysicsShapeInfo*)shape->getUserPointer();
	btTransform masscenter(btMatrix3x3::getIdentity());
	if (shapeInfo) masscenter.setOrigin(shapeInfo->massCenter);

	float mass = 0;

	if (pParams && !isStatic)
		mass = pParams->mass;

	btVector3 inertia(0, 0, 0);

	if (!isStatic)
		shape->calculateLocalInertia(mass, inertia);
	btMotionState *motionstate = new btMassCenterMotionState(transform, masscenter);
	btRigidBody::btRigidBodyConstructionInfo info(mass, motionstate, shape, inertia);

	if (pParams) {
		info.m_linearDamping = pParams->damping;
		info.m_angularDamping = pParams->rotdamping;
		//info.m_localInertia = btVector3(pParams->inertia, pParams->inertia, pParams->inertia);
	}

	btRigidBody *body = new btRigidBody(info);

	if (isStatic)
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);

	if (!isStatic)
		pEnvironment->GetBulletEnvironment()->addRigidBody(body);
	else
		pEnvironment->GetBulletEnvironment()->addRigidBody(body, 2, ~2);	// Static objects don't collide with static objects.

	CPhysicsObject *pObject = new CPhysicsObject();
	pObject->Init(pEnvironment, body, materialIndex, pParams);
	if (!isStatic && pParams && pParams->dragCoefficient != 0.0f) pObject->EnableDrag(true);

	/*if (mass > 0)
	{
		btVector3 mins, maxs;
		shape->getAabb(btTransform::getIdentity(), mins, maxs);
		float maxradius = min(min(abs(maxs.getX()), abs(maxs.getY())), abs(maxs.getZ()));
		float minradius = min(min(abs(mins.getX()), abs(mins.getY())), abs(mins.getZ()));
		float radius = min(maxradius,minradius)/2.0f;
		body->setCcdMotionThreshold(radius*0.5f);
		body->setCcdSweptSphereRadius(0.2f*radius);
	}*/
	
	return pObject;
}

CPhysicsObject *CreatePhysicsSphere(CPhysicsEnvironment *pEnvironment, float radius, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic) {
	if (!pEnvironment) return NULL;

	btSphereShape *shape = new btSphereShape(ConvertDistanceToBull(radius));
	
	btVector3 vector;
	btMatrix3x3 matrix;
	ConvertPosToBull(position, vector);
	ConvertRotationToBull(angles, matrix);
	btTransform transform(matrix, vector);

	float mass = 0;
	float volume = 0;

	if (pParams) {
		mass = isStatic ? 0 : pParams->mass;

		volume = pParams->volume;
		if (volume <= 0) {
			pParams->volume = 4.0f * radius * radius * radius * M_PI / 3.0f;
		}
	}

	btMotionState *motionstate = new btMassCenterMotionState(transform);
	btRigidBody::btRigidBodyConstructionInfo info(mass, motionstate, shape);

	btRigidBody *body = new btRigidBody(info);

	if (isStatic)
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
	else
		body->setCollisionFlags(body->getCollisionFlags() & ~(btCollisionObject::CF_STATIC_OBJECT));

	if (mass > 0)
		pEnvironment->GetBulletEnvironment()->addRigidBody(body);
	else
		pEnvironment->GetBulletEnvironment()->addRigidBody(body, 2, ~2);

	CPhysicsObject *pObject = new CPhysicsObject();
	pObject->Init(pEnvironment, body, materialIndex, pParams);

	return pObject;
}