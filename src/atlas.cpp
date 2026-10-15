#include "atlas.hpp"
#include "neuro_bioclock.h"
#include "surreal/hash.hpp"
#include "surreal/bigint.hpp"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace nm_atlas {
namespace {
std::string digest(const std::string& s) { return surreal::hex(surreal::sha256(s)); }
template<class T> T number(const std::string& word) {
    T value{};
    auto [end, error] = std::from_chars(word.data(), word.data()+word.size(), value);
    if(error != std::errc{} || end != word.data()+word.size())
        throw std::runtime_error("invalid atlas integer: " + word);
    return value;
}
template<class T> T argument(std::istringstream& input) {
    std::string word;
    if(!(input >> word)) throw std::runtime_error("missing atlas argument");
    return number<T>(word);
}
void end(std::istringstream& input) {
    std::string extra;
    if(input >> extra) throw std::runtime_error("extra atlas argument");
}
std::uint64_t time_tick(std::uint64_t ns, std::uint64_t clock, std::uint64_t limit) {
    std::uint64_t tick = 0;
    if(!clock || nm_bio_to_ticks(clock, ns, limit, &tick))
        throw std::runtime_error("atlas time is off-grid or out of range");
    return tick;
}
struct Pulse { std::uint64_t tick; std::uint16_t neuron; std::int64_t current; };
struct Goal { bool evoke; std::uint16_t neuron; std::uint64_t first,last; };
struct Plan { Goal goal{}; std::vector<Pulse> pulses; };
Plan parse_plan(const std::string& text, std::uint64_t clock, std::uint64_t ticks,
                const std::vector<std::uint16_t>& configured) {
    if(!ticks || ticks > 512) throw std::runtime_error("atlas requires 1 to 512 ticks");
    Plan result; bool have_goal = false;
    std::istringstream source(text); std::string line; std::size_t line_number = 0;
    while(std::getline(source,line)) {
        ++line_number;
        std::istringstream input(line); std::string op;
        if(!(input >> op) || op[0]=='#') continue;
        try {
            if(op=="goal" || op=="goal_ns") {
                if(have_goal) throw std::runtime_error("duplicate atlas goal");
                std::string kind; if(!(input >> kind)) throw std::runtime_error("missing goal kind");
                if(kind!="evoke" && kind!="suppress") throw std::runtime_error("invalid goal kind");
                auto neuron=argument<std::uint16_t>(input);
                auto first=argument<std::uint64_t>(input),last=argument<std::uint64_t>(input);
                end(input);
                if(op=="goal_ns") {
                    first=time_tick(first,clock,ticks-1);
                    last=time_tick(last,clock,ticks-1);
                }
                if(first>last || last>=ticks || !std::binary_search(configured.begin(),configured.end(),neuron))
                    throw std::runtime_error("invalid atlas goal window or neuron");
                result.goal={kind=="evoke",neuron,first,last}; have_goal=true;
            } else if(op=="pulse" || op=="pulse_ns") {
                auto at=argument<std::uint64_t>(input);
                auto neuron=argument<std::uint16_t>(input);
                auto current=argument<std::int64_t>(input); end(input);
                if(op=="pulse_ns") at=time_tick(at,clock,ticks-1);
                if(at>=ticks || current<=0 ||
                   !std::binary_search(configured.begin(),configured.end(),neuron) ||
                   result.pulses.size()>=16)
                    throw std::runtime_error("invalid atlas pulse or capacity");
                result.pulses.push_back({at,neuron,current});
            } else if(op=="sweep" || op=="sweep_ns") {
                auto at=argument<std::uint64_t>(input);
                auto neuron=argument<std::uint16_t>(input);
                auto first=argument<std::int64_t>(input),last=argument<std::int64_t>(input);
                auto stride=argument<std::int64_t>(input);end(input);
                if(op=="sweep_ns")at=time_tick(at,clock,ticks-1);
                if(at>=ticks || !std::binary_search(configured.begin(),configured.end(),neuron) ||
                   first<=0 || last<first || stride<=0)
                    throw std::runtime_error("invalid atlas sweep");
                auto count=1+static_cast<std::uint64_t>((last-first)/stride);
                if(count>16-result.pulses.size())throw std::runtime_error("atlas candidate capacity exceeded");
                for(std::uint64_t k=0;k<count;++k)
                    result.pulses.push_back({at,neuron,first+static_cast<std::int64_t>(k)*stride});
            } else throw std::runtime_error("unknown atlas operation: "+op);
        } catch(const std::exception& e) {
            throw std::runtime_error("atlas line "+std::to_string(line_number)+": "+e.what());
        }
    }
    if(!have_goal || result.pulses.empty()) throw std::runtime_error("atlas needs a goal and at least one pulse");
    std::set<std::tuple<std::uint64_t,std::uint16_t,std::int64_t>> distinct;
    for(const auto& pulse:result.pulses)
        if(!distinct.emplace(pulse.tick,pulse.neuron,pulse.current).second)
            throw std::runtime_error("duplicate atlas pulse");
    return result;
}
struct Trace {
    std::vector<std::string> roots;
    std::vector<std::vector<unsigned char>> spikes;
    std::vector<std::uint64_t> counts;
};
Trace trace(const std::string& receipt, std::uint64_t ticks) {
    if(!receipt.starts_with("NEUROMANTLE_RECEIPT_V1\n"))
        throw std::runtime_error("atlas candidate was refused");
    Trace result; result.counts.resize(1024,0);
    std::istringstream source(receipt);std::string line;
    while(std::getline(source,line)) {
        if(!line.starts_with("tick=")) continue;
        auto separator=line.find(';');
        if(separator==std::string::npos ||
           line.substr(5,separator-5)!=std::to_string(result.roots.size()))
            throw std::runtime_error("invalid atlas receipt tick");
        auto state=line.find(";state=");
        if(state==std::string::npos || state+7+64>=line.size() || line[state+7+64]!=';')
            throw std::runtime_error("invalid atlas state commitment");
        auto root=line.substr(state+7,64);
        if(!std::all_of(root.begin(),root.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))
            throw std::runtime_error("invalid atlas state digest");
        auto spike=line.find(";spikes=");
        auto modules=line.find(";modules=",spike);
        if(spike==std::string::npos || modules==std::string::npos || modules>state)
            throw std::runtime_error("invalid atlas spikes");
        std::vector<unsigned char> fired(1024,0);
        auto list=line.substr(spike+8,modules-(spike+8));
        std::istringstream items(list);std::string item;
        while(std::getline(items,item,',')) {
            if(item.empty())continue;
            auto arrow=item.find('>');
            if(arrow==std::string::npos)throw std::runtime_error("invalid atlas spike token");
            auto id=number<std::uint16_t>(item.substr(0,arrow));
            if(id>=1024 || fired[id])throw std::runtime_error("invalid atlas spike ID");
            fired[id]=1; ++result.counts[id];
        }
        result.roots.push_back(std::move(root));result.spikes.push_back(std::move(fired));
    }
    if(result.roots.size()!=ticks)throw std::runtime_error("atlas receipt duration mismatch");
    return result;
}
struct Arm {
    Pulse pulse;std::string scenario_hash,receipt_hash,mask;
    std::vector<std::int64_t> spike_delta;
    std::uint64_t divergent_ticks=0;
    std::uint64_t goal_count=0;
    std::size_t first_state=0,first_spike=0,first_goal=0,latency=0;
    bool success=false,frontier=false;
};
struct Pair { std::size_t a,b;std::int64_t interaction;std::uint64_t goal_count;std::string receipt_hash;std::size_t first_goal; };
struct MonotonicGroup {
    std::uint64_t tick;std::uint16_t neuron;
    std::vector<std::size_t> members;
    bool monotone=true;
    std::size_t low=0,high=0;
};
std::string optional(std::size_t at,std::uint64_t ticks) {
    return at==ticks?"none":std::to_string(at);
}
std::string svg_open(unsigned width,unsigned height,const std::string& label,
                     const std::string& root) {
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""+std::to_string(width)+
        "\" height=\""+std::to_string(height)+"\" viewBox=\"0 0 "+std::to_string(width)+
        " "+std::to_string(height)+"\" role=\"img\" aria-label=\""+label+"\">\n"
        "<title>"+label+"</title><desc>Deterministic atlas root "+root+"</desc>\n"
        "<rect width=\"100%\" height=\"100%\" fill=\"#101522\"/>\n";
}
std::string text_at(int x,int y,const std::string& value,int size=13,const std::string& color="#e5eaf2") {
    return "<text x=\""+std::to_string(x)+"\" y=\""+std::to_string(y)+
        "\" fill=\""+color+"\" font-family=\"monospace\" font-size=\""+
        std::to_string(size)+"\">"+value+"</text>\n";
}
std::string rect(int x,int y,int width,int height,const std::string& color) {
    return "<rect x=\""+std::to_string(x)+"\" y=\""+std::to_string(y)+
        "\" width=\""+std::to_string(width)+"\" height=\""+std::to_string(height)+
        "\" fill=\""+color+"\"/>\n";
}
std::string divergence_graph(const std::vector<Arm>& arms,std::uint64_t ticks,
                             const std::string& root) {
    int cell=std::max<int>(1,768/static_cast<int>(ticks));
    int width=300+static_cast<int>(ticks)*cell;
    int height=124+static_cast<int>(arms.size())*32;
    auto svg=svg_open(width,height,"State divergence by pulse and tick",root);
    svg+=text_at(20,30,"NEUROMANTLE 0.4.1  |  RESPONSE ATLAS",18);
    svg+=text_at(20,54,"Each colored cell is a different committed network state.",12);
    svg+=text_at(20,80,"pulse",12);svg+=text_at(250,80,"tick 0",12);
    svg+=text_at(width-90,80,"tick "+std::to_string(ticks-1),12);
    for(size_t i=0;i<arms.size();++i) {
        int y=92+static_cast<int>(i)*32;
        const auto& arm=arms[i];
        svg+=text_at(20,y+14,"#"+std::to_string(i)+" n"+std::to_string(arm.pulse.neuron)+
                     " t"+std::to_string(arm.pulse.tick)+" I"+std::to_string(arm.pulse.current),12);
        for(size_t t=0;t<arm.mask.size();++t)
            svg+=rect(250+static_cast<int>(t)*cell,y,cell,20,
                      arm.mask[t]=='1'?"#40b8c9":"#273447");
        if(arm.success)svg+=rect(width-20,y,8,20,"#ffcc66");
    }
    svg+=text_at(20,height-8,"teal: state differs   gold: spike goal met   root: "+root.substr(0,16),11);
    return svg+"</svg>\n";
}
std::string frontier_graph(const std::vector<Arm>& arms,const Plan& plan,
                           const std::string& root) {
    constexpr int width=980,height=430,left=80,top=90,plot_w=780,plot_h=250;
    auto svg=svg_open(width,height,"Exhaustive pulse search cost latency frontier",root);
    svg+=text_at(20,30,"NEUROMANTLE 0.4.1  |  FINITE SEARCH FRONTIER",18);
    svg+=text_at(20,55,std::string("goal: ")+(plan.goal.evoke?"evoke":"suppress")+
                 " neuron "+std::to_string(plan.goal.neuron)+"  candidates "+std::to_string(arms.size()),12);
    svg+=rect(left,top,plot_w,1,"#7b8da5")+rect(left,top+plot_h,plot_w,1,"#7b8da5");
    svg+=rect(left,top,1,plot_h,"#7b8da5")+rect(left+plot_w,top,1,plot_h,"#7b8da5");
    std::int64_t min_i=std::numeric_limits<std::int64_t>::max(),max_i=0;
    std::uint64_t max_latency=0;int winners=0;
    for(const auto& arm:arms)if(arm.success){
        min_i=std::min(min_i,arm.pulse.current);max_i=std::max(max_i,arm.pulse.current);
        max_latency=std::max(max_latency,static_cast<std::uint64_t>(arm.latency));++winners;
    }
    if(winners)for(size_t i=0;i<arms.size();++i)if(arms[i].success){
        const auto& arm=arms[i];
        auto coordinate=(surreal::BigInt(arm.pulse.current-min_i)*surreal::BigInt(plot_w))/
            surreal::BigInt(std::max<std::int64_t>(1,max_i-min_i));
        int x=left+std::stoi(coordinate.convert_to<std::string>());
        int y=top+plot_h-static_cast<int>((static_cast<std::uint64_t>(arm.latency)*plot_h)/
                                          std::max<std::uint64_t>(1,max_latency));
        std::string color=arm.frontier?"#ffcc66":"#568099";
        svg+="<circle cx=\""+std::to_string(x)+"\" cy=\""+std::to_string(y)+
             "\" r=\"7\" fill=\""+color+"\"/>\n";
        svg+=text_at(x+8,y-7,std::to_string(i),11);
    }
    if(winners){
        svg+=text_at(left,top+plot_h+18,std::to_string(min_i),11);
        svg+=text_at(left+plot_w-35,top+plot_h+18,std::to_string(max_i),11);
        svg+=text_at(left-36,top+7,std::to_string(max_latency),11);
        svg+=text_at(left-25,top+plot_h,"0",11);
    }else svg+=text_at(left+100,top+100,"No declared candidate met the goal.",16);
    svg+=text_at(left,top+plot_h+35,"injected current (integer units)",12);
    svg+=text_at(left+470,top+plot_h+35,"response latency in ticks (vertical)",12);
    svg+=text_at(20,height-18,"gold: nondominated   blue: dominated   successes: "+
                 std::to_string(winners)+"   root: "+root.substr(0,16),11);
    return svg+"</svg>\n";
}
std::string interaction_graph(const std::vector<Arm>& arms,const std::vector<Pair>& pairs,
                              const std::string& root) {
    const int count=static_cast<int>(arms.size());
    const int cell=std::max(32,512/std::max(1,count));
    const int width=220+cell*count,height=155+cell*count;
    auto svg=svg_open(width,height,"Paired pulse spike interaction matrix",root);
    svg+=text_at(20,30,"NEUROMANTLE 0.4.1  |  TWO-PULSE INTERACTIONS",18);
    svg+=text_at(20,52,"I = joint - A - B + baseline target spikes",12);
    for(int i=0;i<count;++i) {
        svg+=text_at(164+cell*i,80,std::to_string(i),12);
        svg+=text_at(110,100+cell*i+cell/2,std::to_string(i),12);
    }
    for(const auto& pair:pairs) {
        const auto color=pair.interaction>0?"#277f78":pair.interaction<0?"#9a526b":"#314153";
        int x=144+static_cast<int>(pair.b)*cell,y=90+static_cast<int>(pair.a)*cell;
        svg+=rect(x,y,cell-3,cell-3,color);
        svg+=text_at(x+5,y+cell/2+5,std::to_string(pair.interaction),12);
        x=144+static_cast<int>(pair.a)*cell;y=90+static_cast<int>(pair.b)*cell;
        svg+=rect(x,y,cell-3,cell-3,color);
        svg+=text_at(x+5,y+cell/2+5,std::to_string(pair.interaction),12);
    }
    svg+=text_at(20,height-13,"green: positive   rose: negative   slate: zero   root: "+root.substr(0,16),11);
    return svg+"</svg>\n";
}
}
Result refusal(const std::string& baseline,const std::string& plan,const std::string& reason) {
    std::string frame="NEUROMANTLE_ATLAS_REFUSAL_V1\nbaseline_scenario="+digest(baseline)+
        "\nplan_sha256="+digest(plan)+"\nreason="+reason+"\n";
    Result result;result.receipt=frame+"atlas_root="+digest(frame)+"\n";return result;
}
Result run(const std::string& baseline,const std::string& plan,std::uint64_t clock_ns,
           std::uint64_t ticks,const std::vector<std::uint16_t>& configured,
           const Executor& execute) {
    try {
        const auto parsed=parse_plan(plan,clock_ns,ticks,configured);
        const auto base_receipt=execute(baseline);
        const auto base=trace(base_receipt,ticks);
        std::uint64_t base_goal_count=0;
        for(auto t=parsed.goal.first;t<=parsed.goal.last;++t)
            base_goal_count+=base.spikes[t][parsed.goal.neuron];
        std::vector<Arm> arms;
        for(const auto& pulse:parsed.pulses) {
            std::string variant=baseline;
            if(!variant.empty() && variant.back()!='\n')variant+='\n';
            variant+="stim "+std::to_string(pulse.tick)+" "+std::to_string(pulse.neuron)+
                " "+std::to_string(pulse.current)+"\n";
            auto variant_receipt=execute(variant);
            auto other=trace(variant_receipt,ticks);
            Arm arm{};arm.pulse=pulse;arm.scenario_hash=digest(variant);
            arm.receipt_hash=digest(variant_receipt);
            arm.first_state=arm.first_spike=arm.first_goal=ticks;
            for(std::size_t t=0;t<ticks;++t) {
                bool different=base.roots[t]!=other.roots[t];
                arm.mask+=different?'1':'0';
                if(different){++arm.divergent_ticks;if(arm.first_state==ticks)arm.first_state=t;}
                if(base.spikes[t]!=other.spikes[t] && arm.first_spike==ticks)arm.first_spike=t;
                if(t>=parsed.goal.first && t<=parsed.goal.last && arm.first_goal==ticks) {
                    bool old=base.spikes[t][parsed.goal.neuron];
                    bool newer=other.spikes[t][parsed.goal.neuron];
                    if(parsed.goal.evoke ? (!old && newer) : (old && !newer))arm.first_goal=t;
                }
                if(t>=parsed.goal.first && t<=parsed.goal.last)
                    arm.goal_count+=other.spikes[t][parsed.goal.neuron];
            }
            arm.success=arm.first_goal!=ticks;
            if(arm.success){
                if(arm.first_goal<pulse.tick)throw std::runtime_error("response precedes perturbation");
                arm.latency=arm.first_goal-pulse.tick;
            }
            for(const auto id:configured)
                arm.spike_delta.push_back(static_cast<std::int64_t>(other.counts[id])-
                                           static_cast<std::int64_t>(base.counts[id]));
            arms.push_back(std::move(arm));
        }
        std::vector<Pair> pairs;
        for(std::size_t i=0;i<arms.size();++i)for(std::size_t j=i+1;j<arms.size();++j) {
            std::string variant=baseline;
            if(!variant.empty() && variant.back()!='\n')variant+='\n';
            for(const auto index:{i,j}) {
                const auto& pulse=arms[index].pulse;
                variant+="stim "+std::to_string(pulse.tick)+" "+std::to_string(pulse.neuron)+
                    " "+std::to_string(pulse.current)+"\n";
            }
            const auto pair_receipt=execute(variant);
            const auto joined=trace(pair_receipt,ticks);
            std::uint64_t count=0;std::size_t first_goal=ticks;
            for(auto t=parsed.goal.first;t<=parsed.goal.last;++t){
                count+=joined.spikes[t][parsed.goal.neuron];
                bool old=base.spikes[t][parsed.goal.neuron],newer=joined.spikes[t][parsed.goal.neuron];
                if(first_goal==ticks && (parsed.goal.evoke?(!old&&newer):(old&&!newer)))first_goal=t;
            }
            auto effect=static_cast<std::int64_t>(count)-static_cast<std::int64_t>(arms[i].goal_count)-
                static_cast<std::int64_t>(arms[j].goal_count)+static_cast<std::int64_t>(base_goal_count);
            pairs.push_back({i,j,effect,count,digest(pair_receipt),first_goal});
        }
        std::vector<MonotonicGroup> groups;
        for(std::size_t i=0;i<arms.size();++i) {
            auto where=std::find_if(groups.begin(),groups.end(),[&](const MonotonicGroup& group){
                return group.tick==arms[i].pulse.tick && group.neuron==arms[i].pulse.neuron;
            });
            if(where==groups.end()){
                MonotonicGroup group{};
                group.tick=arms[i].pulse.tick;group.neuron=arms[i].pulse.neuron;
                groups.push_back(std::move(group));
                where=groups.end()-1;
            }
            where->members.push_back(i);
        }
        for(auto& group:groups) {
            std::sort(group.members.begin(),group.members.end(),[&](std::size_t a,std::size_t b){
                return arms[a].pulse.current<arms[b].pulse.current;
            });
            for(std::size_t low=0;low<group.members.size();++low) {
                if(!arms[group.members[low]].success)continue;
                for(std::size_t high=low+1;high<group.members.size();++high) {
                    if(!arms[group.members[high]].success){
                        group.monotone=false;group.low=group.members[low];group.high=group.members[high];break;
                    }
                }
                if(!group.monotone)break;
            }
        }
        std::size_t best=arms.size();std::vector<std::size_t> frontier;
        for(std::size_t i=0;i<arms.size();++i)if(arms[i].success) {
            if(best==arms.size() || std::tie(arms[i].pulse.current,arms[i].latency,arms[i].pulse.tick,
                   arms[i].pulse.neuron,i) <
               std::tie(arms[best].pulse.current,arms[best].latency,arms[best].pulse.tick,
                   arms[best].pulse.neuron,best))best=i;
            bool dominated=false;
            for(std::size_t j=0;j<arms.size();++j)if(i!=j && arms[j].success){
                const bool lower=arms[j].pulse.current<=arms[i].pulse.current;
                const bool faster=arms[j].latency<=arms[i].latency;
                const bool strict=arms[j].pulse.current<arms[i].pulse.current ||
                                  arms[j].latency<arms[i].latency;
                if(lower&&faster&&strict){dominated=true;break;}
            }
            if(!dominated){arms[i].frontier=true;frontier.push_back(i);}
        }
        // Exact finite protocol synthesis over all singles and unordered pairs.
        std::vector<std::size_t> selected;
        std::tuple<std::uint64_t,std::size_t,std::size_t,std::size_t,std::size_t> score{};
        auto choose=[&](std::vector<std::size_t> members,std::size_t response){
            std::uint64_t cost=0;
            for(auto member:members)cost+=static_cast<std::uint64_t>(arms[member].pulse.current);
            auto candidate=std::make_tuple(cost,members.size(),response,members[0],
                                          members.size()==2?members[1]:arms.size());
            if(selected.empty()||candidate<score){score=candidate;selected=std::move(members);}
        };
        for(std::size_t i=0;i<arms.size();++i)if(arms[i].success)choose({i},arms[i].first_goal);
        for(const auto& pair:pairs)if(pair.first_goal!=ticks)choose({pair.a,pair.b},pair.first_goal);
        std::string protocol,protocol_receipt;
        if(!selected.empty()){
            protocol=baseline;if(!protocol.empty()&&protocol.back()!='\n')protocol+='\n';
            for(auto member:selected){const auto& pulse=arms[member].pulse;
                protocol+="stim "+std::to_string(pulse.tick)+" "+std::to_string(pulse.neuron)+
                          " "+std::to_string(pulse.current)+"\n";}
            protocol_receipt=execute(protocol);
            std::string expected;
            if(selected.size()==1)expected=arms[selected[0]].receipt_hash;
            else for(const auto& pair:pairs)if(pair.a==selected[0]&&pair.b==selected[1])expected=pair.receipt_hash;
            if(expected.empty()||digest(protocol_receipt)!=expected)
                throw std::runtime_error("selected protocol failed repeat execution");
        }
        std::string frame="NEUROMANTLE_ATLAS_V1\nbaseline_scenario="+digest(baseline)+
            "\nplan_sha256="+digest(plan)+"\nbaseline_receipt_sha256="+digest(base_receipt)+
            "\nclock_ns="+std::to_string(clock_ns)+"\nticks="+std::to_string(ticks)+
            "\ngoal="+(parsed.goal.evoke?"evoke":"suppress")+":"+
            std::to_string(parsed.goal.neuron)+":"+std::to_string(parsed.goal.first)+":"+
            std::to_string(parsed.goal.last)+"\nbaseline_goal_spikes="+
            std::to_string(base_goal_count)+"\nneurons=";
        for(size_t j=0;j<configured.size();++j){if(j)frame+=',';frame+=std::to_string(configured[j]);}
        frame+="\narm_count="+std::to_string(arms.size())+"\n";
        for(size_t i=0;i<arms.size();++i){
            const auto& arm=arms[i];
            frame+="arm="+std::to_string(i)+";tick="+std::to_string(arm.pulse.tick)+
                ";neuron="+std::to_string(arm.pulse.neuron)+";current="+
                std::to_string(arm.pulse.current)+";scenario="+arm.scenario_hash+
                ";receipt="+arm.receipt_hash+";first_state="+optional(arm.first_state,ticks)+
                ";divergent_ticks="+std::to_string(arm.divergent_ticks)+
                ";first_spike="+optional(arm.first_spike,ticks)+
                ";goal_spikes="+std::to_string(arm.goal_count)+
                ";first_goal="+optional(arm.first_goal,ticks)+
                ";success="+(arm.success?"1":"0")+";latency="+
                (arm.success?std::to_string(arm.latency):"none")+";frontier="+
                (arm.frontier?"1":"0")+";mask="+arm.mask+";spike_delta=";
            for(size_t j=0;j<configured.size();++j){
                if(j)frame+=',';
                frame+=std::to_string(configured[j])+":"+std::to_string(arm.spike_delta[j]);
            }
            frame+='\n';
        }
        frame+="monotonic_groups="+std::to_string(groups.size())+"\n";
        for(const auto& group:groups) {
            frame+="group="+std::to_string(group.tick)+":"+std::to_string(group.neuron)+
                ";sampled="+std::to_string(group.members.size())+
                ";nondecreasing_success="+(group.monotone?"1":"0")+
                ";inversion="+(group.monotone?"none":std::to_string(group.low)+":"+
                std::to_string(group.high))+"\n";
        }
        frame+="pair_count="+std::to_string(pairs.size())+"\n";
        for(const auto& pair:pairs)
            frame+="pair="+std::to_string(pair.a)+":"+std::to_string(pair.b)+
                ";goal_spikes="+std::to_string(pair.goal_count)+
                ";interaction="+std::to_string(pair.interaction)+
                ";first_goal="+optional(pair.first_goal,ticks)+
                ";receipt="+pair.receipt_hash+"\n";
        frame+="best="+(best==arms.size()?"none":std::to_string(best))+"\nfrontier=";
        for(size_t i=0;i<frontier.size();++i){if(i)frame+=',';frame+=std::to_string(frontier[i]);}
        frame+="\nsolution=";
        if(selected.empty())frame+="none\n";
        else {
            for(std::size_t i=0;i<selected.size();++i){if(i)frame+=',';frame+=std::to_string(selected[i]);}
            frame+="\nsolution_cost="+std::to_string(std::get<0>(score))+
                "\nsolution_response_tick="+std::to_string(std::get<2>(score))+
                "\nsolution_scenario_sha256="+digest(protocol)+
                "\nsolution_receipt_sha256="+digest(protocol_receipt)+"\n";
        }
        const auto root=digest(frame);
        Result result;result.receipt=frame+"atlas_root="+root+"\n";
        result.divergence_svg=divergence_graph(arms,ticks,root);
        result.frontier_svg=frontier_graph(arms,parsed,root);
        result.interaction_svg=interaction_graph(arms,pairs,root);
        result.protocol=std::move(protocol);result.protocol_receipt=std::move(protocol_receipt);
        result.has_solution=!selected.empty();
        result.admitted=true;
        return result;
    } catch(const std::exception& e) {return refusal(baseline,plan,e.what());}
}
}
