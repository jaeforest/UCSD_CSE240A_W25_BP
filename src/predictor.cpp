//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "Jaehyung Lim";
const char *studentID = "A14858841";
const char *email = "jal172@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = {"Static", "Gshare",
                         "Tournament", "Custom"};

// define number of bits required for indexing the BHT here.

int bpType;            // Branch Prediction Type
int verbose;

// gshare
int ghistoryBits = 17; // Number of bits used for Global History (ghr of gshare)

// tournament
int pcBits = 13;       // Number of bits used for PC lower bit (Tournament)
int lhtBits = 15;      // Number of bits used for Local History Table (Tournament)
int phistoryBits = 14; // Number of bits used for Path History (ghr of Tournament)

// tage
int table_pcBits[5] = {14, 13, 12, 11, 10};       // Number of bits used for PC lower bit to index each table
int table_ghrBits[4] = {2, 4, 8, 16};             // Number of bits used for ghr bit to index each table (geometric series)

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
// TODO: Add your own Branch Predictor data structures here
//
// gshare
uint8_t *bht_gshare;
uint64_t ghistory;

// tournament
uint16_t *lht_tournament;
uint8_t *bht_tournament;
uint8_t *ght_tournament;
uint8_t *choice_tournament;
uint64_t pathHistory;     // same as ghistory (ghr)

// custom (tage)
uint8_t *t0_table;         // bimodal predictor
uint16_t *t1_table;        // last 2 branches
uint16_t *t2_table;        // last 4 branches
uint16_t *t3_table;        // last 8 branches
uint16_t *t4_table;        // last 16 branches
uint64_t ghr;              // same as ghistory

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

/***********************************************gshare functions************************************************/
void init_gshare() {
  //this function initializes BHT and global hisotry register (ghr) for gshare

  int bht_entries = 1 << ghistoryBits;                            // bht_entries = 2^17
  bht_gshare = (uint8_t *)malloc(bht_entries * sizeof(uint8_t));  // 2^17 * 1 bytes allocated for bht_gshare. In reality, we only use 2 bits for each entry, so 2^17 * 2 = 2^18 = 256 Kbits 
  int i = 0;
  for (i = 0; i < bht_entries; i++)     // for every entry of BHT
  {
    bht_gshare[i] = WN;                 // initializes to WN or 01
  }
  ghistory = 0;
}

uint8_t gshare_predict(uint32_t pc) {
  // this function returns the prediction result by accessing the BHT
  uint32_t bht_entries = 1 << ghistoryBits;         // bht_entries = 2^17
  uint32_t pc_lower_bits = pc & (bht_entries - 1);  // pc masking with 17 1s
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);  // ghistory masking with 17 1s
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;         // xoring pc lower bits and ghr lower bits for indexing
  
  switch (bht_gshare[index]) {  // looks at the bht entry to decide whether branch should be predicted taken or not taken
  case WN:
    return NOTTAKEN;
  case SN:
    return NOTTAKEN;
  case WT:
    return TAKEN;
  case ST:
    return TAKEN;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    return NOTTAKEN;
  }
}

void train_gshare(uint32_t pc, uint8_t outcome) {
  // this function updates the BHT entry based on the actual outcome
  uint32_t bht_entries = 1 << ghistoryBits;         // bht_entries = 2^17
  uint32_t pc_lower_bits = pc & (bht_entries - 1);  // pc masking with 17 1s
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);  // ghistory masking with 17 1s
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;         // xoring pc lower bits and ghr lower bits for indexing

  switch (bht_gshare[index]) {
  case WN:
    bht_gshare[index] = (outcome == TAKEN) ? WT : SN;
    break;
  case SN:
    bht_gshare[index] = (outcome == TAKEN) ? WN : SN;
    break;
  case WT:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WN;
    break;
  case ST:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WT;
    break;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    break;
  }

  // Update history register
  ghistory = ((ghistory << 1) | outcome);
}

void cleanup_gshare()
{
  free(bht_gshare);
}

/***********************************************end of gshare functions************************************************/

/***********************************************tournament predictor functions************************************************/
void init_tournament() {
  // this function initializes BHT and path history register (= ghr) for tournament predictor
  int lht_entries = 1 << pcBits;                                        // lht_entries = 2^13
  int bht_entries = 1 << lhtBits;                                       // bht_entries = 2^15
  lht_tournament = (uint16_t *)malloc(lht_entries * sizeof(uint16_t));  // 2^13 * 2 byte = 16 KB allocated for lht_tournament. In reality, we only use 15 bits for each entry, so 2^13 * 15 = 120 Kbits                                 
  bht_tournament = (uint8_t *)malloc(bht_entries * sizeof(uint8_t));    // 2^15 * 1 byte = 32 KB allocated for bht_tournament. In reality, we only use 2 bits for each entry, so 2^15 * 2 = 64 Kbits  

  int ght_entries = 1 << phistoryBits;                                  // ght_entries = 2^14, both GHT and choice prediction table share the same number of entries
  ght_tournament = (uint8_t *)malloc(ght_entries * sizeof(uint8_t));    // 2^14 * 1 byte = 4 KB allocated for ght_tournament. In reality, we only use 2 bits for each entry, so 2^14 * 2 = 32 Kbits  
  choice_tournament = (uint8_t *)malloc(ght_entries * sizeof(uint8_t));  // 2^14 * 1 byte = 4 KB allocated for choice_tournament. In reality, we only use 2 bits for each entry, so 2^14 * 2 = 32 Kbits  

  int i = 0;

  for (i = 0; i < lht_entries; i++) {       // for every entry of LHT and BHT 
    lht_tournament[i] = 0;                 // initializes to 0        
  }

  for (i = 0; i < bht_entries; i++) {       // for every entry of LHT and BHT 
    bht_tournament[i] = WN;                 // initializes to WN or 01
  }

  for (i = 0; i < ght_entries; i++) {       // for every entry of GHT and choice prediction table 
    ght_tournament[i] = WN;                 // initializes to WN or 01
    choice_tournament[i] = WN;
  }
  pathHistory = 0;
}

uint8_t tournament_predict(uint32_t pc) {
  // this function returns the prediction result by choosing either local or global predictor
  uint32_t lht_entries = 1 << pcBits;           // lht_entries = 2^13
  uint32_t bht_entries = 1 << lhtBits;          // bht_entries = 2^15
  uint32_t lht_index = pc & (lht_entries - 1);  // lht is indexed by lower 13 bits of pc (masked with 13 1s)
  uint32_t bht_index = lht_tournament[lht_index] & (bht_entries - 1);  // bht is indexed by lht entry (15 bits)

  uint32_t ght_entries = 1 << phistoryBits;                 // ght_entries = 2^14
  uint32_t ght_index = pathHistory & (ght_entries - 1);     // ght (global history table) is indexed by path history bits (14 bits)
  uint32_t choice_index = pathHistory & (ght_entries - 1);  // ght (global history table) is indexed by path history bits (14 bits)

  switch ((choice_tournament[choice_index] == WN || SN) ? bht_tournament[bht_index] : ght_tournament[ght_index]) {  // if selected entry of choice is 00 or 01, select local predictor. otherwise, select global predictor
  case WN:
    return NOTTAKEN;
  case SN:
    return NOTTAKEN;
  case WT:
    return TAKEN;
  case ST:
    return TAKEN;
  default:
    printf("Warning: Undefined state of entry!\n");
    return NOTTAKEN;
  }
}

void train_tournament(uint32_t pc, uint8_t outcome) {
  // this function updates tables based on the actual outcome
  uint32_t lht_entries = 1 << pcBits;           // lht_entries = 2^13
  uint32_t bht_entries = 1 << lhtBits;          // bht_entries = 2^15
  uint32_t lht_index = pc & (lht_entries - 1);  // lht is indexed by lower 14 bits of pc (masked with 13 1s)
  uint32_t bht_index = lht_tournament[lht_index] & (bht_entries - 1);  // bht is indexed by lht entry (15 bits)

  uint32_t ght_entries = 1 << phistoryBits;                 // ght_entries = 2^14
  uint32_t ght_index = pathHistory & (ght_entries - 1);     // ght (global history table) is indexed by path history bits (14 bits)
  uint32_t choice_index = pathHistory & (ght_entries - 1);  // ght (global history table) is indexed by path history bits (14 bits)
  
  // update LHT
  lht_tournament[lht_index] = ((lht_tournament[lht_index] << 1) | outcome);   

  // update BHT
  switch (bht_tournament[bht_index]) {                
  case WN:
    bht_tournament[bht_index] = (outcome == TAKEN) ? WT : SN;
    break;
  case SN:
    bht_tournament[bht_index] = (outcome == TAKEN) ? WN : SN;
    break;
  case WT:
    bht_tournament[bht_index] = (outcome == TAKEN) ? ST : WN;
    break;
  case ST:
    bht_tournament[bht_index] = (outcome == TAKEN) ? ST : WT;
    break;
  default:
    printf("Warning: Undefined state of entry in BHT!\n");
    break;
  }

  // update GHT
  switch (ght_tournament[ght_index]) {                
    case WN:
      ght_tournament[ght_index] = (outcome == TAKEN) ? WT : SN;
      break;
    case SN:
      ght_tournament[ght_index] = (outcome == TAKEN) ? WN : SN;
      break;
    case WT:
      ght_tournament[ght_index] = (outcome == TAKEN) ? ST : WN;
      break;
    case ST:
      ght_tournament[ght_index] = (outcome == TAKEN) ? ST : WT;
      break;
    default:
      printf("Warning: Undefined state of entry in GHT!\n");
      break;
  }

  //update choice prediction table
  switch (choice_tournament[choice_index]) {                
    // local predictor
    case WN:
      choice_tournament[choice_index] = (outcome == TAKEN) ? SN : WT;
      break;
    case SN:  
      choice_tournament[choice_index] = (outcome == TAKEN) ? SN : WN;
      break;
    
    // global predictor
    case WT:
      choice_tournament[choice_index] = (outcome == TAKEN) ? ST : WN;
      break;
    case ST:
      choice_tournament[choice_index] = (outcome == TAKEN) ? ST : WT;
      break;
    default:
      printf("Warning: Undefined state of entry in choice table!\n");
      break;
  }

  // update path history (ghr of tournament)
  pathHistory= ((pathHistory << 1) | outcome);        
}

void cleanup_tournament(){
  free(lht_tournament);
  free(bht_tournament);
  free(ght_tournament);
  free(choice_tournament);
}

/*********************************************end of tournament predictor functions**********************************************/

/************************************************* tage predictor functions*****************************************************/
void init_tage() {
  //this function initializes tables for tage predictor 
  int t0_entries = 1 << table_pcBits[0];          // t0_entries = 2^14
  int t1_entries = 1 << table_pcBits[1];          // t1_entries = 2^13
  int t2_entries = 1 << table_pcBits[2];          // t2_entries = 2^12
  int t3_entries = 1 << table_pcBits[3];          // t3_entries = 2^11
  int t4_entries = 1 << table_pcBits[4];          // t4_entries = 2^10

  t0_table = (uint8_t *)malloc(t0_entries * sizeof(uint8_t));     // t0_table = 2^14 * 1 byte = 16 KB. In reality, we only use 2 bits for each entry, so 2^14 * 2 = 32 Kbits 
  t1_table = (uint16_t *)malloc(t1_entries * sizeof(uint16_t));   // t1_table = 2^13 * 2 byte = 16 KB. In reality, we only use 13 bits for each entry, so 2^13 * 13 =  104 Kbits
  t2_table = (uint16_t *)malloc(t2_entries * sizeof(uint16_t));   // t2_table = 2^12 * 2 byte = 8 KB. In reality, we only use 14 bits for each entry, so 2^12 * 14 = 48 Kbits
  t3_table = (uint16_t *)malloc(t3_entries * sizeof(uint16_t));   // t3_table = 2^11 * 2 byte = 4 KB. In reality, we only use 15 bits for each entry, so 2^11 * 15 = 30 Kbits
  t4_table = (uint16_t *)malloc(t4_entries * sizeof(uint16_t));   // t4_table = 2^10 * 2 byte = 2 KB. In reality, we use all 16 bits for each entry, so 2^10 * 16 = 16 Kbits

  int i = 0;
  for (i = 0; i < t0_entries; i++) {    // for every entry of t0 table (bimodal)
    t0_table[i] = WN;                   // initializes to WN or 01
  }

  for (i = 0; i < t1_entries; i++) {      // for every entry of t1 table 
    t1_table[i] = (1 << 11) | (1 << 10);  // initializes to 011 00000000 00 = (3 counter | 8 tag | 2 useful)
  }

  for (i = 0; i < t2_entries; i++) {      // for every entry of t1 table
    t2_table[i] = (1 << 12) | (1 << 11);  // initializes to 011 000000000 00 = (3 counter | 9 tag | 2 useful)
  }

  for (i = 0; i < t3_entries; i++) {      // for every entry of t1 table
    t3_table[i] = (1 << 13) | (1 << 12);  // initializes to 011 0000000000 00 = (3 counter | 10 tag | 2 useful)
  }

  for (i = 0; i < t4_entries; i++) {      // for every entry of t1 table
    t4_table[i] = (1 << 14) | (1 << 13);  // initializes to 011 00000000000 00 = (3 counter | 11 tag | 3 useful)
  }
  ghr = 0;
}

uint8_t tage_predict(uint32_t pc) {
  // this function returns the prediction result

  // indexing each table
  uint32_t t0_pc_lower_bits = pc & (1 << table_pcBits[0] - 1);        // pc masking with 14 1s

  uint32_t t1_pc_lower_bits = pc & ((1 << table_pcBits[1]) - 1);      // pc masking with 13 1s
  uint32_t t1_ghr_lower_bits = ghr & ((1 << table_ghrBits[0]) - 1);   // ghr masking with 2 1s
  uint32_t t1_index = t1_pc_lower_bits ^ t1_ghr_lower_bits;           // xoring pc lower bits and ghr lower bits for indexing t1

  uint32_t t2_pc_lower_bits = pc & ((1 << table_pcBits[2]) - 1);      // pc masking with 12 1s
  uint32_t t2_ghr_lower_bits = ghr & ((1 << table_ghrBits[1]) - 1);   // ghr masking with 4 1s
  uint32_t t2_index = t2_pc_lower_bits ^ t2_ghr_lower_bits;           // xoring pc lower bits and ghr lower bits for indexing t1

  uint32_t t3_pc_lower_bits = pc & ((1 << table_pcBits[3]) - 1);      // pc masking with 11 1s
  uint32_t t3_ghr_lower_bits = ghr & ((1 << table_ghrBits[2]) - 1);   // ghr masking with 8 1s
  uint32_t t3_index = t3_pc_lower_bits ^ t3_ghr_lower_bits;           // xoring pc lower bits and ghr lower bits for indexing t1

  uint32_t t4_pc_lower_bits = pc & ((1 << table_pcBits[4]) - 1);      // pc masking with 10 1s
  uint32_t t4_ghr_lower_bits = ghr & ((1 << table_ghrBits[3]) - 1);   // ghr masking with 16 1s
  uint32_t t4_index = t1_pc_lower_bits ^ t4_ghr_lower_bits;           // xoring pc lower bits and ghr lower bits for indexing t1

  // computing tag by hasing pc lower bits and ghr lower bits
  uint32_t t1_tag = (t1_pc_lower_bits ^ (pc >> 5)) ^ t1_ghr_lower_bits;   
  uint32_t t2_tag = (t2_pc_lower_bits ^ (pc >> 5)) ^ t2_ghr_lower_bits;
  uint32_t t3_tag = (t3_pc_lower_bits ^ (pc >> 5)) ^ t3_ghr_lower_bits;
  uint32_t t4_tag = (t4_pc_lower_bits ^ (pc >> 5)) ^ t4_ghr_lower_bits;

  uint16_t t1_actual_tag = (t1_table[t1_index] >> 2) & ((1 << 8) - 1);    // extracting 8 tag bits [9:2] in t1 entry
  uint16_t t2_actual_tag = (t2_table[t2_index] >> 2) & ((1 << 9) - 1);    // extracting 9 tag bits [10:2] in t1 entry
  uint16_t t3_actual_tag = (t3_table[t3_index] >> 2) & ((1 << 10) - 1);   // extracting 10 tag bits [11:2] in t1 entry
  uint16_t t4_actual_tag = (t4_table[t4_index] >> 2) & ((1 << 11) - 1);   // extracting 11 tag bits [12:2] in t1 entry

  uint8_t t1_prediction = (t1_tag == t1_actual_tag) ? ((t1_table[t1_index] >> 10) & ((1 << 3) - 1)) : t0_table[t0_pc_lower_bits];   // if t1 tag match, use prediction result from t1. otherwise, use prediction result from t0
  uint8_t t2_prediction = (t2_tag == t2_actual_tag) ? ((t2_table[t2_index] >> 11) & ((1 << 3) - 1)) : t1_prediction;                // if t2 tag match, use prediction result from t2. otherwise, go to lower table to look for prediction
  uint8_t t3_prediction = (t3_tag == t3_actual_tag) ? ((t3_table[t3_index] >> 12) & ((1 << 3) - 1)) : t2_prediction;                // if t3 tag match, use prediction result from t3. otherwise, go to lower table to look for prediction
  uint8_t t4_prediction = (t4_tag == t4_actual_tag) ? ((t4_table[t4_index] >> 13) & ((1 << 3) - 1)) : t3_prediction;                // if t4 tag match, use prediction result from t4. otherwise, go to lower table to look for prediction

  switch (t4_prediction) {  // looks at the bht entry to decide whether branch should be predicted taken or not taken
  case DNT:
    return NOTTAKEN;
  case LNT:
    return NOTTAKEN;
  case MNT:
    return NOTTAKEN;
  case BNT:
    return NOTTAKEN;
  case DT:
    return TAKEN;
  case LT:
    return TAKEN;
  case MT:
    return TAKEN;
  case BT:
    return TAKEN;
  default:
    printf("Warning: Undefined state of entry in tables!\n");
    return NOTTAKEN;
  }
}

void train_tage(uint32_t pc, uint8_t outcome) {
  // this function updates the BHT entry based on the actual outcome
  uint32_t bht_entries = 1 << ghistoryBits;         // bht_entries = 2^17
  uint32_t pc_lower_bits = pc & (bht_entries - 1);  // pc masking with 17 1s
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);  // ghistory masking with 17 1s
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;         // xoring pc lower bits and ghr lower bits for indexing

  switch (bht_gshare[index]) {
  case WN:
    bht_gshare[index] = (outcome == TAKEN) ? WT : SN;
    break;
  case SN:
    bht_gshare[index] = (outcome == TAKEN) ? WN : SN;
    break;
  case WT:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WN;
    break;
  case ST:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WT;
    break;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    break;
  }

  // Update history register
  ghistory = ((ghistory << 1) | outcome);
}

void cleanup_tage()
{
  free(t0_table);
  free(t1_table);
  free(t2_table);
  free(t3_table);
  free(t4_table);
}

/*********************************************end of tage predictor functions **********************************************/


void init_predictor()
{
  switch (bpType)
  {
  case STATIC:
    break;
  case GSHARE:
    init_gshare();
    break;
  case TOURNAMENT:
    init_tournament();
    break;
  case CUSTOM:
    init_tage();
    break;
  default:
    break;
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint32_t make_prediction(uint32_t pc, uint32_t target, uint32_t direct)
{

  // Make a prediction based on the bpType
  switch (bpType)
  {
  case STATIC:
    return TAKEN;
  case GSHARE:
    return gshare_predict(pc);
  case TOURNAMENT:
    return tournament_predict(pc);
  case CUSTOM:
    return tage_predict(pc);
  default:
    break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void train_predictor(uint32_t pc, uint32_t target, uint32_t outcome, uint32_t condition, uint32_t call, uint32_t ret, uint32_t direct)
{
  if (condition)
  {
    switch (bpType)
    {
    case STATIC:
      return;
    case GSHARE:
      return train_gshare(pc, outcome);
    case TOURNAMENT:
      return train_tournament(pc, outcome);
    case CUSTOM:
      return train_tage(pc, outcome);
    default:
      break;
    }
  }
}
