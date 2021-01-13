#pragma once

#include <cmath>

using namespace eosio;

/***********************************TOKEN****************************************/
const char* const DEPOSIT = "deposit,";
const int DEPOSIT_LENGTH = strlen(DEPOSIT);
const char* const SWAP = "swap,";
const int SWAP_LENGTH = strlen(SWAP);
const char* const WITHDRAW = "withdraw,";
const int WITHDRAW_LENGTH = strlen(WITHDRAW);

const char LPTOKEN_SYMBOL_BASE[] = "ECHAAA";
const int LPTOKEN_SYMBOL_BASE_LENGTH = sizeof(LPTOKEN_SYMBOL_BASE);
const int LPTOKEN_SYMBOL_MAX  = 3;
const char* const LPTOKEN_SYMBOL = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"; 
const int LPTOKEN_SYMBOL_RADIX = strlen(LPTOKEN_SYMBOL);
const uint64_t LPTOKEN_MAXIMUM_SUPPLY = 1000000000000000000l;

auto ECH_ISSUER = "ech"_n;
const char* const ECH_SYMBOL = "ECH";
const int ECH_SYMBOL_PRECISION = 4;

auto ORDER_STATE_INIT = "init"_n;
auto ORDER_STATE_FINISH = "finish"_n;
uint32_t ORDER_LOCK_SECONDS = 99 * 24 * 3600;
const int ORDER_UNLOCK_RADIO = 3;

const int FEE_BASE = 10000;
const int SWAP_FEE_BURN = 500;      //换币交易的手续费中用于销毁的比例,这里表示 5%
const int SWAP_FEE_LIQUIDITY = 150; //换币交易的手续费中用于奖励给流动性提供者的比例,这里表示 1.5%
const int SWAP_FEE_INVITE1 = 100;   //换币交易的手续费中用于奖励给1级推荐人的比例,这里表示 1%
const int SWAP_FEE_INVITE2 = 50;    //换币交易的手续费中用于奖励给2级推荐人的比例,这里表示 0.5%
const int SWAP_FEE_LIBRARY = 200;   //换币交易的手续费中用于奖励给defi实验室的比例,这里表示2%
const int SWAP_FEE_BACK = 0;        //换币交易的手续费中用于返回流动性池子的比例,这里表示 0%
const int SWAP_FEE_TOTAL = SWAP_FEE_BURN + SWAP_FEE_LIQUIDITY + SWAP_FEE_INVITE1 + SWAP_FEE_INVITE2 + SWAP_FEE_LIBRARY + SWAP_FEE_BACK; //换币交易的总手续费,这里表示10%

const int CONFIG_ID_TOTAL_ORDERS = 1;
const uint64_t DESTROY_BOTTOM = 9900;

const int MAX_EXPIRE_DEAL_NUM = 10;
const int EXPIRE_KEEP_SECONDS = 24 * 3600;

/***********************************MINER****************************************/
const char* const LOCK = "lock,";
const int LOCK_LENGTH = strlen(LOCK);
const char* const MINE = "mine,";
const int MINE_LENGTH = strlen(MINE);

const uint128_t MINE_RADIO = 1000000000000000000l;

const int MAX_SWAPLOGS_NUM = 100; //swaphistory表中最多保存这么多记录
const int MAX_ORDER_NUM = 100; //lockinfo表中最多保存这么多记录
const int MAX_INVITE1_NUM = 100;  //invite1表中最多表留这么多记录
const int MAX_INVITE2_NUM = 100;  //invite2表中最多表留这么多记录

const int CONFIG_ID_TOTAL_SWAPLOGS = 1;
const int CONFIG_ID_ORDER_NUM = 2;
const int CONFIG_ID_INVITE1_NUM = 3;
const int CONFIG_ID_INVITE2_NUM = 4;

#ifdef DEV
auto LPTOKEN_CONTRACT = "lptoken.defi"_n;
auto MINER_CONTRACT = "miner.defi"_n;
auto SWAP_CONTRACT = "swap.defi"_n;
auto ECH_CONTRACT = LPTOKEN_CONTRACT;
auto DEFI_LIBRARY = "library.defi"_n;
#else
auto LPTOKEN_CONTRACT = "lptoken.defi"_n;
auto MINER_CONTRACT = "miner.defi"_n;
auto SWAP_CONTRACT = "swap.defi"_n;
auto ECH_CONTRACT = LPTOKEN_CONTRACT;
auto DEFI_LIBRARY = "library.defi"_n;
#endif
