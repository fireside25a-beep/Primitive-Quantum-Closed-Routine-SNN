#include "world.hpp"
#include "observer.hpp"
#include "neuro_world.h"
#include "neuro_improve.h"
#include "qecc_gate.hpp"
#include "surreal/causal.hpp"
#include "surreal/entropy.hpp"
#include "surreal/hash.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace nm_world_cli {
namespace {
using U64=std::uint64_t;
using I64=std::int64_t;
struct Neuron {std::uint16_t id;I64 reset,threshold;std::uint32_t leak,refractory;};
struct Edge {std::uint16_t a,b;std::int32_t weight,lo,hi;std::uint16_t delay,plus,minus,window;};
struct Field {U64 mass,threshold;std::uint16_t sensor;I64 current;};
struct Stim {U64 tick;std::uint16_t neuron;I64 current;bool noise;};
struct Trial {U64 source,target;I64 current;};
struct ReadoutFlip {U64 tick;unsigned rule,round,generator;};
struct Config {
    U64 clock=0,seed=0,steps=0;unsigned qubits=0,goal_node=0,goal_neuron=0;
    bool have_seed=false,have_goal=false,five_code=false;
    std::string certificate_label,certificate_hash,certificate_proof;
    std::vector<Neuron> neurons;std::vector<Edge> edges;std::vector<Field> fields;
    std::vector<nm_world_link> links;std::vector<nm_world_rule> rules;
    std::vector<Stim> stimuli,pulses;std::vector<Trial> train,validation,holdout;
    std::vector<ReadoutFlip> readout_flips;
};
template<class T>T number(std::istringstream &in){
    std::string s;if(!(in>>s))throw std::runtime_error("missing integer");
    T x{};auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),x);
    if(e!=std::errc{}||p!=s.data()+s.size())throw std::runtime_error("invalid integer: "+s);
    return x;
}
void finish(std::istringstream &in){std::string x;if(in>>x)throw std::runtime_error("extra argument");}
std::string hash(const std::string &s){return surreal::hex(surreal::sha256(s));}
U64 little64(const std::string &s,size_t offset){
    if(offset+8>s.size())throw std::runtime_error("short QECC certificate");
    U64 v=0;for(unsigned i=0;i<8;i++)v|=(U64)(unsigned char)s[offset+i]<<(8*i);
    return v;
}
void admit_five_certificate(Config &s){
    auto path=std::filesystem::canonical(s.certificate_label);
    if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)!=144)
        throw std::runtime_error("invalid five-qubit certificate file");
    std::ifstream stream(path,std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(stream)),{});
    if(bytes.size()!=144||bytes.substr(0,8)!="SQECC001"||
       little64(bytes,8)!=1||little64(bytes,16)!=5||little64(bytes,24)!=4||
       little64(bytes,32)!=31||little64(bytes,40)!=0||
       little64(bytes,48)!=0||little64(bytes,56)!=31)
        throw std::runtime_error("certificate differs from encoded five-qubit code");
    constexpr U64 x[4]={9,18,5,10},z[4]={6,12,24,17};
    for(unsigned i=0;i<4;i++)if(little64(bytes,80+16*i)!=x[i]||
                                little64(bytes,88+16*i)!=z[i])
        throw std::runtime_error("certificate generator differs from runtime stabilizer");
    s.certificate_proof=nm_verified_qecc_proof(path.string());
    s.certificate_hash=hash(bytes);
}
Config parse(const std::string &text){
    if(text.size()>1048576)throw std::runtime_error("world scenario exceeds one MiB");
    Config s;std::istringstream source(text);std::string line;unsigned line_no=0;
    while(std::getline(source,line)){
        ++line_no;std::istringstream in(line);std::string op;
        if(!(in>>op)||op[0]=='#')continue;
        try{
            if(op=="clock_ns"){
                if(s.clock)throw std::runtime_error("duplicate clock");
                s.clock=number<U64>(in);finish(in);
            }else if(op=="seed"){
                if(s.have_seed)throw std::runtime_error("duplicate seed");
                s.seed=number<U64>(in);s.have_seed=true;finish(in);
            }else if(op=="qubits"){
                if(s.qubits)throw std::runtime_error("duplicate qubits");
                s.qubits=number<unsigned>(in);finish(in);
            }else if(op=="steps"){
                if(s.steps)throw std::runtime_error("duplicate steps");
                s.steps=number<U64>(in);finish(in);
            }else if(op=="qecc_certificate"){
                if(!s.certificate_label.empty())throw std::runtime_error("duplicate QECC certificate");
                if(!(in>>s.certificate_label))throw std::runtime_error("missing certificate path");
                finish(in);
                for(unsigned char c:s.certificate_label)if(c<32||c==127||c==':')
                    throw std::runtime_error("invalid certificate path label");
                s.five_code=true;
            }else if(op=="neuron"){
                Neuron n{number<std::uint16_t>(in),number<I64>(in),number<I64>(in),
                         number<std::uint32_t>(in),number<std::uint32_t>(in)};
                finish(in);s.neurons.push_back(n);
            }else if(op=="edge"){
                Edge e{};e.a=number<std::uint16_t>(in);e.b=number<std::uint16_t>(in);
                e.weight=number<std::int32_t>(in);e.delay=number<std::uint16_t>(in);
                e.plus=number<std::uint16_t>(in);e.minus=number<std::uint16_t>(in);
                e.window=number<std::uint16_t>(in);e.lo=number<std::int32_t>(in);
                e.hi=number<std::int32_t>(in);finish(in);s.edges.push_back(e);
            }else if(op=="field"){
                Field f{number<U64>(in),number<U64>(in),number<std::uint16_t>(in),number<I64>(in)};
                finish(in);s.fields.push_back(f);
            }else if(op=="flux"){
                nm_world_link e{};e.from=number<std::uint16_t>(in);e.to=number<std::uint16_t>(in);
                e.gate_neuron=number<std::uint16_t>(in);e.context_node=number<std::uint16_t>(in);
                e.rate_q16=number<std::uint32_t>(in);e.bonus_q16=number<std::uint32_t>(in);
                e.cap=number<U64>(in);e.context_threshold=number<U64>(in);
                finish(in);s.links.push_back(e);
            }else if(op=="qrule"){
                nm_world_rule r{};r.trigger=number<std::uint16_t>(in);
                std::string gate;if(!(in>>gate)||gate.size()!=1)throw std::runtime_error("invalid quantum gate");
                r.op=gate[0];r.a=number<std::uint16_t>(in);r.b=number<std::uint16_t>(in);
                r.output_neuron=number<std::uint16_t>(in);r.output_current=number<I64>(in);
                finish(in);s.rules.push_back(r);
            }else if(op=="qreadout_flip"){
                ReadoutFlip f{number<U64>(in),number<unsigned>(in),
                              number<unsigned>(in),number<unsigned>(in)};
                finish(in);s.readout_flips.push_back(f);
            }else if(op=="stim"||op=="field_noise"||op=="pulse"){
                Stim x{number<U64>(in),number<std::uint16_t>(in),number<I64>(in),op=="field_noise"};
                finish(in);if(x.current<0)throw std::runtime_error("negative current");
                (op=="pulse"?s.pulses:s.stimuli).push_back(x);
            }else if(op=="goal"){
                if(s.have_goal)throw std::runtime_error("duplicate goal");
                s.goal_node=number<unsigned>(in);s.goal_neuron=number<unsigned>(in);
                s.have_goal=true;finish(in);
            }else if(op=="train_case"||op=="validation_case"||op=="holdout_case"){
                Trial c{number<U64>(in),number<U64>(in),number<I64>(in)};
                finish(in);if(c.current<0)throw std::runtime_error("negative trial stimulus");
                (op=="train_case"?s.train:op=="validation_case"?s.validation:s.holdout).push_back(c);
            }else throw std::runtime_error("unknown world directive: "+op);
        }catch(const std::exception &e){throw std::runtime_error("world line "+std::to_string(line_no)+": "+e.what());}
    }
    if(!s.clock||s.clock>1000000000u||!s.have_seed||!s.qubits||s.qubits>PQ_MAX_QUBITS||
       !s.steps||s.steps>128||s.neurons.empty()||s.neurons.size()>NM_MAX_NEURONS||
       s.edges.size()>NM_MAX_EDGES||s.fields.empty()||s.fields.size()>NM_WORLD_NODES||
       s.links.empty()||s.links.size()>NM_WORLD_LINKS||s.rules.size()>NM_WORLD_RULES||
       s.stimuli.size()>128||s.pulses.size()>3||s.train.size()>8||s.validation.size()>8||s.holdout.size()>8||
       s.readout_flips.size()>128)
        throw std::runtime_error("world declaration missing or capacity exceeded");
    if(s.have_goal&&(s.goal_node>=s.fields.size()||s.goal_neuron>=NM_MAX_NEURONS))
        throw std::runtime_error("invalid goal");
    if(s.five_code){
        if(s.qubits!=5)throw std::runtime_error("certified code needs exactly five qubits");
        admit_five_certificate(s);
    }
    for(size_t i=0;i<s.readout_flips.size();i++){
        const auto &f=s.readout_flips[i];
        if(!s.five_code||f.tick>=s.steps||f.rule>=s.rules.size()||
           s.rules[f.rule].op!='Q'||f.round>=3||f.generator>=4)
            throw std::runtime_error("invalid Q readout fault declaration");
        for(size_t j=0;j<i;j++)if(s.readout_flips[j].tick==f.tick&&
                                  s.readout_flips[j].rule==f.rule)
            throw std::runtime_error("multiple readout faults for one Q rule");
    }
    for(const auto &x:s.stimuli)if(x.tick>=s.steps)throw std::runtime_error("world stimulus beyond duration");
    for(const auto &x:s.pulses)if(x.tick>=s.steps)throw std::runtime_error("world pulse beyond duration");
    for(const auto &c:s.train)if(c.source>NM_WORLD_MASS_MAX||c.target>NM_WORLD_MASS_MAX)
        throw std::runtime_error("training mass out of range");
    for(const auto &c:s.validation)if(c.source>NM_WORLD_MASS_MAX||c.target>NM_WORLD_MASS_MAX)
        throw std::runtime_error("validation mass out of range");
    for(const auto &c:s.holdout)if(c.source>NM_WORLD_MASS_MAX||c.target>NM_WORLD_MASS_MAX)
        throw std::runtime_error("holdout mass out of range");
    return s;
}
using World=std::unique_ptr<nm_world,decltype(&nm_world_free)>;
World instantiate(const Config &s,const Trial *trial,const std::int32_t *parameters=nullptr){
    World w(nm_world_new(s.qubits,s.clock),&nm_world_free);
    if(!w)throw std::runtime_error("cannot allocate coupled world");
    if(s.five_code&&nm_world_enable_five_code(w.get()))
        throw std::runtime_error("cannot prepare encoded logical zero");
    for(const auto &n:s.neurons)if(nm_add_neuron(w->network,n.id,n.reset,n.threshold,n.leak,n.refractory))
        throw std::runtime_error("invalid coupled neuron");
    for(const auto &e:s.edges)if(nm_add_edge(w->network,e.a,e.b,e.weight,e.delay,
                                           e.plus,e.minus,e.window,e.lo,e.hi))
        throw std::runtime_error("invalid coupled synapse");
    for(size_t i=0;i<s.fields.size();i++){
        auto f=s.fields[i];if(trial&&i<2)f.mass=i==0?trial->source:trial->target;
        if(nm_world_add_node(w.get(),f.mass,f.sensor,f.threshold,f.current))
            throw std::runtime_error("invalid field node");
    }
    for(size_t i=0;i<s.links.size();i++){
        auto link=s.links[i];
        if(parameters){link.rate_q16=(std::uint32_t)parameters[2*i];link.bonus_q16=(std::uint32_t)parameters[2*i+1];}
        if(nm_world_add_link(w.get(),link))throw std::runtime_error("invalid conservative link");
    }
    for(auto rule:s.rules)if(nm_world_add_rule(w.get(),rule))
        throw std::runtime_error("invalid quantum rule");
    if(s.have_goal&&!w->network->neurons[s.goal_neuron].configured)
        throw std::runtime_error("goal neuron is not configured");
    return w;
}
std::string state(const nm_world &w){
    std::string data="NM_COUPLED_STATE_V1\ntick="+std::to_string(w.network->tick)+"\n";
    for(unsigned i=0;i<NM_MAX_NEURONS;i++)if(const auto &n=w.network->neurons[i];n.configured)
        data+="n="+std::to_string(i)+":"+std::to_string(n.voltage)+":"+
              std::to_string(n.remaining)+":"+std::to_string(n.spikes)+":"+
              std::to_string(n.last_spike)+"\n";
    for(size_t i=0;i<w.network->edge_count;i++)data+="e="+std::to_string(i)+":"+
        std::to_string(w.network->edges[i].weight)+"\n";
    for(unsigned t=0;t<=NM_MAX_DELAY;t++)for(unsigned i=0;i<NM_MAX_NEURONS;i++)
        if(auto v=w.network->pending[t][i];v) data+="p="+std::to_string(t)+":"+
              std::to_string(i)+":"+std::to_string(v)+"\n";
    for(unsigned i=0;i<w.node_count;i++)data+="f="+std::to_string(i)+":"+
        std::to_string(w.nodes[i].mass)+"\n";
    for(unsigned i=0;i<2u*w.quantum.n;i++){
        auto r=w.quantum.rows[i];data+="q="+std::to_string(i)+":"+
            std::to_string(r.x)+":"+std::to_string(r.z)+":"+
            std::to_string(r.phase)+"\n";
    }
    if(w.five_code)data+="code5=1:faults="+std::to_string(w.five_faults)+"\n";
    return hash(data);
}
struct Sample {U64 tick,field,flux;std::size_t spikes;unsigned measured;};
struct Run {std::string receipt;std::vector<Sample> samples;U64 final_mass=0,goal_spikes=0;};
Run execute(const Config &s,const std::string &original,const Trial *trial=nullptr,
            const std::int32_t *parameters=nullptr,int pulse_mask=-1){
    auto w=instantiate(s,trial,parameters);
    const auto scenario_digest=hash(original);
    nm_observer::Observer observer;observer.begin("world",scenario_digest);
    surreal::EntropyLedger quantum(s.seed,UINT64_C(0x5051));
    surreal::EntropyLedger field(s.seed,UINT64_C(0x464c));
    surreal::CausalGraph graph;
    Run run;std::string &receipt=run.receipt;
    receipt=(s.five_code?"NEUROMANTLE_COUPLED_QECC5_V1\n":"NEUROMANTLE_COUPLED_V1\n");
    receipt+="scenario="+scenario_digest+"\nclock_ns="+
        std::to_string(s.clock)+"\nseed="+std::to_string(s.seed)+"\n";
    if(s.five_code)receipt+="qecc_certificate="+s.certificate_label+":"+
        s.certificate_hash+":"+s.certificate_proof+"\n";
    if(trial)receipt+="trial="+std::to_string(trial->source)+":"+
        std::to_string(trial->target)+":"+std::to_string(trial->current)+"\n";
    if(pulse_mask>=0)receipt+="pulse_mask="+std::to_string(pulse_mask)+"\n";
    if(parameters){
        std::string vector="";
        for(size_t i=0;i<2*s.links.size();i++)
            vector+=std::to_string(parameters[i])+",";
        receipt+="parameter_root="+hash(vector)+"\n";
    }
    std::string parent;
    for(U64 tick=0;tick<s.steps;tick++){
        nm_world_input input{};bool has_input=false;
        if(trial && tick==0){input.stimulus_neuron=s.neurons.front().id;
            input.stimulus_current=trial->current;has_input=true;}
        const auto &chosen=pulse_mask>=0?s.pulses:s.stimuli;
        for(size_t idx=0;idx<chosen.size()&&(!trial||pulse_mask>=0);idx++){
            const auto &x=chosen[idx];
            if(x.tick!=tick||(pulse_mask>=0&&!(pulse_mask&(1<<idx))))continue;
            I64 current=x.current;
            if(x.noise)current=(I64)(field.draw()%((U64)x.current+1u));
            if(has_input&&input.stimulus_neuron!=x.neuron)
                throw std::runtime_error("multiple world stimulus targets in one tick");
            if(has_input&&__builtin_add_overflow(input.stimulus_current,current,&input.stimulus_current))
                throw std::runtime_error("coupled stimulus overflow");
            if(!has_input){input.stimulus_neuron=x.neuron;input.stimulus_current=current;has_input=true;}
        }
        for(unsigned i=0;i<w->rule_count;i++)if(w->rules[i].op=='M'||w->rules[i].op=='R')
            input.draws[i]=quantum.draw();
        for(const auto &f:s.readout_flips)if(f.tick==tick)
            input.readout_flip[f.rule]=static_cast<std::uint8_t>(1u+4u*f.round+f.generator);
        auto before=state(*w);nm_world_report report{};
        if(nm_world_step(w.get(),&input,&report))throw std::runtime_error("coupled transition refused");
        auto after=state(*w);
        auto eroot=hash(quantum.root()+"|"+field.root());
        surreal::Event event;
        event.local_time=surreal::Rational(static_cast<long long>(tick*s.clock));
        event.region="coupled";event.law=s.five_code?
            "nm.certified_qecc5_lif_v1":"nm.conservative_clifford_lif_v1";
        event.before_hash=before;event.after_hash=after;
        event.invariant_hash=hash(std::to_string(report.mass_before)+":"+
            std::to_string(report.mass_after)+":"+std::to_string(report.flux_total));
        event.entropy_root=eroot;if(!parent.empty())event.parents.push_back(parent);
        parent=graph.append(std::move(event));
        std::string fired;for(size_t i=0;i<report.spikes.count;i++){
            if(i)fired+=",";
            fired+=std::to_string(report.spikes.ids[i]);
        }
        const auto observer_offset=receipt.size();
        receipt+="tick="+std::to_string(tick)+";time_ns="+
            std::to_string(tick*s.clock)+";mass="+std::to_string(report.mass_after)+
            ";target="+std::to_string(w->nodes[s.have_goal?s.goal_node:0].mass)+
            ";flux="+std::to_string(report.flux_total)+";spikes="+fired+
            ";rules="+std::to_string(report.fired_rules)+";measure="+
            std::to_string(report.measurement_rules)+":"+
            std::to_string(report.measurement_ones)+":"+
            std::to_string(report.random_rules);
        if(s.five_code){
            receipt+=";corrected="+std::to_string(report.corrected_rules)+";syndrome=";
            for(unsigned i=0;i<w->rule_count;i++)if(w->rules[i].op=='Q')
                receipt+=std::to_string(i)+":"+std::to_string(report.syndromes[i])+":"+
                    std::to_string(report.corrections[i])+",";
            for(unsigned i=0;i<w->rule_count;i++)if(input.readout_flip[i]){
                auto flip=input.readout_flip[i]-1u;
                receipt+=";readout_flip="+std::to_string(i)+":"+
                    std::to_string(flip/4u)+":"+std::to_string(flip%4u);
            }
        }
        receipt+=";state="+after+";entropy="+eroot+";event="+parent+"\n";
        observer.tick(tick,std::string_view(receipt).substr(observer_offset));
        run.samples.push_back({tick,w->nodes[s.have_goal?s.goal_node:0].mass,
                               report.flux_total,report.spikes.count,report.measurement_ones});
    }
    run.final_mass=w->nodes[s.have_goal?s.goal_node:0].mass;
    if(s.have_goal)run.goal_spikes=w->network->neurons[s.goal_neuron].spikes;
    receipt+="causal_root="+graph.root_hash()+"\nquantum_entropy_root="+
        quantum.root()+"\nfield_entropy_root="+field.root()+"\n";
    receipt+="receipt_root="+hash(receipt)+"\n";
    observer.finish(std::string_view(receipt).substr(receipt.rfind("receipt_root=")));
    return run;
}
std::string svg(const Run &run,bool five_code){
    U64 maximum=1;for(auto row:run.samples)maximum=std::max(maximum,row.field);
    const unsigned width=800,height=300;
    std::string graph="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"300\" viewBox=\"0 0 800 300\">\n";
    graph+=five_code?"<title>Certified five-qubit syndrome and SNN feedback</title>\n":
        "<title>Executed coupled field, spike, and PrimitiveQ measurements</title>\n";
    graph+="<rect width=\"800\" height=\"300\" fill=\"#101b2c\"/>\n";
    graph+="<path d=\"M 48 30 V 240 H 780\" stroke=\"#91a6bb\" fill=\"none\"/>\n";
    std::string path="M ";
    for(size_t i=0;i<run.samples.size();i++){
        const auto &r=run.samples[i];unsigned x=48+(unsigned)(i*720/(run.samples.size()>1?run.samples.size()-1:1));
        unsigned y=240-(unsigned)(r.field*180/maximum);
        if(i)path+=" L ";
        path+=std::to_string(x)+" "+std::to_string(y);
        if(r.spikes)graph+="<circle cx=\""+std::to_string(x)+"\" cy=\"262\" r=\"3\" fill=\"#f7bd67\"/>\n";
        if(r.measured)graph+="<circle cx=\""+std::to_string(x)+"\" cy=\"277\" r=\"3\" fill=\"#a891ff\"/>\n";
    }
    graph+="<path d=\""+path+"\" stroke=\"#57d6af\" stroke-width=\"3\" fill=\"none\"/>\n";
    graph+=five_code?
        "<text x=\"48\" y=\"22\" fill=\"white\">target mass; orange: spikes; violet: nonzero QECC syndrome</text>\n":
        "<text x=\"48\" y=\"22\" fill=\"white\">target mass; orange: spikes; violet: measured one</text>\n";
    graph+="<text x=\"48\" y=\"291\" fill=\"#b7c9dc\">receipt SHA-256: "+hash(run.receipt)+"</text>\n</svg>\n";
    (void)width;(void)height;return graph;
}
Result refusal(const std::string &original,const std::string &reason){
    Result r;r.receipt="NEUROMANTLE_COUPLED_REFUSAL_V1\nscenario="+hash(original)+
        "\nreason="+reason+"\n";
    r.receipt+="receipt_root="+hash(r.receipt)+"\n";return r;
}
struct SearchContext {const Config *config;const std::string *original;std::string reason;};
int evaluate(const std::int32_t *parameters,std::uint32_t count,unsigned phase,void *opaque,nm_improve_score *out){
    auto *context=static_cast<SearchContext*>(opaque);const auto &s=*context->config;
    if(count!=2*s.links.size()||phase>1||!out)return -1;
    try{
        const auto &cases=phase?s.validation:s.train;
        I64 score=0;U64 total_spikes=0;
        for(const auto &c:cases){
            auto run=execute(s,*context->original,&c,parameters);
            if(run.final_mass>1000000000000ULL||run.goal_spikes>1000000ULL)return -1;
            I64 value=(I64)run.final_mass*100+(I64)run.goal_spikes*10;
            if(__builtin_add_overflow(score,value,&score))return -1;
            total_spikes+=run.goal_spikes;
        }
        *out={score,(std::uint32_t)(std::min<U64>(total_spikes,15u))};return 0;
    }catch(const std::exception &e){context->reason=e.what();return -1;}
}
std::string search_graph(const nm_improve_trace *trace,unsigned count,const std::string &root){
    I64 lo=trace[0].champion_validation,hi=lo;
    for(unsigned i=0;i<count;i++){lo=std::min(lo,trace[i].champion_validation);hi=std::max(hi,trace[i].champion_validation);}
    std::string points;
    for(unsigned i=0;i<count;i++){
        unsigned x=50+(i*700)/(count>1?count-1:1);
        unsigned y=220-(unsigned)((trace[i].champion_validation-lo)*160/(hi-lo+1));
        points+=std::to_string(x)+","+std::to_string(y)+" ";
    }
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"260\">\n"
           "<title>Executed native improvement: validation champion</title>\n"
           "<rect width=\"800\" height=\"260\" fill=\"#101b2c\"/>\n"
           "<polyline fill=\"none\" stroke=\"#57d6af\" stroke-width=\"3\" points=\""+points+"\"/>\n"
           "<text x=\"50\" y=\"25\" fill=\"white\">Validated score across actual coupled runs</text>\n"
           "<text x=\"50\" y=\"250\" fill=\"white\">search receipt: "+root+"</text>\n</svg>\n";
}
} // namespace

Result run(const std::string &scenario){
    try {auto s=parse(scenario);auto done=execute(s,scenario);
        return {done.receipt,svg(done,s.five_code),true};
    }catch(const std::exception &e){return refusal(scenario,e.what());}
}
Result improve(const std::string &scenario){
    try{
        auto s=parse(scenario);
        if(!s.have_goal||s.fields.size()<2||s.links.size()>16||
           s.train.empty()||s.validation.empty()||s.holdout.empty())
            throw std::runtime_error("world improvement requires a goal, field pair, and separated trials");
        nm_improve_spec spec{};spec.count=(std::uint32_t)(2*s.links.size());
        spec.budget=64;spec.seed=(std::uint32_t)(s.seed?s.seed:0x43a5b92du);spec.group_width=2;
        for(size_t i=0;i<s.links.size();i++){
            spec.initial[2*i]=(std::int32_t)s.links[i].rate_q16;
            spec.initial[2*i+1]=(std::int32_t)s.links[i].bonus_q16;
        }
        for(unsigned i=0;i<spec.count;i++){spec.lower[i]=0;spec.upper[i]=65536;}
        auto traces=std::make_unique<nm_improve_trace[]>(spec.budget);
        auto result=std::make_unique<nm_improve_result>();
        SearchContext ctx{&s,&scenario,{}};
        if(nm_improve_run(&spec,evaluate,&ctx,result.get(),traces.get(),spec.budget))
            throw std::runtime_error("native improvement refused: "+ctx.reason);
        std::string receipt="NEUROMANTLE_COUPLED_SEARCH_V1\nscenario="+hash(scenario)+
            "\nseed="+std::to_string(spec.seed)+"\ntrain="+
            std::to_string(result->train_score)+"\nvalidation="+
            std::to_string(result->validation_score)+"\naccepted="+
            std::to_string(result->admitted)+"\narchive="+
            std::to_string(result->archive_occupied)+"\n";
        for(unsigned j=0;j<spec.count;j++)receipt+="parameter="+std::to_string(j)+":"+
            std::to_string(spec.initial[j])+":"+std::to_string(result->parameters[j])+"\n";
        for(unsigned i=0;i<result->attempts;i++){
            const auto &t=traces[i];
            receipt+="attempt="+std::to_string(i)+":"+std::to_string(t.operator_id)+":"+
                std::to_string(t.parent_slot)+":"+std::to_string(t.radius)+":"+
                std::to_string(t.random_state)+":"+std::to_string(t.train_score)+":"+
                std::to_string(t.validation_score)+":"+std::to_string(t.champion_train)+":"+
                std::to_string(t.champion_validation)+":"+std::to_string(t.behavior)+":"+
                std::to_string(t.admitted)+"\n";
        }
        for(size_t i=0;i<s.holdout.size();i++){
            auto before=execute(s,scenario,&s.holdout[i],spec.initial);
            auto after=execute(s,scenario,&s.holdout[i],result->parameters);
            receipt+="holdout="+std::to_string(i)+":"+std::to_string(before.final_mass)+":"+
                std::to_string(after.final_mass)+":"+std::to_string(before.goal_spikes)+":"+
                std::to_string(after.goal_spikes)+":"+hash(before.receipt)+":"+
                hash(after.receipt)+"\n";
        }
        receipt+="receipt_root="+hash(receipt)+"\n";
        return {receipt,search_graph(traces.get(),result->attempts,hash(receipt)),true};
    }catch(const std::exception &e){return refusal(scenario,e.what());}
}
Result interaction(const std::string &scenario){
    try{
        auto s=parse(scenario);
        if(s.pulses.size()!=3||!s.have_goal)
            throw std::runtime_error("third-order experiment requires exactly three pulses and a goal");
        std::array<Run,8> runs;
        for(unsigned mask=0;mask<8;mask++)runs[mask]=execute(s,scenario,nullptr,nullptr,(int)mask);
        auto metric=[&](unsigned i)->I64{return static_cast<I64>(runs[i].final_mass);};
        auto spike=[&](unsigned i)->I64{return static_cast<I64>(runs[i].goal_spikes);};
        auto d=[&](auto f,unsigned mask)->I64{
            I64 value=0;
            for(unsigned subset=mask;;subset=(subset-1u)&mask){
                unsigned parity=(unsigned)__builtin_popcount(mask^subset)&1u;
                value+=parity?-f(subset):f(subset);
                if(!subset)break;
            }
            return value;
        };
        std::string receipt="NEUROMANTLE_TRIAD_V1\nscenario="+hash(scenario)+"\n";
        for(unsigned mask=0;mask<8;mask++)
            receipt+="run="+std::to_string(mask)+":"+std::to_string(runs[mask].final_mass)+
                ":"+std::to_string(runs[mask].goal_spikes)+":"+hash(runs[mask].receipt)+"\n";
        for(unsigned mask=1;mask<8;mask++)
            receipt+="interaction="+std::to_string(mask)+":"+
                std::to_string(d(metric,mask))+":"+std::to_string(d(spike,mask))+"\n";
        receipt+="receipt_root="+hash(receipt)+"\n";
        U64 highest=1;for(const auto &r:runs)highest=std::max(highest,r.final_mass);
        std::string graph="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"900\" height=\"340\" viewBox=\"0 0 900 340\">\n"
            "<title>Eight executed pulse masks and exact third-order interaction</title>\n"
            "<rect width=\"900\" height=\"340\" fill=\"#101b2c\"/>\n"
            "<text x=\"35\" y=\"28\" fill=\"white\">Measured target mass under every three-pulse subset</text>\n";
        for(unsigned mask=0;mask<8;mask++){
            unsigned h=(unsigned)(runs[mask].final_mass*190/highest),x=40+mask*105;
            graph+="<rect x=\""+std::to_string(x)+"\" y=\""+
                std::to_string(245-h)+"\" width=\"62\" height=\""+
                std::to_string(h)+"\" fill=\"#57d6af\"/>\n";
            graph+="<text x=\""+std::to_string(x)+"\" y=\"267\" fill=\"white\">"+
                std::to_string(mask)+"</text>\n";
        }
        graph+="<text x=\"35\" y=\"298\" fill=\"#f7bd67\">third-order field difference: "+
            std::to_string(d(metric,7))+"; goal spike difference: "+
            std::to_string(d(spike,7))+"</text>\n";
        graph+="<text x=\"35\" y=\"324\" fill=\"#b7c9dc\">receipt SHA-256: "+hash(receipt)+
            "</text>\n</svg>\n";
        return {receipt,graph,true};
    }catch(const std::exception &e){return refusal(scenario,e.what());}
}
}
