//
// Copyright (c) 2003-2009, by Yet Another POD-Bot Development Team.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// $Id:$
//

#include "../include/core.h"

ConVar ebot_zombie_wall_hack("ebot_zombie_wall_hack", "0");
ConVar ebot_dark_mode("ebot_dark_mode", "0");

int Bot::GetNearbyFriendsNearPosition(const Vector& origin, const float radius)
{
	int count = 0;
	for (const Clients& client : g_clients)
	{
		if (!(client.flags & CFLAG_USED) || !(client.flags & CFLAG_ALIVE) || client.team != m_team || client.ent == GetEntity())
			continue;

		if ((client.origin - origin).GetLengthSquared() < radius)
			count++;
	}

	return count;
}

int Bot::GetNearbyEnemiesNearPosition(const Vector& origin, const float radius)
{
	int count = 0;
	for (const Clients& client : g_clients)
	{
		if (!(client.flags & CFLAG_USED) || !(client.flags & CFLAG_ALIVE) || client.team == m_team)
			continue;

		if ((client.origin - origin).GetLengthSquared() < radius)
			count++;
	}

	return count;
}

inline float GetDistance(const int16_t& start, const int16_t& goal)
{
	if (!IsValidWaypoint(start) || !IsValidWaypoint(goal))
		return 999999999.0f;

	if (g_isMatrixReady)
	{
		const int16_t dist = *(g_waypoint->m_distMatrix.Get() + (start * g_numWaypoints) + goal);
		if (dist >= 32766)
			return GetVectorDistanceSSE(g_waypoint->m_paths[start].origin, g_waypoint->m_paths[goal].origin);

		return static_cast<float>(dist);
	}

	return GetVectorDistanceSSE(g_waypoint->m_paths[start].origin, g_waypoint->m_paths[goal].origin);
}

void Bot::FindFriendsAndEnemiens(void)
{
	m_numEnemiesLeft = 0;
	m_numFriendsLeft = 0;
	m_numFriendsNear = 0;
	m_hasEnemiesNear = false;
	m_hasFriendsNear = false;
	m_enemyDistance = 999999.0f;
	m_friendDistance = 999999.0f;
	if (g_roundEnded)
	{
		m_nearestEnemy = nullptr;
		m_nearestFriend = nullptr;
		return;
	}

	float distance;
	TraceResult tr;
	const Vector myOrigin = EyePosition();
	int16_t myWP = m_currentWaypointIndex;
	if (!IsValidWaypoint(myWP))
		myWP = g_clients[m_index].wp;

	if (m_isZombieBot)
	{
		const bool needTarget = (!IsAlive(m_nearestEnemy) || GetTeam(m_nearestEnemy) == m_team);
		for (const Clients& client : g_clients)
		{
			if (client.ignore)
				continue;

			if (!(client.flags & CFLAG_USED))
				continue;

			if (!(client.flags & CFLAG_ALIVE))
				continue;

			if (client.ent == GetEntity())
				continue;

			if (!IsAlive(client.ent))
				continue;

			if (client.ent->v.flags & FL_NOTARGET)
				continue;

			if (client.team == m_team)
			{
				m_numFriendsLeft++;
				distance = GetDistance(myWP, client.wp);
				if (distance < m_friendDistance)
				{
					// simple check
					TraceLine(myOrigin, client.ent->v.origin + client.ent->v.view_ofs, TraceIgnore::Everything, GetEntity(), &tr);
					if (tr.flFraction < 1.0f)
						continue;
					
					m_numFriendsNear++;
					m_friendDistance = distance;
					m_nearestFriend = client.ent;
					m_hasFriendsNear = true;
				}
			}
			else
			{
				m_numEnemiesLeft++;
				distance = GetDistance(myWP, client.wp);
				if (distance < m_enemyDistance)
				{
					if (IsEnemyInvincible(client.ent))
						continue;

					if (IsEnemyHidden(client.ent))
						continue;

					if (!CheckVisibility(client.ent))
					{
						if (needTarget && ebot_zombie_wall_hack.GetBool())
							m_nearestEnemy = client.ent;

						continue;
					}

					m_enemyDistance = distance;
					if (needTarget || m_enemyDistance < GetDistance(myWP, g_clients[ENTINDEX(m_nearestEnemy) - 1].wp))
						m_nearestEnemy = client.ent;

					m_nearestEnemy = client.ent;
					m_hasEnemiesNear = true;
				}
			}
		}
	}
	else
	{
		for (const Clients& client : g_clients)
		{
			if (client.ignore)
				continue;
			
			if (!(client.flags & CFLAG_USED))
				continue;

			if (!(client.flags & CFLAG_ALIVE))
				continue;

			if (client.ent == GetEntity())
				continue;

			if (!IsAlive(client.ent))
				continue;

			if (client.ent->v.flags & FL_NOTARGET)
				continue;

			if (client.team == m_team)
			{
				m_numFriendsLeft++;
				distance = GetDistance(myWP, client.wp);
				if (distance < m_friendDistance)
				{
					// simple check
					TraceLine(myOrigin, client.ent->v.origin + client.ent->v.view_ofs, TraceIgnore::Everything, GetEntity(), &tr);
					if (tr.flFraction < 1.0f)
						continue;
					
					m_numFriendsNear++;
					m_friendDistance = distance;
					m_nearestFriend = client.ent;
					m_hasFriendsNear = true;
				}
			}
			else
			{
				m_numEnemiesLeft++;
				distance = GetDistance(myWP, client.wp);
				if (distance < m_enemyDistance)
				{
					if (IsEnemyInvincible(client.ent))
						continue;

					if (IsEnemyHidden(client.ent))
						continue;

					if (!CheckVisibility(client.ent))
						continue;

					// we don't know this entity
					if (client.ent != m_nearestEnemy)
					{
						if (ebot_dark_mode.GetBool() && !IsAttacking(client.ent) && !IsInViewCone(client.ent->v.origin + client.ent->v.view_ofs))
							continue;
					}

					m_enemyDistance = distance;
					m_nearestEnemy = client.ent;
					m_hasEnemiesNear = true;
				}
			}
		}
	}

	if (m_hasEnemiesNear)
	{
		m_enemySeeTime = engine->GetTime();

		if (!FNullEnt(m_nearestEnemy)) // we need 2d distance
			m_enemyOrigin = GetPlayerHeadOrigin(m_nearestEnemy, (myOrigin - m_nearestEnemy->v.origin).GetLengthSquared(), m_currentWeapon, m_skill, (pev->punchangle.x < -1.45f));
	}
	else
		m_isEnemyReachable = false;

	if (m_hasFriendsNear)
	{
		m_friendSeeTime = engine->GetTime();

		if (!FNullEnt(m_nearestFriend))
			m_friendOrigin = m_nearestFriend->v.origin;
	}
}

void Bot::FindEnemyEntities(void)
{
	m_numEntitiesLeft = 0;
	m_hasEntitiesNear = false;
	m_entityDistance = 999999.0f;
	if (g_roundEnded)
	{
		m_nearestEntity = nullptr;
		return;
	}

	int i;
	Vector origin;
	float distance;
	edict_t* entity;
	TraceResult tr{};
	const Vector myOrigin = EyePosition();

	for (i = 0; i < g_entities.Size(); i++)
	{
		entity = INDEXENT(g_entities[i]);
		if (!IsAlive(entity) || entity->v.flags & FL_NOTARGET || entity->v.effects & EF_NODRAW || entity->v.takedamage == DAMAGE_NO)
			continue;

		m_numEntitiesLeft++;
		m_entityOrigin = GetBoxOrigin(entity);
		distance = (myOrigin - m_entityOrigin).GetLengthSquared();
		if (distance < m_entityDistance)
		{
			TraceLine(myOrigin, m_entityOrigin, TraceIgnore::Everything, GetEntity(), &tr);
			if (tr.flFraction < 1.0f)
			{
				if (!FNullEnt(tr.pHit) && tr.pHit != entity)
					continue;
			}

			m_entityDistance = distance;
			m_nearestEntity = entity;
			m_hasEntitiesNear = true;
		}
	}

	if (m_hasEntitiesNear)
	{
		m_entityDistance = csqrtf(m_entityDistance);
		m_entitySeeTime = engine->GetTime();
	}
}

// this function will return true if weapon was fired, false otherwise
void Bot::FireWeapon(const float distance)
{
	if (m_isZombieBot)
		return;
	
	char i;
	WeaponSelect* selectTab = &g_weaponSelect[0];
	if (m_isSlowThink)
	{
		char id, chosenWeaponIndex = -1;
		for (i = Const_NumWeapons; i; i--)
		{
			id = selectTab[i].id;
			if (pev->weapons & (1 << id))
			{
				if (m_ammoInClip[id] && !IsWeaponBadInDistance(id, distance))
				{
					chosenWeaponIndex = i;
					break;
				}
			}
		}

		if (chosenWeaponIndex == -1)
		{
			for (i = Const_NumWeapons; i; i--)
			{
				id = selectTab[i].id;
				if (pev->weapons & (1 << id))
				{
					if (m_ammo[g_weaponDefs[id].ammo1] && !IsWeaponBadInDistance(id, distance))
					{
						chosenWeaponIndex = i;
						break;
					}
				}
			}
		}

		if (chosenWeaponIndex == -1)
		{
			for (i = Const_NumWeapons; i; i--)
			{
				id = selectTab[i].id;
				if (pev->weapons & (1 << id))
				{
					if (m_ammo[g_weaponDefs[id].ammo1])
					{
						chosenWeaponIndex = i;
						break;
					}
				}
			}
		}

		if (chosenWeaponIndex == -1)
			chosenWeaponIndex = GetHighestWeapon();

		if (chosenWeaponIndex == -1)
		{
			if (m_currentWeapon == Weapon::Knife)
			{
				KnifeAttack();
				return;
			}

			SelectKnife();
			return;
		}

		if (m_currentWeapon != selectTab[chosenWeaponIndex].id)
		{
			SelectWeaponByName(selectTab[chosenWeaponIndex].weaponName);
			return;
		}

		if (GetAmmoInClip() <= 0)
		{
			chosenWeaponIndex = GetHighestWeapon();
			if (chosenWeaponIndex != -1 && m_currentWeapon != selectTab[chosenWeaponIndex].id)
				SelectWeaponByName(selectTab[chosenWeaponIndex].weaponName);
			else if (m_currentWeapon == Weapon::Knife)
				KnifeAttack();
			else
				m_buttons |= IN_RELOAD;
		}

		return;
	}

	if (UsesSniper())
	{
		if (m_zoomCheckTime < engine->GetTime())
		{
			if (pev->fov >= 40.0f && distance > 1500.0f) // should the bot switch to the long-range zoom?
				m_buttons |= IN_ATTACK2;
			else if (pev->fov >= 90.0f && distance > 250.0f) // else should the bot switch to the close-range zoom ?
				m_buttons |= IN_ATTACK2;
			else if (pev->fov < 90.0f && distance < 250.0f) // else should the bot restore the normal view ?
				m_buttons |= IN_ATTACK2;
			
			m_zoomCheckTime = engine->GetTime() + crandomfloat(0.75f, 1.25f);
		}
		else if (!(m_buttons & IN_ATTACK) && !(m_oldButtons & IN_ATTACK) && !(m_buttons & IN_ATTACK2) && !(m_oldButtons & IN_ATTACK2))
			m_buttons |= IN_ATTACK;

		return;
	}

	if (m_currentWeapon == Weapon::Knife)
		KnifeAttack();
	else if (m_firePause < engine->GetTime())
	{
		i = 0;
		while (m_currentWeapon != selectTab[i].id && i < Const_NumWeapons)
			i++;

		if (selectTab[i].primaryFireHold) // if automatic weapon, just press attack
		{
			m_buttons |= IN_ATTACK;
			if (distance > 300.0f)
			{
				const float xPunch = squaredf(pev->punchangle.x * 0.01745329252f);
				const float yPunch = squaredf(pev->punchangle.y * 0.01745329252f);
				const float difficulty = static_cast<float>(m_skill);
				const float tolerance = (100.0f - (difficulty * 0.25f)) / 99.0f;
				const float baseTime = 0.55f;
				const float maxRecoil = 5.0f;

				if (ctanf(csqrtf(cabsf(xPunch) + cabsf(yPunch))) * distance > (25.0f + maxRecoil + tolerance))
				{
					if (m_firePause < engine->GetTime())
					{
						float pauseMultiplier = crandomfloat(baseTime, baseTime + maxRecoil * 0.01f * tolerance) - m_frameInterval;
						m_firePause = engine->GetTime() + pauseMultiplier;
					}
				}
			}
		}
		else // if not, toggle the buttons
		{
			if (!(m_buttons & IN_ATTACK) && !(m_oldButtons & IN_ATTACK))
				m_buttons |= IN_ATTACK;
		}
	}
}

void Bot::KnifeAttack(void)
{
	edict_t* entity = nullptr;
	float distance = 9999.0f;

	if (m_hasEnemiesNear && !FNullEnt(m_nearestEnemy))
	{
		entity = m_nearestEnemy;
		distance = (pev->origin - GetEntityOrigin(m_nearestEnemy)).GetLengthSquared2D();
	}

	if (!FNullEnt(m_breakableEntity) && m_breakableOrigin != nullvec)
	{
		const float breakableDist = (pev->origin - m_breakableOrigin).GetLengthSquared2D();
		if (breakableDist < distance)
		{
			entity = m_breakableEntity;
			distance = breakableDist;
		}
	}

	if (FNullEnt(entity))
		return;

	constexpr float kad1 = 64.0f * 64.0f;
	constexpr float kad2 = 64.0f * 64.0f;

	int kaMode = 0;
	if (distance < kad1)
		kaMode = 1;
	if (distance < kad2)
		kaMode += 2;

	if (kaMode > 0)
	{
		const Vector entityOrigin = GetEntityOrigin(entity);
		const float distanceSkipZ = (pev->origin - entityOrigin).GetLengthSquared2D();
		if (distanceSkipZ < squaredf(64.0f) && pev->origin.z > entityOrigin.z)
		{
			m_duckTime = engine->GetTime() + 1.0f;
			m_buttons &= ~IN_JUMP;
		}
		else
		{
			m_duckTime = 0.0f;
			if (IsOnFloor() && chanceof(15) && pev->origin.z + 150.0f < entityOrigin.z && distanceSkipZ < squaredf(300.0f))
				m_buttons |= IN_JUMP;
		}

		if (kaMode == 1)
			m_buttons |= IN_ATTACK;
		else if (kaMode == 2)
			m_buttons |= IN_ATTACK2;
		else if (chanceof(30))
			m_buttons |= IN_ATTACK;
		else
			m_buttons |= IN_ATTACK2;
	}
}

// this function checks, is it better to use pistol instead of current primary weapon
// to attack our enemy, since current weapon is not very good in this situation
bool Bot::IsWeaponBadInDistance(const int weaponIndex, const float distance)
{
	// shotguns is too inaccurate at long distances, so weapon is bad
	if ((weaponIndex == Weapon::M3 || weaponIndex == Weapon::Xm1014) && (pev->waterlevel > 2 || (distance > (weaponIndex == m_currentWeapon ? 768.0f : 512.0f))))
		return true;

	return false;
}

bool Bot::UsesShotgun(void)
{
	return m_currentWeapon == Weapon::M3 || m_currentWeapon == Weapon::Xm1014;
}

bool Bot::UsesSniper(void)
{
	return m_currentWeapon == Weapon::Awp || m_currentWeapon == Weapon::G3SG1 || m_currentWeapon == Weapon::Scout || m_currentWeapon == Weapon::Sg550;
}

bool Bot::IsSniper(void)
{
	if (pev->weapons & (1 << Weapon::Awp))
		return true;
	else if (pev->weapons & (1 << Weapon::G3SG1))
		return true;
	else if (pev->weapons & (1 << Weapon::Scout))
		return true;
	else if (pev->weapons & (1 << Weapon::Sg550))
		return true;

	return false;
}

bool Bot::UsesRifle(void)
{
	WeaponSelect* selectTab = &g_weaponSelect[0];
	int count = 0;

	while (selectTab->id)
	{
		if (m_currentWeapon == selectTab->id)
			break;

		selectTab++;
		count++;
	}

	if (selectTab->id && count > 13)
		return true;

	return false;
}

bool Bot::UsesPistol(void)
{
	WeaponSelect* selectTab = &g_weaponSelect[0];
	int count = 0;

	// loop through all the weapons until terminator is found
	while (selectTab->id)
	{
		if (m_currentWeapon == selectTab->id)
			break;

		selectTab++;
		count++;
	}

	if (selectTab->id && count < 7)
		return true;

	return false;
}

bool Bot::UsesSubmachineGun(void)
{
	return m_currentWeapon == Weapon::Mp5 || m_currentWeapon == Weapon::Tmp || m_currentWeapon == Weapon::P90 || m_currentWeapon == Weapon::Mac10 || m_currentWeapon == Weapon::Ump45;
}

bool Bot::UsesZoomableRifle(void)
{
	return m_currentWeapon == Weapon::Aug || m_currentWeapon == Weapon::Sg552;
}

bool Bot::UsesBadPrimary(void)
{
	return m_currentWeapon == Weapon::M3 || m_currentWeapon == Weapon::Ump45 || m_currentWeapon == Weapon::Mac10 || m_currentWeapon == Weapon::Tmp || m_currentWeapon == Weapon::P90;
}

int Bot::CheckGrenades(void)
{
	if (pev->weapons & (1 << Weapon::HeGrenade))
		return Weapon::HeGrenade;
	else if (pev->weapons & (1 << Weapon::FbGrenade))
		return Weapon::FbGrenade;
	return -1;
}

void Bot::SelectKnife(void)
{
	if (m_currentWeapon == Weapon::Knife)
		return;

	SelectWeaponByName("weapon_knife");
	m_currentWeapon = Weapon::Knife;
}

void Bot::SelectBestWeapon(void)
{
	if (m_isZombieBot)
	{
		SelectKnife();
		return;
	}

	if (!m_isSlowThink)
		return;
	
	int i, id;
	const WeaponSelect* selectTab = &g_weaponSelect[0];
	int chosenWeaponIndex = -1;
	for (i = Const_NumWeapons; i; i--)
	{
		id = selectTab[i].id;
		if (pev->weapons & (1 << id))
		{
			if (m_ammo[g_weaponDefs[id].ammo1])
			{
				chosenWeaponIndex = i;
				break;
			}
		}
	}

	if (chosenWeaponIndex != -1)
		SelectWeaponByName(selectTab[chosenWeaponIndex].weaponName);
	else
	{
		chosenWeaponIndex = GetHighestWeapon();
		if (chosenWeaponIndex != -1)
			SelectWeaponByName(selectTab[chosenWeaponIndex].weaponName);
		else
			SelectWeaponByName("weapon_knife");
	}
}

int Bot::GetHighestWeapon(void)
{
	int i;
	const WeaponSelect* selectTab = &g_weaponSelect[0];
	for (i = Const_NumWeapons; i; i--)
	{
		if (pev->weapons & (1 << selectTab[i].id))
			return i;
	}

	return -1;
}

static float wpnTimer = 0.05;
void Bot::SelectWeaponByName(const char* name)
{
	if (wpnTimer < engine->GetTime())
	{
		FakeClientCommand(GetEntity(), name);
		wpnTimer = engine->GetTime() + 0.05;
	}
}
