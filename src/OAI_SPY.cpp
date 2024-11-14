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

//Filename   : OAI_SPY.CPP
//Description: AI - Spy activities

#include <ALL.h>
#include <OFIRM.h>
#include <OTOWN.h>
#include <OUNIT.h>
#include <ONATION.h>
#include <OSPY.h>

//--------- Begin of function Nation::think_spy --------//

void Nation::think_spy()
{


}
//---------- End of function Nation::think_spy --------//


//----- Begin of function Nation::ai_assign_spy -----//
//
// action_x_loc, action_y_loc - location of the target firm or town
// ref_x_loc,    ref_y_loc    - not used
// unit_recno 					   - unit recno of the spy
// action_para						- the cloak nation recno the spy should set to.
//
int Nation::ai_assign_spy(ActionNode* actionNode)
{
	if(!seek_path.total_node_avail)
		return 0;

	if( unit_array.is_deleted(actionNode->unit_recno) )
		return -1;

	Unit* spyUnit = unit_array[actionNode->unit_recno];

	if( !spyUnit->is_visible() )		// it's still under training, not available yet
		return -1;

	if( !spyUnit->spy_recno || spyUnit->true_nation_recno() != nation_recno )
		return -1;

	Spy* spyPtr = spy_array[spyUnit->spy_recno];

	//------ change the cloak of the spy ------//
	Town* nearbyTown = nullptr;
	Location* locPtr = nullptr;
	for (int x = spyUnit->next_x_loc() - 1; x <= spyUnit->next_x_loc() + 1; x++)
	{
		for (int y = spyUnit->next_y_loc() - 1; y <= spyUnit->next_y_loc() + 1; y++)
		{
			x = MAX(0, x);
			x = MIN(MAX_WORLD_X_LOC - 1, x);
			y = MAX(0, y);
			y = MIN(MAX_WORLD_Y_LOC - 1, y);
			locPtr = world.get_loc(x, y);
			if (locPtr->is_town())
			{
				nearbyTown = town_array[locPtr->town_recno()];
				goto doublebreak;
			}
		}
	}

	doublebreak:
	spyPtr->notify_cloaked_nation_flag = 0;
	if (nearbyTown != nullptr && nearbyTown->nation_recno == 0 && spyPtr->cloaked_nation_recno == 0)
	{
		locPtr = world.get_loc(actionNode->action_x_loc, actionNode->action_y_loc);
		if (locPtr->is_town())
		{
			Town* targetTown = town_array[locPtr->town_recno()];
			if (targetTown->nation_recno != 0 && targetTown->nation_recno != spyUnit->true_nation_recno())
			{
				spyPtr->notify_cloaked_nation_flag = 1;
			}
		}
	}

	if (!spyUnit->can_spy_change_nation())		// if the spy can't change nation recno now
	{
		int destXLoc = spyUnit->next_x_loc() + misc.random(20) - 10;
		int destYLoc = spyUnit->next_y_loc() + misc.random(20) - 10;

		destXLoc = MAX(0, destXLoc);
		destXLoc = MIN(MAX_WORLD_X_LOC-1, destXLoc);

		destYLoc = MAX(0, destYLoc);
		destYLoc = MIN(MAX_WORLD_Y_LOC-1, destXLoc);

		spyUnit->move_to( destXLoc, destYLoc );

		actionNode->retry_count++;			// never give up
		return 0;								// return now and try again later
	}

	spyUnit->spy_change_nation(actionNode->action_para,COMMAND_AI);

	//------- assign the spy to the target -------//

	if (spyPtr->notify_cloaked_nation_flag == 0)
		spyUnit->assign(actionNode->action_x_loc, actionNode->action_y_loc);
	else
		spyUnit->ai_move_to_nearby_town();

	//----------------------------------------------------------------//
	// Since the spy has already changed its cloaked nation recno
	// we cannot set the ai_action_id of the unit as when it needs
	// to call action_finished() or action_failure() it will
	// use the cloaked nation recno, which is incorrect.
	// So we just return -1, noting that the action has been completed.
	//----------------------------------------------------------------//

	return -1;
}
//----- End of function Nation::ai_assign_spy -----//


//-------- Begin of function Nation::think_assign_spy_target_camp --------//
//
// Think about planting spies into enemy buildings.
//
int Nation::think_assign_spy_target_camp(int raceId, int regionId, int loc_x1, int loc_y1, int cloakedNationRecno)
{
	if (reputation < 0)
		return 0;

	Town* nearbyTown = nullptr;
	for (int x = loc_x1 - 1; x <= loc_x1 + 1; x++)
	{
		for (int y = loc_y1 - 1; y <= loc_y1 + 1; y++)
		{
			x = MAX(0, x);
			x = MIN(MAX_WORLD_X_LOC - 1, x);
			y = MAX(0, y);
			y = MIN(MAX_WORLD_Y_LOC - 1, y);
			Location* locPtr = world.get_loc(x, y);
			if (locPtr->is_town())
			{
				nearbyTown = town_array[locPtr->town_recno()];
				goto doublebreak;
			}
		}
	}

	doublebreak:
	int curRating = 1000, bestRating = 1000, bestFirmRecno = 0;
	int loc_x = 0;
	int loc_y = 0;
	if (nearbyTown != nullptr && cloakedNationRecno == nearbyTown->nation_recno)
	{
		for (int i = nearbyTown->linked_firm_count - 1; i >= 0; i--)
		{
			Firm* firmPtr = firm_array[nearbyTown->linked_firm_array[i]];

			if( firmPtr->nation_recno == nation_recno )		// don't assign to own firm
				continue;

			if (cloakedNationRecno != 0 && firmPtr->nation_recno != cloakedNationRecno)
				continue;

			if (firmPtr->overseer_recno == 0 || firmPtr->worker_count == MAX_WORKER)
				continue;

			if (firmPtr->majority_race() != raceId)
				continue;

			Unit* overseerUnit = unit_array[firmPtr->overseer_recno];

			if (overseerUnit->spy_recno)		// if the overseer is already a spy
				continue;

			curRating = overseerUnit->loyalty * 2 + misc.points_distance(firmPtr->center_x, firmPtr->center_y, loc_x1, loc_y1);

			if (curRating < bestRating)
			{
				bestRating = curRating;
				bestFirmRecno = firmPtr->firm_recno;
				loc_x = firmPtr->center_x;
				loc_y = firmPtr->center_y;
			}
		}

		if (bestFirmRecno != 0)
			return bestFirmRecno;
	}

	curRating = 1000, bestRating = 1000, bestFirmRecno = 0;
	for( int firmRecno=firm_array.size() ; firmRecno>0 ; firmRecno-- )
	{
		if( firm_array.is_deleted(firmRecno) )
			continue;

		Firm* firmPtr = firm_array[firmRecno];

		if( firmPtr->nation_recno == nation_recno )		// don't assign to own firm
			continue;

		if( firmPtr->region_id != regionId )
			continue;

		if( firmPtr->overseer_recno == 0 || firmPtr->worker_count == MAX_WORKER )
			continue;

		if( firmPtr->majority_race() != raceId )
			continue;

		//---------------------------------//

		Unit* overseerUnit = unit_array[firmPtr->overseer_recno];

		if( overseerUnit->spy_recno )		// if the overseer is already a spy
			continue;

		curRating = overseerUnit->loyalty * 2 + misc.points_distance(firmPtr->center_x, firmPtr->center_y, loc_x1, loc_y1);

		if( curRating < bestRating )
		{
			bestRating 	  = curRating;
			bestFirmRecno = firmRecno;
			loc_x = firmPtr->center_x;
			loc_y = firmPtr->center_y;
		}
	}

	return bestFirmRecno;
}
//-------- End of function Nation::think_assign_spy_target_camp --------//


//-------- Begin of function Nation::think_assign_spy_target_town --------//
//
// Think about planting spies into independent towns and enemy towns.
//
int Nation::think_assign_spy_target_town(int raceId, int regionId)
{
	int   townCount = town_array.size();
	int   townRecno = misc.random(townCount)+1;

	for( int i=town_array.size() ; i>0 ; i-- )
	{
		if( ++townRecno > townCount )
			townRecno = 1;

		if( town_array.is_deleted(townRecno) )
			continue;

		Town* townPtr = town_array[townRecno];

		if( townPtr->nation_recno == nation_recno )		// don't assign to own firm
			continue;

		if( townPtr->region_id != regionId )
			continue;

		if( townPtr->population > MAX_TOWN_POPULATION-5 )		// -5 so that even if we assign too many spies to a town at the same time, there will still room for them
			continue;

		if( townPtr->majority_race() != raceId )
			continue;

		if (reputation < 0 && townPtr->nation_recno != 0)
			continue;

		return townRecno;
	}

	return 0;
}
//-------- End of function Nation::think_assign_spy_target_town --------//


//-------- Begin of function Nation::think_assign_spy_own_town --------//
//
// Think about planting spies into own towns.
//
int Nation::think_assign_spy_own_town(int raceId, int regionId)
{
	Town  *townPtr;
	int   townCount = town_array.size();
	int   townRecno = misc.random(townCount)+1;
	int   spyCount;

	for( int i=town_array.size() ; i>0 ; i-- )
	{
		if( ++townRecno > townCount )
			townRecno = 1;

		if( town_array.is_deleted(townRecno) )
			continue;

		townPtr = town_array[townRecno];

		if( townPtr->nation_recno != nation_recno )		// only assign to own firm
			continue;

		if( townPtr->region_id != regionId )
			continue;

		if( townPtr->population > MAX_TOWN_POPULATION-5 )
			continue;

		if( townPtr->majority_race() != raceId )
			continue;

		int curSpyLevel    = spy_array.total_spy_skill_level( SPY_TOWN, townRecno, nation_recno, spyCount );
		int neededSpyLevel = townPtr->needed_anti_spy_level();

		if( neededSpyLevel > curSpyLevel + 30 )
			return townRecno;
	}

	return 0;
}
//-------- End of function Nation::think_assign_spy_own_town --------//


//-------- Begin of function Nation::think_spy_new_mission --------//
//
// Think about a new mission for a spy
//
int Nation::think_spy_new_mission(int raceId, int regionId, int& loc_x1, int& loc_y1, int& cloakedNationRecno)
{
	int firmRecno = 0, townRecno = 0;

	if (misc.random(2) == 0)
	{
		firmRecno = think_assign_spy_target_camp(raceId, regionId, loc_x1, loc_x1, cloakedNationRecno);
		if (firmRecno)
		{
			Firm* firmPtr = firm_array[firmRecno];
			loc_x1 = firmPtr->loc_x1;
			loc_y1 = firmPtr->loc_y1;
			cloakedNationRecno = firmPtr->nation_recno;

			return 1;
		}
	}
	else
	{
		townRecno = think_assign_spy_target_town(raceId, regionId);
		if (townRecno)
		{
			Town* townPtr = town_array[townRecno];
			loc_x1 = townPtr->loc_x1;
			loc_y1 = townPtr->loc_y1;
			cloakedNationRecno = townPtr->nation_recno;

			return 1;
		}
	}

	townRecno = think_assign_spy_own_town(raceId, regionId);
	if (townRecno)
	{
		Town* townPtr = town_array[townRecno];
		loc_x1 = townPtr->loc_x1;
		loc_y1 = townPtr->loc_y1;
		cloakedNationRecno = townPtr->nation_recno;

		return 1;
	}

	return 0;
}
//-------- End of function Nation::think_spy_new_mission --------//


//-------- Begin of function Nation::ai_begin_spy_new_mission --------//
//
// Start a new mission for a spy
//
void Nation::ai_start_spy_new_mission(int unitRecno, int loc_x1, int loc_y1, int cloakedNationRecno)
{
	if (cloakedNationRecno == 0 || cloakedNationRecno == nation_recno)
	{
		//--- move to the independent or our town ---//
		add_action(loc_x1, loc_y1, -1, -1, ACTION_AI_ASSIGN_SPY, cloakedNationRecno, 1, unitRecno);
	}
	else
	{
		//--- move to the random location and then change its color there ---//
		int destXLoc = misc.random(MAX_WORLD_X_LOC);
		int destYLoc = misc.random(MAX_WORLD_Y_LOC);
		Unit* spyUnit = unit_array[unitRecno];
		spyUnit->move_to(destXLoc, destYLoc);
	}
}
//-------- End of function Nation::ai_begin_spy_new_mission --------//


//--------- Begin of function Nation::think_drop_spy_identity --------//
void Nation::think_drop_spy_identity()
{
	Spy* worstSpy = nullptr;
	int worstSkill = 101;

	for (int i = spy_array.size(); i > 0 ; i--)
	{
		if (spy_array.is_deleted(i))
			continue;

		Spy* spyPtr = spy_array[i];

		if (spyPtr->true_nation_recno != nation_recno || spyPtr->cloaked_nation_recno != nation_recno)
			continue;

		if (spyPtr->spy_skill < worstSkill)
		{
			worstSpy = spyPtr;
			worstSkill = spyPtr->spy_skill;
		}
	}

	if (worstSpy != nullptr)
		worstSpy->drop_spy_identity();
}
//---------- End of function Nation::think_drop_spy_identity --------//


//--------- Begin of function Nation::ai_should_create_new_spy --------//
int Nation::ai_should_create_new_spy(int onlyCounterSpy)
{
	int aiPref = (onlyCounterSpy != 0 ? pref_counter_spy : pref_spy);

	if (total_spy_count > total_population * (10 + aiPref / 10) / 100)		// 10% to 20%
		return 0;

	if (!ai_should_spend(aiPref / 2))
		return 0;

	//--- the expense of spies should not be too large ---//

	if (expense_365days(EXPENSE_SPY) > expense_365days() * (50 + aiPref) / 400)
		return 0;

	return 1;
}
//---------- End of function Nation::ai_should_create_new_spy --------//
