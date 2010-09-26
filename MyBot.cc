/**
 * Copyright (c) 2010, Benjamin C. Meyer <ben@meyerhome.net> 
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Benjamin Meyer nor the names of the projects contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


/**
  A modified version of the stock bot that is built around defending its own
  planets first and foremost rather than optimizing for growth.

  Not a winning strategy, but interesting.
 **/

#include <iostream>
using namespace std;

#include "PlanetWars.h"

// #define PLANET_DEBUG 1

#ifdef PLANET_DEBUG
#include <fstream>
ofstream debugfile;
#endif

int turn = 0;
int buffer = 2;

class Move {
public:
    int source;
    int ships;

    int distance;
};

class Action {
public:
    std::vector<Move> moves;
    int planet_id;
    int growth;
    bool wait;

    int maxDistance() const {
        int d = 0;
        for (uint i = 0; i < moves.size(); ++i)
            d = std::max(d, moves[i].distance);
        return d;
    }

    int ships() const {
        int c = 0;
        for (uint i = 0; i < moves.size(); ++i)
            c += moves[i].ships;
        return c;
    }

    bool isValid(const PlanetWars& pw) const {
        for (uint i = 0; i < moves.size(); ++i) {
            int c = pw.real_ship_count(moves[i].source);
            c += wait * pw.GetPlanet(moves[i].source).GrowthRate();
            if (c <= moves[i].ships)
                return false;
        }
        return true;
    }

    int investment() const {
        return maxDistance() + wait + ships()/(std::max(growth, 1));
    }
};

// Which action will pay off faster?
bool actions_sort (Action action1, Action action2) {
    int investment1 = action1.investment();
    int investment2 = action2.investment();

    int action1m = action1.growth;
    int action2m = action2.growth;

    return ((float)investment1/(action1m+1)) < ((float)investment2/(action2m+1));
}

int neighbors_destination;
bool neighbors_sort(Planet i, Planet j) {
    int id = i.pw->Distance(i.PlanetID(), neighbors_destination);
    int jd = j.pw->Distance(j.PlanetID(), neighbors_destination);
    return id < jd;
}

void DoTurn(const PlanetWars& pw) {
#ifdef PLANET_DEBUG
    debugfile << "Turn: " << turn;
    for (int i = 0; i < 3; ++i)
        debugfile << " " << i << ":" << pw.GrowthRate(i);
    debugfile << std::endl;
#endif

    const std::vector<Planet> my_planets = pw.MyPlanets();
    const std::vector<Planet> planets = pw.Planets();

    const std::vector<Planet> enemy_planets = pw.EnemyPlanets();
    if (my_planets.size() == 1 && enemy_planets.size() == 1 && pw.EnemyFleets().size() == 0)
        return;

    /*    
    const std::vector<Planet> enemy_planets = pw.EnemyPlanets();
    int average_enemy_size = 0;
    for (uint i = 0; i < enemy_planets.size(); ++i)
        average_enemy_size += enemy_planets[i].NumShips();
    average_enemy_size /= enemy_planets.size();
    if (not_my_planets.size() > 1) {
        buffer = 0;
        for (uint i = 0; i < not_my_planets.size(); ++i)
            buffer += not_my_planets[i].NumShips();
        buffer /= (not_my_planets.size() * 3 + 1);
        buffer = std::max(2, buffer);
        buffer = std::min(5, buffer);
    }
*/
    std::vector<Fleet> not_my_fleets = pw.EnemyFleets();
    sort (not_my_fleets.begin(), not_my_fleets.end(), attacking_fleet_sort);

    std::vector<Planet> neighbors = my_planets;


    // Offensive Actions
    std::vector<Action> actions;
    for (int t = 0; t < 3; ++t) {
    for (uint i = 0; i < planets.size(); ++i) {
        const Planet& p = planets[i];
        if (p.Owner() == 1)
            continue;

        int required = pw.real_attack_count(p.PlanetID()) + 2;
        // if we are already going to that planet don't bother
        if (p.Owner() > 1 && required <= 2)
            continue;

        neighbors_destination = p.PlanetID();
        sort(neighbors.begin(), neighbors.end(), neighbors_sort);

        Action action;
        action.planet_id = neighbors_destination;
        action.growth = p.GrowthRate();
        action.wait = t > 0;

        int offense = 0;

        // Owned by someone else
        if (p.Owner() > 1) {
            //action.growth *= 2;
            continue;

        }
        // Owned by no one
        if (p.Owner() == 0) {
            // but under attack
            int attackingStrength = pw.UnderAttack(p.PlanetID());
            if (attackingStrength > 0) {
                attackingStrength -= p.GrowthRate() * (pw.UnderAttackDistance(p.PlanetID()) - t);
                required += attackingStrength;
            }
        }
#ifdef PLANET_DEBUG
        debugfile << "o:" << p.Owner() << " g:" << action.growth << " r:" << required << " s:" << p.NumShips() << endl;
#endif
        for (uint j = 0; j < neighbors.size(); ++j) {
            const Planet& n = neighbors[j];
            Move move;
            move.source = n.PlanetID();
            int have = pw.real_ship_count(move.source) + t * n.GrowthRate();
            move.distance = pw.Distance(move.source, action.planet_id);
            move.ships = std::min(required, have - buffer);
            int leftOver = have - buffer - move.ships;
            int TravelBonus = 0;
            if (p.Owner() > 1) {
                int currentDistance = action.maxDistance();
                if (move.distance > currentDistance) {
                    TravelBonus += move.distance - currentDistance;
                    TravelBonus *= p.GrowthRate();
                    int whatICanSend = std::min(leftOver, TravelBonus);
                    move.ships += whatICanSend;
                    TravelBonus -= whatICanSend;
                }
            }
            //debugfile <<  "\tr" << required << "s" << move.ships <<endl;
            if (move.ships > 0 && move.ships >= std::min(required, 7)) {
                // Assume they will defend themselves
                if (p.Owner() > 1)
                    offense += TravelBonus;
                action.moves.push_back(move);
                required -= move.ships;
                if (required < 0)
                    offense += required;
            }
            if (required <= 0 && offense <= 0)
                break;
        }
#ifdef PLANET_DEBUG
        debugfile << "\t" << action.moves.size() << " " << required << endl;
#endif
        // Don't attempt a long term distnace attack if it isn't 100%
        if (offense > 0) {
            if (action.maxDistance() > turn + 5)
                action.moves.clear();
        }

        if (action.moves.size() > 0 && required <= 0)
            actions.push_back(action);
    }
 
#ifdef PLANET_DEBUG
    debugfile << "attack: " << actions.size() << std::endl;
#endif

    // Defensive Actions
    for (uint i = 0; i < my_planets.size(); ++i) {
        Planet p = my_planets[i];
        int help_id = p.PlanetID();
        int real_ship_count = pw.real_ship_count(help_id);
        if (real_ship_count > 0)
            continue;
        int required = real_ship_count * -1;
        int time_left = pw.time_left(help_id);

        neighbors_destination = p.PlanetID();
        sort(neighbors.begin(), neighbors.end(), neighbors_sort);

        Action action;
        action.planet_id = neighbors_destination;
        action.growth = p.GrowthRate() * 2;
        action.wait = t > 0;

        for (uint j = 0; j < neighbors.size(); ++j) {
            Planet n = neighbors[j];
            if (n.PlanetID() == help_id)
                continue;
            int distance_away = pw.Distance(help_id, n.PlanetID());
            if (time_left < distance_away)
                continue;
            if (n.NumShips() < 4)
                continue;

            Move move;
            move.source = n.PlanetID();
            int have = pw.real_ship_count(move.source) + t * n.GrowthRate();
            move.ships = std::min(required, have - 2);
            move.distance = pw.Distance(move.source, action.planet_id);

            if (move.ships > 3) {
                action.moves.push_back(move);
                required -= move.ships;
            }
            if (required <= 0)
                break;
        }
        if (action.moves.size() > 0 && required <= 0)
            actions.push_back(action);
    }
#ifdef PLANET_DEBUG
    debugfile << "defense: " << actions.size() << std::endl;
#endif
    }

    sort (actions.begin(), actions.end(), actions_sort);
#ifdef PLANET_DEBUG
    debugfile << "sorted: " << endl;
#endif
    std::vector<int> destinations;
    for (uint i = 0; i < actions.size(); ++i) {
        const Action &action = actions[i];
#ifdef PLANET_DEBUG
        debugfile << "Action: " << "w" << action.wait << " i" << action.investment() << "\tsource:" << action.planet_id << "\tdist:" << action.maxDistance() << "\tships:" << action.ships() << "\tgrowth: " << action.growth << "\tmoves: " << action.moves.size() << "\tisvalid:" << action.isValid(pw) << std::endl;
#endif
        if (!action.isValid(pw))
            continue;

        int destination = action.planet_id;
        if (std::find(destinations.begin(), destinations.end(), destination) != destinations.end())
            continue;
#ifdef PLANET_DEBUG
        debugfile << "\t^^ Taken" << endl;
#endif
        for (uint j = 0; j < action.moves.size(); ++j) {
            if (action.wait == false) {
                pw.IssueOrder(action.moves[j].source, destination, action.moves[j].ships);
            } else {
                pw.removeShips(action.moves[j].source, action.moves[j].ships);
            }
        }
        destinations.push_back(destination);
    }
#ifdef PLANET_DEBUG
    debugfile << endl;
#endif
}

// This is just the main game loop that takes care of communicating with the
// game engine for you. You don't have to understand or change the code below.
int main(int /*argc*/, char * /*argv*/ []) {
#ifdef PLANET_DEBUG
  debugfile.open ("stderr.txt");
#endif

  std::string current_line;
  std::string map_data;
  while (true) {
    int c = std::cin.get();
    current_line += (char)c;
    if (c == '\n') {
      if (current_line.length() >= 2 && current_line.substr(0, 2) == "go") {
        PlanetWars pw(map_data);
        map_data = "";
		DoTurn(pw);
		pw.FinishTurn();
        turn++;
      } else {
        map_data += current_line;
      }
      current_line = "";
    }
  }
  return 0;
}
