#include "../../include/core.h"
ConVar ebot_kill_breakables("ebot_kill_breakables", "0");

void Bot::DestroyBreakableStart(void)
{
	if (m_isZombieBot)
		SelectKnife();
	else
		SelectBestWeapon();
}

void Bot::DestroyBreakableUpdate(void)
{
	if (!DestroyBreakableReq())
	{
		FinishCurrentProcess("sucsessfully destroyed a breakable");
		return;
	}

	if (ebot_kill_breakables.GetBool())
		m_breakableEntity->v.health = -1.0f;

	LookAt(m_breakableOrigin);
	const bool visible = IsVisible(m_breakableOrigin, GetEntity());
	const float distance = (pev->origin - m_breakableOrigin).GetLengthSquared();

	if (pev->origin.z > m_breakableOrigin.z)
		m_duckTime = engine->GetTime() + 1.0f;
	else if (!visible)
		m_duckTime = engine->GetTime() + 1.0f;

	if (!m_isZombieBot && m_currentWeapon != Weapon::Knife && visible)
	{
		m_moveSpeed = 0.0f;
		m_strafeSpeed = 0.0f;
		FireWeapon(distance);
		m_buttons |= IN_ATTACK;
	}
	else
	{
		MoveTo(m_breakableOrigin, true);
		if (m_isZombieBot)
			SelectKnife();
		else
			SelectBestWeapon();
		m_buttons |= IN_ATTACK;
	}

	IgnoreCollisionShortly();
	m_pauseTime = engine->GetTime() + crandomfloat(0.12f, 0.35f);
}

void Bot::DestroyBreakableEnd(void)
{
	m_breakableEntity = nullptr;
}

bool Bot::DestroyBreakableReq(void)
{
	if (FNullEnt(m_breakableEntity))
		return false;

	if (m_breakableEntity->v.health <= 0.0f)
		return false;

	if (m_breakableEntity->v.takedamage == DAMAGE_NO)
		return false;

	if ((pev->origin - m_breakableOrigin).GetLengthSquared() > squaredf(1536.0f))
		return false;

	return true;
}
