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
int ghistoryBits = 17; // Number of bits used for Global History (ghr of gshare)
int pcBits = 10;       // Number of bits used for Local History Table (Tournament)
int phistoryBits = 12; // Number of bits used for Path History (ghr of Tournament)
int bpType;            // Branch Prediction Type
int verbose;

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
uint8_t *lht_tournament;
uint8_t *bht_tournament;
uint8_t *ght_tournament;
uint8_t *choice_tournament;
uint64_t pathHistory;     // same as ghr

// custom


//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

/***********************************************gshare functions************************************************/
void init_gshare() {
  //this function initializes BHT and global hisotry register (ghr) for gshare

  int bht_entries = 1 << ghistoryBits;                            // bht_entries = 2^17
  bht_gshare = (uint8_t *)malloc(bht_entries * sizeof(uint8_t));  // 2^17 * 1 byte = 128 KB allocated for bht_gshare
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
  int lht_entries = 1 << pcBits;                                      // lht_entries = 2^10, both LHT and BHT share the same number of entries
  lht_tournament = (uint8_t *)malloc(lht_entries * sizeof(uint8_t));  // 2^10 * 1 byte = 1 KB allocated for bht_tournament                                    
  bht_tournament = (uint8_t *)malloc(lht_entries * sizeof(uint8_t));  // 2^10 * 1 byte = 1 KB allocated for bht_tournament

  int ght_entries = 1 << phistoryBits;                                // ght_entries = 2^12, both GHT and choice prediction table share the same number of entries
  ght_tournament = (uint8_t *)malloc(ght_entries * sizeof(uint8_t));  // 2^12 * 1 byte = 4 KB allocated for ght_tournament
  choice_tournament = (uint8_t *)malloc(ght_entries * sizeof(uint8_t));  // 2^10 * 1 byte = 4 KB allocated for choice_tournament

  int i = 0;

  for (i = 0; i < lht_entries; i++) {       // for every entry of LHT and BHT 
    lht_tournament[i] = WN;
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
  uint32_t lht_entries = 1 << pcBits;           // lht_entries = 2^10
  uint32_t lht_index = pc & (lht_entries - 1);  // lht is indexed by lower 10 bits of pc (masked with 10 1s)
  uint32_t bht_index = lht_tournament[lht_index];  // bht is indexed by lht entry (also 10 bits)

  uint32_t ght_entries = 1 << phistoryBits;                 // ght_entries = 2^12
  uint32_t ght_index = pathHistory & (ght_entries - 1);     // ght (global history table) is indexed by path history bits (12 bits)
  uint32_t choice_index = pathHistory & (ght_entries - 1);  // ght (global history table) is indexed by path history bits (12 bits)

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
  pathHistory= ((pathHistory << 1) | outcome);
}

void cleanup_tournament()
{
  free(lht_tournament);
  free(bht_tournament);
  free(ght_tournament);
  free(choice_tournament);
}

/*********************************************end of tournament predictor functions**********************************************/

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
    break;
  case CUSTOM:
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
    return NOTTAKEN;
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
      return;
    default:
      break;
    }
  }
}
