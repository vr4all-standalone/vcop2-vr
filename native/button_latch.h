// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace vcop2 {
class ButtonLatch {
    bool waiting_{true},was_{};
public:
    void suspend(){waiting_=true;}
    bool edge(bool down,bool enabled) {
        if(!enabled)waiting_=true;
        else if(!down)waiting_=false;
        bool result=enabled&&down&&!was_&&!waiting_;was_=down;return result;
    }
};
}
