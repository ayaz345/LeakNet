//========= Copyright � 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Implements two types of doors: linear and rotating.
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "doors.h"
#include "entitylist.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "engine/IEngineSound.h"


//Default sounds for the various door noises
#define	DEFAULT_DOOR_MOVING_NOISE	"doors/func_door/default_move.wav"
#define	DEFAULT_DOOR_ARRIVE_NOISE	"doors/func_door/default_stop.wav"
#define	DEFAULT_DOOR_LOCKED_NOISE	"doors/func_door/default_locked.wav"
#define	DEFAULT_DOOR_UNLOCKED_NOISE	"common/null.wav"

//Default noises for rotating door noises
#define	DEFAULT_DOOR_ROTATING_MOVING_NOISE		"doors/func_door_rotating/default_move.wav"
#define	DEFAULT_DOOR_ROTATING_ARRIVE_NOISE		"doors/func_door_rotating/default_stop.wav" 
#define	DEFAULT_DOOR_ROTATING_LOCKED_NOISE		"doors/func_door_rotating/default_locked.wav"
#define	DEFAULT_DOOR_ROTATING_UNLOCKED_NOISE	"common/null.wav"


BEGIN_DATADESC( CBaseDoor )

	DEFINE_KEYFIELD( CBaseDoor, m_vecMoveDir, FIELD_VECTOR, "movedir" ),

	DEFINE_FIELD( CBaseDoor, m_bLockedSentence, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_bUnlockedSentence, FIELD_CHARACTER ),	
	DEFINE_KEYFIELD( CBaseDoor, m_NoiseMoving, FIELD_SOUNDNAME, "noise1" ),
	DEFINE_KEYFIELD( CBaseDoor, m_NoiseArrived, FIELD_SOUNDNAME, "noise2" ),
	// DEFINE_FIELD( CBaseDoor, m_ls, locksound_t ),
	DEFINE_KEYFIELD( CBaseDoor, m_ls.sLockedSound, FIELD_SOUNDNAME, "locked_sound" ),
	DEFINE_KEYFIELD( CBaseDoor, m_ls.sUnlockedSound, FIELD_SOUNDNAME, "unlocked_sound" ),
	DEFINE_FIELD( CBaseDoor, m_bLocked, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( CBaseDoor, m_flWaveHeight, FIELD_FLOAT, "WaveHeight" ),
	DEFINE_KEYFIELD( CBaseDoor, m_flBlockDamage, FIELD_FLOAT, "dmg" ),

	DEFINE_INPUTFUNC( CBaseDoor, FIELD_VOID, "Open", InputOpen ),
	DEFINE_INPUTFUNC( CBaseDoor, FIELD_VOID, "Close", InputClose ),
	DEFINE_INPUTFUNC( CBaseDoor, FIELD_VOID, "Toggle", InputToggle ),
	DEFINE_INPUTFUNC( CBaseDoor, FIELD_VOID, "Lock", InputLock ),
	DEFINE_INPUTFUNC( CBaseDoor, FIELD_VOID, "Unlock", InputUnlock ),

	DEFINE_OUTPUT( CBaseDoor, m_OnBlockedOpening, "OnBlockedOpening" ),
	DEFINE_OUTPUT( CBaseDoor, m_OnBlockedClosing, "OnBlockedClosing" ),
	DEFINE_OUTPUT( CBaseDoor, m_OnUnblockedOpening, "OnUnblockedOpening" ),
	DEFINE_OUTPUT( CBaseDoor, m_OnUnblockedClosing, "OnUnblockedClosing" ),
	DEFINE_OUTPUT( CBaseDoor, m_OnFullyClosed, "OnFullyClosed" ),
	DEFINE_OUTPUT( CBaseDoor, m_OnFullyOpen, "OnFullyOpen" ),
	DEFINE_OUTPUT( CBaseDoor, m_OnClose, "OnClose" ),
	DEFINE_OUTPUT( CBaseDoor, m_OnOpen, "OnOpen" ),

	// Function Pointers
	DEFINE_FUNCTION( CBaseDoor, DoorTouch ),
	DEFINE_FUNCTION( CBaseDoor, DoorGoUp ),
	DEFINE_FUNCTION( CBaseDoor, DoorGoDown ),
	DEFINE_FUNCTION( CBaseDoor, DoorHitTop ),
	DEFINE_FUNCTION( CBaseDoor, DoorHitBottom ),

END_DATADESC()


LINK_ENTITY_TO_CLASS( func_door, CBaseDoor );

//
// func_water is implemented as a linear door so we can raise/lower the water level.
//
LINK_ENTITY_TO_CLASS( func_water, CBaseDoor );


// SendTable stuff.
IMPLEMENT_SERVERCLASS_ST(CBaseDoor, DT_BaseDoor)
	SendPropFloat	(SENDINFO(m_flWaveHeight),		8,	SPROP_ROUNDUP,	0.0f,	8.0f),
END_SEND_TABLE()

#define DOOR_SENTENCEWAIT	6
#define DOOR_SOUNDWAIT		3
#define BUTTON_SOUNDWAIT	0.5


//-----------------------------------------------------------------------------
// Purpose: play door or button locked or unlocked sounds. 
//			NOTE: this routine is shared by doors and buttons
// Input  : pEdict - 
//			pls - 
//			flocked - if true, play 'door is locked' sound, otherwise play 'door
//				is unlocked' sound.
//			fbutton - 
//-----------------------------------------------------------------------------
void PlayLockSounds(CBaseEntity *pEdict, locksound_t *pls, int flocked, int fbutton)
{
	if ( pEdict->HasSpawnFlags( SF_DOOR_SILENT ) )
	{
		return;
	}
	float flsoundwait = ( fbutton ) ? BUTTON_SOUNDWAIT : DOOR_SOUNDWAIT;

	if ( flocked )
	{
		int		fplaysound = (pls->sLockedSound != NULL_STRING && gpGlobals->curtime > pls->flwaitSound);
		int		fplaysentence = (pls->sLockedSentence != NULL_STRING && !pls->bEOFLocked && gpGlobals->curtime > pls->flwaitSentence);
		float	fvol = ( fplaysound && fplaysentence ) ? 0.25f : 1.0f;

		// if there is a locked sound, and we've debounced, play sound
		if (fplaysound)
		{
			// play 'door locked' sound
			CPASAttenuationFilter filter( pEdict );
			CBaseEntity::EmitSound( filter, pEdict->entindex(), CHAN_ITEM, (char*)STRING(pls->sLockedSound), fvol, ATTN_NORM);
			pls->flwaitSound = gpGlobals->curtime + flsoundwait;
		}

		// if there is a sentence, we've not played all in list, and we've debounced, play sound
		if (fplaysentence)
		{
			// play next 'door locked' sentence in group
			int iprev = pls->iLockedSentence;
			
			pls->iLockedSentence = SENTENCEG_PlaySequentialSz(	pEdict->edict(), 
																STRING(pls->sLockedSentence), 
																0.85f, 
																SNDLVL_NORM, 
																0, 
																100, 
																pls->iLockedSentence, 
																FALSE);
			pls->iUnlockedSentence = 0;

			// make sure we don't keep calling last sentence in list
			pls->bEOFLocked = (iprev == pls->iLockedSentence);
		
			pls->flwaitSentence = gpGlobals->curtime + DOOR_SENTENCEWAIT;
		}
	}
	else
	{
		// UNLOCKED SOUND

		int fplaysound = (pls->sUnlockedSound != NULL_STRING && gpGlobals->curtime > pls->flwaitSound);
		int fplaysentence = (pls->sUnlockedSentence != NULL_STRING && !pls->bEOFUnlocked && gpGlobals->curtime > pls->flwaitSentence);
		float fvol;

		// if playing both sentence and sound, lower sound volume so we hear sentence
		fvol = ( fplaysound && fplaysentence ) ? 0.25f : 1.0f;

		// play 'door unlocked' sound if set
		if (fplaysound)
		{
			CPASAttenuationFilter filter( pEdict );
			CBaseEntity::EmitSound( filter, pEdict->entindex(), CHAN_ITEM, (char*)STRING(pls->sUnlockedSound), fvol, ATTN_NORM );
			pls->flwaitSound = gpGlobals->curtime + flsoundwait;
		}

		// play next 'door unlocked' sentence in group
		if (fplaysentence)
		{
			int iprev = pls->iUnlockedSentence;
			
			pls->iUnlockedSentence = SENTENCEG_PlaySequentialSz(pEdict->edict(), STRING(pls->sUnlockedSentence), 
					  0.85, SNDLVL_NORM, 0, 100, pls->iUnlockedSentence, FALSE);
			pls->iLockedSentence = 0;

			// make sure we don't keep calling last sentence in list
			pls->bEOFUnlocked = (iprev == pls->iUnlockedSentence);
			pls->flwaitSentence = gpGlobals->curtime + DOOR_SENTENCEWAIT;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Cache user-entity-field values until spawn is called.
// Input  : szKeyName - 
//			szValue - 
// Output : Returns true.
//-----------------------------------------------------------------------------
bool CBaseDoor::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "locked_sentence"))
	{
		m_bLockedSentence = atof(szValue);
	}
	else if (FStrEq(szKeyName, "unlocked_sentence"))
	{
		m_bUnlockedSentence = atof(szValue);
	}
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseDoor::Spawn()
{
	Precache();

#ifdef HL1_DLL
	SetSolid( IsRotatingDoor() ? SOLID_BSP : SOLID_VPHYSICS );
#else
	if ( GetMoveParent() && GetRootMoveParent()->GetSolid() == SOLID_BSP )
	{
		SetSolid( SOLID_BSP );
	}
	else
	{
		SetSolid( SOLID_VPHYSICS );
	}
#endif

	// Convert movedir from angles to a vector
	QAngle angMoveDir = QAngle( m_vecMoveDir.x, m_vecMoveDir.y, m_vecMoveDir.z );
	AngleVectors( angMoveDir, &m_vecMoveDir );

	SetModel( STRING( GetModelName() ) );
	m_vecPosition1	= GetLocalOrigin();
	// Subtract 2 from size because the engine expands bboxes by 1 in all directions making the size too big
	m_vecPosition2	= m_vecPosition1 + (m_vecMoveDir * (fabs( m_vecMoveDir.x * (EntitySpaceSize().x-2) ) + fabs( m_vecMoveDir.y * (EntitySpaceSize().y-2) ) + fabs( m_vecMoveDir.z * (EntitySpaceSize().z-2) ) - m_flLip));
	ASSERTSZ(m_vecPosition1 != m_vecPosition2, "door start/end positions are equal");
	if ( !IsRotatingDoor() )
	{
		if ( HasSpawnFlags(SF_DOOR_START_OPEN) )
		{	// swap pos1 and pos2, put door at pos2
			UTIL_SetOrigin( this, m_vecPosition2);
			m_toggle_state = TS_AT_TOP;
		}
		else
		{
			m_toggle_state = TS_AT_BOTTOM;
		}
	}

	if (HasSpawnFlags(SF_DOOR_LOCKED))
	{
		m_bLocked = true;
	}

	SetMoveType( MOVETYPE_PUSH );
	Relink();
	
	if (m_flSpeed == 0)
	{
		m_flSpeed = 100;
	}
	
	SetTouch( &CBaseDoor::DoorTouch );

	if ( !FClassnameIs( this, "func_water" ) && HasSpawnFlags(SF_DOOR_PASSABLE) )
	{
		//normal door
		AddSolidFlags( FSOLID_NOT_SOLID );
	}
	
	CreateVPhysics();
}
 
//-----------------------------------------------------------------------------
bool CBaseDoor::CreateVPhysics( )
{
	if ( !FClassnameIs( this, "func_water" ) )
	{
		//normal door
		// VXP: NOTE: Create this even when the door is not solid to support constraints.
	//	if ( !IsSolidFlagSet( FSOLID_NOT_SOLID ) )
	//	{
			VPhysicsInitShadow( false, false );
	//	}
	}
	else
	{
		// special contents
		AddSolidFlags( FSOLID_VOLUME_CONTENTS );
		SETBITS( m_spawnflags, SF_DOOR_SILENT );	// water is silent for now

		IPhysicsObject *pPhysics = VPhysicsInitShadow( false, false );
		fluidparams_t fluid;
		
		fluid.density = 1000.f;
		fluid.damping = 0.01f;
		fluid.surfacePlane[0] = 0;
		fluid.surfacePlane[1] = 0;
		fluid.surfacePlane[2] = 1;
		fluid.surfacePlane[3] = GetAbsMaxs().z-1;
		fluid.currentVelocity.Init(0,0,0);
		fluid.torqueFactor = 0.1f;
		fluid.viscosityFactor = 0.01f;
		fluid.pGameData = static_cast<void *>(this);
		physenv->CreateFluidController( pPhysics, &fluid );
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseDoor::Activate( void )
{
	BaseClass::Activate();

	CBaseDoor *pDoorList[64];

	// force movement groups to sync!!!
	int doorCount = GetDoorMovementGroup( pDoorList, ARRAYSIZE(pDoorList) );
	for ( int i = 0; i < doorCount; i++ )
	{
		if ( pDoorList[i]->m_vecMoveDir == m_vecMoveDir )
		{
			bool error = false;
			if ( pDoorList[i]->IsRotatingDoor() )
			{
				error = ( pDoorList[i]->GetLocalAngles() != GetLocalAngles() ) ? true : false;
			}
			else 
			{
				error = ( pDoorList[i]->GetLocalOrigin() != GetLocalOrigin() ) ? true : false;
			}
			if ( error )
			{
				// UNDONE: This should probably fixup m_vecPosition1 & m_vecPosition2
				Warning("Door group %s has misaligned origin!\n", STRING(GetEntityName()) ); // VXP: Happened on d1_town_05 on begin
			}
		}
	}
	
	switch ( m_toggle_state )
	{
	case TS_AT_TOP:
		UpdateAreaPortals( true );
		break;
	case TS_AT_BOTTOM:
		UpdateAreaPortals( false );
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
// This is ONLY used by the node graph to test movement through a door
void CBaseDoor::SetToggleState( int state )
{
	if ( state == TS_AT_TOP )
		UTIL_SetOrigin( this, m_vecPosition2 );
	else
		UTIL_SetOrigin( this, m_vecPosition1 );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseDoor::Precache( void )
{
	//Fill in a default value if necessary
	if ( IsRotatingDoor() )
	{
		UTIL_ValidateSoundName( m_NoiseMoving,		DEFAULT_DOOR_ROTATING_MOVING_NOISE );
		UTIL_ValidateSoundName( m_NoiseArrived,		DEFAULT_DOOR_ROTATING_ARRIVE_NOISE );
		UTIL_ValidateSoundName( m_ls.sLockedSound,	DEFAULT_DOOR_ROTATING_LOCKED_NOISE );
		UTIL_ValidateSoundName( m_ls.sUnlockedSound,DEFAULT_DOOR_ROTATING_UNLOCKED_NOISE );
	}
	else
	{
		UTIL_ValidateSoundName( m_NoiseMoving,		DEFAULT_DOOR_MOVING_NOISE );
		UTIL_ValidateSoundName( m_NoiseArrived,		DEFAULT_DOOR_ARRIVE_NOISE );
		UTIL_ValidateSoundName( m_ls.sLockedSound,	DEFAULT_DOOR_LOCKED_NOISE );
		UTIL_ValidateSoundName( m_ls.sUnlockedSound,DEFAULT_DOOR_UNLOCKED_NOISE );
	}

	//Precache them all
	enginesound->PrecacheSound( (char *) STRING(m_NoiseMoving) );
	enginesound->PrecacheSound( (char *) STRING(m_NoiseArrived) );
	enginesound->PrecacheSound( (char *) STRING(m_ls.sLockedSound) );
	enginesound->PrecacheSound( (char *) STRING(m_ls.sUnlockedSound) );

	//Get sentence group names, for doors which are directly 'touched' to open
	switch (m_bLockedSentence)
	{
		case 1: m_ls.sLockedSentence = AllocPooledString("NA"); break; // access denied
		case 2: m_ls.sLockedSentence = AllocPooledString("ND"); break; // security lockout
		case 3: m_ls.sLockedSentence = AllocPooledString("NF"); break; // blast door
		case 4: m_ls.sLockedSentence = AllocPooledString("NFIRE"); break; // fire door
		case 5: m_ls.sLockedSentence = AllocPooledString("NCHEM"); break; // chemical door
		case 6: m_ls.sLockedSentence = AllocPooledString("NRAD"); break; // radiation door
		case 7: m_ls.sLockedSentence = AllocPooledString("NCON"); break; // gen containment
		case 8: m_ls.sLockedSentence = AllocPooledString("NH"); break; // maintenance door
		case 9: m_ls.sLockedSentence = AllocPooledString("NG"); break; // broken door
		
		default: m_ls.sLockedSentence = NULL_STRING; break;
	}

	switch (m_bUnlockedSentence)
	{
		case 1: m_ls.sUnlockedSentence = AllocPooledString("EA"); break; // access granted
		case 2: m_ls.sUnlockedSentence = AllocPooledString("ED"); break; // security door
		case 3: m_ls.sUnlockedSentence = AllocPooledString("EF"); break; // blast door
		case 4: m_ls.sUnlockedSentence = AllocPooledString("EFIRE"); break; // fire door
		case 5: m_ls.sUnlockedSentence = AllocPooledString("ECHEM"); break; // chemical door
		case 6: m_ls.sUnlockedSentence = AllocPooledString("ERAD"); break; // radiation door
		case 7: m_ls.sUnlockedSentence = AllocPooledString("ECON"); break; // gen containment
		case 8: m_ls.sUnlockedSentence = AllocPooledString("EH"); break; // maintenance door
		
		default: m_ls.sUnlockedSentence = NULL_STRING; break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Doors not tied to anything (e.g. button, another door) can be touched,
//			to make them activate.
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CBaseDoor::DoorTouch( CBaseEntity *pOther )
{
	// Ignore touches by anything but players.
	if ( !pOther->IsPlayer() )
		return;

	// If door is not opened by touch, do nothing.
	if ( !HasSpawnFlags(SF_DOOR_PTOUCH) )
	{
		return; 
	}
	
	// If door has master, and it's not ready to trigger, 
	// play 'locked' sound.
	if (m_sMaster != NULL_STRING && !UTIL_IsMasterTriggered(m_sMaster, pOther))
	{
		PlayLockSounds(this, &m_ls, TRUE, FALSE);
	}

	if (m_bLocked)
	{
		PlayLockSounds(this, &m_ls, TRUE, FALSE);
		return; 
	}
	
	// Remember who activated the door.
	m_hActivator = pOther;

	if (DoorActivate( ))
	{
		// Temporarily disable the touch function, until movement is finished.
		SetTouch( NULL );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : isOpen - 
//-----------------------------------------------------------------------------
void CBaseDoor::UpdateAreaPortals( bool isOpen )
{
	string_t name = GetEntityName();
	if ( !name )
		return;
	
	CBaseEntity *pPortal = NULL;
	while ( ( pPortal = gEntList.FindEntityByClassname( pPortal, "func_areaportal" ) ) != NULL )
	{
		if ( pPortal->HasTarget( name ) )
		{
			// USE_ON means open the portal, off means close it
			pPortal->Use( this, this, isOpen?USE_ON:USE_OFF, 0 );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when the player uses the door.
// Input  : pActivator - 
//			pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CBaseDoor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	// if not ready to be used, ignore "use" command.
	if (m_toggle_state == TS_AT_BOTTOM || (HasSpawnFlags(SF_DOOR_NO_AUTO_RETURN) && m_toggle_state == TS_AT_TOP))
	{
		if (m_bLocked)
		{
			PlayLockSounds(this, &m_ls, TRUE, FALSE);
		}
		else
		{
			DoorActivate();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Closes the door if it is not already closed.
//-----------------------------------------------------------------------------
void CBaseDoor::InputClose( inputdata_t &inputdata )
{
	if ( m_toggle_state != TS_AT_BOTTOM )
	{	
	//	m_OnClose.FireOutput(inputdata.pActivator, this); // VXP: Already in DoorGoDown
		DoorGoDown();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that locks the door.
//-----------------------------------------------------------------------------
void CBaseDoor::InputLock( inputdata_t &inputdata )
{
	Lock();
}


//-----------------------------------------------------------------------------
// Purpose: Opens the door if it is not already open.
//-----------------------------------------------------------------------------
void CBaseDoor::InputOpen( inputdata_t &inputdata )
{
	if (m_toggle_state != TS_AT_TOP && m_toggle_state != TS_GOING_UP )
	{	
		// I'm locked, can't open
		if (m_bLocked)
			return; 

		// Play door unlock sounds.
		PlayLockSounds(this, &m_ls, false, false);
	//	m_OnOpen.FireOutput( inputdata.pActivator, this ); // VXP: Already in DoorGoUp
		DoorGoUp();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Opens the door if it is not already open.
//-----------------------------------------------------------------------------
void CBaseDoor::InputToggle( inputdata_t &inputdata )
{
	// I'm locked, can't open
	if (m_bLocked)
		return; 

	if (m_toggle_state == TS_AT_BOTTOM)
	{	
		DoorGoUp();
	}
	else if (m_toggle_state == TS_AT_TOP)
	{
		DoorGoDown();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that unlocks the door.
//-----------------------------------------------------------------------------
void CBaseDoor::InputUnlock( inputdata_t &inputdata )
{
	Unlock();
}


//-----------------------------------------------------------------------------
// Purpose: Locks the door so that it cannot be opened.
//-----------------------------------------------------------------------------
void CBaseDoor::Lock( void )
{
	m_bLocked = true;
}


//-----------------------------------------------------------------------------
// Purpose: Unlocks the door so that it can be opened.
//-----------------------------------------------------------------------------
void CBaseDoor::Unlock( void )
{
	m_bLocked = false;
}


//-----------------------------------------------------------------------------
// Purpose: Causes the door to "do its thing", i.e. start moving, and cascade activation.
// Output : int
//-----------------------------------------------------------------------------
int CBaseDoor::DoorActivate( )
{
	if (!UTIL_IsMasterTriggered(m_sMaster, m_hActivator))
		return 0;

	if (HasSpawnFlags(SF_DOOR_NO_AUTO_RETURN) && m_toggle_state == TS_AT_TOP)
	{// door should close
		DoorGoDown();
	}
	else
	{// door should open
		// play door unlock sounds
		PlayLockSounds(this, &m_ls, FALSE, FALSE);
		
		if ( m_toggle_state != TS_AT_TOP && m_toggle_state != TS_GOING_UP )
		{
			DoorGoUp();
		}
	}

	return 1;
}


//-----------------------------------------------------------------------------
// Purpose: Starts the door going to its "up" position (simply ToggleData->vecPosition2).
//-----------------------------------------------------------------------------
void CBaseDoor::DoorGoUp( void )
{
	edict_t	*pevActivator;

	UpdateAreaPortals( true );
	// It could be going-down, if blocked.
	ASSERT(m_toggle_state == TS_AT_BOTTOM || m_toggle_state == TS_GOING_DOWN);

	// emit door moving and stop sounds on CHAN_STATIC so that the multicast doesn't
	// filter them out and leave a client stuck with looping door sounds!
	if ( !HasSpawnFlags(SF_DOOR_SILENT ) )
	{
		// If we're not moving already, start the moving noise
		if ( m_toggle_state != TS_GOING_UP && m_toggle_state != TS_GOING_DOWN )
		{
			CPASAttenuationFilter filter( this );
			filter.MakeReliable();
			EmitSound( filter, entindex(), CHAN_STATIC, (char*)STRING(m_NoiseMoving), 1, ATTN_NORM );
		}
	}

	m_toggle_state = TS_GOING_UP;
	
	SetMoveDone( &CBaseDoor::DoorHitTop );
	if ( IsRotatingDoor() )		// !!! BUGBUG Triggered doors don't work with this yet
	{
		float	sign = 1.0;

		if ( m_hActivator != NULL )
		{
			pevActivator = m_hActivator->edict();
			
			if ( !HasSpawnFlags( SF_DOOR_ONEWAY ) && m_vecMoveAng.y ) 		// Y axis rotation, move away from the player
			{
				Vector vec = m_hActivator->GetLocalOrigin() - GetLocalOrigin();
				QAngle angles = m_hActivator->GetLocalAngles();
				angles.x = 0;
				angles.z = 0;
				Vector forward;
				AngleVectors( angles, &forward );
				Vector vnext = (m_hActivator->GetLocalOrigin() + (forward * 10)) - GetLocalOrigin();
				if ( (vec.x*vnext.y - vec.y*vnext.x) < 0 )
					sign = -1.0;
			}
		}
		AngularMove(m_vecAngle2*sign, m_flSpeed);
	}
	else
	{
		LinearMove(m_vecPosition2, m_flSpeed);
	}

	//Fire our open ouput
	m_OnOpen.FireOutput( this, this );
}


//-----------------------------------------------------------------------------
// Purpose: The door has reached the "up" position.  Either go back down, or
//			wait for another activation.
//-----------------------------------------------------------------------------
void CBaseDoor::DoorHitTop( void )
{
	if ( !HasSpawnFlags( SF_DOOR_SILENT ) )
	{
		CPASAttenuationFilter filter( this );
		filter.MakeReliable();
		StopSound( entindex(), CHAN_STATIC, (char*)STRING(m_NoiseMoving) );
		EmitSound( filter, entindex(), CHAN_STATIC, (char*)STRING(m_NoiseArrived), 1, ATTN_NORM );
	}

	ASSERT(m_toggle_state == TS_GOING_UP);
	m_toggle_state = TS_AT_TOP;
	
	// toggle-doors don't come down automatically, they wait for refire.
	if (HasSpawnFlags( SF_DOOR_NO_AUTO_RETURN))
	{
		// Re-instate touch method, movement is complete
		SetTouch( &CBaseDoor::DoorTouch );
	}
	else
	{
		// In flWait seconds, DoorGoDown will fire, unless wait is -1, then door stays open
		SetMoveDoneTime( m_flWait );
		SetMoveDone( &CBaseDoor::DoorGoDown );

		if ( m_flWait == -1 )
		{
			SetNextThink( TICK_NEVER_THINK );
		}
	}

	if (HasSpawnFlags(SF_DOOR_START_OPEN) )
	{
		m_OnFullyClosed.FireOutput(this, this);
	}
	else
	{
		m_OnFullyOpen.FireOutput(this, this);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Starts the door going to its "down" position (simply ToggleData->vecPosition1).
//-----------------------------------------------------------------------------
void CBaseDoor::DoorGoDown( void )
{
	if ( !HasSpawnFlags( SF_DOOR_SILENT ) )
	{
		// If we're not moving already, start the moving noise
		if ( m_toggle_state != TS_GOING_UP && m_toggle_state != TS_GOING_DOWN )
		{
			CPASAttenuationFilter filter( this );
			filter.MakeReliable();
			EmitSound( filter, entindex(), CHAN_STATIC, (char*)STRING(m_NoiseMoving), 1, ATTN_NORM );
		}
	}
	
#ifdef DOOR_ASSERT
	ASSERT(m_toggle_state == TS_AT_TOP);
#endif // DOOR_ASSERT
	m_toggle_state = TS_GOING_DOWN;

	SetMoveDone( &CBaseDoor::DoorHitBottom );
	if ( IsRotatingDoor() )//rotating door
		AngularMove( m_vecAngle1, m_flSpeed);
	else
		LinearMove( m_vecPosition1, m_flSpeed);

	//Fire our closed output
	m_OnClose.FireOutput( this, this );
}


//-----------------------------------------------------------------------------
// Purpose: The door has reached the "down" position.  Back to quiescence.
//-----------------------------------------------------------------------------
void CBaseDoor::DoorHitBottom( void )
{
	if ( !HasSpawnFlags( SF_DOOR_SILENT ) )
	{
		CPASAttenuationFilter filter( this );
		filter.MakeReliable();

		StopSound(entindex(), CHAN_STATIC, (char*)STRING(m_NoiseMoving) );
		EmitSound( filter, entindex(), CHAN_STATIC, (char*)STRING(m_NoiseArrived), 1, ATTN_NORM );
	}

	ASSERT(m_toggle_state == TS_GOING_DOWN);
	m_toggle_state = TS_AT_BOTTOM;

	// Re-instate touch method, cycle is complete
	SetTouch( &CBaseDoor::DoorTouch );

	if (HasSpawnFlags(SF_DOOR_START_OPEN))
	{
		m_OnFullyOpen.FireOutput(m_hActivator, this);
	}
	else
	{
		m_OnFullyClosed.FireOutput(m_hActivator, this);
	}

	UpdateAreaPortals( false );
}


// Lists all doors in the same movement group as this one
int CBaseDoor::GetDoorMovementGroup( CBaseDoor *pDoorList[], int listMax )
{
	int count = 0;
	CBaseEntity	*pTarget = NULL;

	// Block all door pieces with the same targetname here.
	if ( GetEntityName() != NULL_STRING )
	{
		for (;;)
		{
			pTarget = gEntList.FindEntityByName( pTarget, GetEntityName(), NULL );

			if ( pTarget != this )
			{
				if ( !pTarget )
					break;

				CBaseDoor *pDoor = dynamic_cast<CBaseDoor *>(pTarget);

				if ( pDoor && count < listMax )
				{
					pDoorList[count] = pDoor;
					count++;
				}
			}
		}
	}

	return count;
}

//-----------------------------------------------------------------------------
// Purpose: Called the first frame that the door is blocked while opening or closing.
// Input  : pOther - The blocking entity.
//-----------------------------------------------------------------------------
void CBaseDoor::StartBlocked( CBaseEntity *pOther )
{
	//
	// Fire whatever events we need to due to our blocked state.
	//
	if (m_toggle_state == TS_GOING_DOWN)
	{
		m_OnBlockedClosing.FireOutput(pOther, this);
	}
	else
	{
		m_OnBlockedOpening.FireOutput(pOther, this);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called every frame when the door is blocked while opening or closing.
// Input  : pOther - The blocking entity.
//-----------------------------------------------------------------------------
void CBaseDoor::Blocked( CBaseEntity *pOther )
{
	// Hurt the blocker a little.
	if ( m_flBlockDamage )
	{
		pOther->TakeDamage( CTakeDamageInfo( this, this, m_flBlockDamage, DMG_CRUSH ) );
	}

	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast
	if (m_flWait >= 0)
	{
		if (m_toggle_state == TS_GOING_DOWN)
		{
			DoorGoUp();
		}
		else
		{
			DoorGoDown();
		}
	}

	// Block all door pieces with the same targetname here.
	if ( GetEntityName() != NULL_STRING )
	{
		CBaseDoor *pDoorList[64];
		int doorCount = GetDoorMovementGroup( pDoorList, ARRAYSIZE(pDoorList) );

		for ( int i = 0; i < doorCount; i++ )
		{
			CBaseDoor *pDoor = pDoorList[i];

			if ( pDoor->m_flWait >= 0)
			{
				if (pDoor->m_vecMoveDir == m_vecMoveDir && pDoor->GetAbsVelocity() == GetAbsVelocity() && pDoor->GetLocalAngularVelocity() == GetLocalAngularVelocity())
				{
					pDoor->m_nSimulationTick = m_nSimulationTick;	// don't run simulation this frame if you haven't run yet

					// this is the most hacked, evil, bastardized thing I've ever seen. kjb
					if ( !pDoor->IsRotatingDoor() )
					{// set origin to realign normal doors
						pDoor->SetLocalOrigin( GetLocalOrigin() );
						pDoor->SetAbsVelocity( vec3_origin );// stop!

					}
					else
					{// set angles to realign rotating doors
						pDoor->SetLocalAngles( GetLocalAngles() );
						pDoor->SetLocalAngularVelocity( vec3_angle );
					}
					pDoor->Relink();
				}
			
				if ( pDoor->m_toggle_state == TS_GOING_DOWN)
					pDoor->DoorGoUp();
				else
					pDoor->DoorGoDown();
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called the first frame that the door is unblocked while opening or closing.
//-----------------------------------------------------------------------------
void CBaseDoor::EndBlocked( void )
{
	//
	// Fire whatever events we need to due to our unblocked state.
	//
	if (m_toggle_state == TS_GOING_DOWN)
	{
		m_OnUnblockedClosing.FireOutput(this, this);
	}
	else
	{
		m_OnUnblockedOpening.FireOutput(this, this);
	}
}


/*func_door_rotating

TOGGLE causes the door to wait in both the start and end states for  
a trigger event.

START_OPEN causes the door to move to its destination when spawned,  
and operate in reverse.  It is used to temporarily or permanently  
close off an area when triggered (not usefull for touch or  
takedamage doors).

You need to have an origin brush as part of this entity.  The  
center of that brush will be
the point around which it is rotated. It will rotate around the Z  
axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"distance" is how many degrees the door will be rotated.
"speed" determines how fast the door moves; default value is 100.

REVERSE will cause the door to rotate in the opposite direction.

"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote  
button or trigger field activates the door.
"health"	if set, door must be shot open
"speed"		movement speed (100 default)
"wait"		wait before returning (3 default, -1 = never return)
"dmg"		damage to inflict when blocked (2 default)
*/

//==================================================
// CRotDoor 
//==================================================

class CRotDoor : public CBaseDoor
{
public:
	DECLARE_CLASS( CRotDoor, CBaseDoor );

	void Spawn( void );
	bool CreateVPhysics();
	// This is ONLY used by the node graph to test movement through a door
	virtual void SetToggleState( int state );
	virtual bool IsRotatingDoor() { return true; }
};

LINK_ENTITY_TO_CLASS( func_door_rotating, CRotDoor );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRotDoor::Spawn( void )
{

	// FIXME: we should be calling BaseClass::Spawn!
	BaseClass::Spawn();

	// set the axis of rotation
	CBaseToggle::AxisDir();

	// check for clockwise rotation
	if ( HasSpawnFlags(SF_DOOR_ROTATE_BACKWARDS) )
		m_vecMoveAng = m_vecMoveAng * -1;
	
	//m_flWait			= 2; who the hell did this? (sjb)
	m_vecAngle1	= GetLocalAngles();
	m_vecAngle2	= GetLocalAngles() + m_vecMoveAng * m_flMoveDistance;

	ASSERTSZ(m_vecAngle1 != m_vecAngle2, "rotating door start/end positions are equal");
	
	// DOOR_START_OPEN is to allow an entity to be lighted in the closed position
	// but spawn in the open position
	if ( HasSpawnFlags(SF_DOOR_START_OPEN) )
	{	
		// swap pos1 and pos2, put door at pos2, invert movement direction
		QAngle vecNewAngles = m_vecAngle2;
		m_vecAngle2 = m_vecAngle1;
		m_vecAngle1 = vecNewAngles;
		m_vecMoveAng = -m_vecMoveAng;

		// We've already had our physics setup in BaseClass::Spawn, so teleport to our
		// current position. If we don't do this, our vphysics shadow will not update.
		Teleport( NULL, &m_vecAngle1, NULL );
		m_toggle_state = TS_AT_BOTTOM;
	}
	else
	{
		m_toggle_state = TS_AT_BOTTOM;
	}

	Relink();
}

//-----------------------------------------------------------------------------

bool CRotDoor::CreateVPhysics()
{
	if ( !IsSolidFlagSet( FSOLID_NOT_SOLID ) )
	{
		VPhysicsInitShadow( false, false );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
// This is ONLY used by the node graph to test movement through a door
void CRotDoor::SetToggleState( int state )
{
	if ( state == TS_AT_TOP )
		SetLocalAngles( m_vecAngle2 );
	else
		SetLocalAngles( m_vecAngle1 );

	Relink();
}
