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

//Filename   : OAI_TRAD.CPP
//Description: AI - process trading

#include <vector>
#include <ALL.h>
#include <ONATION.h>
#include <OF_CAMP.h>
#include <OF_MARK.h>
#include <OU_CARA.h>

//--------- Begin of function Nation::think_trading --------//

void Nation::think_trading()
{
}
//---------- End of function Nation::think_trading --------//



/*static void CollectTownMainCamps(std::vector<TownMainCamp>& townMainCamps);
static void CollectNonMainCamps(std::vector<short>& nonMainCamps);
static void CollectIdleWarriors(std::vector<int>& ourIdleWarriors);
static bool OverseerAlreadyAssigned(FirmCamp* campPtr);

void Nation::ThinkFillCamps()
{
	//printf("think_fill_camps\n");

	//For each town select its main camp
	std::vector<TownMainCamp> townMainCamps;
    CollectTownMainCamps(townMainCamps);

	//Now every town, that has our camp linked to it, is added to townMainCamps, all other our camps are not main camps

	std::vector<int> ourIdleWarriors;
    CollectIdleWarriors(ourIdleWarriors);

	//For every main camp of independent town we need to set general that has the same race as the second majority race if we have one
	//And after he did his job we need to set general that has the same race as the first majority race to finish capturing
	//For every main camp of enemy town we need to set general that has the same race as the first majority race to finish capturing
	for (int townMainCampsIndex = 0; townMainCampsIndex < townMainCamps.size(); townMainCampsIndex++)
	{
		TownMainCamp townMainCamp = townMainCamps[townMainCampsIndex];
		Town* townPtr = town_array[townMainCamp.TownRecno];
		if (townPtr->nation_recno == nation_recno)		//Select independent towns and enemy towns
			continue;

		int mostRace1 = 0;
		int mostRace2 = 0;
		townPtr->get_most_populated_race(mostRace1, mostRace2);
		FirmCamp* campPtr = (FirmCamp*)firm_array[townMainCamp.CampRecno];
		if (campPtr->overseer_recno == 0)		//If there is no general, find the appropriate and send here
		{
			if (mostRace1 != 0 && !OverseerAlreadyAssigned(campPtr))
			{
				int bestTargetResistance = 100;
				int bestCapturerRecno = find_best_capturer(townMainCamp.TownRecno, mostRace1, bestTargetResistance);
				if (bestCapturerRecno == 0 && townPtr->nation_recno == 0 && mostRace2 != 0)		//For independent towns but not enemy towns
				{
					bestCapturerRecno = find_best_capturer(townMainCamp.TownRecno, mostRace2, bestTargetResistance);
				}
				if (bestCapturerRecno != 0)
				{
					Unit* generalPtr = unit_array[bestCapturerRecno];
					if (generalPtr->unit_mode == UNIT_MODE_OVERSEE)
					{
						FirmCamp* generalCampPtr = (FirmCamp*)firm_array[generalPtr->unit_mode_para];
						if (campPtr->worker_count < MAX_WORKER)
						{
							generalCampPtr->patrol();
							unit_array.assign_to_camp(campPtr->loc_x1, campPtr->loc_y1, COMMAND_AI, generalCampPtr->patrol_unit_array, generalCampPtr->patrol_unit_count);
						}
						else
						{
							generalCampPtr->mobilize_overseer();
							if (generalPtr->is_visible())
							{
								generalPtr->assign(campPtr->loc_x1, campPtr->loc_y1);
							}
						}
					}
					else
					{
						if (generalPtr->is_visible())
						{
							generalPtr->assign(campPtr->loc_x1, campPtr->loc_y1);
						}
					}
				}
			}
		}
		else
		{
			Unit* unitPtr = unit_array[campPtr->overseer_recno];
			if (unitPtr->race_id == mostRace1)
			{
				if (townPtr->nation_recno == 0 && mostRace2 != 0)		//For independent towns but not enemy towns try to replace general with a general of the second majority race
				{
					int bestTargetResistance = 100;
					int bestCapturerRecno = find_best_capturer(townMainCamp.TownRecno, mostRace2, bestTargetResistance);
					if (bestCapturerRecno != 0)
					{
						Unit* generalPtr = unit_array[bestCapturerRecno];
						if (generalPtr->unit_mode == UNIT_MODE_OVERSEE)
						{
							FirmCamp* generalCampPtr = (FirmCamp*)firm_array[generalPtr->unit_mode_para];
							if (campPtr->worker_count < MAX_WORKER)
							{
								generalCampPtr->patrol();
								unit_array.assign_to_camp(campPtr->loc_x1, campPtr->loc_y1, COMMAND_AI, generalCampPtr->patrol_unit_array, generalCampPtr->patrol_unit_count);
							}
							else
							{
								generalCampPtr->mobilize_overseer();
								if (generalPtr->is_visible())
								{
									generalPtr->assign(campPtr->loc_x1, campPtr->loc_y1);
								}
							}
						}
						else
						{
							if (generalPtr->is_visible())
							{
								generalPtr->assign(campPtr->loc_x1, campPtr->loc_y1);
							}
						}
					}
				}
			}
			else
			{
				if (unitPtr->race_id == mostRace2)
				{
					//If general did its job replace him with a general of the first majority race
					//TODO
				}
				else
				{
					//Not appropriate general. Move him somewhere else
					campPtr->mobilize_overseer();
				}
			}
		}
	}

	//For every main camp of our town we need to set general that has the same race as the first majority race
	for (int townMainCampsIndex = 0; townMainCampsIndex < townMainCamps.size(); townMainCampsIndex++)
	{
		TownMainCamp townMainCamp = townMainCamps[townMainCampsIndex];
		Town* townPtr = town_array[townMainCamp.TownRecno];
		if (townPtr->nation_recno != nation_recno)		//Select only our towns
			continue;

		int majorityRace = townPtr->majority_race();
		FirmCamp* campPtr = (FirmCamp*)firm_array[townMainCamp.CampRecno];
		Unit* overseerPtr = nullptr;
		if (campPtr->overseer_recno != 0)
		{
			overseerPtr = unit_array[campPtr->overseer_recno];
		}
		int bestSkillLevel = 0;
		int bestCampSoldierIndex = 0;
		if (overseerPtr != nullptr && overseerPtr->race_id == majorityRace)
			bestSkillLevel = overseerPtr->skill.skill_level;

		Worker* soldierPtr = campPtr->worker_array;
		for (int soldierIndex = 1; soldierIndex <= campPtr->worker_count; soldierIndex++, soldierPtr++)
		{
			if (soldierPtr->race_id == majorityRace && soldierPtr->skill_id == SKILL_LEADING && soldierPtr->skill_level > bestSkillLevel)
			{
				bestCampSoldierIndex = soldierIndex;
				bestSkillLevel = soldierPtr->skill_level;
			}
		}

		int bestIdleWarriorRecno = 0;
		int bestIdleWarriorIndex = -1;
		for (int warriorIndex = 0; warriorIndex < ourIdleWarriors.size(); warriorIndex++)
		{
			Unit* warriorPtr = unit_array[ourIdleWarriors[warriorIndex]];
			if (warriorPtr->region_id() != townPtr->region_id)
				continue;

			if (misc.points_distance(townPtr->center_x, townPtr->center_y, warriorPtr->cur_x_loc(), warriorPtr->cur_y_loc()) > 20)
				continue;

			if (warriorPtr->race_id == majorityRace && warriorPtr->skill.skill_id == SKILL_LEADING && warriorPtr->skill.skill_level > bestSkillLevel)
			{
				bestCampSoldierIndex = 0;
				bestSkillLevel = warriorPtr->skill.skill_level;
				bestIdleWarriorRecno = ourIdleWarriors[warriorIndex];
				bestIdleWarriorIndex = warriorIndex;
			}
		}
		if (bestCampSoldierIndex != 0 || bestIdleWarriorRecno != 0)
		{
			//We have found a new leader
			if (bestCampSoldierIndex != 0)
			{
				bestIdleWarriorRecno = campPtr->resign_worker(bestCampSoldierIndex);
				bestIdleWarriorIndex = -1;
			}
			if (bestIdleWarriorRecno != 0)
			{
				if (overseerPtr != nullptr)
				{
					//printf("resign_overseer 1\n");
					campPtr->resign_overseer();
					overseerPtr->move_to(townPtr->center_x, townPtr->center_y);
				}

				Unit* bestIdleWarriorPtr = unit_array[bestIdleWarriorRecno];
				if (bestIdleWarriorPtr->rank_id == RANK_SOLDIER)
					bestIdleWarriorPtr->set_rank(RANK_GENERAL);

				bestIdleWarriorPtr->assign(campPtr->loc_x1, campPtr->loc_y1);
				if (bestIdleWarriorIndex != -1)
					ourIdleWarriors.erase(ourIdleWarriors.begin() + bestIdleWarriorIndex);
			}
		}
		else
		{
			int trainedLeaderRecno = 0;
			//We have not found a new leader
			if (bestSkillLevel < 20 && townPtr->can_train(majorityRace))
			{
				trainedLeaderRecno = townPtr->recruit(SKILL_LEADING, majorityRace, COMMAND_AI);
			}
			if (bestSkillLevel < 10 && trainedLeaderRecno == 0)
			{
				//printf("think_fill_camps recruit peasant\n");
				trainedLeaderRecno = townPtr->recruit(-1, majorityRace, COMMAND_AI);
				if (trainedLeaderRecno != 0)
				{
					Unit* trainedLeaderPtr = unit_array[trainedLeaderRecno];
					if (trainedLeaderPtr != nullptr)
					{
						if (overseerPtr != nullptr)
						{
							//printf("resign_overseer 2\n");
							campPtr->resign_overseer();
							overseerPtr->move_to(townPtr->center_x, townPtr->center_y);
						}
						trainedLeaderPtr->assign(campPtr->loc_x1, campPtr->loc_y1);
					}
				}
			}
		}

		//If we have a general in the main camp it's time to recruit soldiers
		int numberOfneededSoldiers = MAX_WORKER - campPtr->worker_count;
		for (int soldierIndex = 0; soldierIndex < numberOfneededSoldiers; soldierIndex++)
		{
			//First check if there are any idle soldiers nearby
			int bestHitPoints = 0;
			int bestIdleWarriorRecno = 0;
			int bestIdleWarriorIndex = -1;
			//TODO take soldiers of majority race first
			for (int warriorIndex = 0; warriorIndex < ourIdleWarriors.size(); warriorIndex++)
			{
				Unit* warriorPtr = unit_array[ourIdleWarriors[warriorIndex]];
				if (warriorPtr->region_id() != townPtr->region_id)
					continue;

				if (misc.points_distance(townPtr->center_x, townPtr->center_y, warriorPtr->cur_x_loc(), warriorPtr->cur_y_loc()) > 20)
					continue;

				if (warriorPtr->rank_id == RANK_SOLDIER && warriorPtr->max_hit_points > bestHitPoints)
				{
					bestHitPoints = warriorPtr->max_hit_points;
					bestIdleWarriorRecno = ourIdleWarriors[warriorIndex];
					bestIdleWarriorIndex = warriorIndex;
				}
			}
			if (bestIdleWarriorRecno != 0)
			{
				Unit* bestIdleWarriorPtr = unit_array[bestIdleWarriorRecno];
				bestIdleWarriorPtr->assign(campPtr->loc_x1, campPtr->loc_y1);
				if (bestIdleWarriorIndex != -1)
					ourIdleWarriors.erase(ourIdleWarriors.begin() + bestIdleWarriorIndex);
			}

			else
			{
				//Second try to recruit peasants
				if (townPtr->population >= 20 + pref_inc_pop_by_growth / 10)		//from 20 to 30 people
				{
					int peasantRecno = townPtr->recruit(-1, majorityRace, COMMAND_AI);
					if (peasantRecno != 0)
					{
						Unit* peasantPtr = unit_array[peasantRecno];
						peasantPtr->assign(campPtr->loc_x1, campPtr->loc_y1);
					}
					else
					{
						//Third try to find war machines
						//if (warMachine)
						//{
							//
						//}
						//else
						//{
							//Fourth try to use soldiers from non-main forts
						//}
					}
				}
			}
		}
	}

	//Now deal with not main camps
	std::vector<short> nonMainCamps;
	CollectNonMainCamps(nonMainCamps);

    for (int campIndex = 0; campIndex < nonMainCamps.size(); campIndex++)
    {
		short campRecno = nonMainCamps[campIndex];
		FirmCamp* campPtr = (FirmCamp*)firm_array[ai_camp_array[campIndex]];
		if (campPtr->linked_town_count != 0)
		{
			//
		}
		else		//Alone standing camp. Close it
		{
			campPtr->patrol();
			campPtr->ai_del_firm();
		}
    }

	//Now deal with idle soldiers and generals
}

static void CollectTownMainCamps(std::vector<TownMainCamp>& townMainCamps)
{
	//First for every independent and enemy town select our linked camp as main camp if it exists
	for (short townRecno = town_array.size(); townRecno > 0; townRecno--)
	{
		if (town_array.is_deleted(townRecno))
			continue;

		Town* townPtr = town_array[townRecno];
		if (townPtr->nation_recno == nation_recno)		//Select only independent or enemy towns
			continue;

		short ourFirmRecno = 0;
		int linkedTowns = 0;
		for (int firmIndex = 0; firmIndex < townPtr->linked_firm_count; firmIndex++)
		{
			short firmRecno = townPtr->linked_firm_array[firmIndex];
			if (firm_array.is_deleted(firmRecno))
				continue;

			Firm* firmPtr = firm_array[firmRecno];
			if (firmPtr->nation_recno == nation_recno && firmPtr->firm_id == FIRM_CAMP && !firmPtr->under_construction)		//Our camp linked to this town
			{
				if (ourFirmRecno == 0 || firmPtr->linked_town_count < linkedTowns)		//Select the one with min number of linked towns
				{
					ourFirmRecno = firmRecno;
					linkedTowns = firmPtr->linked_town_count;
				}
			}
		}

		if (ourFirmRecno != 0)		//Mark our camp as the main camp for this town
		{
			bool found = false;
			for (int townMainCampsIndex = 0; townMainCampsIndex < townMainCamps.size(); townMainCampsIndex++)
			{
				if (townMainCamps[townMainCampsIndex].TownRecno == townRecno)
				{
					found = true;
					break;
				}
			}
			if (!found)
				townMainCamps.push_back({townRecno, ourFirmRecno});
		}
	}

	//Next for every our town select our linked camp as main camp if it exists
	for (short townRecno = town_array.size(); townRecno > 0; townRecno--)
	{
		if (town_array.is_deleted(townRecno))
			continue;

		Town* townPtr = town_array[townRecno];
		if (townPtr->nation_recno != nation_recno)		//Select only our towns
			continue;

		short ourFirmRecno = 0;
		int linkedTowns = 0;
		for (int firmIndex = 0; firmIndex < townPtr->linked_firm_count; firmIndex++)
		{
			short firmRecno = townPtr->linked_firm_array[firmIndex];
			if (firm_array.is_deleted(firmRecno))
				continue;

			Firm* firmPtr = firm_array[firmRecno];
			if (firmPtr->nation_recno == nation_recno && firmPtr->firm_id == FIRM_CAMP)		//Our camp linked to this town
			{
				if (ourFirmRecno == 0 || firmPtr->linked_town_count > linkedTowns)		//Select the one with max number of linked towns
				{
					ourFirmRecno = firmRecno;
					linkedTowns = firmPtr->linked_town_count;
				}
			}
		}

		if (ourFirmRecno != 0)		//Mark our camp as the main camp for this town
		{
			bool found = false;
			for (int townMainCampsIndex = 0; townMainCampsIndex < townMainCamps.size(); townMainCampsIndex++)
			{
				if (townMainCamps[townMainCampsIndex].TownRecno == townRecno)
				{
					found = true;
					break;
				}
			}
			if (!found)
				townMainCamps.push_back({townRecno, ourFirmRecno});
		}
	}
}

static void CollectNonMainCamps(std::vector<short>& nonMainCamps)
{
	for (int campIndex = 0; campIndex < ai_camp_count; campIndex++)
	{
		short campRecno = ai_camp_array[campIndex];
		bool found = false;
		for (int townMainCampsIndex = 0; townMainCampsIndex < townMainCamps.size(); townMainCampsIndex++)
		{
			TownMainCamp townMainCamp = townMainCamps[townMainCampsIndex];
			if (campRecno == townMainCamp.CampRecno)
			{
				found = true;
				break;
			}
		}
		if (!found)
        {
            nonMainCamps.push_back(campRecno);
        }
	}
}

static void CollectIdleWarriors(std::vector<int>& ourIdleWarriors)
{
	for (int unitIndex = unit_array.size(); unitIndex > 0; unitIndex--)
	{
		if (unit_array.is_deleted(unitIndex))
			continue;

		Unit* unitPtr = unit_array[unitIndex];
		if (unitPtr->nation_recno != nation_recno || !unitPtr->is_visible() || unitPtr->action_mode != ACTION_STOP)
			continue;

		if (unitPtr->rank_id == RANK_SOLDIER || unitPtr->rank_id == RANK_GENERAL || unitPtr->rank_id == RANK_KING)
			ourIdleWarriors.push_back(unitIndex);
	}
}

static bool OverseerAlreadyAssigned(FirmCamp* campPtr)
{
	for (int unitIndex = unit_array.size(); unitIndex > 0; unitIndex--)
	{
		if (unit_array.is_deleted(unitIndex))
			continue;

		Unit* unitPtr = unit_array[unitIndex];
		if (unitPtr->nation_recno != campPtr->nation_recno || !unitPtr->is_visible() || unitPtr->action_mode != ACTION_ASSIGN_TO_FIRM)
			continue;

		if (unitPtr->action_x_loc == campPtr->loc_x1 && unitPtr->action_y_loc == campPtr->loc_y1)
		{
			return true;
		}
	}
	return false;
}*/
