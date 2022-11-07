#ifndef __HBM_H
#define __HBM_H

#include "DRAM.h"
#include "Request.h"
#include <vector>
#include <functional>

using namespace std;

namespace ramulator
{

class HBM
{
public:
    static string standard_name;
    enum class Org;
    enum class Speed;
    HBM(Org org, Speed speed);
    HBM(const string& org_str, const string& speed_str);

    static map<string, enum Org> org_map;
    static map<string, enum Speed> speed_map;

    /* Level */
    enum class Level : int
    {
        Channel, Rank, BankGroup, Bank, Row, Column, MAX
    };
    
    static std::string level_str [int(Level::MAX)];

    /* Command */
    enum class Command : int
    {
        ACT, PRE,   PREA,
        RD,  WR,    RDA, WRA,
        REF, REFSB, PDE, PDX,  SRE, SRX,
        MAX
    };

    // REFSB and REF is not compatible, choose one or the other.
    // REFSB can be issued to banks in any order, as long as REFISB
    // is satisfied for all banks

    string command_name[int(Command::MAX)] = {
        "ACT", "PRE",   "PREA",
        "RD",  "WR",    "RDA",  "WRA",
        "REF", "REFSB", "PDE",  "PDX",  "SRE", "SRX"
    };

    Level scope[int(Command::MAX)] = {
        Level::Row,    Level::Bank,   Level::Rank,
        Level::Column, Level::Column, Level::Column, Level::Column,
        Level::Rank,   Level::Bank,   Level::Rank,   Level::Rank,   Level::Rank,   Level::Rank
    };

    bool is_opening(Command cmd)
    {
        switch(int(cmd)) {
            case int(Command::ACT):
                return true;
            default:
                return false;
        }
    }

    bool is_accessing(Command cmd)
    {
        switch(int(cmd)) {
            case int(Command::RD):
            case int(Command::WR):
            case int(Command::RDA):
            case int(Command::WRA):
                return true;
            default:
                return false;
        }
    }

    bool is_closing(Command cmd)
    {
        switch(int(cmd)) {
            case int(Command::RDA):
            case int(Command::WRA):
            case int(Command::PRE):
            case int(Command::PREA):
                return true;
            default:
                return false;
        }
    }

    bool is_refreshing(Command cmd)
    {
        switch(int(cmd)) {
            case int(Command::REF):
            case int(Command::REFSB):
                return true;
            default:
                return false;
        }
    }

    /* State */
    enum class State : int
    {
        Opened, Closed, PowerUp, ActPowerDown, PrePowerDown, SelfRefresh, MAX
    } start[int(Level::MAX)] = {
        State::MAX, State::PowerUp, State::MAX, State::Closed, State::Closed, State::MAX
    };

    /* Translate */
    Command translate[int(Request::Type::MAX)] = {
        Command::RD,  Command::WR,
        Command::REF, Command::PDE, Command::SRE
    };

    /* Prereq */
    function<Command(DRAM<HBM>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    // SAUGATA: added function object container for row hit status
    /* Row hit */
    function<bool(DRAM<HBM>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];
    function<bool(DRAM<HBM>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

    /* Timing */
    struct TimingEntry
    {
        Command cmd;
        int dist;
        int val;
        bool sibling;
    };
    vector<TimingEntry> timing[int(Level::MAX)][int(Command::MAX)];

    /* Lambda */
    function<void(DRAM<HBM>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    /* Organization */
    enum class Org : int
    { // per channel density here. Each stack comes with 8 channels
      // PC is Pseudo Channel
        HBM_1Gb_Legacy,     HBM_2Gb_Legacy,     HBM_4Gb_Legacy,
        HBM_2Gb_PC,         HBM_4Gb_PC,         HBM_6Gb_PC,     HBM_8Gb_PC,
        HBM_8Gb_8High_PC,   HBM_8Gb_12High_PC,
        HBM_12Gb_8High_PC,  HBM_12Gb_12High_PC,
        HBM_16Gb_8High_PC,  HBM_16Gb_12High_PC,
        MAX
    };

    /* The bank group configuration of the HBM is as follows:
     * 8 banks: 4 BG * 2 banks per BG
     * 16 banks: 4 BG * 4 banks per BG
     * 32 banks: 8 BG * 4 banks per BG
     * 48 banks: 12 BG * 4 banks per BG
     * +1 in the Column is because of the burst length
     * BL4 is treated as 2 consecutive access with BL2
     */

    struct OrgEntry {
        int size;
        int dq;
        int count[int(Level::MAX)]; // Channel, Rank, BankGroup, Bank, Row, Column
    } org_table[int(Org::MAX)] = {
        {1<<10, 128, {0, 0, 4,  2, 1<<13, 1<<(6+1)}},  // HBM_1Gb_Legacy
        {2<<10, 128, {0, 0, 4,  2, 1<<14, 1<<(6+1)}},  // HBM_2Gb_Legacy
        {4<<10, 128, {0, 0, 4,  4, 1<<14, 1<<(6+1)}},  // HBM_4Gb_Legacy

        {2<<10, 64,  {0, 0, 4,  2, 1<<14, 1<<(5+1)}},  // HBM_2Gb_PC
        {4<<10, 64,  {0, 0, 4,  4, 1<<14, 1<<(5+1)}},  // HBM_4Gb_PC
        {6<<10, 64,  {0, 0, 4,  4, 3<<13, 1<<(5+1)}},  // HBM_6Gb_PC
        {8<<10, 64,  {0, 0, 4,  4, 1<<15, 1<<(5+1)}},  // HBM_8Gb_PC

        {8<<10, 64,  {0, 0, 8,  4, 1<<14, 1<<(5+1)}},  // HBM_8Gb_8High_PC
        {4<<10, 64,  {0, 0, 12, 4, 1<<14, 1<<(5+1)}},  // HBM_8Gb_12High_PC

        {1<<10, 64,  {0, 0, 8,  4, 3<<13, 1<<(5+1)}},  // HBM_12Gb_8High_PC
        {2<<10, 64,  {0, 0, 12, 4, 3<<13, 1<<(5+1)}},  // HBM_12Gb_12High_PC

        {4<<10, 64,  {0, 0, 8,  4, 1<<15, 1<<(5+1)}},  // HBM_16Gb_8High_PC
        {4<<10, 64,  {0, 0, 12, 4, 1<<15, 1<<(5+1)}},  // HBM_16Gb_12High_PC
    }, org_entry;

    void set_channel_number(int channel);
    void set_rank_number(int rank);

    /* Speed */
    enum class Speed : int
    {
        HBM_1Gbps,
        HBM_1_6Gbps,
        HBM_2Gbps,
        HBM_2_4Gbps,
        HBM_2_8Gbps,
        HBM_3_2Gbps,
        MAX
    };

    int prefetch_size = 4; // burst length could be 2 and 4 (choose 4 here), 2n prefetch
    int channel_width = 128;

    // This assumes BL4
    struct SpeedEntry {
        int rate;
        double freq, tCK;
        int nBL, nCCDS, nCCDL;
        int nCL, nRCDR, nRCDW, nRP, nCWL;
        int nRAS, nRC;
        int nRTP, nWTRS, nWTRL, nWR;
        int nRRDS, nRRDL, nFAW;
        int nRFC, nREFI, nREFISB;
        int nPD, nXP;
        int nCKESR, nXS;
    } speed_table[int(Speed::MAX)] = {
      // Rate,  freq,   tCK, nBL, nCCDS, nCCDL, nCL, nRCDR, nRCDW, nRP, nCWL, nRAS, nRC, nRTP, nWTRS, nWTRL, nWR, nRRDS, nRRDL, nFAW,  nRFC, nREFI, nREFISB,  nPD,  nXP, nCKESR, nXS 
        {1000,   500,   2.0,   2,     2,     4,   7,     7,     6,   7,    4,   17,  24,    7,     2,     4,   8,     4,     5,   20,     0,  1950,       0,    5,    5,      5,   0},
        {1600,   800,  1.25,   2,     2,     4,  12,    12,    10,  12,    7,   28,  39,   12,     4,     7,  13,     7,     8,   32,     0,  3120,       0,    8,    8,      8,   0},
        {2000,  1000,   1.0,   2,     2,     4,  14,    14,    12,  14,    8,   34,  48,   14,     4,     8,  16,     8,    10,   40,     0,  3900,       0,   10,   10,     10,   0},
        {2400,  1200, 0.833,   2,     2,     4,  17,    17,    15,  17,   10,   41,  58,   17,     5,    10,  20,    10,    12,   48,     0,  4680,       0,   12,   12,     12,   0},
        {2800,  1400, 0.714,   2,     2,     4,  20,    20,    17,  20,   12,   48,  68,   20,     6,    12,  23,    12,    14,   56,     0,  5460,       0,   14,   14,     14,   0},
        {3200,  1600, 0.625,   2,     2,     5,  23,    23,    20,  23,   13,   55,  77,   23,     7,    13,  26,    13,    16,   64,     0,  6240,       0,   16,   16,     16,   0}
    }, speed_entry;

    int read_latency;

private:
    void init_speed();
    void init_lambda();
    void init_prereq();
    void init_rowhit();  // SAUGATA: added function to check for row hits
    void init_rowopen();
    void init_timing();
};

} /*namespace ramulator*/

#endif /*__HBM_H*/
