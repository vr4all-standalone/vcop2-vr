// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>

namespace vcop2 {
struct NativeSpeedSample {bool ready{},paused{};double native{},percent{};};
class NativeSpeedTracker {
    bool active_{},paused_{};
    double native_{},wall_{},last_native_{},last_wall_{};
public:
    NativeSpeedSample update(double native,double wall,bool paused) {
        if(!std::isfinite(native)||!std::isfinite(wall)){active_=false;return {};}
        if(!active_||paused!=paused_||native<last_native_||wall<last_wall_) {
            active_=true;paused_=paused;native_=last_native_=native;wall_=last_wall_=wall;return {};
        }
        last_native_=native;last_wall_=wall;
        if(wall-wall_<5)return {};
        NativeSpeedSample sample{true,paused,native,(native-native_)/(wall-wall_)*100};
        native_=native;wall_=wall;return sample;
    }
};
// Pace cumulative native time. A costly refresh may borrow the spare capacity
// of following refreshes; discarding that debt repeatedly slows the audio feed.
class RealtimePacer {
    bool active_{};
    double native_anchor_{},wall_anchor_{},last_native_{},last_wall_{},previous_wait_{},speed_{1};
    void anchor(double native,double wall,double speed) {
        active_=true;native_anchor_=last_native_=native;wall_anchor_=last_wall_=wall;previous_wait_=0;speed_=speed;
    }
public:
    void reset(){active_=false;}
    double wait_seconds(double native,double wall,double speed=1) {
        if(!std::isfinite(native)||!std::isfinite(wall)||!std::isfinite(speed)||speed<=0){reset();return 0;}
        if(!active_||speed!=speed_||native<last_native_||wall<last_wall_
                ||wall-last_wall_>previous_wait_+.5||native-last_native_>.5) {
            anchor(native,wall,speed);return 0;
        }
        last_native_=native;last_wall_=wall;
        const double due=wall_anchor_+(native-native_anchor_)/speed;
        // Bound recovery after sustained overload; pause/reset is explicitly
        // reanchored so removing the headset cannot fast-forward the game.
        if(wall-due>.25){anchor(native,wall,speed);return 0;}
        previous_wait_=std::max(0.,due-wall);
        return previous_wait_;
    }
};
}
