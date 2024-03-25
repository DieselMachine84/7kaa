/*
 * Seven Kingdoms: Ancient Adversaries
 *
 * Copyright 1997,1998 Enlight Software Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

//Filename   : OAI_DEFE.CPP
//Description: AI on defense

#include <vector>
#include <ALL.h>
#include <OF_CAMP.h>
#include <OTALKRES.h>
#include <ONATION.h>

//----- Begin of function Nation::ai_defend -----//
//
// <int> attackerUnitRecno - unit recno of the attacker.
//
int Nation::ai_defend(int attackerUnitRecno)
{
	//--- don't call for defense too frequently, only call once 7 days (since this function will be called every time our king/firm/town is attacked, so this filtering is necessary ---//

	if( info.game_date < ai_last_defend_action_date+7 )
		return 0;

	ai_last_defend_action_date = info.game_date;

	//---------- analyse the situation first -----------//

	Unit* attackerUnit = unit_array[attackerUnitRecno];

	err_when( attackerUnit->nation_recno == nation_recno );

	int attackerXLoc = attackerUnit->next_x_loc();
	int attackerYLoc = attackerUnit->next_y_loc();
	int targetRegionId = world.get_loc(attackerXLoc, attackerYLoc)->region_id;

	int enemyCombatLevel = ai_evaluate_target_combat_level(attackerXLoc, attackerYLoc, attackerUnit->nation_recno);

	//-- the value returned is enemy strength minus your own strength, so if it's positive, it means that your enemy is stronger than you, otherwise you're stronger than your enemy --//

	int targetCombatLevel = ai_attack_order_nearby_mobile(attackerXLoc, attackerYLoc, enemyCombatLevel);
	if (targetCombatLevel < 0)	// the mobile force alone can finish all the enemies
		return 1;

	std::vector<short> protectionCamps;
	for (int i = 0; i < ai_town_count; i++)
	{
		Town* townPtr = town_array[ai_town_array[i]];

		if (townPtr->region_id != targetRegionId)
			continue;

		townPtr->add_protection_camps(protectionCamps, true);
	}

	std::vector<short> defendingCamps;
	int totalCombatLevel = 0;
	for (int i = 0; i < ai_camp_count; i++)
	{
		short firmRecno = ai_camp_array[i];
		FirmCamp* firmCamp = (FirmCamp*)firm_array[firmRecno];

		if (firmCamp->region_id != targetRegionId)
			continue;

		bool isProtectionCamp = false;
		for (int j = 0; j < protectionCamps.size(); j++)
		{
			if (protectionCamps[j] == firmRecno)
			{
				isProtectionCamp = true;
				break;
			}
		}
		if (isProtectionCamp)
			continue;

		int distanceFromAttacker = misc.points_distance(firmCamp->center_x, firmCamp->center_y, attackerXLoc, attackerYLoc);
		if (distanceFromAttacker > MAX_WORLD_X_LOC / 2)
			continue;

		if (firmCamp->is_attack_camp && distanceFromAttacker < 15)
		{
			bool calledFromAttack = false;
			for (int j = attack_camp_count - 1; j >= 0; j--)
			{
				if (attack_camp_array[j].firm_recno == firmRecno)
				{
					if (attack_camp_array[j].patrol_date == 0)	//troop is patrolling, call them back
					{
						firmCamp->validate_patrol_unit();
						if (firmCamp->patrol_unit_count > 0)
						{
							unit_array.move_to(attackerXLoc, attackerYLoc, 0, firmCamp->patrol_unit_array, firmCamp->patrol_unit_count, COMMAND_AI);
						}
						calledFromAttack = true;
					}
					//remove this camp from the attack camps
					firmCamp->is_attack_camp = 0;
					misc.del_array_rec(attack_camp_array, attack_camp_count, sizeof(AttackCamp), i + 1);
					attack_camp_count--;
					break;
				}
			}

			if (calledFromAttack)
				continue;
		}

		totalCombatLevel += firmCamp->total_combat_level();
		defendingCamps.push_back(firmRecno);
	}

	//---- now we get all suitable camps in the list, it's time to defend ---//
	//--- don't send troop if available combat level < minTargetCombatLevel ---//
	int minTargetCombatLevel = targetCombatLevel * (100 - pref_military_courage / 2) / 100;		// 50% to 100%

	//--- if we are not strong enough to defend -----//
	if (totalCombatLevel < minTargetCombatLevel)
		return 0;

	for (int i = 0; i < defendingCamps.size(); i++)
	{
		FirmCamp* firmCamp = (FirmCamp*)firm_array[defendingCamps[i]];
		firmCamp->patrol();
		firmCamp->validate_patrol_unit();
		if (firmCamp->patrol_unit_count > 0)
		{
			unit_array.move_to(attackerXLoc, attackerYLoc, 0, firmCamp->patrol_unit_array, firmCamp->patrol_unit_count, COMMAND_AI);
		}
	}

	//------ request military aid from allies ----//
	if (totalCombatLevel < enemyCombatLevel && attackerUnit->nation_recno > 0)
	{
		ai_request_military_aid();
	}

	return 1;
}
//----- End of function Nation::ai_defend -----//


//--------- Begin of function Nation::ai_attack_order_nearby_mobile --------//
//
// Order nearby mobile units who are on their way to home camps to
// join this attack mission.
//
// <int> targetXLoc, targetYLoc - location of the target
// <int> targetCombatLevel      - the combat level of the target, will
//                                only attack the target if the attacker's
//                                force is larger than it.
//
// return: <int> the remaining target combat level of the target
//					  after ordering the mobile units to deal with some of them.
//
int Nation::ai_attack_order_nearby_mobile(int targetXLoc, int targetYLoc, int targetCombatLevel)
{
	int		 scanRange = 15+pref_military_development/20;		// 15 to 20
	int		 xOffset, yOffset;
	int		 xLoc, yLoc;
	int		 targetRegionId = world.get_region_id(targetXLoc, targetYLoc);
	Location* locPtr;

	for( int i=2 ; i<scanRange*scanRange ; i++ )
	{
		misc.cal_move_around_a_point(i, scanRange, scanRange, xOffset, yOffset);

		xLoc = targetXLoc + xOffset;
		yLoc = targetYLoc + yOffset;

		xLoc = MAX(0, xLoc);
		xLoc = MIN(MAX_WORLD_X_LOC-1, xLoc);

		yLoc = MAX(0, yLoc);
		yLoc = MIN(MAX_WORLD_Y_LOC-1, yLoc);

		locPtr = world.get_loc(xLoc, yLoc);

		if( locPtr->region_id != targetRegionId )
			continue;

		if( !locPtr->has_unit(UNIT_LAND) )
			continue;

		//----- if there is a unit on the location ------//

		int unitRecno = locPtr->unit_recno(UNIT_LAND);

		if( unit_array.is_deleted(unitRecno) )		// the unit is dying
			continue;

		Unit* unitPtr = unit_array[unitRecno];

		//--- if if this is our own military unit ----//

		if( unitPtr->nation_recno != nation_recno ||
			 unitPtr->skill.skill_id != SKILL_LEADING )
		{
			continue;
		}

		//--------- if this unit is injured ----------//

		if( unitPtr->hit_points <
			 unitPtr->max_hit_points * (150-pref_military_courage/2) / 200 )
		{
			continue;
		}

		//---- only if this is not assigned to an action ---//

		if( unitPtr->ai_action_id )
			continue;

		//---- if this unit is stop or assigning to a firm ----//

		if( unitPtr->action_mode2 == ACTION_STOP ||
			 unitPtr->action_mode2 == ACTION_ASSIGN_TO_FIRM )
		{
			//-------- set should_attack on the target to 1 --------//

			enable_should_attack_on_target(targetXLoc, targetYLoc);

			//---------- attack now -----------//

			unitPtr->attack_unit(targetXLoc, targetYLoc);

			targetCombatLevel -= (int) unitPtr->hit_points;		// reduce the target combat level

			if( targetCombatLevel <= 0 )
				break;
		}
	}

	return targetCombatLevel;
}
//--------- End of function Nation::ai_attack_order_nearby_mobile --------//
//


//----- Begin of function Nation::ai_request_military_aid -----//
//
// Request allied nations to provide immediate military aid.
//
int Nation::ai_request_military_aid()
{
	for( int i=nation_array.size() ; i>0 ; i-- )
	{
		if( nation_array.is_deleted(i) )
			continue;

		if( get_relation(i)->status != NATION_ALLIANCE )
			continue;

		if( should_diplomacy_retry(TALK_REQUEST_MILITARY_AID, i) )
		{
			talk_res.ai_send_talk_msg(i, nation_recno, TALK_REQUEST_MILITARY_AID);
			return 1;
		}
	}

	return 0;
}
//----- End of function Nation::ai_request_military_aid -----//
