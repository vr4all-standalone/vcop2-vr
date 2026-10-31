// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/settings_io.h"
#include <sstream>
#include "../native/reticle_size.h"
#include <stdexcept>
#include <iostream>
#include <map>
using namespace vcop2;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static std::map<std::string,double> stored(const Settings& s){std::ostringstream out;write_settings(out,s);std::istringstream in(out.str());std::map<std::string,double> result;std::string line;while(std::getline(in,line)){auto at=line.find('=');if(at!=std::string::npos)result[line.substr(0,at)]=std::stod(line.substr(at+1));}return result;}
static auto parse(const char* text){std::istringstream in(text);return stored(read_settings(in));}
static bool near(double a,double b){return std::abs(a-b)<1e-5;}
int main()try{
 auto defaults=stored(Settings{});require(defaults.count("reticle_mode"),"Reticle mode must persist (production baseline missing)");
 require(defaults["reticle_mode"]==1&&near(defaults["reticle_min_size"],.20)&&near(defaults["reticle_max_size"],.35)&&near(defaults["reticle_shrink_distance"],7),"Captured Custom/20 percent minimum/35 percent maximum/7m mismatch");
 auto legacy=parse("panel_distance=7.3\nlaser=0\np1_hand=1\n");require(near(legacy["reticle_shrink_distance"],7),"Missing legacy shrink must use7m, independently of saved panel distance");require(legacy["laser"]==0&&legacy["p1_hand"]==1,"Migration changed unrelated settings");
 auto explicit_values=parse("reticle_shrink_distance=19.9\nreticle_min_size=0.06\nreticle_mode=2\npanel_distance=2.4\n");require(near(explicit_values["reticle_shrink_distance"],19.9)&&near(explicit_values["reticle_min_size"],.06)&&explicit_values["reticle_mode"]==2,"Explicit choices did not load independently of panel distance/order");
 auto invalid=parse("reticle_mode=0\nreticle_min_size=.01\nreticle_shrink_distance=nan\npanel_distance=3.4\n");require(invalid["reticle_mode"]==1&&near(invalid["reticle_min_size"],.20)&&near(invalid["reticle_shrink_distance"],7),"Unsupported native/invalid sizes did not use safe Custom/default/migration");
 for(auto text:{"reticle_mode=3\nreticle_shrink_distance=20.1\nreticle_min_size=1.1\n","reticle_mode=1.5\nreticle_shrink_distance=.1\nreticle_min_size=-1\n","reticle_mode=nan\nreticle_shrink_distance=inf\nreticle_min_size=nan\n"}){auto s=parse(text);require(s["reticle_mode"]==1&&near(s["reticle_min_size"],.20)&&near(s["reticle_shrink_distance"],7),"Invalid values corrupted safe defaults");}
 for(double minimum:{.05,.25,1.})for(double distance:{.2,1.8,20.}){std::ostringstream source;source<<"reticle_mode=2\nreticle_min_size="<<minimum<<"\nreticle_shrink_distance="<<distance<<"\n";std::istringstream first(source.str());auto saved=stored(read_settings(first));require(saved["reticle_mode"]==2&&near(saved["reticle_min_size"],minimum)&&near(saved["reticle_shrink_distance"],distance),"Reticle boundary persistence failed");}
 for(double panel:{.5,1.8,7.3,10.})for(const char* value:{"", "garbage", "nan", "inf", ".1", "20.1"}){std::ostringstream input;input<<"panel_distance="<<panel<<"\n";if(*value)input<<"reticle_shrink_distance="<<value<<"\n";std::istringstream source(input.str());auto saved=stored(read_settings(source));require(near(saved["reticle_shrink_distance"],7),"Missing/malformed/nonfinite/out-of-range shrink did not use7m independently of panel");}
 for(double distance:{.2,1.8,6.,19.9,20.}){std::ostringstream input;input<<"reticle_shrink_distance="<<distance<<"\npanel_distance=7.3\n";std::istringstream source(input.str());auto saved=stored(read_settings(source));require(near(saved["reticle_shrink_distance"],distance),"A valid explicit tuned shrink distance was reset");}
 require(near(reticle_size_settings(NAN,NAN).minimum_ratio,.25)&&near(reticle_size_settings(NAN,NAN).shrink_distance,6),"Size helper invalid defaults must be25 percent/6m");
 require(near(reticle_size_reference(7.8,1.8),3.9)&&near(reticle_size_reference(1,1.8),1.8),"6m starting falloff changed the actual panel reference/formula");
 std::cout<<"Production reticle defaults, migration, invalid-value fallback and save/load passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
