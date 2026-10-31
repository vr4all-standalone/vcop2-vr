#include "../native/weapon_choice.h"
#include <iostream>
#define CHECK(x) do {if(!(x)){std::cerr<<"Failed " #x " line "<<__LINE__<<'\n';return 1;}}while(0)
int main() {using namespace vcop2;
    CHECK(GunChoiceCount==3);
    CHECK(SelectableGunChoices[0]==AutoOriginalGun&&SelectableGunChoices[1]==0&&SelectableGunChoices[2]==1);
    CHECK(cycle_gun_choice(8,1)==0&&cycle_gun_choice(0,1)==1&&cycle_gun_choice(1,1)==8);
    CHECK(cycle_gun_choice(8,-1)==1&&cycle_gun_choice(1,-1)==0&&cycle_gun_choice(0,-1)==8);
    CHECK(cycle_gun_choice(8,3)==8&&cycle_gun_choice(8,-3)==8&&cycle_gun_choice(0,0)==0);
    CHECK(cycle_gun_choice(8,4)==0&&cycle_gun_choice(8,-4)==1);
    CHECK(cycle_gun_choice(2,1)==8&&cycle_gun_choice(7,-1)==8);
    for(int choice=-1;choice<=9;++choice)CHECK(selectable_gun_choice(choice)==(choice==8||choice==0||choice==1));
    GameState state;state.valid=true;state.players[0].weapon=2;state.players[1].weapon=6;
    auto p1=select_player_weapon(AutoOriginalGun,state,0),p2=select_player_weapon(AutoOriginalGun,state,1);
    CHECK(p1.follows_game && p1.native_weapon==2 && p1.manual_choice==3);
    CHECK(p2.follows_game && p2.native_weapon==6 && p2.manual_choice==7);
    // Each pickup changes only its owner's displayed gun.
    state.players[0].weapon=3;
    CHECK(select_player_weapon(AutoOriginalGun,state,0).native_weapon==3);
    CHECK(select_player_weapon(AutoOriginalGun,state,1).native_weapon==6);
    for(unsigned weapon=2;weapon<=6;++weapon) {
        const auto pick=select_weapon(AutoOriginalGun,weapon);
        CHECK(pick.follows_game && pick.native_weapon==weapon && pick.manual_choice==weapon+1 && !pick.standard_fallback);
    }
    const auto standard=select_weapon(AutoOriginalGun,1);
    CHECK(standard.follows_game && standard.standard_fallback && standard.native_weapon==1 && standard.manual_choice==8);
    // The standard cosmetic is a proven original scene pistol, not an asserted
    // player-held asset. Unknown state cannot use stale other-player state.
    CHECK(select_weapon(AutoOriginalGun,0).native_weapon==0);
    CHECK(!select_weapon(AutoOriginalGun,0).standard_fallback);
    CHECK(select_weapon(AutoOriginalGun,7).native_weapon==0);
    CHECK(select_player_weapon(AutoOriginalGun,state,2).native_weapon==0);
    state.valid=false;
    CHECK(select_player_weapon(AutoOriginalGun,state,0).native_weapon==0);
    for(unsigned choice=0;choice<2;++choice) {
        const auto pick=select_weapon(choice,6);
        CHECK(!pick.follows_game && !pick.standard_fallback && pick.manual_choice==choice);
        CHECK(pick.native_weapon==0);
    }
    // Internal pickup slots and the removed custom option cannot force an
    // equipped weapon; only In-game may resolve those native mesh slots.
    for(unsigned choice=2;choice<=7;++choice) {
        const auto pick=select_weapon(choice,6);
        CHECK(pick.manual_choice==0&&pick.native_weapon==0&&!pick.follows_game&&!pick.standard_fallback);
    }
    CHECK(select_weapon(-1,2).manual_choice==0);
    CHECK(select_weapon(9,2).manual_choice==0);
    std::cout<<"Three selectable guns, bidirectional cycling and per-player native equipment passed\n";
}
