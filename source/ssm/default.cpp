#include "../../include/core.h"

void Bot::DefaultStart(void)
{

}

void Bot::DefaultUpdate(void)
{
	if (m_isZombieBot)
	{
		m_aimingAtEnemy = false;

		// nearest enemy never resets to nullptr, so bot always know where are alive humans
		if (IsAlive(m_nearestEnemy) && GetTeam(m_nearestEnemy) != m_team)
		{
			int16_t enemyWaypoint = g_clients[ENTINDEX(m_nearestEnemy) - 1].wp;
			if (!IsValidWaypoint(enemyWaypoint))
				enemyWaypoint = g_waypoint->FindNearest(m_nearestEnemy->v.origin);

			if (m_isSlowThink)
			{
				CheckReachable();
				if (!CheckVisibility(m_nearestEnemy))
					m_isEnemyReachable = false;

				if (IsValidWaypoint(enemyWaypoint) && enemyWaypoint != m_currentGoalIndex)
				{
					m_currentGoalIndex = enemyWaypoint;
					FindPath(m_currentWaypointIndex, enemyWaypoint);
				}
			}
			else if (m_enemyDistance < 512.0f && chanceof(m_skill))
				KnifeAttack();

			// path matrix returns 0 if we are on the same waypoint, so basically its reachable
			if (m_hasEnemiesNear && (Math::FltZero(m_enemyDistance) || m_isEnemyReachable))
			{
				Vector nextVec;
				nextVec.x = m_enemyOrigin.x + m_nearestEnemy->v.velocity.x;
				nextVec.y = m_enemyOrigin.y + m_nearestEnemy->v.velocity.y;
				nextVec.z = m_enemyOrigin.z;
				if ((pev->origin - m_enemyOrigin).GetLengthSquared2D() < (pev->origin - nextVec).GetLengthSquared2D())
					MoveTo(nextVec);
				else
					MoveTo(m_enemyOrigin);

				m_aimingAtEnemy = true;
				LookAt(m_enemyOrigin);

				if (m_isSlowThink)
				{
					m_navNode.Clear();
					FindEnemyEntities();
					FindFriendsAndEnemiens();
					FindWaypoint();
				}
				else
					KnifeAttack();

				m_navNode.Stop();
				return;
			}
			else if (!m_navNode.IsEmpty())
			{
				if (m_isSlowThink && m_navNode.HasNext() && (g_waypoint->m_paths[m_navNode.Last()].origin - pev->origin).GetLengthSquared() < (g_waypoint->m_paths[m_navNode.Last()].origin - m_nearestEnemy->v.origin).GetLengthSquared())
				{
					KnifeAttack();
					if (IsValidWaypoint(enemyWaypoint))
					{
						m_currentGoalIndex = enemyWaypoint;
						FindPath(m_currentWaypointIndex, enemyWaypoint);
					}
					else
					{
						int16_t index = g_waypoint->FindNearest(m_nearestEnemy->v.origin);
						if (IsValidWaypoint(index))
						{
							m_currentGoalIndex = index;
							FindPath(m_currentWaypointIndex, index);
						}
					}
				}
				else
					FollowPath();
			}
			else
			{
				if (IsValidWaypoint(enemyWaypoint))
				{
					m_currentGoalIndex = enemyWaypoint;
					FindPath(m_currentWaypointIndex, enemyWaypoint);
				}
				else
				{
					int16_t index = g_waypoint->FindNearest(m_nearestEnemy->v.origin);
					if (IsValidWaypoint(index))
					{
						m_currentGoalIndex = index;
						FindPath(m_currentWaypointIndex, index);
					}
					else
					{
						index = static_cast<int16_t>(crandomint(0, g_numWaypoints - 1));
						m_currentGoalIndex = index;
						FindPath(m_currentWaypointIndex, index);
					}
				}
			}
		}
		else
		{
			m_nearestEnemy = nullptr;
			m_hasEnemiesNear = false;
			m_isEnemyReachable = false;

			// search other bots to get valid enemy
			if (m_isSlowThink)
			{
				for (auto bot : g_botManager->m_bots)
				{
					if (bot && bot->m_team == m_team && bot->m_hasEnemiesNear && IsAlive(bot->m_nearestEnemy) && GetTeam(bot->m_nearestEnemy) != m_team)
					{
						m_nearestEnemy = bot->m_nearestEnemy;
						return;
					}
				}
			}

			if (!m_navNode.IsEmpty() && IsValidWaypoint(m_currentGoalIndex) && m_currentWaypointIndex == m_currentGoalIndex)
				m_navNode.Clear();

			if (!m_navNode.IsEmpty() && IsValidWaypoint(m_navNode.First()) && m_navNode.First() == m_currentWaypointIndex && !m_navNode.HasNext())
				m_navNode.Clear();

			if (!m_navNode.IsEmpty())
				FollowPath();
			else
			{
				KnifeAttack();
				int16_t ref = FindGoalZombie();
				if (IsValidWaypoint(ref) && ref != m_currentWaypointIndex)
					FindPath(m_currentWaypointIndex, ref);
			}
		}

		if (m_isSlowThink)
		{
			m_zhCampPointIndex = -1;

			FindEnemyEntities();
			FindFriendsAndEnemiens();
		}
		else
			UpdateLooking();
	}
		else
		{
			UpdateLooking();

			if (IsValidWaypoint(m_zhCampPointIndex))
				m_currentGoalIndex = m_zhCampPointIndex;

			if (m_isSlowThink)
			{
				FindEnemyEntities();
				FindFriendsAndEnemiens();
			CheckReachable();

			// revert the zoom to normal
			if (!m_hasEnemiesNear && !m_hasEntitiesNear && UsesSniper() && pev->fov != 90.0f)
				m_buttons |= IN_ATTACK2;
		}
		else if (m_hasEnemiesNear && (m_isEnemyReachable || Math::FltZero(m_enemyDistance)))
		{
			if (!m_navNode.HasNext())
			{
				// find new safe spot
				m_myMeshWaypoint = -1;
				m_zhCampPointIndex = -1;
				FindGoalHuman();

				// use known waypoint first, then switch to auto
				FindEscapePath(m_currentWaypointIndex, m_enemyOrigin);

				if (m_navNode.IsEmpty())
				{
					MoveOut(m_enemyOrigin);
					m_navNode.Stop();
				}
				else
					FollowPath();

				m_currentWaypointIndex = -1;
			}
			else if (m_navNode.Length() > 1)
			{
				const int16_t firstWP = m_navNode.First();
				const int16_t nextWP = m_navNode.Next();
				if (IsValidWaypoint(firstWP) && IsValidWaypoint(nextWP) && ((pev->origin - g_waypoint->m_paths[firstWP].origin).GetLengthSquared() > ((m_nearestEnemy->v.origin + m_nearestEnemy->v.velocity) - g_waypoint->m_paths[firstWP].origin).GetLengthSquared() || (pev->origin - g_waypoint->m_paths[nextWP].origin).GetLengthSquared() > ((m_nearestEnemy->v.origin + m_nearestEnemy->v.velocity) - g_waypoint->m_paths[nextWP].origin).GetLengthSquared()) && ::IsInViewCone(pev->origin, m_nearestEnemy))
				{
					// find new safe spot if possible
					m_myMeshWaypoint = -1;
					m_zhCampPointIndex = -1;
					FindGoalHuman();

					m_navNode.Clear();
					FindEscapePath(m_currentWaypointIndex, m_enemyOrigin);
					MoveOut(m_enemyOrigin);
					m_navNode.Stop();

					return;
				}
			}

			// if our enemy is closer to this waypoint, just skip it otherwise we will get infected
			const int16_t firstWP = m_navNode.First();
			if (IsValidWaypoint(firstWP) && (g_waypoint->m_paths[firstWP].origin - m_enemyOrigin).GetLengthSquared() < (g_waypoint->m_paths[firstWP].origin - pev->origin).GetLengthSquared())
				m_navNode.Shift();

			if (m_navNode.IsEmpty())
			{
				MoveOut(m_enemyOrigin);
				m_navNode.Stop();
			}
			else
				FollowPath();

			return;
		}

		if (m_currentWaypointIndex == m_zhCampPointIndex && IsValidWaypoint(m_zhCampPointIndex))
		{
			if (!m_navNode.IsEmpty())
			{
				FollowPath();
				return;
			}

			const Path zhPath = g_waypoint->m_paths[m_zhCampPointIndex];
			if (!(zhPath.flags & WAYPOINT_ZMHMCAMP) && !(zhPath.flags & WAYPOINT_HMCAMPMESH) && !(zhPath.flags & WAYPOINT_HUMANHIGHSPOT))
			{
				m_moveSpeed = pev->maxspeed;
				m_zhCampPointIndex = -1;
				m_myMeshWaypoint = -1;
				FindGoalHuman();
				return;
			}

			if (m_isSlowThink)
			{
				const float maxRange = zhPath.flags & WAYPOINT_CROUCH ? 125.0f : 200.0f;
				if (((zhPath.origin - pev->origin).GetLengthSquared2D() > squaredf(maxRange) || (zhPath.origin.z - 54.0f > pev->origin.z)))
				{
					FindWaypoint();
					if (m_currentWaypointIndex == m_currentGoalIndex)
						MoveTo(zhPath.origin);
					else
						FindPath(m_currentWaypointIndex, m_currentGoalIndex);
					return;
				}
			}
			else if (zhPath.flags & WAYPOINT_CROUCH)
				m_duckTime = engine->GetTime() + 1.0f;

			m_moveSpeed = 0.0f;
			m_strafeSpeed = 0.0f;

			ResetStuck();

			if (!g_waypoint->m_hmMeshPoints.IsEmpty())
			{
				const float time2 = engine->GetTime();
				if (m_currentProcessTime < time2 + 1.0f || m_currentProcessTime > time2 + 60.0f)
				{
					int16_t i, index, myCampPoint;
					CArray<int16_t>MeshWaypoints;

					for (i = 0; i < g_waypoint->m_hmMeshPoints.Size(); i++)
					{
						index = g_waypoint->m_hmMeshPoints.Get(i);
						if (!g_waypoint->GetPath(index)->mesh)
							continue;

						if (zhPath.mesh != g_waypoint->GetPath(index)->mesh)
							continue;

						if (!m_hasEnemiesNear && index == m_currentWaypointIndex && g_waypoint->m_hmMeshPoints.Size() > 1)
							continue;

						MeshWaypoints.Push(index);
					}

					if (!MeshWaypoints.IsEmpty())
					{
						myCampPoint = MeshWaypoints.Random();
						m_myMeshWaypoint = myCampPoint;
						MeshWaypoints.Destroy();

						float max = 16.0f;
						if (m_hasEnemiesNear)
						{
							if (m_personality == Personality::Rusher)
								max = 20.0f;
							else if (m_personality != Personality::Careful)
								max = 12.0f;
						}
						else
							max = crandomfloat(3.5f, 7.5f);

						m_currentProcessTime = time2 + (m_hasEnemiesNear ? crandomfloat(8.0f, max) : max);
						m_zhCampPointIndex = m_myMeshWaypoint;
						m_currentGoalIndex = m_zhCampPointIndex;
						FindPath(m_currentWaypointIndex, m_myMeshWaypoint);
					}
				}
			}

			if (!m_hasEnemiesNear && !m_hasEntitiesNear && !m_isSlowThink && chanceof(2))
			{
				const float yaw = Math::DegreeToRadian(crandomfloat(0.0f, 360.0f));
				const float pitch = crandomfloat(-28.0f, 18.0f);
				const Vector glance = pev->origin + pev->view_ofs + Vector(ccosf(yaw) * 256.0f, csinf(yaw) * 256.0f, pitch);
				LookAt(glance);
			}

			// standing still
			if (m_hasEnemiesNear && m_currentWeapon != Weapon::Knife && m_personality != Personality::Rusher && pev->velocity.GetLengthSquared2D() < 20.0f)
			{
				bool crouch = true;
				if (m_currentWeapon != Weapon::M3 &&
					m_currentWeapon != Weapon::Xm1014 &&
					m_currentWeapon != Weapon::G3SG1 &&
					m_currentWeapon != Weapon::Scout &&
					m_currentWeapon != Weapon::Awp &&
					m_currentWeapon != Weapon::M249 &&
					m_currentWeapon != Weapon::Sg550)
					crouch = false;

				if (m_personality == Personality::Normal && m_enemyDistance < 512.0f)
					crouch = false;

				if (crouch && IsVisible(m_enemyOrigin, GetEntity()))
					m_duckTime = engine->GetTime() + 1.0f;
			}
		}
		else
		{
			if (m_navNode.HasNext() || (!m_navNode.IsEmpty() && IsValidWaypoint(m_navNode.First()) && m_navNode.First() != m_currentWaypointIndex))
				FollowPath();
			else if (IsValidWaypoint(m_zhCampPointIndex) && m_zhCampPointIndex != m_currentWaypointIndex)
				FindPath(m_currentWaypointIndex, m_zhCampPointIndex);
			else
			{
				m_zhCampPointIndex = FindGoalHuman();
				if (IsValidWaypoint(m_zhCampPointIndex) && m_zhCampPointIndex != m_currentWaypointIndex)
					FindPath(m_currentWaypointIndex, m_zhCampPointIndex);
			}
		}
	}
}

void Bot::DefaultEnd(void)
{

}

bool Bot::DefaultReq(void)
{
	return true;
}
