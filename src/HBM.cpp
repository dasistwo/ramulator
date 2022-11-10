#include "HBM.h"
#include "DRAM.h"

#include <vector>
#include <functional>
#include <cassert>

using namespace std;
using namespace ramulator;

string HBM::standard_name = "HBM";
string HBM::level_str [int(Level::MAX)] = {"Ch", "Ra", "Bg", "Ba", "Ro", "Co"};

map<string, enum HBM::Org> HBM::org_map = {
    {"HBM_1Gb_Legacy", HBM::Org::HBM_1Gb_Legacy},
    {"HBM_2Gb_Legacy", HBM::Org::HBM_2Gb_Legacy},
    {"HBM_4Gb_Legacy", HBM::Org::HBM_4Gb_Legacy},
    {"HBM_2Gb_PC", HBM::Org::HBM_2Gb_PC},
    {"HBM_4Gb_PC", HBM::Org::HBM_4Gb_PC},
    {"HBM_6Gb_PC", HBM::Org::HBM_6Gb_PC},
    {"HBM_8Gb_PC", HBM::Org::HBM_8Gb_PC},
    {"HBM_8Gb_8High_PC", HBM::Org::HBM_8Gb_8High_PC},
    {"HBM_8Gb_12High_PC", HBM::Org::HBM_8Gb_12High_PC},
    {"HBM_12Gb_8High_PC", HBM::Org::HBM_12Gb_8High_PC},
    {"HBM_12Gb_12High_PC", HBM::Org::HBM_12Gb_12High_PC},
    {"HBM_16Gb_8High_PC", HBM::Org::HBM_16Gb_8High_PC},
    {"HBM_16Gb_12High_PC", HBM::Org::HBM_16Gb_12High_PC}    
};

map<string, enum HBM::Speed> HBM::speed_map = {
    {"HBM_1Gbps", HBM::Speed::HBM_1Gbps},
    {"HBM_1.6Gbps", HBM::Speed::HBM_1_6Gbps},
    {"HBM_2Gbps", HBM::Speed::HBM_2Gbps},
    {"HBM_2.4Gbps", HBM::Speed::HBM_2_4Gbps},
    {"HBM_3.2Gbps", HBM::Speed::HBM_3_2Gbps},
};

HBM::HBM(Org org, Speed speed)
    : org_entry(org_table[int(org)]),
    speed_entry(speed_table[int(speed)]),
    read_latency(speed_entry.nCL + speed_entry.nBL)
{
    /* HBM2 reconfiguration */
    organization = org;
    if (org > HBM::Org::HBM_4Gb_Legacy) channel_width >>= 1;
    init_speed();
    init_prereq();
    init_rowhit(); // SAUGATA: added row hit function
    init_rowopen();
    init_lambda();
    init_timing();
}

HBM::HBM(const string& org_str, const string& speed_str) :
    HBM(org_map[org_str], speed_map[speed_str])
{
}

void HBM::set_channel_number(int& channel) {
  if (organization > HBM::Org::HBM_4Gb_Legacy) channel <<= 1;
  org_entry.count[int(Level::Channel)] = channel;
}

void HBM::set_rank_number(int& rank) {
  org_entry.count[int(Level::Rank)] = rank;
}


void HBM::init_speed()
{
    const static int RFC_TABLE[int(Speed::MAX)][int(Org::MAX)] = {
        {55, 80, 130, 175, 225},
        {35, 50, 82,  110, 141},
        {28, 40, 65,  88,  113},
        {23, 34, 55,  73,  94},
        {20, 29, 47,  63,  81},
        {18, 25, 41,  55,  71}
    };
    const static int REFISB_TABLE[int(Speed::MAX)][4] = {
        {244, 122, 61, 41},
        {153, 77,  39, 26},
        {122, 61,  31, 21},
        {102, 51,  26, 17},
        {88,  44,  22, 15},
        {77,  39,  20, 13}
    };
    const static int XS_TABLE[int(Speed::MAX)][int(Org::MAX)] = {
        {60, 85, 135, 180, 230},
        {40, 55, 87,  115, 146},
        {33, 45, 70,  93,  118},
        {28, 39, 60,  78,  99},
        {25, 34, 52,  68,  86},
        {23, 30, 46,  60,  76}
    };

    int speed = 0, density = 0, banks = 0;
    switch (speed_entry.rate) {
        case 1000: speed = 0; break;
        case 1600: speed = 1; break;
        case 2000: speed = 2; break;
        case 2400: speed = 3; break;
        case 2800: speed = 4; break;
        case 3200: speed = 5; break;
        default: assert(false);
    };
    banks = org_entry.count[int(Level::Bank)] * org_entry.count[int(Level::BankGroup)];
    banks = (banks - 8) >> 3;
    
    density = (org_entry.size >> 10);
    if (org_entry.dq == 64) density <<= 1;
    
    switch (density){
        case 1 : density = 0; break;
        case 2 : density = 1; break;
        case 4 : density = 2; break;
        case 6 :
        case 8 : density = 3; break;
        case 12:
        case 16:
        case 18:
        case 24: density = 4; break;
        default: assert(false);
    }
    speed_entry.nRFC = RFC_TABLE[speed][density];
    speed_entry.nREFISB = REFISB_TABLE[speed][banks];
    speed_entry.nXS = XS_TABLE[speed][density];
}


void HBM::init_prereq()
{
    // RD
    prereq[int(Level::Rank)][int(Command::RD)] = [] (DRAM<HBM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::MAX;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SRX;
            default: assert(false);
        }};
    prereq[int(Level::Bank)][int(Command::RD)] = [] (DRAM<HBM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::Closed): return Command::ACT;
            case int(State::Opened):
                if (node->row_state.find(id) != node->row_state.end())
                    return cmd;
                else return Command::PRE;
            default: assert(false);
        }};

    // WR
    prereq[int(Level::Rank)][int(Command::WR)] = prereq[int(Level::Rank)][int(Command::RD)];
    prereq[int(Level::Bank)][int(Command::WR)] = prereq[int(Level::Bank)][int(Command::RD)];

    // REF
    prereq[int(Level::Rank)][int(Command::REF)] = [] (DRAM<HBM>* node, Command cmd, int id) {
        for (auto bg : node->children)
            for (auto bank: bg->children) {
                if (bank->state == State::Closed)
                    continue;
                return Command::PREA;
            }
        return Command::REF;};

    // REFSB
    prereq[int(Level::Bank)][int(Command::REFSB)] = [] (DRAM<HBM>* node, Command cmd, int id) {
        if (node->state == State::Closed) return Command::REFSB;
        return Command::PRE;};

    // PD
    prereq[int(Level::Rank)][int(Command::PDE)] = [] (DRAM<HBM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::PDE;
            case int(State::ActPowerDown): return Command::PDE;
            case int(State::PrePowerDown): return Command::PDE;
            case int(State::SelfRefresh): return Command::SRX;
            default: assert(false);
        }};

    // SR
    prereq[int(Level::Rank)][int(Command::SRE)] = [] (DRAM<HBM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::PowerUp): return Command::SRE;
            case int(State::ActPowerDown): return Command::PDX;
            case int(State::PrePowerDown): return Command::PDX;
            case int(State::SelfRefresh): return Command::SRE;
            default: assert(false);
        }};
}

// SAUGATA: added row hit check functions to see if the desired location is currently open
void HBM::init_rowhit()
{
    // RD
    rowhit[int(Level::Bank)][int(Command::RD)] = [] (DRAM<HBM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::Closed): return false;
            case int(State::Opened):
                if (node->row_state.find(id) != node->row_state.end())
                    return true;
                return false;
            default: assert(false);
        }};

    // WR
    rowhit[int(Level::Bank)][int(Command::WR)] = rowhit[int(Level::Bank)][int(Command::RD)];
}

void HBM::init_rowopen()
{
    // RD
    rowopen[int(Level::Bank)][int(Command::RD)] = [] (DRAM<HBM>* node, Command cmd, int id) {
        switch (int(node->state)) {
            case int(State::Closed): return false;
            case int(State::Opened): return true;
            default: assert(false);
        }};

    // WR
    rowopen[int(Level::Bank)][int(Command::WR)] = rowopen[int(Level::Bank)][int(Command::RD)];
}

void HBM::init_lambda()
{
    lambda[int(Level::Bank)][int(Command::ACT)] = [] (DRAM<HBM>* node, int id) {
        node->state = State::Opened;
        node->row_state[id] = State::Opened;};
    lambda[int(Level::Bank)][int(Command::PRE)] = [] (DRAM<HBM>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Rank)][int(Command::PREA)] = [] (DRAM<HBM>* node, int id) {
        for (auto bg : node->children)
            for (auto bank : bg->children) {
                bank->state = State::Closed;
                bank->row_state.clear();
            }};
    lambda[int(Level::Rank)][int(Command::REF)] = [] (DRAM<HBM>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::RD)] = [] (DRAM<HBM>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::WR)] = [] (DRAM<HBM>* node, int id) {};
    lambda[int(Level::Bank)][int(Command::RDA)] = [] (DRAM<HBM>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Bank)][int(Command::WRA)] = [] (DRAM<HBM>* node, int id) {
        node->state = State::Closed;
        node->row_state.clear();};
    lambda[int(Level::Rank)][int(Command::PDE)] = [] (DRAM<HBM>* node, int id) {
        for (auto bg : node->children)
            for (auto bank : bg->children) {
                if (bank->state == State::Closed)
                    continue;
                node->state = State::ActPowerDown;
                return;
            }
        node->state = State::PrePowerDown;};
    lambda[int(Level::Rank)][int(Command::PDX)] = [] (DRAM<HBM>* node, int id) {
        node->state = State::PowerUp;};
    lambda[int(Level::Rank)][int(Command::SRE)] = [] (DRAM<HBM>* node, int id) {
        node->state = State::SelfRefresh;};
    lambda[int(Level::Rank)][int(Command::SRX)] = [] (DRAM<HBM>* node, int id) {
        node->state = State::PowerUp;};
}


void HBM::init_timing()
{
    SpeedEntry& s = speed_entry;
    vector<TimingEntry> *t;

    /*** Channel ***/
    t = timing[int(Level::Channel)];

    // CAS <-> CAS
    t[int(Command::RD)].push_back({Command::RD, 1, s.nBL});
    t[int(Command::RD)].push_back({Command::RDA, 1, s.nBL});
    t[int(Command::RDA)].push_back({Command::RD, 1, s.nBL});
    t[int(Command::RDA)].push_back({Command::RDA, 1, s.nBL});
    t[int(Command::WR)].push_back({Command::WR, 1, s.nBL});
    t[int(Command::WR)].push_back({Command::WRA, 1, s.nBL});
    t[int(Command::WRA)].push_back({Command::WR, 1, s.nBL});
    t[int(Command::WRA)].push_back({Command::WRA, 1, s.nBL});


    /*** Rank ***/
    t = timing[int(Level::Rank)];

    // CAS <-> CAS
    t[int(Command::RD)].push_back({Command::RD, 1, s.nCCDS});
    t[int(Command::RD)].push_back({Command::RDA, 1, s.nCCDS});
    t[int(Command::RDA)].push_back({Command::RD, 1, s.nCCDS});
    t[int(Command::RDA)].push_back({Command::RDA, 1, s.nCCDS});
    t[int(Command::WR)].push_back({Command::WR, 1, s.nCCDS});
    t[int(Command::WR)].push_back({Command::WRA, 1, s.nCCDS});
    t[int(Command::WRA)].push_back({Command::WR, 1, s.nCCDS});
    t[int(Command::WRA)].push_back({Command::WRA, 1, s.nCCDS});
    t[int(Command::RD)].push_back({Command::WR, 1, s.nCL + s.nCCDS + 2 - s.nCWL});
    t[int(Command::RD)].push_back({Command::WRA, 1, s.nCL + s.nCCDS + 2 - s.nCWL});
    t[int(Command::RDA)].push_back({Command::WR, 1, s.nCL + s.nCCDS + 2 - s.nCWL});
    t[int(Command::RDA)].push_back({Command::WRA, 1, s.nCL + s.nCCDS + 2 - s.nCWL});
    t[int(Command::WR)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTRS});
    t[int(Command::WR)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nWTRS});
    t[int(Command::WRA)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTRS});
    t[int(Command::WRA)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nWTRS});

    t[int(Command::RD)].push_back({Command::PREA, 1, s.nRTP});
    t[int(Command::WR)].push_back({Command::PREA, 1, s.nCWL + s.nBL + s.nWR});

    // CAS <-> PD
    t[int(Command::RD)].push_back({Command::PDE, 1, s.nCL + s.nBL + 1});
    t[int(Command::RDA)].push_back({Command::PDE, 1, s.nCL + s.nBL + 1});
    t[int(Command::WR)].push_back({Command::PDE, 1, s.nCWL + s.nBL + s.nWR});
    t[int(Command::WRA)].push_back({Command::PDE, 1, s.nCWL + s.nBL + s.nWR + 1}); // +1 for pre
    t[int(Command::PDX)].push_back({Command::RD, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::RDA, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::WR, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::WRA, 1, s.nXP});

    // CAS <-> SR: none (all banks have to be precharged)

    // RAS <-> RAS
    t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRRDS});
    t[int(Command::ACT)].push_back({Command::ACT, 4, s.nFAW});
    t[int(Command::ACT)].push_back({Command::PREA, 1, s.nRAS});
    t[int(Command::PREA)].push_back({Command::ACT, 1, s.nRP});

    // RAS <-> REF
    t[int(Command::PRE)].push_back({Command::REF, 1, s.nRP});
    t[int(Command::PREA)].push_back({Command::REF, 1, s.nRP});
    t[int(Command::REF)].push_back({Command::ACT, 1, s.nRFC});

    // RAS <-> PD
    t[int(Command::ACT)].push_back({Command::PDE, 1, 1});
    t[int(Command::PDX)].push_back({Command::ACT, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::PRE, 1, s.nXP});
    t[int(Command::PDX)].push_back({Command::PREA, 1, s.nXP});

    // RAS <-> SR
    t[int(Command::PRE)].push_back({Command::SRE, 1, s.nRP});
    t[int(Command::PREA)].push_back({Command::SRE, 1, s.nRP});
    t[int(Command::SRX)].push_back({Command::ACT, 1, s.nXS});

    // REF <-> REF
    t[int(Command::REF)].push_back({Command::REF, 1, s.nRFC});

    // REF <-> PD
    t[int(Command::REF)].push_back({Command::PDE, 1, 1});
    t[int(Command::PDX)].push_back({Command::REF, 1, s.nXP});

    // REF <-> SR
    t[int(Command::SRX)].push_back({Command::REF, 1, s.nXS});

    // PD <-> PD
    t[int(Command::PDE)].push_back({Command::PDX, 1, s.nPD});
    t[int(Command::PDX)].push_back({Command::PDE, 1, s.nXP});

    // PD <-> SR
    t[int(Command::PDX)].push_back({Command::SRE, 1, s.nXP});
    t[int(Command::SRX)].push_back({Command::PDE, 1, s.nXS});

    // SR <-> SR
    t[int(Command::SRE)].push_back({Command::SRX, 1, s.nCKESR});
    t[int(Command::SRX)].push_back({Command::SRE, 1, s.nXS});

    /*** Bank Group ***/
    t = timing[int(Level::BankGroup)];
    // CAS <-> CAS
    t[int(Command::RD)].push_back({Command::RD, 1, s.nCCDL});
    t[int(Command::RD)].push_back({Command::RDA, 1, s.nCCDL});
    t[int(Command::RDA)].push_back({Command::RD, 1, s.nCCDL});
    t[int(Command::RDA)].push_back({Command::RDA, 1, s.nCCDL});
    t[int(Command::WR)].push_back({Command::WR, 1, s.nCCDL});
    t[int(Command::WR)].push_back({Command::WRA, 1, s.nCCDL});
    t[int(Command::WRA)].push_back({Command::WR, 1, s.nCCDL});
    t[int(Command::WRA)].push_back({Command::WRA, 1, s.nCCDL});
    t[int(Command::WR)].push_back({Command::WR, 1, s.nCCDL});
    t[int(Command::WR)].push_back({Command::WRA, 1, s.nCCDL});
    t[int(Command::WRA)].push_back({Command::WR, 1, s.nCCDL});
    t[int(Command::WRA)].push_back({Command::WRA, 1, s.nCCDL});
    t[int(Command::WR)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTRL});
    t[int(Command::WR)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nWTRL});
    t[int(Command::WRA)].push_back({Command::RD, 1, s.nCWL + s.nBL + s.nWTRL});
    t[int(Command::WRA)].push_back({Command::RDA, 1, s.nCWL + s.nBL + s.nWTRL});

    // RAS <-> RAS
    t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRRDL});

    /*** Bank ***/
    t = timing[int(Level::Bank)];

    // CAS <-> RAS
    t[int(Command::ACT)].push_back({Command::RD, 1, s.nRCDR});
    t[int(Command::ACT)].push_back({Command::RDA, 1, s.nRCDR});
    t[int(Command::ACT)].push_back({Command::WR, 1, s.nRCDW});
    t[int(Command::ACT)].push_back({Command::WRA, 1, s.nRCDW});

    t[int(Command::RD)].push_back({Command::PRE, 1, s.nRTP});
    t[int(Command::WR)].push_back({Command::PRE, 1, s.nCWL + s.nBL + s.nWR});

    t[int(Command::RDA)].push_back({Command::ACT, 1, s.nRTP + s.nRP});
    t[int(Command::WRA)].push_back({Command::ACT, 1, s.nCWL + s.nBL + s.nWR + s.nRP});

    // RAS <-> RAS
    t[int(Command::ACT)].push_back({Command::ACT, 1, s.nRC});
    t[int(Command::ACT)].push_back({Command::PRE, 1, s.nRAS});
    t[int(Command::PRE)].push_back({Command::ACT, 1, s.nRP});

    // REFSB
    t[int(Command::PRE)].push_back({Command::REFSB, 1, s.nRP});
    t[int(Command::REFSB)].push_back({Command::REFSB, 1, s.nRFC});
    t[int(Command::REFSB)].push_back({Command::ACT, 1, s.nRFC});
}
