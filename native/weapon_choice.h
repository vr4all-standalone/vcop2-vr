// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "game_state.h"
namespace vcop2 {
inline constexpr int AutoOriginalGun=8;
inline constexpr std::array<int,3> SelectableGunChoices{AutoOriginalGun,0,1};
inline constexpr unsigned GunChoiceCount=SelectableGunChoices.size();
constexpr bool selectable_gun_choice(int choice) {
    for(int selectable:SelectableGunChoices)if(choice==selectable)return true;
    return false;
}
constexpr int cycle_gun_choice(int current,int direction) {
    for(unsigned i=0;i<GunChoiceCount;++i)if(SelectableGunChoices[i]==current)
        return SelectableGunChoices[(int(i)+direction%int(GunChoiceCount)+int(GunChoiceCount))%int(GunChoiceCount)];
    return AutoOriginalGun;
}
struct WeaponChoice {
    unsigned manual_choice{}; // Resolved mesh cache slot, not a selectable user option.
    unsigned native_weapon{};
    bool follows_game{};
    bool standard_fallback{}; // Original scene pistol used cosmetically for standard weapon 1.
};
// The five source-identified original pickup meshes have native weapon IDs
// 2..6. Standard weapon 1 uses the game's scene-pistol mesh cosmetically; the
// original arcade does not render a player-held standard gun. Cache slot 8 is
// reserved for that scene mesh and is never a different native weapon ID.
constexpr WeaponChoice select_weapon(int choice,unsigned native_weapon) {
    if(choice==AutoOriginalGun) {
        if(native_weapon==1)return {8,1,true,true};
        if(native_weapon>=2 && native_weapon<=6)return {native_weapon+1,native_weapon,true,false};
        return {0,0,true,false};
    }
    if(choice==0||choice==1)return {unsigned(choice),0,false,false};
    return {};
}
inline WeaponChoice select_player_weapon(int choice,const GameState& state,unsigned player) {
    return select_weapon(choice,state.valid && player<state.players.size()?state.players[player].weapon:0);
}
}
