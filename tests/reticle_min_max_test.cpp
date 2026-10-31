#include "../native/reticle_size.h"
#include "../native/settings_io.h"
#include <cassert>
#include <cmath>
#include <sstream>
using namespace vcop2;
static bool near(double a,double b){return std::abs(a-b)<1e-5;}
int main(){
    Settings s;
    assert(near(s.reticle_min_size,.20)&&near(s.reticle_max_size,.35));
    assert(near(reticle_size_reference(1,1.8,reticle_size_for_mode(s)),.63));
    s.reticle_min_size=.4f;s.reticle_max_size=1.5f;
    assert(near(reticle_size_reference(1,1.8,reticle_size_for_mode(s)),2.7));
    std::ostringstream output;write_settings(output,s);std::istringstream input(output.str());
    auto saved=read_settings(input);
    assert(near(saved.reticle_min_size,.4)&&near(saved.reticle_max_size,1.5));
    std::istringstream bad("reticle_max_size=nan\n");
    assert(near(read_settings(bad).reticle_max_size,.35));
}
