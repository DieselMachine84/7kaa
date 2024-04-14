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

//Filename   : OAI_ATTK.CPP
//Description: AI - attacking

#include <stdlib.h>
#include <vector>
#include <ALL.h>
#include <OUNIT.h>
#include <OCONFIG.h>
#include <OFIRMALL.h>
#include <OTALKRES.h>
#include <ONATION.h>


//------ Declare static functions --------//

static int get_target_nation_recno(int targetXLoc, int targetYLoc);
static int sort_attack_camp_function( const void *a, const void *b );


//--------- Begin of function Nation::ai_attack_target --------//
//
// Think about attacking a specific target.
//
// <int> targetXLoc, targetYLoc - location of the target
// <int> targetCombatLevel      - the combat level of the target, will
//                                only attack the target if the attacker's
//                                force is larger than it.
// [int] justMoveToFlag         - whether just all move there and wait for
//                                the units to attack the enemies automatically
//                                (default: 0)
// [int] attackerMinCombatLevel - the minimum combat level of the attacker,
//											 do not send troops whose combat level
//											 is lower than this.
//                                (default: 0)
// [int] leadAttackCampRecno	  - if this is given, this camp will be
//											 included in the attacker list by passing
//											 checking on it.
//                                (default: 0)
// [int] useAllCamp 				  - use all camps to attack even if defenseMode is 0
//											 (default: defenseMode, which is default to 0)
//
// return: <int> 0  - no attack action
//					  >0 - the total combat level of attacking force.
//
int Nation::ai_attack_target(int targetXLoc, int targetYLoc, int targetCombatLevel, int justMoveToFlag, int attackerMinCombatLevel, int leadAttackCampRecno, int useAllCamp)
{
	//--- if the AI is already on an attack mission ---//
	if (attack_camp_count > 0)
		return 0;

	int targetRegionId = world.get_loc(targetXLoc, targetYLoc)->region_id;
	ai_attack_target_x_loc = targetXLoc;
	ai_attack_target_y_loc = targetYLoc;
	ai_attack_target_nation_recno = get_target_nation_recno(targetXLoc, targetYLoc);

	//------- if there is a pre-selected camp -------//
	lead_attack_camp_recno = leadAttackCampRecno;
	if (leadAttackCampRecno != 0)
	{
		err_when (firm_array[leadAttackCampRecno]->nation_recno != nation_recno);
		err_when (firm_array[leadAttackCampRecno]->firm_id != FIRM_CAMP);

		FirmCamp* firmCamp = (FirmCamp*)firm_array[leadAttackCampRecno];
		attack_camp_array[attack_camp_count].firm_recno = leadAttackCampRecno;
		attack_camp_array[attack_camp_count].combat_level = firmCamp->total_combat_level();
		attack_camp_array[attack_camp_count].distance = misc.points_distance(firmCamp->center_x, firmCamp->center_y, targetXLoc, targetYLoc);
		attack_camp_count++;
	}

	std::vector<short> protectionCamps;
	for (int i = 0; i < ai_town_count; i++)
	{
		Town* townPtr = town_array[ai_town_array[i]];

		if (townPtr->region_id != targetRegionId)
			continue;

		townPtr->add_protection_camps(protectionCamps, useAllCamp);
	}

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

		if (firmCamp->ai_is_capturing_independent_village())	// the camp is trying to capture an independent town
			continue;

		if (firmCamp->is_attack_camp)	// the camp is on another attack mission already
			continue;

		if (leadAttackCampRecno != 0 && firmRecno == leadAttackCampRecno)	// the camp was already added before
			continue;

		if (attackerMinCombatLevel > 0 && firmCamp->average_combat_level() < attackerMinCombatLevel)
			continue;

		if (attack_camp_count < MAX_SUITABLE_ATTACK_CAMP)
		{
			attack_camp_array[attack_camp_count].firm_recno = firmRecno;
			attack_camp_array[attack_camp_count].combat_level = firmCamp->total_combat_level();
			attack_camp_array[attack_camp_count].distance = misc.points_distance(firmCamp->center_x, firmCamp->center_y, targetXLoc, targetYLoc);
			attack_camp_count++;
		}
	}

	//---- now we get all suitable camps in the list, it's time to attack ---//
	//----- think about which ones in the list should be used -----//
	//--- try to send troop with maxTargetCombatLevel, and don't send troop if available combat level < minTargetCombatLevel ---//
	int minTargetCombatLevel = targetCombatLevel * (125 + pref_force_projection / 4) / 100;		// 125% to 150%
	int maxTargetCombatLevel = targetCombatLevel * (150 + pref_force_projection / 2) / 100;		// 150% to 200%

	//--- first calculate the total combat level of these camps ---//
	int totalCombatLevel = 0;
	for (int i = 0; i < attack_camp_count; i++)
		totalCombatLevel += attack_camp_array[i].combat_level;

	//--- see if we are not strong enough to attack yet -----//
	if (totalCombatLevel < minTargetCombatLevel)
	{
		attack_camp_count = 0;
		return 0;
	}

	//---- now sort the camps based on their distances & combat levels ----//
	qsort(&attack_camp_array, attack_camp_count, sizeof(attack_camp_array[0]), sort_attack_camp_function);

	//----- now take out the lowest rating ones -----//
	for (int i = attack_camp_count - 1; i >= 0; i--)
	{
		//----- if camps are close to the target use all of them -----//
		if (attack_camp_array[attack_camp_count].distance < MAX_WORLD_X_LOC / 3)
			break;

		if (totalCombatLevel - attack_camp_array[i].combat_level > maxTargetCombatLevel)
		{
			totalCombatLevel -= attack_camp_array[i].combat_level;
			attack_camp_count--;
		}
	}

	err_when( attack_camp_count < 0 );

	//------- declare war with the target nation -------//

	if (ai_attack_target_nation_recno)
		talk_res.ai_send_talk_msg(ai_attack_target_nation_recno, nation_recno, TALK_DECLARE_WAR);

	//------- synchronize the attack date for different camps ----//
	ai_attack_target_sync();

	ai_attack_target_execute(!justMoveToFlag);

	return totalCombatLevel;
}
//--------- End of function Nation::ai_attack_target --------//


//--------- Begin of function Nation::ai_attack_target_sync --------//
//
// Synchronize the timing of attacking a target. Camps that are further
// away from the target will move first while camps that are closer
// to the target will move later.
//
void Nation::ai_attack_target_sync()
{
	//---- find the distance of the camp that is farest to the target ----//

	int maxDistance = 0;

	for (int i = 0; i < attack_camp_count; i++)
	{
		err_when( attack_camp_array[i].distance < 0 );

		if (attack_camp_array[i].distance > maxDistance)
			maxDistance = attack_camp_array[i].distance;
	}

	int maxTravelDays = sprite_res[ unit_res[UNIT_NORMAN]->sprite_id ]->travel_days(maxDistance);

	//------ set the date which the troop should start moving -----//

	for (int i = 0; i < attack_camp_count; i++)
	{
		int travelDays = maxTravelDays * attack_camp_array[i].distance / maxDistance;

		attack_camp_array[i].patrol_date = info.game_date + (maxTravelDays - travelDays);
		//use distance to store attack date, but we need to store date diff instead because distance is of short type
		attack_camp_array[i].distance = (info.game_date + maxTravelDays - info.game_start_date) / 10;
	}

	//----- set the is_attack_camp flag of the camps ------//

	for (int i = 0; i < attack_camp_count; i++)
	{
		Firm* firmPtr = firm_array[ attack_camp_array[i].firm_recno ];

		err_when (firmPtr->firm_id != FIRM_CAMP || firmPtr->nation_recno != nation_recno);

		((FirmCamp*)firmPtr)->is_attack_camp = 1;
	}
}
//---------- End of function Nation::ai_attack_target_sync --------//


//--------- Begin of function Nation::ai_attack_target_execute --------//
//
// Processed attack. Send camps to the target, think about cancelling attack
//
// <int> directAttack - whether directly attack the target or
//								just move close to the target.
//
void Nation::ai_attack_target_execute(int directAttack)
{
	err_when( ai_attack_target_x_loc < 0 || ai_attack_target_x_loc >= MAX_WORLD_X_LOC );
	err_when( ai_attack_target_y_loc < 0 || ai_attack_target_y_loc >= MAX_WORLD_Y_LOC );

	//---- if the target no longer exist -----//

	//DieselMachine TODO need better check for reset
	if (ai_attack_target_nation_recno != get_target_nation_recno(ai_attack_target_x_loc, ai_attack_target_y_loc))
	{
		reset_ai_attack_target();
	}

	//Attack date came. Let them fight, now we can prepare for the next attack
	//restore attack date from distance
	if (attack_camp_count > 0 && (info.game_date > (int)attack_camp_array[0].distance * 10 + info.game_start_date))
	{
		reset_ai_attack_target();
	}

	//---- if enemy forces came and we need to cancel our attack -----//

	if (info.game_date % 5 == nation_recno % 5)
	{
		int targetCombatLevel = ai_evaluate_target_combat_level(ai_attack_target_x_loc, ai_attack_target_y_loc, ai_attack_target_nation_recno);
		int ourCombatLevel = 0;
		for (int i = 0; i < attack_camp_count; i++)
		{
			ourCombatLevel += attack_camp_array[i].combat_level;
		}

		//DieselMachine TODO this code is duplicated with ai_attack_target()
		int minTargetCombatLevel = targetCombatLevel * (125 + pref_force_projection / 4) / 100;		// 125% to 150%
		if (ourCombatLevel < minTargetCombatLevel)
		{
			for (int i = 0; i < attack_camp_count; i++)
			{
				int firmRecno = attack_camp_array[i].firm_recno;
				if (!firm_array.is_deleted(firmRecno))
				{
					if (attack_camp_array[i].patrol_date == 0)	//troop is patrolling, call them back
					{
						FirmCamp* firmCamp = (FirmCamp*)firm_array[firmRecno];
						firmCamp->validate_patrol_unit();
						if (firmCamp->patrol_unit_count > 0)
						{
							unit_array.assign_to_camp(firmCamp->loc_x1, firmCamp->loc_y1, COMMAND_AI, firmCamp->patrol_unit_array, firmCamp->patrol_unit_count);
						}
					}
				}
			}
			reset_ai_attack_target();
		}
	}

	//---- send camps to the target -----//

	for (int i = 0; i < attack_camp_count; i++)
	{
		//----- if camp was sent already or if it's still not the date to move to attack ----//
		if (attack_camp_array[i].patrol_date == 0 || info.game_date < attack_camp_array[i].patrol_date)
			continue;

		attack_camp_array[i].patrol_date = 0;

		int firmRecno = attack_camp_array[i].firm_recno;
		FirmCamp* firmCamp = (FirmCamp*)firm_array[firmRecno];

		if( firmCamp && (firmCamp->overseer_recno || firmCamp->worker_count) )
		{
			//--- if this is the lead attack camp, don't mobilize the overseer ---//

			if( lead_attack_camp_recno == firmRecno )
				firmCamp->patrol_all_soldier(); 	// don't mobilize the overseer
			else
				firmCamp->patrol();    // mobilize the overseer and the soldiers

			//----------------------------------------//

			firmCamp->validate_patrol_unit();
			if( firmCamp->patrol_unit_count > 0 )     // there could be chances that there are no some for mobilizing the units
			{
				//------- declare war with the target nation -------//

				//if( ai_attack_target_nation_recno )
					//talk_res.ai_send_talk_msg(ai_attack_target_nation_recno, nation_recno, TALK_DECLARE_WAR);

				//--- in defense mode, just move close to the target, the unit will start attacking themselves as their relationship is hostile already ---//

				if( !directAttack )
				{
					unit_array.move_to(ai_attack_target_x_loc, ai_attack_target_y_loc, 0, firmCamp->patrol_unit_array,
											 firmCamp->patrol_unit_count, COMMAND_AI);
				}
				else
				{
					//-------- set should_attack on the target to 1 --------//

					enable_should_attack_on_target(ai_attack_target_x_loc, ai_attack_target_y_loc);

					//---------- attack now -----------//

					// ##### patch begin Gilbert 5/8 ######//
					unit_array.attack(ai_attack_target_x_loc, ai_attack_target_y_loc, 0, firmCamp->patrol_unit_array,
											firmCamp->patrol_unit_count, COMMAND_AI, 0);
					// ##### patch end Gilbert 5/8 ######//
				}
			}
		}
	}
}
//---------- End of function Nation::ai_attack_target_execute --------//


//--------- Begin of function Nation::reset_ai_attack_target --------//
//
void Nation::reset_ai_attack_target()
{
	//------ reset all is_attack_camp -------//

	for (int i = 0; i < attack_camp_count; i++)
	{
		Firm* firmPtr = firm_array[ attack_camp_array[i].firm_recno ];

		err_when (firmPtr->firm_id != FIRM_CAMP || firmPtr->nation_recno != nation_recno);

		((FirmCamp*)firmPtr)->is_attack_camp = 0;
	}

	attack_camp_count = 0;
}
//---------- End of function Nation::reset_ai_attack_target --------//


//--------- Begin of function Nation::ai_collect_military_force --------//
//
void Nation::ai_collect_military_force(int targetXLoc, int targetYLoc, int targetRecno, std::vector<int>& camps, std::vector<int>& units, std::vector<int>& ourUnits)
{
	//--- the scanning distance is determined by the AI aggressiveness setting ---//

	int scanRangeX = 5 + config.ai_aggressiveness * 2;
	int scanRangeY = scanRangeX;

	int xLoc1 = targetXLoc - scanRangeX;
	int yLoc1 = targetYLoc - scanRangeY;
	int xLoc2 = targetXLoc + scanRangeX;
	int yLoc2 = targetYLoc + scanRangeY;

	xLoc1 = MAX(xLoc1, 0);
	yLoc1 = MAX(yLoc1, 0);
	xLoc2 = MIN(xLoc2, MAX_WORLD_X_LOC - 1);
	yLoc2 = MIN(yLoc2, MAX_WORLD_Y_LOC - 1);

	//------------------------------------------//

	//Collect all camps in the square + all camps linked to towns in the square
	//Collect all units in the square
	std::vector<int> towns;
	Location* locPtr = nullptr;
	for(int yLoc = yLoc1; yLoc <= yLoc2; yLoc++)
	{
		locPtr = world.get_loc(xLoc1, yLoc);
		for(int xLoc = xLoc1; xLoc <= xLoc2; xLoc++, locPtr++)
		{
			if (locPtr->is_town())
			{
				int townRecno = locPtr->town_recno();
				Town* townPtr = town_array[townRecno];
				if (townPtr->nation_recno == targetRecno)
				{
					bool found = false;
					for (int i = 0; i < towns.size(); i++)
					{
						if (towns[i] == townRecno)
						{
							found = true;
							break;
						}
					}
					if (!found)
					{
						towns.push_back(townRecno);
					}
				}
			}
			if (locPtr->is_firm())
			{
				int firmRecno = locPtr->firm_recno();
				Firm* firmPtr = firm_array[firmRecno];
				if (firmPtr->nation_recno == targetRecno && firmPtr->firm_id == FIRM_CAMP)
				{
					bool found = false;
					for (int i = 0; i < camps.size(); i++)
					{
						if (camps[i] == firmRecno)
						{
							found = true;
							break;
						}
					}
					if (!found)
					{
						camps.push_back(firmRecno);
					}
				}
			}
			if (locPtr->has_unit(UNIT_LAND))
			{
				int unitRecno = locPtr->unit_recno(UNIT_LAND);
				Unit* unitPtr = unit_array[unitRecno];
				if (unitPtr->nation_recno == targetRecno)
				{
					units.push_back(unitRecno);
				}
				if (unitPtr->nation_recno == nation_recno)
				{
					ourUnits.push_back(nation_recno);
				}
			}
		}
	}

	for (int i = 0; i < towns.size(); i++)
	{
		Town* townPtr = town_array[towns[i]];
		for (int j = 0; j < townPtr->linked_firm_count; j++)
		{
			int firmRecno = townPtr->linked_firm_array[j];
			Firm* firmPtr = firm_array[firmRecno];
			if (firmPtr->nation_recno == targetRecno && firmPtr->firm_id == FIRM_CAMP)
			{
				bool found = false;
				for (int k = 0; i < camps.size(); k++)
				{
					if (camps[k] == firmRecno)
					{
						found = true;
						break;
					}
				}
				if (!found)
				{
					camps.push_back(firmRecno);
				}
			}
		}
	}
}
//--------- End of function Nation::ai_collect_military_force --------//
//


//--------- Begin of function Nation::ai_evaluate_target_combat_level --------//
//
int Nation::ai_evaluate_target_combat_level(int targetXLoc, int targetYLoc, int targetRecno)
{
	std::vector<int> camps, units, ourUnits;
	ai_collect_military_force(targetXLoc, targetYLoc, targetRecno, camps, units, ourUnits);
	int totalCombatLevel = 0;
	for (int i = 0; i < camps.size(); i++)
	{
		FirmCamp* firmCampPtr = (FirmCamp*)firm_array[camps[i]];
		err_when(firmCampPtr == 0);
		totalCombatLevel += firmCampPtr->total_combat_level();
	}
	for (int i = 0; i < units.size(); i++)
	{
		if (unit_array.is_deleted(units[i]))		// the unit is dying
			continue;

		Unit* unitPtr = unit_array[units[i]];
		err_when(unitPtr == 0);
		totalCombatLevel += unitPtr->unit_power();
	}
	if (targetRecno != nation_recno)		// if targetRecno == nation_recno units and ourUnits are the same, so we already counted them
	{
		for (int i = 0; i < ourUnits.size(); i++)
		{
			if (unit_array.is_deleted(ourUnits[i]))		// the unit is dying
				continue;

			Unit* unitPtr = unit_array[ourUnits[i]];
			err_when(unitPtr == 0);
			if (unitPtr->cur_action == SPRITE_ATTACK ||		// only units that are currently attacking or idle are counted, moving units may just be passing by
				unitPtr->cur_action == SPRITE_IDLE )
			{
				totalCombatLevel -= unitPtr->unit_power();
			}
		}
	}

	return totalCombatLevel;
}
//--------- End of function Nation::ai_evaluate_target_combat_level --------//
//


//--------- Begin of function Nation::enable_should_attack_on_target --------//
//
void Nation::enable_should_attack_on_target(int targetXLoc, int targetYLoc)
{
	//------ set should attack to 1 --------//

	int targetNationRecno = get_target_nation_recno(targetXLoc, targetYLoc);
	if( targetNationRecno )
	{
		set_relation_should_attack(targetNationRecno, 1, COMMAND_AI);
	}
}
//--------- End of function Nation::enable_should_attack_on_target --------//


//--------- Begin of static function get_target_nation_recno --------//
//
// Return the nation recno of the target.
//
static int get_target_nation_recno(int targetXLoc, int targetYLoc)
{
	Location* locPtr = world.get_loc(targetXLoc, targetYLoc);

	if( locPtr->is_firm() )
	{
		return firm_array[locPtr->firm_recno()]->nation_recno;
	}
	else if( locPtr->is_town() )
	{
		return town_array[locPtr->town_recno()]->nation_recno;
	}
	else if( locPtr->has_unit(UNIT_LAND) )
	{
		return unit_array[locPtr->unit_recno(UNIT_LAND)]->nation_recno;
	}

	return 0;
}
//---------- End of static function get_target_nation_recno --------//


//------ Begin of function sort_attack_camp_function ------//
//
static int sort_attack_camp_function( const void *a, const void *b )
{
	//DieselMachine TODO count distance better
	//int ratingA = ((AttackCamp*)a)->combat_level - ((AttackCamp*)a)->distance;
	//int ratingB = ((AttackCamp*)b)->combat_level - ((AttackCamp*)b)->distance;
	int ratingA = ((AttackCamp*)a)->distance;
	int ratingB = ((AttackCamp*)b)->distance;
	int rc = ratingA - ratingB;
	if( rc )
		return rc;
	return ((AttackCamp*)b)->firm_recno - ((AttackCamp*)a)->firm_recno;
}
//------- End of function sort_attack_camp_function ------//


//--------- Begin of function Nation::think_secret_attack --------//
//
// Think about secret assault plans.
//
int Nation::think_secret_attack()
{
	//--- never secret attack if its peacefulness >= 80 ---//

	if( pref_peacefulness >= 80 )
		return 0;

	//--- don't try to get new enemies if we already have many ---//

	int totalEnemyMilitary = total_enemy_military();

	if( totalEnemyMilitary > 20+pref_military_courage-pref_peacefulness )
		return 0;

	//---------------------------------------------//

	int bestRating = 0;
	int bestNationRecno = 0;
	int     ourMilitary = military_rank_rating();
	Nation* nationPtr;

	for( int i=1 ; i<=nation_array.size() ; i++ )
	{
		int tradeRating;
		int curRating;
		NationRelation *nationRelation;

		if( nation_array.is_deleted(i) || nation_recno == i )
			continue;

		nationPtr = nation_array[i];

		nationRelation = get_relation(i);

		tradeRating = trade_rating(i)/2 +        // existing trade
			      ai_trade_with_rating(i)/2; // possible trade

		//---- if the secret attack flag is not enabled yet ----//

		if( !nationRelation->ai_secret_attack )
		{
			int relationStatus = nationRelation->status;

			//---- if we have a friendly treaty with this nation ----//

			if( relationStatus == NATION_FRIENDLY )
			{
				if( totalEnemyMilitary > 0 )     // do not attack if we still have enemies
					continue;
			}

			//-------- never attacks an ally ---------//

			else if( relationStatus == NATION_ALLIANCE )
			{
				continue;
			}

			//---- don't attack if we have a big trade volume with the nation ---//

			if( tradeRating > (50-pref_trading_tendency/2) )	// 0 to 50, 0 if trade tendency is 100, it is 0
			{
				continue;
			}
		}

		//--------- calculate the rating ----------//

		curRating = (ourMilitary - nationPtr->military_rank_rating()) * 2
						 + (overall_rank_rating() - 50) 		// if <50 negative, if >50 positive
						 - tradeRating*2
						 - get_relation(i)->ai_relation_level/2
						 - pref_peacefulness/2;

		//------- if aggressiveness config is medium or high ----//

		if( !nationPtr->is_ai() )		// more aggressive towards human players
		{
			switch( config.ai_aggressiveness )
			{
				case OPTION_MODERATE:
					curRating += 100;
					break;

				case OPTION_HIGH:
					curRating += 300;
					break;

				case OPTION_VERY_HIGH:
					curRating += 500;
					break;
			}
		}

		//----- if the secret attack is already on -----//

		if( nationRelation->ai_secret_attack )
		{
			//--- cancel secret attack if the situation has changed ---//

			if( curRating < 0 )
			{
				nationRelation->ai_secret_attack = 0;
				continue;
			}
		}

		//--------- compare ratings -----------//

		if( curRating > bestRating )
      {
         bestRating = curRating;
         bestNationRecno = i;
      }
   }

   //-------------------------------//

   if( bestNationRecno )
   {
		get_relation(bestNationRecno)->ai_secret_attack = 1;
		return 1;
   }

   return 0;
}
//---------- End of function Nation::think_secret_attack --------//
