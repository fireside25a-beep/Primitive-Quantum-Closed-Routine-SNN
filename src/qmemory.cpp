#include "qmemory.hpp"
#include "qmemory_readout.h"
#include "surreal/hash.hpp"
#include <array>
#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace nm_qmemory {
namespace {
std::uint64_t le64(const std::string &bytes,std::size_t at){
    if(at+8>bytes.size())throw std::runtime_error("short QECC certificate");
    std::uint64_t value=0;
    for(unsigned i=0;i<8;i++)value|=std::uint64_t(static_cast<unsigned char>(bytes[at+i]))<<(8*i);
    return value;
}
void append32(std::string &s,std::uint32_t value){
    for(unsigned i=0;i<4;i++)s+=static_cast<char>((value>>(8*i))&255u);
}
}
void verify_certificate_identity(const std::string &bytes){
    if(bytes.size()!=144||bytes.substr(0,8)!="SQECC001"||
       le64(bytes,8)!=1||le64(bytes,16)!=5||le64(bytes,24)!=4||
       le64(bytes,32)!=31||le64(bytes,40)!=0||
       le64(bytes,48)!=0||le64(bytes,56)!=31)
        throw std::runtime_error("QMemory certificate differs from encoded five-qubit code");
    constexpr std::array<unsigned,4> xs={9,18,5,10},zs={6,12,24,17};
    for(unsigned i=0;i<4;i++)if(le64(bytes,80+16*i)!=xs[i]||le64(bytes,88+16*i)!=zs[i])
        throw std::runtime_error("QMemory certificate generator differs from tableau");
}
Weights::Weights(const nm_network &network):edges_(network.edge_count){
    if(!edges_||edges_>NM_MAX_EDGES)throw std::runtime_error("QMemory needs configured synapses");
    cells_.resize(edges_*32u);
    fault_count_.resize(cells_.size());
    capture(network);
}
void Weights::encode_edge(std::size_t edge,std::int32_t weight){
    pq_state zero{};
    if(pq_init_five_code(&zero))throw std::runtime_error("QMemory encoder failed");
    pq_state one=zero;
    for(unsigned q=0;q<5;q++)if(pq_pauli(&one,q,'X'))throw std::runtime_error("QMemory logical X failed");
    std::uint32_t word=static_cast<std::uint32_t>(weight);
    for(unsigned bit=0;bit<32;bit++){
        const auto &codeword=((word>>bit)&1u)?one:zero;
        std::memcpy(cells_[edge*32+bit].rows,codeword.rows,10*sizeof(pq_row));
    }
}
void Weights::capture(const nm_network &network){
    if(network.edge_count!=edges_)throw std::runtime_error("QMemory edge count changed");
    for(std::size_t edge=0;edge<edges_;edge++){
        encode_edge(edge,network.edges[edge].weight);
    }
    std::fill(fault_count_.begin(),fault_count_.end(),0);
    active_faults_=0;
}
void Weights::capture_updated(const nm_network &network,std::span<const std::int32_t> before){
    if(network.edge_count!=edges_||before.size()!=edges_||active_faults_)
        throw std::runtime_error("QMemory updated capture refused");
    for(std::size_t edge=0;edge<edges_;edge++)
        if(network.edges[edge].weight!=before[edge])encode_edge(edge,network.edges[edge].weight);
}
void Weights::fault(std::size_t edge,unsigned bit,unsigned qubit,char op){
    if(edge>=edges_||bit>=32||qubit>=5||(op!='X'&&op!='Y'&&op!='Z'))
        throw std::runtime_error("invalid QMemory fault");
    auto index=edge*32+bit;
    if(fault_count_[index])throw std::runtime_error("QMemory refuses multiple faults in one logical block");
    pq_state state{};state.n=5;
    std::memcpy(state.rows,cells_[index].rows,10*sizeof(pq_row));
    if(pq_pauli(&state,qubit,op))throw std::runtime_error("QMemory Pauli fault failed");
    std::memcpy(cells_[index].rows,state.rows,10*sizeof(pq_row));
    fault_count_[index]=1;
    ++active_faults_;
}
unsigned Weights::restore(nm_network &network){
    if(network.edge_count!=edges_)throw std::runtime_error("QMemory edge count changed");
    // Decode into temporary storage. No synaptic weight changes before every cell passes.
    std::vector<std::pair<std::size_t,Cell>> staged;
    std::vector<std::int8_t> logical(cells_.size());
    std::vector<std::int32_t> weights(edges_);
    const bool observed=active_faults_!=0;
    std::vector<std::int8_t> syndromes,raw_logical,decoded,codes;
    std::vector<std::uint8_t> scalar_codes;
    if(observed){
        syndromes.resize(cells_.size());raw_logical.resize(cells_.size());
        decoded.resize(cells_.size());
        codes.resize(cells_.size());scalar_codes.resize(cells_.size());
    }
    pq_state zero{},one{};
    if(pq_init_five_code(&zero))throw std::runtime_error("QMemory encoder failed");
    one=zero;
    for(unsigned q=0;q<5;q++)if(pq_pauli(&one,q,'X'))throw std::runtime_error("QMemory logical X failed");
    unsigned corrected=0;
    for(std::size_t i=0;i<cells_.size();i++){
        const auto &raw=cells_[i].rows[9];
        if(raw.x||raw.z!=31u||(raw.phase!=0&&raw.phase!=2))
            throw std::runtime_error("QMemory malformed raw logical observation");
        const auto raw_bit=static_cast<std::int8_t>(raw.phase>>1);
        if(observed)raw_logical[i]=raw_bit;
        if(!fault_count_[i]){
            const auto &expected=raw_bit?one:zero;
            if(std::memcmp(cells_[i].rows,expected.rows,10*sizeof(pq_row)))
                throw std::runtime_error("QMemory undeclared tableau change refused");
            logical[i]=raw_bit;
            continue;
        }
        pq_state cell{};cell.n=5;
        std::memcpy(cell.rows,cells_[i].rows,10*sizeof(pq_row));
        unsigned circuit_bit=0;
        if(pq_five_logical_z_circuit(&cell,&circuit_bit)||circuit_bit!=static_cast<unsigned>(raw_bit))
            throw std::runtime_error("QMemory ancilla logical readout refused");
        raw_logical[i]=static_cast<std::int8_t>(circuit_bit);
        unsigned syndrome=0,code=0;
        if(pq_five_correct(&cell,&syndrome,&code))throw std::runtime_error("QMemory syndrome refused");
        syndromes[i]=static_cast<std::int8_t>(syndrome);
        scalar_codes[i]=static_cast<std::uint8_t>(code);
        if(code){
            if(fault_count_[i]!=1)throw std::runtime_error("QMemory unexpected syndrome refused");
            corrected++;
        }else if(fault_count_[i])throw std::runtime_error("QMemory declared fault had no syndrome");
        if(fault_count_[i]){
            Cell corrected_cell{};
            std::memcpy(corrected_cell.rows,cell.rows,10*sizeof(pq_row));
            staged.emplace_back(i,corrected_cell);
        }
        const auto &r=cell.rows[9];
        if(r.x||r.z!=31u||(r.phase!=0&&r.phase!=2))
            throw std::runtime_error("QMemory logical operator refused");
        logical[i]=static_cast<std::int8_t>(r.phase>>1);
    }
    int status=0;
    if(observed){
        nm_qmemory_decode_observed(syndromes.data(),raw_logical.data(),
                                   edges_,decoded.data(),
                                   codes.data(),weights.data(),&status);
        if(!status)for(std::size_t i=0;i<cells_.size();i++)
            if(decoded[i]!=logical[i] || static_cast<std::uint8_t>(codes[i])!=scalar_codes[i])
                throw std::runtime_error("QMemory readout decoder disagrees with tableau");
    }else nm_qmemory_decode(logical.data(),edges_,weights.data(),&status);
    if(status)throw std::runtime_error("QMemory Fortran decoder refused");
    // A one-fault syndrome is only trusted when the corrected tableau returns
    // to the canonical codeword. This also checks the logical sign and duals.
    for(const auto &[index,cell]:staged){
        const auto &expected=logical[index]?one:zero;
        if(std::memcmp(cell.rows,expected.rows,10*sizeof(pq_row)))
            throw std::runtime_error("QMemory recovery differs from encoded codeword");
    }
    for(const auto &[index,cell]:staged)cells_[index]=cell;
    for(std::size_t edge=0;edge<edges_;edge++)network.edges[edge].weight=weights[edge];
    std::fill(fault_count_.begin(),fault_count_.end(),0);
    active_faults_=0;
    return corrected;
}
TickReport Weights::step(nm_world &world,const nm_world_input &input,
                         std::span<const Fault> faults){
    if(!world.network||!world.five_code||world.quantum.n!=5||
       world.network->edge_count!=edges_)
        throw std::runtime_error("combined tick requires certified world and matching synapses");
    auto network=std::make_unique<nm_network>(*world.network);
    nm_world staged=world;staged.network=network.get();
    Weights memory=*this;
    TickReport result;result.memory_before=root();
    for(const auto &fault:faults){
        if(fault.tick!=network->tick)
            throw std::runtime_error("combined memory fault has wrong tick");
        memory.fault(fault.edge,fault.bit,fault.qubit,fault.op);
    }
    result.memory_corrected=memory.restore(*network);
    std::vector<std::int32_t> before;
    before.reserve(edges_);
    for(std::size_t i=0;i<edges_;i++)before.push_back(network->edges[i].weight);
    if(nm_world_step(&staged,&input,&result.world))
        throw std::runtime_error("combined world transition refused");
    memory.capture_updated(*network,before);
    result.memory_after=memory.root();
    // All allocations and checks precede this nonthrowing commit. No pointer
    // into staged storage escapes; the caller's network allocation is retained.
    std::memcpy(world.network,network.get(),sizeof(*network));
    staged.network=world.network;
    world=staged;
    cells_.swap(memory.cells_);fault_count_.swap(memory.fault_count_);
    std::swap(active_faults_,memory.active_faults_);
    return result;
}
std::string Weights::root() const{
    std::string frame="NM_QMEMORY_TABLEAU_V1";
    append32(frame,static_cast<std::uint32_t>(edges_));
    frame.reserve(frame.size()+cells_.size()*92u);
    for(const auto &cell:cells_){
        frame+=static_cast<char>(5);
        for(unsigned i=0;i<10;i++){
            append32(frame,cell.rows[i].x);
            append32(frame,cell.rows[i].z);
            frame+=static_cast<char>(cell.rows[i].phase);
        }
    }
    return surreal::hex(surreal::sha256(frame));
}
std::string Weights::export_snapshot(std::uint64_t tick) const{
    static const char digits[]="0123456789abcdef";
    std::string out="tick="+std::to_string(tick)+";root="+root()+"\n";
    for(std::size_t i=0;i<cells_.size();i++){
        out+="cell="+std::to_string(i+1)+";tableau=";
        const auto &cell=cells_[i];
        // Canonical 90-byte per cell, independent of pq_row padding.
        for(unsigned j=0;j<10;j++){
            const auto &r=cell.rows[j];
            for(unsigned k=0;k<9;k++){
                unsigned char byte=k<4?static_cast<unsigned char>(r.x>>(8*k)):
                    k<8?static_cast<unsigned char>(r.z>>(8*(k-4))):r.phase;
                out+=digits[byte>>4];out+=digits[byte&15];
            }
        }
        out+='\n';
    }
    return out;
}

}
