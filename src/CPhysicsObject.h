#ifndef CPHYSICSOBJECT_H
#define CPHYSICSOBJECT_H

class CPhysicsEnvironment;
class CShadowController;
class CPhysicsFluidController;
class CPhysicsConstraint;

class CPhysicsObject : public IPhysicsObject {
	public:
											CPhysicsObject();
											~CPhysicsObject();
		bool								IsStatic() const;
		bool								IsAsleep() const;
		bool								IsTrigger() const;
		bool								IsFluid() const;
		bool								IsHinged() const;
		bool								IsCollisionEnabled() const;
		bool								IsGravityEnabled() const;
		bool								IsDragEnabled() const;
		bool								IsMotionEnabled() const;
		bool								IsMoveable() const;
		bool								IsAttachedToConstraint(bool bExternalOnly) const;

		void								EnableCollisions(bool enable);
		void								EnableGravity(bool enable);
		void								EnableDrag(bool enable);
		void								EnableMotion(bool enable);

		void								SetGameData(void *pGameData);
		void *								GetGameData() const;
		void								SetGameFlags(unsigned short userFlags);
		unsigned short						GetGameFlags() const;
		void								SetGameIndex(unsigned short gameIndex);
		unsigned short						GetGameIndex() const;
		void								SetCallbackFlags(unsigned short callbackflags);
		unsigned short						GetCallbackFlags() const;

		// UNEXPOSED
		void								AddCallbackFlags(unsigned short flag);
		void								RemoveCallbackFlags(unsigned short flag);

		void								Wake();
		void								Sleep();

		void								RecheckCollisionFilter();
		void								RecheckContactPoints();

		void								SetMass(float mass);
		float								GetMass() const;
		float								GetInvMass() const;
		Vector								GetInertia() const;
		Vector								GetInvInertia() const;
		void								SetInertia(const Vector& inertia);
		void								SetDamping(const float *speed, const float *rot);
		void								GetDamping(float *speed, float *rot) const;
		void								SetDragCoefficient(float *pDrag, float *pAngularDrag);
		void								SetBuoyancyRatio(float ratio);
		int									GetMaterialIndex() const;
		void								SetMaterialIndex(int materialIndex);
		unsigned int						GetContents() const;
		void								SetContents(unsigned int contents);
		float								GetSphereRadius() const;
		float								GetEnergy() const;
		Vector								GetMassCenterLocalSpace() const;

		void								SetPosition(const Vector& worldPosition, const QAngle& angles, bool isTeleport);
		void								SetPositionMatrix(const matrix3x4_t&matrix, bool isTeleport);
		void								GetPosition(Vector *worldPosition, QAngle *angles) const;
		void								GetPositionMatrix(matrix3x4_t *positionMatrix) const;

		void								SetVelocity(const Vector *velocity, const AngularImpulse *angularVelocity);
		void								SetVelocityInstantaneous(const Vector *velocity, const AngularImpulse *angularVelocity);
		void								GetVelocity(Vector *velocity, AngularImpulse *angularVelocity) const;
		void								AddVelocity(const Vector *velocity, const AngularImpulse *angularVelocity);
		void								GetVelocityAtPoint(const Vector& worldPosition, Vector *pVelocity) const;
		void								GetImplicitVelocity(Vector *velocity, AngularImpulse *angularVelocity) const;

		void								LocalToWorld(Vector *worldPosition, const Vector& localPosition) const;
		void								WorldToLocal(Vector *localPosition, const Vector& worldPosition) const;
		void								LocalToWorldVector(Vector *worldVector, const Vector& localVector) const;
		void								WorldToLocalVector(Vector *localVector, const Vector& worldVector) const;
	
		void								ApplyForceCenter(const Vector& forceVector);
		void								ApplyForceOffset(const Vector& forceVector, const Vector& worldPosition);
		void								ApplyTorqueCenter(const AngularImpulse& torque);

		void								CalculateForceOffset(const Vector& forceVector, const Vector& worldPosition, Vector *centerForce, AngularImpulse *centerTorque) const;
		void								CalculateVelocityOffset(const Vector& forceVector, const Vector& worldPosition, Vector *centerVelocity, AngularImpulse *centerAngularVelocity) const;
		float								CalculateLinearDrag(const Vector& unitDirection) const;
		float								CalculateAngularDrag(const Vector& objectSpaceRotationAxis) const;

		bool								GetContactPoint(Vector *contactPoint, IPhysicsObject **contactObject) const;

		void								SetShadow(float maxSpeed, float maxAngularSpeed, bool allowPhysicsMovement, bool allowPhysicsRotation);
		void								UpdateShadow(const Vector& targetPosition, const QAngle& targetAngles, bool tempDisableGravity, float timeOffset);
	
		int									GetShadowPosition(Vector *position, QAngle *angles) const;
		IPhysicsShadowController *			GetShadowController() const;
		void								RemoveShadowController();
		float								ComputeShadowControl(const hlshadowcontrol_params_t& params, float secondsToArrival, float dt);

		const CPhysCollide *				GetCollide() const;
		const char *						GetName() const;

		void								BecomeTrigger();
		void								RemoveTrigger();

		void								BecomeHinged(int localAxis);
		void								RemoveHinged();

		IPhysicsFrictionSnapshot *			CreateFrictionSnapshot();
		void								DestroyFrictionSnapshot(IPhysicsFrictionSnapshot *pSnapshot);

		void								OutputDebugInfo() const;

		CPhysicsFluidController *			GetFluidController(void) { return m_pFluidController; }
		void								SetFluidController(CPhysicsFluidController *controller) { m_pFluidController = controller; }
	public:
		// UNEXPOSED FUNCTIONS
		void								Init(CPhysicsEnvironment *pEnv, btRigidBody *pObject, int materialIndex, objectparams_t *pParams);
		CPhysicsEnvironment *				GetVPhysicsEnvironment();
		btRigidBody *						GetObject();
		float								GetDragInDirection(btVector3  *direction) const; // Function is not interfaced anymore
		float								GetAngularDragInDirection(btVector3  *direction) const;
		float								GetVolume() const { return m_fVolume; }
		float								GetBuoyancyRatio() const { return m_fBuoyancyRatio; }

		int									m_iLastActivationState;
	private:
		CPhysicsEnvironment *				m_pEnv;
		void *								m_pGameData;
		btRigidBody *						m_pObject;
		const char *						m_pName;

		unsigned short						m_materialIndex;

		unsigned short						m_callbacks;
		unsigned short						m_gameFlags;
		unsigned int						m_contents;
		unsigned short						m_iGameIndex;


		bool								m_bMotionEnabled;
		float								m_fMass;
		float								m_fVolume;
		float								m_fBuoyancyRatio;
		float								m_dragCoefficient;
		float								m_angDragCoefficient;
		btVector3							m_dragBasis;
		btVector3							m_angDragBasis;
		Vector								m_massCenterOverride;
		CShadowController *					m_pShadow;
		CPhysicsFluidController *			m_pFluidController;
};

CPhysicsObject *CreatePhysicsObject(CPhysicsEnvironment *pEnvironment, const CPhysCollide *pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams, bool isStatic);
CPhysicsObject *CreatePhysicsSphere(CPhysicsEnvironment *pEnvironment, float radius, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic);

#endif